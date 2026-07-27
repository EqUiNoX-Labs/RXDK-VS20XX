using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace Rxdk.Engine.Import;

/// <summary>
/// Imports a Visual Studio .NET 2003 solution (<c>.sln</c>) of XDK <c>.vcproj</c> projects into a
/// native RXDK multi-project solution: each project becomes its own <c>.vcxproj</c> (via
/// <see cref="Vcproj2003Importer"/>) under its own subfolder, the solution's project-dependency graph
/// is reproduced as native <c>&lt;ProjectReference&gt;</c> items (so RXDK links each child <c>.lib</c>
/// into its dependents), and a fresh <c>.sln</c> ties them together. Each project keeps its own full
/// (filtered) config set; the generated <c>.sln</c> maps every solution config to each project's
/// matching config (by name, else by flavor) so cross-project references resolve even when projects
/// name their configs differently — exactly how the original <c>.sln</c> did.
/// </summary>
public static class SolutionImporter
{
    // VC++ project type GUID used in .sln Project() entries.
    private const string VcxprojTypeGuid = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}";

    public sealed class SlnImportResult
    {
        public string SlnPath = "";
        public List<Vcproj2003Importer.ImportResult> Projects = new();
        public List<string> Warnings = new();
    }

    private sealed class ProjInfo
    {
        public string SlnName = "";      // display name in the .sln
        public string Guid = "";         // original .sln project GUID
        public string AbsVcproj = "";    // resolved source .vcproj
        public string VcprojName = "";   // Name attribute (→ output .vcxproj filename)
        public string OutSubdir = "";    // where the imported project is written
        public List<string> DepGuids = new();
        public Vcproj2003Importer.ImportResult? Result;
    }

    public static SlnImportResult ImportSolution(string slnPath, string outDir, string? scaffoldDir,
        bool copySources = false, Action<string>? log = null)
    {
        slnPath = Path.GetFullPath(slnPath);
        if (!File.Exists(slnPath)) throw new FileNotFoundException($"solution not found: {slnPath}");
        var slnDir = Path.GetDirectoryName(slnPath)!;
        outDir = Path.GetFullPath(string.IsNullOrWhiteSpace(outDir) ? Path.Combine(slnDir, "rxdk") : outDir);
        Directory.CreateDirectory(outDir);

        var result = new SlnImportResult();

        // ---- parse the .sln (projects + dependency graph) ----
        var projects = ParseSln(slnPath, slnDir, result);
        if (projects.Count == 0) throw new InvalidOperationException("no VC++ (.vcproj) projects found in the solution.");

        // ---- resolve output layout + each project's real .vcxproj name (needed for references) ----
        foreach (var p in projects)
        {
            p.VcprojName = PeekName(p.AbsVcproj) ?? Path.GetFileNameWithoutExtension(p.AbsVcproj);
            p.OutSubdir = Path.Combine(outDir, SafeName(p.SlnName));
        }
        var byGuid = projects.ToDictionary(p => p.Guid, p => p, StringComparer.OrdinalIgnoreCase);

        // ---- import each project, wiring references from its dependencies ----
        foreach (var p in projects)
        {
            var refs = new List<Vcproj2003Importer.ProjRef>();
            foreach (var dg in p.DepGuids)
            {
                if (!byGuid.TryGetValue(dg, out var dep)) continue;
                var depVcxproj = Path.Combine(dep.OutSubdir, dep.VcprojName + ".vcxproj");
                var rel = Path.GetRelativePath(p.OutSubdir, depVcxproj).Replace('/', '\\');
                refs.Add(new Vcproj2003Importer.ProjRef(dep.VcprojName, rel));
            }

            log?.Invoke($"Importing {p.SlnName} -> {p.OutSubdir}");
            p.Result = Vcproj2003Importer.Import(p.AbsVcproj, p.OutSubdir, scaffoldDir,
                copySources: copySources, projectRefs: refs, log: log);
            result.Projects.Add(p.Result);
            result.Warnings.AddRange(p.Result.Warnings.Select(w => $"[{p.SlnName}] {w}"));
        }

        // ---- write the umbrella .sln ----
        var slnName = Path.GetFileNameWithoutExtension(slnPath);
        var outSln = Path.Combine(outDir, slnName + ".sln");
        File.WriteAllText(outSln, BuildSln(projects), new UTF8Encoding(false));
        result.SlnPath = outSln;

        log?.Invoke($"Imported {projects.Count} project(s) -> {outSln}");
        return result;
    }

    // ---- .sln parsing ----

    // Project("{type}") = "Name", "relative\path.vcproj", "{projGuid}"
    private static readonly Regex ProjectLine = new(
        @"^Project\(""\{[^}]+\}""\)\s*=\s*""([^""]+)"",\s*""([^""]+)"",\s*""(\{[^}]+\})""",
        RegexOptions.Compiled);

    private static readonly Regex DepLine = new(@"^\s*(\{[^}]+\})\s*=\s*\{[^}]+\}", RegexOptions.Compiled);

    private static List<ProjInfo> ParseSln(string slnPath, string slnDir, SlnImportResult result)
    {
        var projects = new List<ProjInfo>();
        var lines = File.ReadAllLines(slnPath, Encoding.UTF8);
        ProjInfo? cur = null;
        var inDeps = false;
        foreach (var raw in lines)
        {
            var line = raw.TrimEnd();
            var m = ProjectLine.Match(line);
            if (m.Success)
            {
                var name = m.Groups[1].Value;
                var rel = m.Groups[2].Value;
                var guid = m.Groups[3].Value;
                cur = null;
                inDeps = false;
                if (!rel.EndsWith(".vcproj", StringComparison.OrdinalIgnoreCase)) continue; // skip non-VC entries
                var abs = Path.GetFullPath(Path.Combine(slnDir, rel.Replace('/', '\\')));
                if (!File.Exists(abs)) { result.Warnings.Add($"project '{name}' not found on disk, skipped: {abs}"); continue; }
                cur = new ProjInfo { SlnName = name, Guid = guid, AbsVcproj = abs };
                projects.Add(cur);
            }
            else if (line.Contains("ProjectSection(ProjectDependencies)"))
            {
                inDeps = true;
            }
            else if (line.Contains("EndProjectSection"))
            {
                inDeps = false;
            }
            else if (line.StartsWith("EndProject"))
            {
                cur = null;
                inDeps = false;
            }
            else if (inDeps && cur != null)
            {
                var dm = DepLine.Match(line);
                if (dm.Success) cur.DepGuids.Add(dm.Groups[1].Value);
            }
        }
        return projects;
    }

    /// <summary>Read the .vcproj's Name attribute (drives the output .vcxproj filename).</summary>
    private static string? PeekName(string vcprojPath)
    {
        try
        {
            var doc = XDocument.Parse(File.ReadAllText(vcprojPath, Encoding.Latin1));
            return (string?)doc.Root?.Attribute("Name");
        }
        catch { return null; }
    }

    // ---- .sln emit ----

    private static string BuildSln(List<ProjInfo> projects)
    {
        var sb = new StringBuilder();
        sb.AppendLine();
        sb.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
        sb.AppendLine("# Visual Studio Version 17");
        sb.AppendLine("VisualStudioVersion = 17.0.0.0");
        sb.AppendLine("MinimumVisualStudioVersion = 10.0.0.0");

        foreach (var p in projects)
        {
            var guid = p.Result?.ProjectGuid ?? p.Guid;
            var rel = Path.Combine(SafeName(p.SlnName), p.VcprojName + ".vcxproj");
            sb.AppendLine($"Project(\"{VcxprojTypeGuid}\") = \"{p.VcprojName}\", \"{rel}\", \"{guid}\"");
            sb.AppendLine("EndProject");
        }

        // Solution configs = the union of every project's configs, in first-encounter order (Debug-
        // flavor configs before Release so the list reads naturally). Each project keeps its own full
        // set; projects that lack a given config fall back by flavor in MapProjectConfig below.
        var solutionConfigs = new List<string>();
        var seenConfigs = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var flavorFirst in new[] { true, false })
            foreach (var p in projects)
                foreach (var c in p.Result?.Configs ?? new List<(string Name, string Flavor)>())
                {
                    var isDebug = c.Flavor.Equals("Debug", StringComparison.OrdinalIgnoreCase);
                    if (isDebug == flavorFirst && seenConfigs.Add(c.Name)) solutionConfigs.Add(c.Name);
                }
        if (solutionConfigs.Count == 0) solutionConfigs.Add("Debug");

        sb.AppendLine("Global");
        sb.AppendLine("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution");
        foreach (var c in solutionConfigs) sb.AppendLine($"\t\t{c}|Win32 = {c}|Win32");
        sb.AppendLine("\tEndGlobalSection");

        sb.AppendLine("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution");
        foreach (var p in projects)
        {
            var guid = p.Result?.ProjectGuid ?? p.Guid;
            var cfgs = p.Result?.Configs ?? new List<(string Name, string Flavor)>();
            foreach (var sc in solutionConfigs)
            {
                var mapped = MapProjectConfig(cfgs, sc);
                sb.AppendLine($"\t\t{guid}.{sc}|Win32.ActiveCfg = {mapped}|Win32");
                sb.AppendLine($"\t\t{guid}.{sc}|Win32.Build.0 = {mapped}|Win32");
            }
        }
        sb.AppendLine("\tEndGlobalSection");

        sb.AppendLine("\tGlobalSection(SolutionProperties) = preSolution");
        sb.AppendLine("\t\tHideSolutionNode = FALSE");
        sb.AppendLine("\tEndGlobalSection");
        sb.AppendLine("EndGlobal");
        return sb.ToString();
    }

    // Map a solution config to one this project actually has: exact name, else same flavor, else first.
    private static string MapProjectConfig(List<(string Name, string Flavor)> cfgs, string solutionCfg)
    {
        if (cfgs.Count == 0) return solutionCfg;
        var exact = cfgs.FirstOrDefault(c => c.Name.Equals(solutionCfg, StringComparison.OrdinalIgnoreCase));
        if (exact.Name != null) return exact.Name;
        var wantDebug = solutionCfg.StartsWith("Debug", StringComparison.OrdinalIgnoreCase);
        var byFlavor = cfgs.FirstOrDefault(c => c.Flavor.Equals(wantDebug ? "Debug" : "Release", StringComparison.OrdinalIgnoreCase));
        return byFlavor.Name ?? cfgs[0].Name;
    }

    private static string SafeName(string name)
    {
        var sb = new StringBuilder(name.Length);
        foreach (var ch in name) sb.Append(Array.IndexOf(Path.GetInvalidFileNameChars(), ch) >= 0 ? '_' : ch);
        return sb.ToString().Trim();
    }
}
