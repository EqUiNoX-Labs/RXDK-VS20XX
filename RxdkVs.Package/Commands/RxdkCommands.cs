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

            var workspaceRoot = await OpenFolderContext.GetWorkspaceRootAsync(_package);
            var args = new List<string> { verb };

            if (requiresProject)
            {
                var projectRoot = OpenFolderContext.GetProjectRoot(workspaceRoot);
                if (projectRoot == null)
                {
                    await ShowInfoAsync("No RXDK project (rxdk.project.json) is open. Use RXDK > New Project first.");
                    return;
                }
                args.Add("--project-root");
                args.Add(projectRoot);
            }
            args.AddRange(extraArgs);

            try
            {
                await _cli.RunAsync(args, workspaceRoot ?? Environment.CurrentDirectory);
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

        private async Task DebugAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var workspaceRoot = await OpenFolderContext.GetWorkspaceRootAsync(_package);
            var projectRoot = OpenFolderContext.GetProjectRoot(workspaceRoot);
            if (projectRoot == null)
            {
                await ShowInfoAsync("No RXDK project is open.");
                return;
            }

            // Keep the on-disk configs current (for the editor / native F5 path too).
            try { ProjectConfigGenerator.Generate(projectRoot); } catch { /* non-fatal */ }

            // Resolve the adapter executable.
            var dapPath = ToolLocator.ResolveDap();
            if (dapPath == null || !File.Exists(dapPath))
            {
                await ShowErrorAsync(
                    "Rxdk.Dap.exe not found. Publish it to %ProgramData%\\RXDK\\engine (or set " +
                    "RXDK_TOOLS_DIR). See RxdkVs.Package/README.md.");
                return;
            }

            // Launch directly through the VS Debug Adapter Host rather than relying on VS to
            // surface launch.vs.json as a startup item (which is unreliable in Open Folder mode
            // and varies by VS version). The "$adapter" property points the Debug Adapter Host
            // straight at our DAP server; the remaining fields are the launch-request arguments
            // our adapter reads (see XboxDebugAdapter.HandleLaunchRequestAsync). Invoked via the
            // documented `DebugAdapterHost.Launch /LaunchJson:<file>` command.
            var name = ReadProjectName(projectRoot);
            var launch = new Dictionary<string, object>
            {
                // $adapter points the Debug Adapter Host at our DAP server. No $adapterArgs:
                // the docs omit it for a native exe adapter, and it's the config proven working.
                ["$adapter"] = dapPath,
                ["type"] = "xbox",
                ["request"] = "launch",
                ["name"] = $"Debug {name}",
                ["program"] = Path.Combine(projectRoot, "out", name + ".exe"),
                ["pdb"] = Path.Combine(projectRoot, "out", name + ".pdb"),
                ["xbePath"] = $@"xe:\{name}\{name}.xbe",
                ["__workspaceFolder"] = projectRoot,
                ["reboot"] = false,
            };
            var launchFile = Path.Combine(Path.GetTempPath(), $"rxdk-launch-{name}.json");
            try
            {
                File.WriteAllText(launchFile,
                    System.Text.Json.JsonSerializer.Serialize(launch,
                        new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            }
            catch (Exception ex)
            {
                await ShowErrorAsync($"Could not write launch config: {ex.Message}");
                return;
            }

            var dte = (EnvDTE.DTE)await _package.GetServiceAsync(typeof(EnvDTE.DTE));
            try
            {
                // Build + Deploy must have run first (this direct launch does not run preLaunchTasks):
                // the .exe/.pdb must exist locally and the .xbe must be deployed to xe:\<name>.
                dte?.ExecuteCommand("DebugAdapterHost.Launch", $"/LaunchJson:\"{launchFile}\"");
            }
            catch (Exception ex)
            {
                await ShowErrorAsync($"Failed to start debugging: {ex.Message}. " +
                    "Ensure Build + Deploy have run, and that the VS Debug Adapter Host component is installed.");
            }
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
            // A full New Project wizard is Phase 3 (see PLAN.md). For the scaffold we invoke the
            // tool window's create flow. TODO: build an IVsTemplateWizard-based wizard that writes
            // rxdk.project.json from the templates/ set + calls ProjectConfigGenerator.Generate.
            await ShowToolWindowAsync();
            await ShowInfoAsync("New Project wizard is not yet implemented (Phase 3). " +
                "Create rxdk.project.json manually, then RXDK > (re)generate opens tasks/launch.");
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

        private async Task OpenDocsAsync(string subfolder)
        {
            var docsRoot = ToolLocator.StagedDocsRoot;
            var index = Path.Combine(docsRoot, subfolder, "index.html");
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            if (File.Exists(index))
            {
                Process.Start(new ProcessStartInfo(index) { UseShellExecute = true });
            }
            else
            {
                await ShowInfoAsync($"Documentation not found: {index}\nRun RXDK > Complete Setup to clone RXDK-Docs.");
            }
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
