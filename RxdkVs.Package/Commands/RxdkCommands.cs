using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.Threading;
using RxdkVs.Package.Services;
using RxdkVs.Package.ToolWindow;
using Task = System.Threading.Tasks.Task;

namespace RxdkVs.Package.Commands
{
    /// <summary>
    /// Binds every RXDK command (from Commands/CommandIds.cs, declared in RxdkPackage.vsct) to a
    /// handler on the OleMenuCommandService, and implements the handlers. Build/Deploy/Run/Reboot
    /// shell out to Rxdk.Cli.exe via <see cref="CliRunner"/>; folder/doc commands open Explorer or
    /// a browser; Debug delegates to the VS debugger (which routes the "xbox" launch config to the
    /// Debug Adapter Host → Rxdk.Dap.exe).
    ///
    /// This is the C# analog of RXDK-VSCode's extension.ts command registration.
    /// </summary>
    internal sealed class RxdkCommands
    {
        private readonly RxdkPackage _package;
        private readonly CliRunner _cli;

        private RxdkCommands(RxdkPackage package)
        {
            _package = package;
            _cli = new CliRunner(package);
        }

        public static async Task InitializeAsync(RxdkPackage package)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var instance = new RxdkCommands(package);

            var commandService = (OleMenuCommandService)await package.GetServiceAsync(typeof(IMenuCommandService));
            if (commandService == null)
            {
                return;
            }
            instance.RegisterAll(commandService);
        }

        private void RegisterAll(OleMenuCommandService svc)
        {
            void Add(int id, Func<Task> handler, EventHandler beforeQueryStatus = null)
            {
                var cmdId = new CommandID(RxdkPackageGuids.CommandSet, id);
                var cmd = new OleMenuCommand((s, e) => _package.JoinableTaskFactory.RunAsync(handler).FileAndForget("rxdk/command"), cmdId);
                if (beforeQueryStatus != null) cmd.BeforeQueryStatus += beforeQueryStatus;
                svc.AddCommand(cmd);
            }

            // Context-menu visibility: Deploy for any RXDK Xbox project; Remove DXT only for DXTs.
            void OnQueryDeploy(object s, EventArgs e)
            {
                ThreadHelper.ThrowIfNotOnUIThread();
                ((OleMenuCommand)s).Visible = Services.XboxDebugLauncher.TryGetSelectedProject(out var sel) && sel.IsXbox;
            }
            void OnQueryRemoveDxt(object s, EventArgs e)
            {
                ThreadHelper.ThrowIfNotOnUIThread();
                ((OleMenuCommand)s).Visible = Services.XboxDebugLauncher.TryGetSelectedProject(out var sel) && sel.IsXbox && sel.IsDxt;
            }

            Add(CommandIds.CmdBuild, () => RunCliAsync("build"));
            Add(CommandIds.CmdDeploy, () => RunCliAsync("deploy"));
            Add(CommandIds.CmdRun, () => RunCliAsync("run"));
            Add(CommandIds.CmdRebootConsole, () => RunCliAsync("reboot", requiresProject: false));
            Add(CommandIds.CmdRemoveDxt, RemoveDxtAsync, OnQueryRemoveDxt);
            Add(CommandIds.CmdDeployProject, DeployProjectAsync, OnQueryDeploy);
            Add(CommandIds.CmdSetXboxIp, SetXboxIpAsync);
            Add(CommandIds.CmdDebug, DebugAsync);
            Add(CommandIds.CmdDebugPrebuiltXbe, NewPrebuiltXbeAsync);
            Add(CommandIds.CmdNewProject, NewProjectAsync);
            Add(CommandIds.CmdImportProject, ImportProjectAsync);
            Add(CommandIds.CmdShowToolWindow, ShowToolWindowAsync);
            Add(CommandIds.CmdOpenSdkFolder, () => OpenFolderAsync(ToolLocator.StagedSdkRoot));
            Add(CommandIds.CmdOpenToolsFolder, () => OpenFolderAsync(ToolLocator.StagedToolsRoot));
            Add(CommandIds.CmdOpenDocsFolder, () => OpenFolderAsync(ToolLocator.StagedDocsRoot));
            Add(CommandIds.CmdOpenSdkDocs, () => OpenDocsAsync("sdk"));
            Add(CommandIds.CmdOpenExtensionDocs, () => OpenDocsAsync("rxdk-vs"));
            Add(CommandIds.CmdFetchLatestSdk, () => RunCliAsync("install-sdk", requiresProject: false));
            Add(CommandIds.CmdInstallDotNet, InstallDotNetAsync);
            Add(CommandIds.CmdLaunchXbwatson, () => LaunchHostToolAsync("xbwatson"));
            Add(CommandIds.CmdLaunchXbNeighborhood, () => LaunchHostToolAsync("xbNeighborhood"));
            Add(CommandIds.CmdOpenXboxNeighborhood, OpenXboxNeighborhoodAsync);
            Add(CommandIds.CmdCycleGlobalsScope, CycleGlobalsScopeAsync);
            Add(CommandIds.CmdSetBuildType, SetBuildTypeAsync);
            Add(CommandIds.CmdSetupPrerequisites, SetupPrerequisitesAsync);
            Add(CommandIds.CmdOpenSettings, OpenSettingsAsync);
        }

        // ---- CLI-backed commands ----

        private async Task RunCliAsync(string verb, bool requiresProject = true, params string[] extraArgs)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();

            var args = new List<string> { verb };
            string projectRoot = null;

            if (requiresProject)
            {
                projectRoot = await OpenFolderContext.ResolveProjectRootAsync(_package);
                if (projectRoot == null)
                {
                    await ShowInfoAsync("No RXDK project selected. Set the Xbox project as the startup project (or open one of its files), then try again.");
                    return;
                }
                args.Add("--project-root");
                args.Add(projectRoot);
            }
            args.AddRange(extraArgs);

            try
            {
                await _cli.RunAsync(args, projectRoot ?? Environment.CurrentDirectory);
            }
            catch (Exception ex)
            {
                await ShowErrorAsync($"RXDK {verb} failed: {ex.Message}");
            }
        }

        // Deploy the selected project's .xbe + media (retry path when the devkit was off at F5).
        // Wired to the Solution Explorer project context menu (RXDK Xbox projects only).
        private Task DeployProjectAsync() => XboxDebugLauncher.DeploySelectedAsync(_package, _cli);

        // Remove the selected DXT project's extension from xe:\dxt and warm-reboot. Context-menu
        // command shown only for RXDK DXT projects, so it targets that specific project by name.
        private async Task RemoveDxtAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            if (!XboxDebugLauncher.TryGetSelectedProject(out var sel) || !sel.IsDxt)
            {
                await ShowInfoAsync("Select an RXDK DXT project in Solution Explorer, then try Remove DXT again.");
                return;
            }
            var args = new[] { "remove-dxt", "--project-root", sel.Dir, "--name", sel.Name };
            if (await _cli.RunAsync(args, sel.Dir) != 0)
            {
                await ShowErrorAsync($"Remove DXT failed — is the devkit on, and was {sel.Name}.dxt deployed?");
                return;
            }
            await _cli.RunAsync(new[] { "reboot" }, sel.Dir);
            await ShowInfoAsync($"Removed {sel.Name}.dxt from the console and warm-rebooted.");
        }

        private async Task SetXboxIpAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var current = await GetXboxIpAsync();
            var input = PromptForString("Set Xbox IP / Hostname", "Enter the devkit IP address or hostname:", current ?? string.Empty);
            if (string.IsNullOrWhiteSpace(input))
            {
                return;
            }
            await _cli.RunAsync(new[] { "set-ip", "--address", input.Trim() }, Environment.CurrentDirectory);
        }

        // ---- Debug (F5 → Debug Adapter Host → Rxdk.Dap.exe) ----

        private Task DebugAsync()
        {
            // Same path as F5 / the green Run button: build + deploy the startup Xbox project,
            // then launch the Xbox debug adapter via the Debug Adapter Host. Reads the output
            // from the .vcxproj (NMakeOutput), not rxdk.project.json.
            return XboxDebugLauncher.LaunchAsync(_package, _cli);
        }

        /// <summary>Read the "name" field from rxdk.project.json (folder name as a fallback).</summary>
        private static string ReadProjectName(string projectRoot)
        {
            try
            {
                var json = File.ReadAllText(Path.Combine(projectRoot, "rxdk.project.json"));
                using var doc = System.Text.Json.JsonDocument.Parse(json);
                if (doc.RootElement.TryGetProperty("name", out var n) && n.ValueKind == System.Text.Json.JsonValueKind.String)
                    return n.GetString();
            }
            catch { /* fall through */ }
            return Path.GetFileName(projectRoot.TrimEnd('\\', '/'));
        }

        // ---- Project scaffolding ----

        private async Task NewProjectAsync()
        {
            // Open VS's standard New Project dialog; the RXDK templates (Original Xbox Game/Empty/
            // Lib/DXT/Video Player/Cube/…) are contributed via the VSIX and filterable by the Xbox tag.
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var dte = (EnvDTE.DTE)await _package.GetServiceAsync(typeof(EnvDTE.DTE));
            try
            {
                dte?.ExecuteCommand("File.NewProject");
            }
            catch (Exception ex)
            {
                await ShowErrorAsync($"Could not open New Project: {ex.Message}");
            }
        }

        private async Task NewPrebuiltXbeAsync()
        {
            await ShowInfoAsync("New Prebuilt XBE project wizard is not yet implemented (Phase 3).");
        }

        // ---- VS2003 project import ----

        private async Task ImportProjectAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();

            var (vcproj, outDir) = RxdkToolWindowControl.PromptForImport();
            if (string.IsNullOrEmpty(vcproj) || string.IsNullOrEmpty(outDir))
            {
                return; // cancelled
            }

            // The scaffold files (props/targets + property-page rules) ship next to this DLL under
            // Scaffold\ (staged by scripts/dev.ps1 templates). The importer copies them into outDir.
            var scaffoldDir = Path.Combine(
                Path.GetDirectoryName(typeof(RxdkCommands).Assembly.Location), "Scaffold");
            if (!Directory.Exists(scaffoldDir))
            {
                await ShowErrorAsync($"Scaffold files not found ({scaffoldDir}). Rebuild/reinstall the RXDK extension.");
                return;
            }

            var args = new[] { "import-vcproj", "--in", vcproj, "--out", outDir, "--scaffold", scaffoldDir };
            int rc;
            try
            {
                rc = await _cli.RunAsync(args, outDir);
            }
            catch (Exception ex)
            {
                await ShowErrorAsync($"Import failed: {ex.Message}");
                return;
            }
            if (rc != 0)
            {
                await ShowErrorAsync("Import failed — see the RXDK output window for details.");
                return;
            }

            // Locate the produced .vcxproj (the importer writes exactly one at the output root).
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            string vcxproj = null;
            try
            {
                var found = Directory.GetFiles(outDir, "*.vcxproj", SearchOption.TopDirectoryOnly);
                if (found.Length > 0) vcxproj = found[0];
            }
            catch { /* fall through to the folder-open path */ }

            // If a solution is open, offer to add the imported project to it; otherwise just reveal
            // the output folder so the user can open the .vcxproj themselves.
            var dte = (EnvDTE.DTE)await _package.GetServiceAsync(typeof(EnvDTE.DTE));
            var solution = dte?.Solution;
            if (vcxproj != null && solution != null && solution.IsOpen)
            {
                var add = VsShellUtilities.ShowMessageBox(_package,
                    $"Imported {Path.GetFileName(vcxproj)}.\n\nAdd it to the current solution?", "RXDK",
                    OLEMSGICON.OLEMSGICON_QUERY, OLEMSGBUTTON.OLEMSGBUTTON_YESNO, OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
                if (add == (int)VSConstants.MessageBoxResult.IDYES)
                {
                    try
                    {
                        solution.AddFromFile(vcxproj, false);
                        return;
                    }
                    catch (Exception ex)
                    {
                        await ShowErrorAsync($"Imported OK but could not add to the solution: {ex.Message}");
                    }
                }
            }
            else if (vcxproj != null)
            {
                await ShowInfoAsync($"Imported {Path.GetFileName(vcxproj)}.\nOpen it from {outDir}.");
            }

            try
            {
                Process.Start(new ProcessStartInfo("explorer.exe", $"\"{outDir}\"") { UseShellExecute = true });
            }
            catch { /* best effort */ }
        }

        // ---- Tool window ----

        private async Task ShowToolWindowAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var window = await _package.ShowToolWindowAsync(typeof(RxdkToolWindow), 0, create: true, cancellationToken: _package.DisposalToken);
            if (window?.Frame == null)
            {
                await ShowErrorAsync("Could not create the RXDK tool window.");
            }
        }

        // ---- Folder / docs / launchers ----

        private async Task OpenFolderAsync(string path)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            if (string.IsNullOrEmpty(path) || !Directory.Exists(path))
            {
                await ShowInfoAsync($"Folder does not exist yet: {path}\nRun Install Prerequisites (RXDK window) first.");
                return;
            }
            Process.Start(new ProcessStartInfo("explorer.exe", $"\"{path}\"") { UseShellExecute = true });
        }

        private async Task OpenDocsAsync(string which)
        {
            // "sdk" -> the Xbox SDK help set (cloned under docs\xboxsdk), "rxdk" -> the extension
            // docs (docs\rxdk). The RXDK-Docs pages are .htm with a toc.json, and the SDK set has
            // no index page, so resolve the landing page rather than assuming docs\<x>\index.html.
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var candidates = which == "sdk" ? new[] { "xboxsdk", "sdk" } : new[] { which };
            string landing = null;
            string tried = null;
            foreach (var folder in candidates)
            {
                tried = Path.Combine(ToolLocator.StagedDocsRoot, folder);
                landing = ResolveDocsLanding(tried);
                if (landing != null) break;
            }
            if (landing != null)
            {
                Process.Start(new ProcessStartInfo(landing) { UseShellExecute = true });
            }
            else
            {
                await ShowInfoAsync($"Documentation not found under {tried}.\nRun Install Prerequisites (RXDK window) to clone RXDK-Docs.");
            }
        }

        /// <summary>
        /// Resolves the landing page for a docs folder: an index.htm/html if present, otherwise the
        /// first "page" referenced by the folder's toc.json (the SDK help set has no index page).
        /// Returns null if the folder is missing or no page can be found.
        /// </summary>
        private static string ResolveDocsLanding(string dir)
        {
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
            {
                return null;
            }
            // Prefer the toc.json's declared landing page ("defaultPage"), then its first page.
            var toc = Path.Combine(dir, "toc.json");
            if (File.Exists(toc))
            {
                try
                {
                    using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(toc));
                    var root = doc.RootElement;
                    if (root.TryGetProperty("defaultPage", out var dp) && dp.ValueKind == System.Text.Json.JsonValueKind.String)
                    {
                        var p = Path.Combine(dir, dp.GetString());
                        if (File.Exists(p)) return p;
                    }
                    var page = FindFirstTocPage(root);
                    if (!string.IsNullOrEmpty(page))
                    {
                        var p = Path.Combine(dir, page);
                        if (File.Exists(p)) return p;
                    }
                }
                catch { /* malformed toc — fall through */ }
            }
            foreach (var name in new[] { "index.htm", "index.html", "default.htm", "default.html" })
            {
                var p = Path.Combine(dir, name);
                if (File.Exists(p)) return p;
            }
            return null;
        }

        /// <summary>Depth-first search for the first "page" string in a toc.json tree.</summary>
        private static string FindFirstTocPage(System.Text.Json.JsonElement el)
        {
            switch (el.ValueKind)
            {
                case System.Text.Json.JsonValueKind.Object:
                    if (el.TryGetProperty("page", out var pg) && pg.ValueKind == System.Text.Json.JsonValueKind.String)
                    {
                        var s = pg.GetString();
                        if (!string.IsNullOrEmpty(s)) return s;
                    }
                    foreach (var prop in el.EnumerateObject())
                    {
                        var r = FindFirstTocPage(prop.Value);
                        if (r != null) return r;
                    }
                    break;
                case System.Text.Json.JsonValueKind.Array:
                    foreach (var item in el.EnumerateArray())
                    {
                        var r = FindFirstTocPage(item);
                        if (r != null) return r;
                    }
                    break;
            }
            return null;
        }

        private async Task LaunchHostToolAsync(string tool)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var exe = Path.Combine(ToolLocator.StagedToolsRoot, tool + ".exe");
            if (File.Exists(exe))
            {
                Process.Start(new ProcessStartInfo(exe) { UseShellExecute = true, WorkingDirectory = ToolLocator.StagedToolsRoot });
            }
            else
            {
                await ShowInfoAsync($"{tool} not found at {exe}. Run Install Prerequisites (RXDK window) to download host tools.");
            }
        }

        private async Task OpenXboxNeighborhoodAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            // Windows-only Xbox Neighborhood shell folder (matches rxdk.openXboxNeighborhood).
            try
            {
                Process.Start(new ProcessStartInfo("explorer.exe", "shell:::{XboxNeighborhood}") { UseShellExecute = true });
            }
            catch (Exception ex)
            {
                await ShowInfoAsync($"Could not open Xbox Neighborhood: {ex.Message}");
            }
        }

        // ---- Runtime / prerequisites / settings ----

        private Task InstallDotNetAsync() => RunCliAsync("install-tools", requiresProject: false);

        private async Task SetupPrerequisitesAsync()
        {
            // Install only what's missing: check each *-status first and skip the download when the
            // component is already present, so this is cheap to re-run (won't re-fetch Zig etc.).
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var cwd = Environment.CurrentDirectory;
            var installed = 0;
            async Task EnsureAsync(string statusVerb, string installVerb)
            {
                if (await _cli.RunAsync(new[] { statusVerb }, cwd) != 0)
                {
                    await _cli.RunAsync(new[] { installVerb }, cwd);
                    installed++;
                }
            }
            await EnsureAsync("zig-status", "install-zig");
            await EnsureAsync("tools-status", "install-tools");
            await EnsureAsync("sdk-status", "install-sdk");
            await EnsureAsync("docs-status", "install-docs");
            await ShowInfoAsync(installed == 0
                ? "RXDK is already set up — SDK, host tools, Zig and docs are all present."
                : $"RXDK setup finished — installed {installed} missing component(s). Use 'Fetch Latest RXDK-SDK' to update an existing SDK.");
        }

        private async Task SetBuildTypeAsync()
        {
            // Persisted in an Options page (Phase 3). For now surface the choices; the actual
            // --optimize value is passed by the build task once wired to settings.
            await ShowInfoAsync("Set Build Type: Debug / ReleaseSafe / ReleaseFast / ReleaseSmall. " +
                "An Options page persists this in Phase 3; until then edit tasks.vs.json's --optimize.");
        }

        private async Task CycleGlobalsScopeAsync()
        {
            // Live debug command; forwarded to Rxdk.Dap via a custom DAP request during a session.
            // TODO: send a custom 'rxdk/cycleGlobalsScope' request through the Debug Adapter Host
            // (parity with RXDK-VSCode rxdk.cycleGlobalsScope). No-op when no session is active.
            await ShowInfoAsync("Cycle Globals Visibility applies during an active debug session (Phase 2).");
        }

        private async Task OpenSettingsAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            // TODO(Phase 3): a DialogPage Options grid under Tools > Options > RXDK. For now open
            // the standard Options dialog.
            var dte = (EnvDTE.DTE)await _package.GetServiceAsync(typeof(EnvDTE.DTE));
            try { dte?.ExecuteCommand("Tools.Options"); } catch { /* best effort */ }
        }

        // ---- helpers shared with the tool window ----

        public async Task<string> GetXboxIpAsync()
        {
            var cliPath = ToolLocator.ResolveCli();
            if (cliPath == null)
            {
                return null;
            }
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = cliPath,
                    Arguments = "xbox-ip",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    CreateNoWindow = true,
                };
                using (var p = Process.Start(psi))
                {
                    var output = await p.StandardOutput.ReadToEndAsync();
                    p.WaitForExit(5000);
                    var line = output.Trim();
                    if (p.ExitCode != 0 || line.StartsWith("no Xbox", StringComparison.OrdinalIgnoreCase))
                    {
                        return null;
                    }
                    return line;
                }
            }
            catch
            {
                return null;
            }
        }

        // ---- tiny UI helpers ----

        private async Task ShowInfoAsync(string message)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            VsShellUtilities.ShowMessageBox(_package, message, "RXDK",
                OLEMSGICON.OLEMSGICON_INFO, OLEMSGBUTTON.OLEMSGBUTTON_OK, OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
        }

        private async Task ShowErrorAsync(string message)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            VsShellUtilities.ShowMessageBox(_package, message, "RXDK",
                OLEMSGICON.OLEMSGICON_CRITICAL, OLEMSGBUTTON.OLEMSGBUTTON_OK, OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
        }

        // Minimal modal string prompt. VS has no first-class input box, so we use a small WPF
        // dialog hosted by the tool window control's helper.
        private static string PromptForString(string title, string prompt, string initial)
        {
            return RxdkToolWindowControl.PromptForString(title, prompt, initial);
        }
    }
}
