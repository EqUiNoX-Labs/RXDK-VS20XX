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
            void Add(int id, Func<Task> handler)
            {
                var cmdId = new CommandID(RxdkPackageGuids.CommandSet, id);
                var cmd = new OleMenuCommand((s, e) => _package.JoinableTaskFactory.RunAsync(handler).FileAndForget("rxdk/command"), cmdId);
                svc.AddCommand(cmd);
            }

            Add(CommandIds.CmdBuild, () => RunCliAsync("build"));
            Add(CommandIds.CmdDeploy, () => RunCliAsync("deploy"));
            Add(CommandIds.CmdRun, () => RunCliAsync("run"));
            Add(CommandIds.CmdRebootConsole, () => RunCliAsync("reboot", requiresProject: false));
            Add(CommandIds.CmdRemoveDxt, RemoveDxtAsync);
            Add(CommandIds.CmdSetXboxIp, SetXboxIpAsync);
            Add(CommandIds.CmdDebug, DebugAsync);
            Add(CommandIds.CmdDebugPrebuiltXbe, NewPrebuiltXbeAsync);
            Add(CommandIds.CmdNewProject, NewProjectAsync);
            Add(CommandIds.CmdShowToolWindow, ShowToolWindowAsync);
            Add(CommandIds.CmdOpenSdkFolder, () => OpenFolderAsync(ToolLocator.StagedSdkRoot));
            Add(CommandIds.CmdOpenToolsFolder, () => OpenFolderAsync(ToolLocator.StagedToolsRoot));
            Add(CommandIds.CmdOpenDocsFolder, () => OpenFolderAsync(ToolLocator.StagedDocsRoot));
            Add(CommandIds.CmdOpenSdkDocs, () => OpenDocsAsync("sdk"));
            Add(CommandIds.CmdOpenExtensionDocs, () => OpenDocsAsync("rxdk"));
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

        private async Task RemoveDxtAsync()
        {
            // Mirror rxdk.removeDxt: the CLI does not yet expose a dedicated verb, so this is a
            // reboot after the engine clears E:\dxt. TODO: add a `remove-dxt` CLI verb (parity
            // with RXDK-VSCode xboxLaunch removeDxt) and call it here instead of plain reboot.
            await RunCliAsync("reboot", requiresProject: false);
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
                await ShowInfoAsync($"Folder does not exist yet: {path}\nRun RXDK > Complete Setup first.");
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
                await ShowInfoAsync($"Documentation not found under {tried}.\nRun RXDK > Complete Setup to clone RXDK-Docs.");
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
                await ShowInfoAsync($"{tool} not found at {exe}. Run RXDK > Complete Setup to download host tools.");
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
            // Sequentially runs the engine's bootstrap verbs. A richer setup UI is Phase 3.
            await RunCliAsync("install-zig", requiresProject: false);
            await RunCliAsync("install-tools", requiresProject: false);
            await RunCliAsync("install-sdk", requiresProject: false);
            await RunCliAsync("install-docs", requiresProject: false);
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
