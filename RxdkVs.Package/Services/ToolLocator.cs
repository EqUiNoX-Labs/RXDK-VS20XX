using System;
using System.IO;
using System.Reflection;

namespace RxdkVs.Package.Services
{
    /// <summary>
    /// Resolves the paths of the out-of-proc net8 executables the package drives:
    /// <c>Rxdk.Cli.exe</c> (build/deploy/run/reboot/ip orchestration) and
    /// <c>Rxdk.Dap.exe</c> (the DAP adapter the Debug Adapter Host launches).
    ///
    /// This is the single choke point for tool resolution — one TODO to update when the
    /// final packaging layout is decided.
    /// </summary>
    internal static class ToolLocator
    {
        public const string CliExeName = "Rxdk.Cli.exe";
        public const string DapExeName = "Rxdk.Dap.exe";

        // Env override, handy for dev/CI: point at a `dotnet publish` output.
        private const string ToolsDirEnvVar = "RXDK_TOOLS_DIR";

        /// <summary>Full path to Rxdk.Cli.exe, or null if it can't be found.</summary>
        public static string ResolveCli() => Resolve(CliExeName);

        /// <summary>Full path to Rxdk.Dap.exe, or null if it can't be found.</summary>
        public static string ResolveDap() => Resolve(DapExeName);

        private static string Resolve(string exeName)
        {
            // 1) Explicit override wins.
            var overrideDir = Environment.GetEnvironmentVariable(ToolsDirEnvVar);
            if (!string.IsNullOrEmpty(overrideDir))
            {
                var overridePath = Path.Combine(overrideDir, exeName);
                if (File.Exists(overridePath))
                {
                    return overridePath;
                }
            }

            // 2) Alongside the VSIX install (the intended shipping layout: the net8 exes are
            //    packaged into a `tools\` subfolder of the extension install dir).
            //
            // TODO(packaging): decide the final layout. Options:
            //   (a) Bundle self-contained `Rxdk.Cli.exe`/`Rxdk.Dap.exe` into the VSIX under
            //       tools\ and ship them (simplest for the user; larger VSIX). Add them as
            //       <Content IncludeInVSIX="true"> in the csproj under that folder.
            //   (b) Download them at first run into %ProgramData%\RXDK\engine (mirrors how the
            //       host tools are already fetched into %ProgramData%\RXDK\tools) and resolve
            //       from there. See RXDK-VSCode hostTools.ts for the download pattern.
            // Until then we probe both a bundled tools\ dir and the ProgramData engine dir.
            var vsixDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            if (!string.IsNullOrEmpty(vsixDir))
            {
                var bundled = Path.Combine(vsixDir, "tools", exeName);
                if (File.Exists(bundled))
                {
                    return bundled;
                }
                var besideVsix = Path.Combine(vsixDir, exeName);
                if (File.Exists(besideVsix))
                {
                    return besideVsix;
                }
            }

            // 3) %ProgramData%\RXDK\engine (download-at-runtime target).
            var programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            var engineDir = Path.Combine(programData, "RXDK", "engine");
            var engineExe = Path.Combine(engineDir, exeName);
            if (File.Exists(engineExe))
            {
                return engineExe;
            }

            // 4) Dev-tree fallback: the solution's build output. Lets you F5 the VSIX against a
            //    freshly-built CLI without packaging. Walk up from the VSIX dir to the repo root
            //    and look in Rxdk.Cli/Rxdk.Dap bin\Debug\net8.0.
            var devPath = TryResolveFromDevTree(exeName);
            if (devPath != null)
            {
                return devPath;
            }

            return null;
        }

        private static string TryResolveFromDevTree(string exeName)
        {
            var projectName = exeName.Equals(DapExeName, StringComparison.OrdinalIgnoreCase)
                ? "Rxdk.Dap"
                : "Rxdk.Cli";

            var dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            for (var i = 0; i < 8 && dir != null; i++)
            {
                foreach (var cfg in new[] { "Debug", "Release" })
                {
                    var candidate = Path.Combine(dir, projectName, "bin", cfg, "net8.0", exeName);
                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
                dir = Path.GetDirectoryName(dir);
            }
            return null;
        }

        /// <summary>
        /// The staged RXDK roots, mirroring RxdkPaths in the engine so the tool window and
        /// "Open … Folder" commands point at the same locations the CLI uses.
        /// </summary>
        public static string StagedRoot =>
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "RXDK");

        public static string StagedSdkRoot => Path.Combine(StagedRoot, "sdk");
        public static string StagedSdkIncludeDir => Path.Combine(StagedSdkRoot, "include");
        public static string StagedToolsRoot => Path.Combine(StagedRoot, "tools");
        public static string StagedDocsRoot => Path.Combine(StagedRoot, "docs");
    }
}
