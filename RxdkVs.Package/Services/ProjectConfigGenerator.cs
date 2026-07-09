using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace RxdkVs.Package.Services
{
    /// <summary>
    /// Generates the VS "Open Folder" config trio into the opened folder, the VS equivalents of
    /// what RXDK-VSCode's vscodeGenerator.ts writes into .vscode:
    ///
    ///   tasks.json           -> tasks.vs.json       (Build/Deploy/Run invoking Rxdk.Cli.exe)
    ///   launch.json          -> launch.vs.json      ("xbox"-type config for the Debug Adapter Host)
    ///   c_cpp_properties.json -> CppProperties.json  (SDK include path + RXDK defines)
    ///
    /// The tasks/launch shapes differ from VS Code's: VS Open Folder tasks are keyed by
    /// taskLabel/appliesTo and shell out via command/args, and launch configs live under a
    /// "configurations" array in launch.vs.json. rxdk.project.json stays the source of truth,
    /// exactly as in the VS Code port — we read the same manifest (Rxdk.Engine.Model) and emit
    /// the analogous files.
    ///
    /// NOTE ON MANIFEST PARSING: this file references Rxdk.Engine.Model.RxdkProjectManifest for the
    /// canonical shape, but Rxdk.Engine is net8 and this package is .NET Framework 4.7.2, so it can
    /// NOT be referenced at runtime. The generator therefore parses the manifest itself with
    /// System.Text.Json and only uses the model type names for documentation. See ParseManifest.
    /// TODO(packaging): if a Framework-compatible (netstandard2.0) build of the model is produced,
    /// swap ParseManifest for a direct deserialize.
    /// </summary>
    internal static class ProjectConfigGenerator
    {
        private const string CliExeToken = "${env.RXDK_CLI}"; // resolved to Rxdk.Cli.exe path at write time
        private static readonly JsonSerializerOptions WriteOptions = new JsonSerializerOptions
        {
            WriteIndented = true,
        };

        /// <summary>
        /// Reads the manifest at <paramref name="projectRoot"/> and writes tasks.vs.json,
        /// launch.vs.json, and CppProperties.json into it. Returns the files written.
        /// </summary>
        public static IReadOnlyList<string> Generate(string projectRoot)
        {
            var manifestPath = Path.Combine(projectRoot, OpenFolderContext.ManifestFileName);
            if (!File.Exists(manifestPath))
            {
                throw new FileNotFoundException("No rxdk.project.json at project root", manifestPath);
            }

            var manifest = ParseManifest(manifestPath);
            var written = new List<string>();

            var cliPath = ToolLocator.ResolveCli() ?? ToolLocator.CliExeName; // literal fallback for a TODO

            written.Add(WriteTasks(projectRoot, manifest, cliPath));

            if (!manifest.IsDxt)
            {
                written.Add(WriteLaunch(projectRoot, manifest, cliPath));
            }

            if (manifest.NeedsIntelliSense)
            {
                written.Add(WriteCppProperties(projectRoot, manifest));
            }

            return written;
        }

        // ---- tasks.vs.json ----
        // VS Open Folder task schema: a top-level "tasks" array. Each RXDK action becomes a
        // launch-type task that shells out to Rxdk.Cli.exe with --project-root ${workspaceRoot}.
        private static string WriteTasks(string projectRoot, ManifestView manifest, string cliPath)
        {
            var tasks = new List<object>();

            void AddTask(string label, string verb, params string[] extraArgs)
            {
                var args = new List<string> { verb, "--project-root", "${workspaceRoot}" };
                args.AddRange(extraArgs);
                tasks.Add(new
                {
                    taskLabel = label,
                    appliesTo = "rxdk.project.json",
                    type = "launch",
                    command = cliPath,
                    args = args.ToArray(),
                    workingDirectory = "${workspaceRoot}",
                });
            }

            if (manifest.IsDxt)
            {
                // DXT: build the .dxt, deploy to E:\dxt, warm reboot. No run/debug.
                AddTask("rxdk: build", "build");
                AddTask("rxdk: deploy", "deploy");
                AddTask("rxdk: reboot", "reboot");
            }
            else if (manifest.IsPrebuilt)
            {
                // Prebuilt XBE: deploy the existing artifacts; no compile.
                AddTask("rxdk: deploy", "deploy");
            }
            else
            {
                AddTask("rxdk: build", "build");
                AddTask("rxdk: deploy", "deploy");
                AddTask("rxdk: run", "run");
            }

            var doc = new { version = "0.2.1", tasks };
            return WriteJson(Path.Combine(projectRoot, "tasks.vs.json"), doc);
        }

        // ---- launch.vs.json ----
        // The "xbox"-type config is consumed by the VS Debug Adapter Host, which launches
        // Rxdk.Dap.exe (see RxdkVs.Package.pkgdef for the type->adapter registration). Attribute
        // names mirror RXDK-VSCode's debuggers[].configurationAttributes (package.json).
        private static string WriteLaunch(string projectRoot, ManifestView manifest, string cliPath)
        {
            var name = manifest.Name;
            object config;

            if (manifest.IsPrebuilt)
            {
                var xbeLeaf = Path.GetFileName(manifest.PrebuiltXbe ?? $"{name}.xbe");
                var remote = string.IsNullOrEmpty(manifest.PrebuiltRemoteName) ? name : manifest.PrebuiltRemoteName;
                config = Compact(new Dictionary<string, object>
                {
                    ["type"] = "xbox",
                    ["request"] = "launch",
                    ["name"] = $"Debug {name}",
                    ["project"] = "rxdk.project.json",
                    ["xbe"] = manifest.PrebuiltXbe,
                    ["xbePath"] = $@"xe:\{remote}\{xbeLeaf}",
                    ["program"] = manifest.PrebuiltExe,
                    ["pdb"] = manifest.PrebuiltPdb,
                    ["map"] = manifest.PrebuiltMap,
                    ["srcRoot"] = manifest.PrebuiltSrcRoot,
                    ["reboot"] = true,
                });
            }
            else
            {
                config = new Dictionary<string, object>
                {
                    ["type"] = "xbox",
                    ["request"] = "launch",
                    ["name"] = $"Debug {name}",
                    ["project"] = "rxdk.project.json",
                    // Build+deploy before attach. Rxdk.Dap performs deploy/launch itself; we run
                    // the build task here (VS launches preLaunchTask by label, same as VS Code).
                    ["preLaunchTask"] = "rxdk: build",
                    ["program"] = $@"${{workspaceRoot}}\out\{name}.exe",
                    ["pdb"] = $@"${{workspaceRoot}}\out\{name}.pdb",
                    ["xbePath"] = $@"xe:\{name}\{name}.xbe",
                    ["reboot"] = false,
                };
            }

            var doc = new
            {
                version = "0.2.1",
                defaults = new { },
                configurations = new[] { config },
            };
            return WriteJson(Path.Combine(projectRoot, "launch.vs.json"), doc);
        }

        // ---- CppProperties.json ----
        // VS Open Folder IntelliSense config. Points includePath at the staged SDK include dir
        // (%ProgramData%\RXDK\sdk\include), the project's own include dirs, and referenced
        // libraries' public includes; defines mirror RXDK-VSCode's (_XBOX/_WIN32/_WINNT/_X86_).
        private static string WriteCppProperties(string projectRoot, ManifestView manifest)
        {
            var includePath = new List<string>
            {
                ToolLocator.StagedSdkIncludeDir.Replace('\\', '/'),
                "${workspaceRoot}/**",
            };

            void PushDir(string root, string rel)
            {
                if (!string.IsNullOrWhiteSpace(rel))
                {
                    var dir = Path.Combine(root, rel).Replace('\\', '/');
                    if (!includePath.Contains(dir))
                    {
                        includePath.Add(dir);
                    }
                }
            }

            foreach (var rel in manifest.IncludePaths) PushDir(projectRoot, rel);
            foreach (var rel in manifest.PublicIncludePaths) PushDir(projectRoot, rel);
            foreach (var dir in CollectReferencedPublicIncludes(projectRoot, manifest, new HashSet<string>()))
            {
                if (!includePath.Contains(dir))
                {
                    includePath.Add(dir);
                }
            }

            var defines = new List<string> { "_XBOX", "_WIN32", "_WINNT", "_X86_" };
            defines.AddRange(manifest.Defines);

            var config = new Dictionary<string, object>
            {
                ["name"] = "Xbox",
                ["includePath"] = includePath.ToArray(),
                ["defines"] = defines.ToArray(),
                ["intelliSenseMode"] = "windows-msvc-x86",
                ["cStandard"] = "c23",
            };
            if (manifest.UsesCpp)
            {
                config["cppStandard"] = "c++23";
            }

            var doc = new { configurations = new[] { config } };
            return WriteJson(Path.Combine(projectRoot, "CppProperties.json"), doc);
        }

        // Transitive publicIncludePaths of projectReferences (mirrors vscodeGenerator.ts).
        private static IEnumerable<string> CollectReferencedPublicIncludes(string projectRoot, ManifestView manifest, HashSet<string> seen)
        {
            var results = new List<string>();
            foreach (var rel in manifest.ProjectReferences)
            {
                if (string.IsNullOrWhiteSpace(rel)) continue;
                var depRoot = Path.GetFullPath(Path.Combine(projectRoot, rel));
                var key = depRoot.ToLowerInvariant();
                if (!seen.Add(key)) continue;

                var depManifestPath = Path.Combine(depRoot, OpenFolderContext.ManifestFileName);
                if (!File.Exists(depManifestPath)) continue;

                ManifestView dep;
                try { dep = ParseManifest(depManifestPath); } catch { continue; }

                foreach (var inc in dep.PublicIncludePaths)
                {
                    if (!string.IsNullOrWhiteSpace(inc))
                    {
                        results.Add(Path.Combine(depRoot, inc).Replace('\\', '/'));
                    }
                }
                results.AddRange(CollectReferencedPublicIncludes(depRoot, dep, seen));
            }
            return results;
        }

        // ---- manifest parsing (System.Text.Json; see class-level note on the runtime split) ----
        private static ManifestView ParseManifest(string path)
        {
            var text = StripBom(File.ReadAllText(path));
            using (var doc = JsonDocument.Parse(text))
            {
                var root = doc.RootElement;
                var m = new ManifestView
                {
                    Name = GetString(root, "name") ?? "title",
                    Type = GetString(root, "type"),
                    Sources = GetStringArray(root, "sources"),
                    IncludePaths = GetStringArray(root, "includePaths"),
                    PublicIncludePaths = GetStringArray(root, "publicIncludePaths"),
                    Defines = GetStringArray(root, "defines"),
                    ProjectReferences = GetStringArray(root, "projectReferences"),
                };

                if (root.TryGetProperty("prebuilt", out var pb) && pb.ValueKind == JsonValueKind.Object)
                {
                    m.PrebuiltXbe = GetString(pb, "xbe");
                    m.PrebuiltPdb = GetString(pb, "pdb");
                    m.PrebuiltMap = GetString(pb, "map");
                    m.PrebuiltExe = GetString(pb, "exe");
                    m.PrebuiltSrcRoot = GetString(pb, "srcRoot");
                    m.PrebuiltRemoteName = GetString(pb, "remoteName");
                }
                return m;
            }
        }

        // A trimmed view of RxdkProjectManifest with the derived helpers the generator needs.
        // (Cross-runtime: see the class-level TODO. Kept as a local record to avoid a net8 ref.)
        private sealed class ManifestView
        {
            public string Name = "title";
            public string Type;
            public List<string> Sources = new List<string>();
            public List<string> IncludePaths = new List<string>();
            public List<string> PublicIncludePaths = new List<string>();
            public List<string> Defines = new List<string>();
            public List<string> ProjectReferences = new List<string>();

            public string PrebuiltXbe;
            public string PrebuiltPdb;
            public string PrebuiltMap;
            public string PrebuiltExe;
            public string PrebuiltSrcRoot;
            public string PrebuiltRemoteName;

            public bool IsPrebuilt => !string.IsNullOrEmpty(PrebuiltXbe);
            public bool IsDxt => string.Equals(Type, "dxt", StringComparison.OrdinalIgnoreCase);
            public bool IsLibrary => string.Equals(Type, "library", StringComparison.OrdinalIgnoreCase);

            public bool UsesCpp => Sources.Any(s => s.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase)
                || s.EndsWith(".cxx", StringComparison.OrdinalIgnoreCase)
                || s.EndsWith(".cc", StringComparison.OrdinalIgnoreCase));

            public bool NeedsIntelliSense => !IsPrebuilt && Sources.Any(s =>
                new[] { ".c", ".cpp", ".cxx", ".cc", ".h", ".hpp" }
                    .Any(ext => s.EndsWith(ext, StringComparison.OrdinalIgnoreCase)));
        }

        // ---- small JSON helpers ----
        private static string GetString(JsonElement obj, string name)
            => obj.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() : null;

        private static List<string> GetStringArray(JsonElement obj, string name)
        {
            var list = new List<string>();
            if (obj.TryGetProperty(name, out var arr) && arr.ValueKind == JsonValueKind.Array)
            {
                foreach (var e in arr.EnumerateArray())
                {
                    if (e.ValueKind == JsonValueKind.String)
                    {
                        list.Add(e.GetString());
                    }
                }
            }
            return list;
        }

        private static Dictionary<string, object> Compact(Dictionary<string, object> d)
        {
            return d.Where(kv => kv.Value != null && !(kv.Value is string s && s.Length == 0))
                    .ToDictionary(kv => kv.Key, kv => kv.Value);
        }

        private static string WriteJson(string path, object doc)
        {
            var json = JsonSerializer.Serialize(doc, WriteOptions) + Environment.NewLine;
            File.WriteAllText(path, json, new UTF8Encoding(false));
            return path;
        }

        private static string StripBom(string s)
            => (s.Length > 0 && s[0] == '﻿') ? s.Substring(1) : s;
    }
}
