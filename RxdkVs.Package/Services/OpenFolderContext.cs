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
    }
}
