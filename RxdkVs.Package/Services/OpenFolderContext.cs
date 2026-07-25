using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace RxdkVs.Package.Services
{
    /// <summary>
    /// Resolves the currently-open workspace/folder root and the RXDK project marker
    /// (<c>rxdk.project.json</c>) inside it. In VS Open Folder mode there is no .sln; the
    /// "solution directory" property of the shell points at the opened folder, which is what
    /// we use as the project root for the CLI's --project-root argument.
    /// </summary>
    internal static class OpenFolderContext
    {
        public const string ManifestFileName = "rxdk.project.json";

        /// <summary>
        /// The root of the open folder/solution, or null if nothing is open. On the UI thread.
        /// </summary>
        public static async Task<string> GetWorkspaceRootAsync(IAsyncServiceProvider serviceProvider)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var solution = (IVsSolution)await serviceProvider.GetServiceAsync(typeof(SVsSolution));
            if (solution == null)
            {
                return null;
            }

            // In Open Folder mode VSPROPID_SolutionDirectory is the opened folder.
            solution.GetSolutionInfo(out string solutionDir, out _, out _);
            if (!string.IsNullOrEmpty(solutionDir) && Directory.Exists(solutionDir))
            {
                return solutionDir.TrimEnd(Path.DirectorySeparatorChar);
            }
            return null;
        }

        /// <summary>
        /// Finds the rxdk.project.json for the open folder: first at the root, then a shallow
        /// (2-level) descent so a repo whose sample lives one folder down still resolves.
        /// Returns null if none is found. Purely file-system based, callable off the UI thread.
        /// </summary>
        public static string FindManifest(string workspaceRoot)
        {
            if (string.IsNullOrEmpty(workspaceRoot) || !Directory.Exists(workspaceRoot))
            {
                return null;
            }

            var atRoot = Path.Combine(workspaceRoot, ManifestFileName);
            if (File.Exists(atRoot))
            {
                return atRoot;
            }

            try
            {
                foreach (var child in Directory.EnumerateDirectories(workspaceRoot))
                {
                    var candidate = Path.Combine(child, ManifestFileName);
                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
            }
            catch (Exception)
            {
                // Unreadable dirs are ignored; missing manifest is a normal state.
            }
            return null;
        }

        /// <summary>The directory that contains rxdk.project.json, i.e. the CLI --project-root.</summary>
        public static string GetProjectRoot(string workspaceRoot)
        {
            var manifest = FindManifest(workspaceRoot);
            return manifest == null ? null : Path.GetDirectoryName(manifest);
        }

        /// <summary>
        /// Resolve the active RXDK project root across both project models. Order:
        ///   1. the solution's startup project (the natural "debug/deploy this" target),
        ///   2. the project containing the active editor document,
        ///   3. Open Folder fallback (rxdk.project.json at/under the opened folder).
        /// A candidate only counts if its directory contains rxdk.project.json. Returns null
        /// if nothing resolves. Must be called on the UI thread.
        /// </summary>
        public static async Task<string> ResolveProjectRootAsync(IAsyncServiceProvider serviceProvider)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();

            var dte = (EnvDTE.DTE)await serviceProvider.GetServiceAsync(typeof(EnvDTE.DTE));
            var fromStartup = TryStartupProjectRoot(dte);
            if (fromStartup != null) return fromStartup;

            var fromActive = TryActiveDocumentProjectRoot(dte);
            if (fromActive != null) return fromActive;

            var workspaceRoot = await GetWorkspaceRootAsync(serviceProvider);
            return GetProjectRoot(workspaceRoot);
        }

        private static string ProjectDirIfRxdk(EnvDTE.Project project)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            try
            {
                var full = project?.FullName;
                if (string.IsNullOrEmpty(full) || !File.Exists(full)) return null;
                var dir = Path.GetDirectoryName(full);
                return File.Exists(Path.Combine(dir, ManifestFileName)) ? dir : null;
            }
            catch (Exception)
            {
                return null;
            }
        }

        private static string TryStartupProjectRoot(EnvDTE.DTE dte)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            try
            {
                if (!(dte?.Solution?.SolutionBuild?.StartupProjects is Array arr) || arr.Length == 0) return null;
                var uniqueName = arr.GetValue(0) as string;
                if (string.IsNullOrEmpty(uniqueName)) return null;
                foreach (EnvDTE.Project project in dte.Solution.Projects)
                {
                    if (project == null) continue;
                    if (string.Equals(project.UniqueName, uniqueName, StringComparison.OrdinalIgnoreCase))
                        return ProjectDirIfRxdk(project);
                }
            }
            catch (Exception)
            {
                // COM hiccups / solution folders that don't enumerate cleanly — fall through.
            }
            return null;
        }

        private static string TryActiveDocumentProjectRoot(EnvDTE.DTE dte)
        {
            ThreadHelper.ThrowIfNotOnUIThread();
            try
            {
                return ProjectDirIfRxdk(dte?.ActiveDocument?.ProjectItem?.ContainingProject);
            }
            catch (Exception)
            {
                return null;
            }
        }
    }
}
