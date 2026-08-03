using System.Globalization;
using System.Text;
using System.Xml.Linq;

namespace Rxdk.Engine.Import;

/// <summary>
/// Imports a Visual Studio .NET 2003 XDK project (<c>.vcproj</c>, VisualStudioProject format) into
/// a native RXDK Visual Studio project (<c>.vcxproj</c> Makefile-type) plus its <c>.filters</c>.
/// Each VS2003 configuration is preserved; its compiler/linker/XboxImage/XboxDeployment settings
/// are mapped onto the RXDK per-configuration properties the property pages drive.
/// The RXDK scaffolding (Rxdk.Xbox.props/targets + the property-page rule XMLs) is copied from a
/// scaffold directory. By default source files are referenced in place (relative to the output
/// directory); pass <paramref name="copySources"/> to mirror them into the output folder so the
/// imported project is self-contained.
/// </summary>
public static class Vcproj2003Importer
{
    public sealed class ImportResult
    {
        public string VcxprojPath = "";
        public string ProjectName = "";
        public string ProjectGuid = "";
        public bool IsLibrary;
        public int ConfigurationCount;
        public int SourceCount;
        public List<(string Name, string Flavor)> Configs = new();
        public List<string> UnmappedLibraries = new();
        public List<string> Warnings = new();
    }

    /// <summary>A native &lt;ProjectReference&gt; to emit into the generated .vcxproj.</summary>
    public sealed record ProjRef(string Name, string RelPath);

    // Scaffold files an RXDK project needs alongside the .vcxproj (copied from scaffoldDir).
    private static readonly string[] ScaffoldFiles =
    {
        "Rxdk.Xbox.props", "Rxdk.Xbox.targets", "RxdkDebugger.xml",
        "RxdkXboxBuild.xml", "RxdkXboxImage.xml", "RxdkXboxDeployment.xml",
        "RxdkXboxCertificate.xml", "RxdkXboxTitleInfo.xml",
    };

    // XDK link library (base name, variant suffix stripped) -> RXDK library. null = no equivalent.
    private static readonly Dictionary<string, string?> LibMap = new(StringComparer.OrdinalIgnoreCase)
    {
        ["d3d8"] = "libd3d8", ["d3dx8"] = "libd3dx8", ["dsound"] = "libdsound",
        ["xapilib"] = "libxapi", ["xgraphics"] = "libxgraphics", ["xmv"] = "libxmv",
        ["xbdm"] = "libxbdm", ["xboxkrnl"] = "libkernel",
        ["xonline"] = "libxonline", ["xnet"] = "libxnet",
        ["xact"] = "libxact", ["xacteng"] = "libxact",
        // No RXDK equivalent yet (audio middleware / perf):
        ["dmusic"] = null, ["xsndtrk"] = null, ["xvoice"] = null, ["xperf"] = null,
    };

    // Defines RXDK provides itself; dropped from the imported per-config define list.
    private static readonly HashSet<string> DroppedDefines =
        new(StringComparer.OrdinalIgnoreCase) { "_XBOX", "XBOX", "_DEBUG", "NDEBUG" };

    private sealed class Cfg
    {
        public string Name = "";       // VS2003 config name without the |Platform suffix
        public string Flavor = "Release";
        public string? ReleaseOptimize;
        public string Defines = "";
        public string IncludePaths = "";
        public string Libraries = "";
        public string DeployPaths = "";
        // imagebld / cert / title
        public string? StackSize, ImageDebug, LimitMemory, DontModifyHd, DontMountUd, NoLibWarn;
        public string? TitleId, TitleName, TitleImage, XbeVersion;
    }

    public static ImportResult Import(string vcprojPath, string outDir, string? scaffoldDir,
        bool copySources = false, IReadOnlyList<ProjRef>? projectRefs = null, Action<string>? log = null)
    {
        vcprojPath = Path.GetFullPath(vcprojPath);
        if (!File.Exists(vcprojPath)) throw new FileNotFoundException($"vcproj not found: {vcprojPath}");
        var vcprojDir = Path.GetDirectoryName(vcprojPath)!;
        outDir = Path.GetFullPath(string.IsNullOrWhiteSpace(outDir) ? vcprojDir : outDir);
        Directory.CreateDirectory(outDir);

        // VS2003 .vcproj files are Windows-1252; net8 lacks that code page without an extra provider.
        // Decode as Latin1 (compatible for the ASCII paths/identifiers here) and parse the string, so
        // the encoding declaration in the prolog is ignored.
        var text = File.ReadAllText(vcprojPath, Encoding.Latin1);
        var doc = XDocument.Parse(text, LoadOptions.None);
        var root = doc.Root ?? throw new InvalidOperationException("Empty .vcproj");
        var name = (string?)root.Attribute("Name") ?? Path.GetFileNameWithoutExtension(vcprojPath);
        var isLib = (string?)FirstConfig(root)?.Attribute("ConfigurationType") == "4";

        var result = new ImportResult { ProjectName = name };
        var unmapped = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        result.IsLibrary = isLib;

        // ---- configurations ----
        // XDK .vcproj files carry Xbox-platform configs and often PC "|Win32" variants too; import
        // only the Xbox ones — the Win32 builds can't target the console and, sharing a base name
        // with their Xbox counterparts, would otherwise collide into duplicate configurations.
        var allElems = (root.Element("Configurations")?.Elements("Configuration") ?? Enumerable.Empty<XElement>()).ToList();
        var xboxElems = allElems.Where(c => ConfigPlatform(c).Equals("Xbox", StringComparison.OrdinalIgnoreCase)).ToList();
        if (xboxElems.Count == 0 && allElems.Count > 0)
        {
            xboxElems = allElems;
            result.Warnings.Add("no Xbox-platform configs found; imported all platforms.");
        }
        var allConfigs = xboxElems.Select(c => ParseConfig(c, unmapped)).ToList();

        // The generated .vcxproj lives one directory deeper than the source .vcproj (in a
        // <name>\ subfolder), so path-valued settings copied verbatim from the .vcproj --
        // which are relative to the .vcproj -- must be rebased to the .vcxproj dir, exactly
        // like the source files are (ResolveSources below). Without this a sample's
        // "..\..\Common\include" / deploy media point one level too shallow and aren't found.
        foreach (var c in allConfigs)
        {
            c.IncludePaths = RebasePathList(c.IncludePaths, vcprojDir, outDir);
            c.DeployPaths  = RebasePathList(c.DeployPaths, vcprojDir, outDir);
        }

        // Drop configs RXDK has no equivalent for (profiling / LTCG / SDL / Win32 name variants),
        // then dedupe by name so the generated project can never carry duplicate configurations.
        var configs = allConfigs.Where(c => !IsUnsupportedConfig(c.Name))
            .GroupBy(c => c.Name, StringComparer.OrdinalIgnoreCase).Select(g => g.First()).ToList();
        var dropped = allConfigs.Count - configs.Count;
        if (dropped > 0) result.Warnings.Add($"dropped {dropped} unsupported config(s) (profile/LTCG/fastcap/SDL/Win32).");
        if (configs.Count == 0) { configs = allConfigs; result.Warnings.Add("all configs were unsupported; kept them as-is."); }

        result.ConfigurationCount = configs.Count;
        result.Configs = configs.Select(c => (c.Name, c.Flavor)).ToList();
        result.UnmappedLibraries = unmapped.OrderBy(x => x).ToList();

        // ---- files (with filter folders) ----
        var rawFiles = new List<(string abs, string tag, string? filter)>();
        var filters = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
        CollectFiles(root.Element("Files"), null, vcprojDir, rawFiles, filters);
        var sources = ResolveSources(rawFiles, vcprojDir, outDir, copySources, result);
        result.SourceCount = sources.Count(s => s.tag == "ClCompile");

        // ---- write .vcxproj + .filters ----
        var projectGuid = "{" + Guid.NewGuid().ToString().ToUpperInvariant() + "}";
        result.ProjectGuid = projectGuid;
        var vcxprojPath = Path.Combine(outDir, name + ".vcxproj");
        File.WriteAllText(vcxprojPath, BuildVcxproj(name, isLib, projectGuid, configs, sources, projectRefs), new UTF8Encoding(false));
        File.WriteAllText(vcxprojPath + ".filters", BuildFilters(sources, filters), new UTF8Encoding(false));
        result.VcxprojPath = vcxprojPath;

        // ---- copy scaffolding ----
        if (!string.IsNullOrWhiteSpace(scaffoldDir) && Directory.Exists(scaffoldDir))
        {
            foreach (var f in ScaffoldFiles)
            {
                var src = Path.Combine(scaffoldDir, f);
                if (File.Exists(src)) File.Copy(src, Path.Combine(outDir, f), overwrite: true);
                else result.Warnings.Add($"scaffold file missing: {f}");
            }
        }
        else
        {
            result.Warnings.Add("no scaffold directory supplied - copy Rxdk.Xbox.props/.targets + the " +
                "RxdkXbox*.xml rule files next to the generated .vcxproj (from any RXDK template) before opening it.");
        }

        if (result.UnmappedLibraries.Count > 0)
            result.Warnings.Add("libraries with no RXDK equivalent (dropped): " +
                string.Join(", ", result.UnmappedLibraries) + " - the title may not link until those APIs are available.");

        log?.Invoke($"Imported {name}: {result.ConfigurationCount} configuration(s), {result.SourceCount} source file(s) -> {vcxprojPath}");
        foreach (var w in result.Warnings) log?.Invoke($"Warning: {w}");
        return result;
    }

    private static XElement? FirstConfig(XElement root) =>
        root.Element("Configurations")?.Elements("Configuration").FirstOrDefault();

    private static Cfg ParseConfig(XElement c, HashSet<string> unmapped)
    {
        var full = (string?)c.Attribute("Name") ?? "";
        var cfg = new Cfg { Name = full.Split('|')[0] };

        XElement? Tool(string n) => c.Elements("Tool").FirstOrDefault(t => (string?)t.Attribute("Name") == n);
        var cl = Tool("VCCLCompilerTool");
        var link = Tool("VCLinkerTool");
        var img = Tool("XboxImageTool");
        var dep = Tool("XboxDeploymentTool");

        // flavor + optimize from Optimization (0=Debug, 1=MinSize, 2/3=Speed) and the config name.
        var opt = (string?)cl?.Attribute("Optimization") ?? "";
        var isDebug = opt == "0" || cfg.Name.StartsWith("Debug", StringComparison.OrdinalIgnoreCase);
        cfg.Flavor = isDebug ? "Debug" : "Release";
        if (!isDebug) cfg.ReleaseOptimize = opt == "1" ? "ReleaseSmall" : "ReleaseFast";

        // defines: drop the ones RXDK provides.
        cfg.Defines = string.Join(";", SplitList((string?)cl?.Attribute("PreprocessorDefinitions"))
            .Where(d => !DroppedDefines.Contains(d)));
        cfg.IncludePaths = string.Join(";", SplitList((string?)cl?.Attribute("AdditionalIncludeDirectories")));

        // libraries: map XDK -> RXDK, dedupe, always add libc/libcpp/libkernel.
        var libs = new List<string>();
        void Add(string l) { if (!libs.Contains(l, StringComparer.OrdinalIgnoreCase)) libs.Add(l); }
        foreach (var raw in SplitLibs((string?)link?.Attribute("AdditionalDependencies")))
        {
            var mapped = MapLib(raw, out var known);
            if (mapped != null) Add(mapped);
            else if (!known) { /* unknown, non-.lib token - ignore */ }
            else unmapped.Add(raw);
        }
        foreach (var forced in new[] { "libc", "libcpp", "libkernel" }) Add(forced);
        cfg.Libraries = string.Join(";", libs);

        // deploy files
        cfg.DeployPaths = string.Join(";", SplitList((string?)dep?.Attribute("AdditionalFiles")));

        // imagebld / cert / title
        cfg.StackSize = NormalizeInt((string?)img?.Attribute("StackSize"));
        cfg.ImageDebug = Bool((string?)img?.Attribute("IncludeDebugInfo"));
        cfg.LimitMemory = Bool((string?)img?.Attribute("LimitAvailableMemoryTo64MB"));
        cfg.DontModifyHd = Bool((string?)img?.Attribute("DontModifyHD"));
        cfg.DontMountUd = Bool((string?)img?.Attribute("DontMountUD"));
        cfg.NoLibWarn = Bool((string?)img?.Attribute("NoLibWarn"));
        cfg.TitleId = NonEmpty((string?)img?.Attribute("TitleID"));
        cfg.TitleName = NonEmpty((string?)img?.Attribute("TitleName"));
        cfg.TitleImage = NonEmpty((string?)img?.Attribute("TitleImage"));
        cfg.XbeVersion = NonEmpty((string?)img?.Attribute("XBEVersion"));
        return cfg;
    }

    // The platform half of a VS2003 config Name ("Debug|Xbox" -> "Xbox").
    private static string ConfigPlatform(XElement c)
    {
        var full = (string?)c.Attribute("Name") ?? "";
        var bar = full.IndexOf('|');
        return bar >= 0 ? full[(bar + 1)..].Trim() : "";
    }

    // Config names RXDK has no build equivalent for: profiling/instrumentation modes and non-Xbox
    // platform variants encoded into the config name (the XDK uses "(Win32)"/"(SDL)" suffixes).
    private static bool IsUnsupportedConfig(string name)
    {
        var n = name.ToLowerInvariant();
        return n.Contains("profile") || n.Contains("ltcg") || n.Contains("fastcap") || n.Contains("fast cap")
            || n.Contains("(win32)") || n.Contains("(win)") || n.Contains("(sdl)") || n.Contains("(pc)");
    }

    private static string? MapLib(string token, out bool known)
    {
        known = false;
        var baseName = token;
        if (baseName.EndsWith(".lib", StringComparison.OrdinalIgnoreCase))
            baseName = baseName[..^4];
        // strip a variant suffix (debug 'd', instrumented 'i', static 's', 'ltcg') to find the base.
        foreach (var (b, rxdk) in LibMap)
        {
            if (baseName.Equals(b, StringComparison.OrdinalIgnoreCase) ||
                (baseName.StartsWith(b, StringComparison.OrdinalIgnoreCase) &&
                 IsVariantSuffix(baseName[b.Length..])))
            {
                known = true;
                return rxdk; // may be null (known but no RXDK equivalent)
            }
        }
        return null;
    }

    private static bool IsVariantSuffix(string s) =>
        s.Length == 0 || s.Equals("d", StringComparison.OrdinalIgnoreCase) ||
        s.Equals("i", StringComparison.OrdinalIgnoreCase) || s.Equals("s", StringComparison.OrdinalIgnoreCase) ||
        s.Equals("ltcg", StringComparison.OrdinalIgnoreCase);

    // ---- files ----

    private static void CollectFiles(XElement? node, string? filterPath, string vcprojDir,
        List<(string abs, string tag, string? filter)> files, SortedSet<string> filters)
    {
        if (node == null) return;
        foreach (var el in node.Elements())
        {
            if (el.Name.LocalName == "Filter")
            {
                var fn = (string?)el.Attribute("Name") ?? "";
                var path = string.IsNullOrEmpty(filterPath) ? fn : filterPath + "\\" + fn;
                if (!string.IsNullOrEmpty(path)) filters.Add(path);
                CollectFiles(el, path, vcprojDir, files, filters);
            }
            else if (el.Name.LocalName == "File")
            {
                var rel = (string?)el.Attribute("RelativePath") ?? "";
                if (string.IsNullOrWhiteSpace(rel)) continue;
                var abs = Path.GetFullPath(Path.Combine(vcprojDir, rel.Replace("/", "\\")));
                var ext = Path.GetExtension(abs).ToLowerInvariant();
                var tag = ext is ".cpp" or ".cxx" or ".cc" or ".c" ? "ClCompile"
                        : ext is ".h" or ".hpp" or ".hxx" or ".inl" ? "ClInclude" : "None";
                files.Add((abs, tag, filterPath));
            }
        }
    }

    // Turn raw absolute file references into the &lt;include&gt; paths written to the .vcxproj.
    // Default: reference the originals in place (relative to outDir). With copySources: mirror each
    // file into outDir, preserving its path relative to the source project (leading "..\" segments
    // are collapsed so nothing escapes outDir), then reference the copy. Same-name clashes from
    // different sources get a numeric suffix. Files already inside outDir are left untouched.
    private static List<(string include, string tag, string? filter)> ResolveSources(
        List<(string abs, string tag, string? filter)> raw, string vcprojDir, string outDir,
        bool copySources, ImportResult result)
    {
        var sources = new List<(string include, string tag, string? filter)>();
        if (!copySources)
        {
            foreach (var (abs, tag, filter) in raw)
                sources.Add((MakeRelative(outDir, abs), tag, filter));
            return sources;
        }

        var usedDest = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase); // destAbs -> srcAbs
        var copied = 0;
        foreach (var (abs, tag, filter) in raw)
        {
            var destRel = SafeDestRel(vcprojDir, abs);
            var destAbs = Path.GetFullPath(Path.Combine(outDir, destRel));
            if (usedDest.TryGetValue(destAbs, out var prevSrc) && !PathEquals(prevSrc, abs))
                destRel = Disambiguate(destRel, outDir, usedDest, out destAbs);

            if (!usedDest.ContainsKey(destAbs))
            {
                usedDest[destAbs] = abs;
                if (!File.Exists(abs))
                {
                    result.Warnings.Add($"source not found, not copied: {abs}");
                }
                else if (!PathEquals(destAbs, abs)) // don't copy a file onto itself (outDir == source)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destAbs)!);
                    File.Copy(abs, destAbs, overwrite: true);
                    copied++;
                }
            }
            sources.Add((destRel.Replace('/', '\\'), tag, filter));
        }
        result.Warnings.Add($"copied {copied} source file(s) into the output folder.");
        return sources;
    }

    // File path relative to the source project, with leading "..\" (or a drive root) collapsed so the
    // copy stays inside outDir. e.g. "..\..\common\util.cpp" -> "common\util.cpp".
    private static string SafeDestRel(string vcprojDir, string abs)
    {
        var rel = Path.GetRelativePath(vcprojDir, abs);
        if (Path.IsPathRooted(rel)) return Path.GetFileName(abs); // different drive: flatten to name
        var parts = rel.Split(new[] { '/', '\\' }, StringSplitOptions.RemoveEmptyEntries)
                       .Where(p => p != "..").ToArray();
        return parts.Length == 0 ? Path.GetFileName(abs) : string.Join("\\", parts);
    }

    // Append _2, _3, ... before the extension until the destination is unused.
    private static string Disambiguate(string destRel, string outDir,
        Dictionary<string, string> usedDest, out string destAbs)
    {
        var dir = Path.GetDirectoryName(destRel) ?? "";
        var stem = Path.GetFileNameWithoutExtension(destRel);
        var ext = Path.GetExtension(destRel);
        for (var i = 2; ; i++)
        {
            var cand = string.IsNullOrEmpty(dir) ? $"{stem}_{i}{ext}" : Path.Combine(dir, $"{stem}_{i}{ext}");
            destAbs = Path.GetFullPath(Path.Combine(outDir, cand));
            if (!usedDest.ContainsKey(destAbs)) return cand;
        }
    }

    private static bool PathEquals(string a, string b) =>
        string.Equals(Path.GetFullPath(a), Path.GetFullPath(b), StringComparison.OrdinalIgnoreCase);

    // ---- .vcxproj / .filters emit ----

    private static string BuildVcxproj(string name, bool isLib, string projectGuid, List<Cfg> configs,
        List<(string include, string tag, string? filter)> sources, IReadOnlyList<ProjRef>? projectRefs)
    {
        var ext = isLib ? "lib" : "xbe";
        var sb = new StringBuilder();
        sb.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
        sb.AppendLine("<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");
        sb.AppendLine("  <ItemGroup Label=\"ProjectConfigurations\">");
        foreach (var c in configs)
        {
            sb.AppendLine($"    <ProjectConfiguration Include=\"{Esc(c.Name)}|Win32\">");
            sb.AppendLine($"      <Configuration>{Esc(c.Name)}</Configuration>");
            sb.AppendLine("      <Platform>Win32</Platform>");
            sb.AppendLine("    </ProjectConfiguration>");
        }
        sb.AppendLine("  </ItemGroup>");
        sb.AppendLine("  <PropertyGroup Label=\"Globals\">");
        sb.AppendLine("    <VCProjectVersion>16.0</VCProjectVersion>");
        sb.AppendLine($"    <ProjectGuid>{projectGuid}</ProjectGuid>");
        sb.AppendLine("    <RootNamespace>XboxNamespace</RootNamespace>");
        sb.AppendLine("    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>");
        sb.AppendLine($"    <ProjectName>{Esc(name)}</ProjectName>");
        sb.AppendLine("  </PropertyGroup>");
        sb.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />");
        sb.AppendLine("  <PropertyGroup Label=\"Configuration\">");
        sb.AppendLine("    <ConfigurationType>Makefile</ConfigurationType>");
        sb.AppendLine("    <PlatformToolset Condition=\"'$(VisualStudioVersion)' == '17.0'\">v143</PlatformToolset>");
        sb.AppendLine("    <PlatformToolset Condition=\"'$(VisualStudioVersion)' == '18.0'\">v145</PlatformToolset>");
        sb.AppendLine("    <PlatformToolset Condition=\"'$(PlatformToolset)' == ''\">v143</PlatformToolset>");
        sb.AppendLine("  </PropertyGroup>");
        sb.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />");
        sb.AppendLine("  <Import Project=\"Rxdk.Xbox.props\" />");
        sb.AppendLine("  <PropertyGroup>");
        if (isLib) sb.AppendLine("    <RxdkType>library</RxdkType>");
        sb.AppendLine($"    <NMakeOutput>$(MSBuildProjectDirectory)\\$(RxdkOutDir)\\$(MSBuildProjectName).{ext}</NMakeOutput>");
        sb.AppendLine("  </PropertyGroup>");

        foreach (var c in configs)
        {
            sb.AppendLine($"  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{Esc(c.Name)}|Win32'\">");
            sb.AppendLine($"    <RxdkBuildFlavor>{c.Flavor}</RxdkBuildFlavor>");
            if (c.ReleaseOptimize != null) sb.AppendLine($"    <RxdkReleaseOptimize>{c.ReleaseOptimize}</RxdkReleaseOptimize>");
            Prop(sb, "RxdkDefines", c.Defines);
            Prop(sb, "RxdkIncludePaths", c.IncludePaths);
            Prop(sb, "RxdkLibraries", c.Libraries);
            Prop(sb, "RxdkDeployPaths", c.DeployPaths);
            Prop(sb, "RxdkStackSize", c.StackSize);
            Prop(sb, "RxdkImageDebug", c.ImageDebug);
            Prop(sb, "RxdkLimitMemory", c.LimitMemory);
            Prop(sb, "RxdkDontModifyHardDisk", c.DontModifyHd);
            Prop(sb, "RxdkDontMountUtilityDrive", c.DontMountUd);
            Prop(sb, "RxdkNoLibWarn", c.NoLibWarn);
            Prop(sb, "RxdkTestId", c.TitleId);
            Prop(sb, "RxdkTestName", c.TitleName);
            Prop(sb, "RxdkTitleImage", c.TitleImage);
            Prop(sb, "RxdkTestVersion", c.XbeVersion);
            sb.AppendLine("  </PropertyGroup>");
        }

        EmitItems(sb, sources, "ClCompile");
        EmitItems(sb, sources, "ClInclude");
        EmitItems(sb, sources, "None");
        // Native project references (build order); RXDK links each child .lib via the manifest
        // projectReferences the targets derive from @(ProjectReference). The ItemDefinitionGroup in
        // Rxdk.Xbox.props already marks these build-order-only, so no per-item metadata is needed.
        if (projectRefs is { Count: > 0 })
        {
            sb.AppendLine("  <ItemGroup>");
            foreach (var r in projectRefs) sb.AppendLine($"    <ProjectReference Include=\"{Esc(r.RelPath)}\" />");
            sb.AppendLine("  </ItemGroup>");
        }
        sb.AppendLine("  <ItemGroup>");
        foreach (var f in ScaffoldFiles) sb.AppendLine($"    <None Include=\"{f}\" />");
        sb.AppendLine("  </ItemGroup>");
        sb.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />");
        sb.AppendLine("</Project>");
        return sb.ToString();
    }

    private static void EmitItems(StringBuilder sb, List<(string include, string tag, string? filter)> sources, string tag)
    {
        var items = sources.Where(s => s.tag == tag).ToList();
        if (items.Count == 0) return;
        sb.AppendLine("  <ItemGroup>");
        foreach (var s in items) sb.AppendLine($"    <{tag} Include=\"{Esc(s.include)}\" />");
        sb.AppendLine("  </ItemGroup>");
    }

    private static string BuildFilters(List<(string include, string tag, string? filter)> sources, SortedSet<string> filters)
    {
        var sb = new StringBuilder();
        sb.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
        sb.AppendLine("<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">");
        if (filters.Count > 0)
        {
            sb.AppendLine("  <ItemGroup>");
            foreach (var f in filters)
            {
                sb.AppendLine($"    <Filter Include=\"{Esc(f)}\">");
                sb.AppendLine($"      <UniqueIdentifier>{{{Guid.NewGuid().ToString().ToUpperInvariant()}}}</UniqueIdentifier>");
                sb.AppendLine("    </Filter>");
            }
            sb.AppendLine("  </ItemGroup>");
        }
        foreach (var tag in new[] { "ClCompile", "ClInclude", "None" })
        {
            var items = sources.Where(s => s.tag == tag).ToList();
            if (items.Count == 0) continue;
            sb.AppendLine("  <ItemGroup>");
            foreach (var s in items)
            {
                if (string.IsNullOrEmpty(s.filter)) sb.AppendLine($"    <{tag} Include=\"{Esc(s.include)}\" />");
                else
                {
                    sb.AppendLine($"    <{tag} Include=\"{Esc(s.include)}\">");
                    sb.AppendLine($"      <Filter>{Esc(s.filter)}</Filter>");
                    sb.AppendLine($"    </{tag}>");
                }
            }
            sb.AppendLine("  </ItemGroup>");
        }
        sb.AppendLine("</Project>");
        return sb.ToString();
    }

    // ---- small helpers ----

    private static void Prop(StringBuilder sb, string name, string? value)
    {
        if (!string.IsNullOrEmpty(value)) sb.AppendLine($"    <{name}>{Esc(value)}</{name}>");
    }

    private static IEnumerable<string> SplitList(string? s) =>
        (s ?? "").Split(new[] { ';', ',' }, StringSplitOptions.RemoveEmptyEntries).Select(x => x.Trim()).Where(x => x.Length > 0);

    private static IEnumerable<string> SplitLibs(string? s) =>
        (s ?? "").Split(new[] { ' ', ';', '\t' }, StringSplitOptions.RemoveEmptyEntries).Select(x => x.Trim()).Where(x => x.Length > 0);

    private static string? Bool(string? v) =>
        v == null ? null : (v.Equals("TRUE", StringComparison.OrdinalIgnoreCase) ? "true" : "false");

    private static string? NonEmpty(string? v) => string.IsNullOrWhiteSpace(v) ? null : v.Trim();

    // "0x40000" or "262144" -> decimal string (the manifest emits stackSize as a raw JSON number).
    private static string? NormalizeInt(string? v)
    {
        v = NonEmpty(v);
        if (v == null) return null;
        try
        {
            var n = v.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
                ? long.Parse(v[2..], NumberStyles.HexNumber, CultureInfo.InvariantCulture)
                : long.Parse(v, CultureInfo.InvariantCulture);
            return n.ToString(CultureInfo.InvariantCulture);
        }
        catch { return null; }
    }

    private static string MakeRelative(string baseDir, string path)
    {
        var rel = Path.GetRelativePath(baseDir, path);
        return rel.Replace('/', '\\');
    }

    // Rebase a ';'-separated list of paths that were relative to `fromDir` (the source
    // .vcproj) so they are relative to `toDir` (the generated .vcxproj). Absolute paths
    // and entries containing MSBuild macros ($(...)) pass through untouched.
    private static string RebasePathList(string list, string fromDir, string toDir)
    {
        if (string.IsNullOrWhiteSpace(list)) return list;
        var rebased = list
            .Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Select(p => (p.Contains('$') || Path.IsPathRooted(p))
                ? p
                : MakeRelative(toDir, Path.GetFullPath(Path.Combine(fromDir, p))));
        return string.Join(";", rebased);
    }

    private static string Esc(string s) =>
        s.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");
}
