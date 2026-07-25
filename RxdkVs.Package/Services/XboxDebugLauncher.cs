using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using Task = System.Threading.Tasks.Task;

namespace RxdkVs.Package.Services
{
    /// <summary>
    /// Launches a debug session for the solution's startup Xbox project. This is the single
    /// entry point used by both the RXDK &gt; Debug menu command and the F5 / green-Run-button
    /// interceptor (StartDebugInterceptor).
    ///
    /// Everything is read from the project's MSBuild properties (the .vcxproj), NOT from
    /// rxdk.project.json: the <c>RxdkXbox</c> marker identifies an Xbox project, and
    /// <c>NMakeOutput</c> gives the built .xbe from which the .exe/.pdb/title name are derived.
    /// The build+deploy still run through Rxdk.Cli against the project directory.
    /// </summary>
    internal static class XboxDebugLauncher
    {
        internal sealed class StartupInfo
        {
            public string ProjectDir;     // dir of the .vcxproj (Rxdk.Cli --project-root)
            public string XbeOutput;      // NMakeOutput (…\out\<name>.xbe)
            public string ConfigName;     // "Debug" / "Release"
            public bool IsXbox;           // RxdkXbox == true
        }

        /// <summary>True when the current startup project is an RXDK Xbox project.</summary>
        public static async Task<bool> IsXboxStartupProjectAsync(AsyncPackage package)
        {
            var info = await GetStartupInfoAsync(package);
            return info != null && info.IsXbox;
        }

        /// <summary>
        /// Build + deploy the startup Xbox project, then start a debug session against it via
        /// the VS Debug Adapter Host. No-op with a message if there's no Xbox startup project.
        /// </summary>
        public static async Task LaunchAsync(AsyncPackage package, CliRunner cli)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();

            var info = await GetStartupInfoAsync(package);
            if (info == null || !info.IsXbox)
            {
                await ShowAsync(package, "No Xbox project is set as the startup project.");
                return;
            }
            if (string.IsNullOrEmpty(info.XbeOutput))
            {
                await ShowAsync(package, "Could not determine the project's output (NMakeOutput). Build the project once, then try again.");
                return;
            }

            var dap = ToolLocator.ResolveDap();
            if (dap == null || !File.Exists(dap))
            {
                await ShowAsync(package, "Rxdk.Dap.exe not found. Publish the engine to %ProgramData%\\RXDK\\engine (or set RXDK_TOOLS_DIR).");
                return;
            }

            // Build + deploy through the engine (streams to the RXDK output pane / Error List).
            var optimize = info.ConfigName == "Release" ? "ReleaseSmall" : "Debug";
            if (await cli.RunAsync(new[] { "build", "--project-root", info.ProjectDir, "--optimize", optimize }, info.ProjectDir) != 0)
            {
                await ShowAsync(package, "Build failed — see the RXDK output pane.");
                return;
            }
            if (await cli.RunAsync(new[] { "deploy", "--project-root", info.ProjectDir }, info.ProjectDir) != 0)
            {
                await ShowAsync(package, "Deploy failed — is the devkit on and reachable?");
                return;
            }

            // A DXT is loaded by xbdm at boot, not attached as a title. Build + deploy to
            // E:\dxt (done above), warm-reboot, and stop — there is no debug-adapter session.
            if (info.XbeOutput.EndsWith(".dxt", StringComparison.OrdinalIgnoreCase))
            {
                await cli.RunAsync(new[] { "reboot" }, info.ProjectDir);
                await ShowAsync(package,
                    "DXT deployed to E:\\dxt and the console was warm-rebooted. A debug-monitor " +
                    "extension loads inside xbdm at boot, so there is no F5 attach-debug for it — " +
                    "it's now live on the console.");
                return;
            }

            // Derive the launch config from the .xbe output path.
            var outDir = Path.GetDirectoryName(info.XbeOutput);
            var name = Path.GetFileNameWithoutExtension(info.XbeOutput);
            var launch = new Dictionary<string, object>
            {
                ["$adapter"] = dap,
                ["type"] = "xbox",
                ["request"] = "launch",
                ["name"] = $"Debug {name}",
                ["program"] = Path.Combine(outDir, name + ".exe"),
                ["pdb"] = Path.Combine(outDir, name + ".pdb"),
                ["xbePath"] = $@"xe:\{name}\{name}.xbe",
                ["__workspaceFolder"] = info.ProjectDir,
                ["reboot"] = false,
            };
            var launchFile = Path.Combine(Path.GetTempPath(), $"rxdk-launch-{name}.json");
            File.WriteAllText(launchFile, SimpleJson(launch));

            var dte = (EnvDTE.DTE)await package.GetServiceAsync(typeof(EnvDTE.DTE));
            try
            {
                dte?.ExecuteCommand("DebugAdapterHost.Launch", $"/LaunchJson:\"{launchFile}\"");
            }
            catch (Exception ex)
            {
                await ShowAsync(package, $"Failed to start debugging: {ex.Message}. Is the VS Debug Adapter Host component installed?");
            }
        }

        // ---- startup-project MSBuild property reads ----

        private static async Task<StartupInfo> GetStartupInfoAsync(AsyncPackage package)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var sbm = (IVsSolutionBuildManager)await package.GetServiceAsync(typeof(SVsSolutionBuildManager));
            if (sbm == null) return null;
            if (sbm.get_StartupProject(out IVsHierarchy hier) != VSConstants.S_OK || hier == null) return null;

            var proj = GetExtObject(hier) as EnvDTE.Project;
            if (proj == null) return null;

            string projectDir;
            try { projectDir = Path.GetDirectoryName(proj.FullName); }
            catch { return null; }

            var configName = "Debug";
            string fullConfig = "Debug|Win32";
            try
            {
                var cfg = proj.ConfigurationManager?.ActiveConfiguration;
                if (cfg != null)
                {
                    configName = cfg.ConfigurationName;
                    fullConfig = $"{cfg.ConfigurationName}|{cfg.PlatformName}";
                }
            }
            catch { /* keep defaults */ }

            var bps = hier as IVsBuildPropertyStorage;
            var isXbox = string.Equals(ReadProp(bps, "RxdkXbox", fullConfig), "true", StringComparison.OrdinalIgnoreCase);
            var xbe = ReadProp(bps, "NMakeOutput", fullConfig);

            return new StartupInfo { ProjectDir = projectDir, XbeOutput = xbe, ConfigName = configName, IsXbox = isXbox };
        }

        private static string ReadProp(IVsBuildPropertyStorage bps, string name, string config)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            if (bps == null) return null;
            try
            {
                if (bps.GetPropertyValue(name, config, (uint)_PersistStorageType.PST_PROJECT_FILE, out string value) == VSConstants.S_OK)
                    return value;
            }
            catch { /* property absent */ }
            return null;
        }

        private static object GetExtObject(IVsHierarchy hier)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            return hier.GetProperty(VSConstants.VSITEMID_ROOT, (int)__VSHPROPID.VSHPROPID_ExtObject, out object ext) == VSConstants.S_OK
                ? ext : null;
        }

        private static async Task ShowAsync(AsyncPackage package, string message)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            VsShellUtilities.ShowMessageBox(package, message, "RXDK",
                OLEMSGICON.OLEMSGICON_INFO, OLEMSGBUTTON.OLEMSGBUTTON_OK, OLEMSGDEFBUTTON.OLEMSGDEFBUTTON_FIRST);
        }

        // Minimal JSON writer for the flat launch dictionary (avoids taking a JSON dependency here).
        private static string SimpleJson(Dictionary<string, object> map)
        {
            var sb = new System.Text.StringBuilder();
            sb.Append("{\n");
            var first = true;
            foreach (var kv in map)
            {
                if (!first) sb.Append(",\n");
                first = false;
                sb.Append("  \"").Append(kv.Key).Append("\": ");
                if (kv.Value is bool b) sb.Append(b ? "true" : "false");
                else sb.Append('"').Append(kv.Value.ToString().Replace("\\", "\\\\").Replace("\"", "\\\"")).Append('"');
            }
            sb.Append("\n}\n");
            return sb.ToString();
        }
    }
}
