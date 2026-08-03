using System.Text.RegularExpressions;
using Rxdk.Engine.Bootstrap;
using Rxdk.Engine.Model;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Build;

public sealed record BuildResult(bool Ok, string OutDir, string? Error = null);

public sealed class BuildOptions
{
    public required string ProjectRoot { get; init; }
    public string? ZigExecutable { get; init; }
    public bool CompileOnly { get; init; }
    public RxdkOptimizeMode Optimize { get; init; } = RxdkOptimizeMode.Debug;
    /// <summary>Explicit manifest path (native .vcxproj flow). Null = ProjectRoot/rxdk.project.json.</summary>
    public string? ManifestPath { get; init; }
    public Action<string>? Log { get; init; }
}

/// <summary>
/// Compiles + links an Xbox title (or static library / DXT) with Zig and imagebld. C# port
/// of RXDK-VSCode xboxBuild.ts. The compile recipe matches the SDK's own title target
/// (build/xbox_target.zig): x86-windows-gnu, -nostdinc, force-included picolibc.h, so only
/// the staged SDK headers are on the path; -march=pentium3 for the Xbox CPU.
/// </summary>
public static class XboxBuild
{
    // -I (not -isystem) everywhere: the SDK's clean-room windef.h/etc. must win over zig's
    // bundled MinGW headers, which -isystem would let shadow them.
    private static readonly string[] XdkClangWarnings =
    {
        "-Wno-macro-redefined", "-Wno-deprecated-declarations", "-Wno-sign-compare",
        "-Wno-sign-conversion", "-Wno-implicit-int-conversion", "-Wno-shorten-64-to-32",
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-Wno-unused-parameter",
        "-Wno-unused-variable", "-Wno-unused-function", "-Wno-missing-field-initializers",
        "-Wno-switch", "-Wno-ignored-qualifiers", "-Wno-invalid-source-encoding",
        "-Wno-pragma-pack", "-Wno-nonportable-include-path", "-Wno-main-return-type",
        "-Wno-missing-prototype-for-cc", "-Wno-ignored-pragma-intrinsic", "-Wno-multichar",
        "-Wno-comment", "-Wno-extra-tokens", "-Wno-unused-command-line-argument",
        // Legacy XDK code narrows freely in braced initializers (e.g. STRING = {(USHORT)strlen(s),
        // (USHORT)strlen(s)+1, s}); the C++11 narrowing rule makes those hard errors. Benign here.
        "-Wno-c++11-narrowing",
    };

    // Resolve a project's manifest: hand-authored rxdk.project.json if present, else the
    // build-generated out\rxdk.manifest.json (native .vcxproj flow — a referenced child
    // library project has no rxdk.project.json, only the manifest its own build emitted).
    private static RxdkProjectManifest ReadManifest(string dir)
    {
        if (File.Exists(Path.Combine(dir, RxdkManifestLoader.ManifestFileName)))
            return RxdkManifestLoader.Load(dir);
        var generated = Path.Combine(dir, "out", "rxdk.manifest.json");
        if (File.Exists(generated))
            return RxdkManifestLoader.LoadFile(generated);
        throw new FileNotFoundException(
            $"No manifest for {dir} (expected rxdk.project.json or out\\rxdk.manifest.json). " +
            "Build the referenced library project first.");
    }

    // A referenced project has a manifest if it ships a hand-authored rxdk.project.json OR
    // (native .vcxproj flow) has already generated one into out\ from its VS build.
    private static bool HasManifest(string dir) =>
        File.Exists(Path.Combine(dir, RxdkManifestLoader.ManifestFileName)) ||
        File.Exists(Path.Combine(dir, "out", "rxdk.manifest.json"));

    private static List<string> ProjectDefineArgs(RxdkProjectManifest m) =>
        (m.Defines ?? new()).Where(d => !string.IsNullOrWhiteSpace(d)).Select(d => $"-D{d}").ToList();

    // ---- per-file compile ----

    private static async Task ZigCompileAsync(
        string zig, string source, string obj, IReadOnlyList<string> includeArgs,
        IReadOnlyList<string> defineArgs, bool isCpp, RxdkOptimizeMode optimize,
        Action<string>? log, CancellationToken ct)
    {
        var common = new List<string> { "-target", "x86-windows-gnu" };
        common.AddRange(OptimizeMode.CompileFlags(optimize));
        common.AddRange(new[]
        {
            "-ffreestanding", "-fno-stack-protector", "-fms-extensions", "-fms-compatibility",
            "-nostdinc", "-include", "picolibc.h", "-march=pentium3",
            // Every Xbox title is built with _XBOX/XBOX defined (the XDK did this); a lot of
            // Xbox headers/code select their platform path on it.
            "-D_XBOX", "-DXBOX",
            // Keep Clang from inline-expanding memmove/memcpy-shaped calls past picolibc's
            // -fno-builtin implementations, and pin the retail (_DEBUG-off) SDK link path.
            "-fno-builtin", "-U_DEBUG",
            // Thread-local storage: emulated TLS (a per-thread table reached via
            // __emutls_get_address, backed by libc tss/emutls.c) instead of the native
            // Windows __tls_index/TEB %fs model, which the RXDK runtime never sets up.
            // Without this, any title `__thread`/`thread_local` (e.g. stb_image's
            // stbi__g_failure_reason / vertically_flip_on_load) reads a wild fixed
            // address and bugchecks. Matches how libcpp is built (xbox_target.zig
            // cppFlags).
            "-femulated-tls",
            // -femulated-tls makes clang still emit a CodeView S_*THREAD32 debug record
            // per thread_local, pointing at the native per-var symbol that emutls never
            // defines -> undefined-symbol at link (xbox_target.zig cppFlags drops ALL
            // debug with -g0 for the same reason). We keep file/line tables (needed for
            // the PDB: F5 stepping + crash symbolization) but omit the per-variable
            // symbol records -- which is exactly -gline-tables-only. It overrides the
            // -g that the Debug/ReleaseSafe optimize modes add, at the cost of local-
            // variable inspection in those modes (acceptable: title thread_locals link
            // cleanly and line-level debugging still works).
            "-gline-tables-only",
        });
        common.AddRange(includeArgs);
        common.AddRange(defineArgs);
        common.AddRange(XdkClangWarnings);
        common.AddRange(new[] { "-c", source, $"-o{obj}" });

        var toolArgs = new List<string>();
        if (isCpp)
        {
            toolArgs.AddRange(new[] { "c++", "-std=c++23", "-nostdinc++", "-fno-exceptions", "-frtti" });
            // C++ standard library: RXDK ships libc++ (built against picolibc) with headers staged
            // at sdk/include/c++/v1. Add it *before* the C include dir so libc++'s C-header wrappers
            // (ctype.h/wchar.h/...) win and include_next into picolibc. -fms-compatibility-version
            // simulates MSVC 2015+, where char16_t/char32_t are native keywords libc++ requires;
            // plain -fms-compatibility emulates older MSVC and disables them. (libcpp.lib is linked
            // via the project's libraries — the importer force-adds it, and the C++ templates list it.)
            var cxxInc = Path.Combine(SdkLayout.GetSdkIncludeDir(), "c++", "v1");
            if (Directory.Exists(cxxInc))
                toolArgs.AddRange(new[]
                {
                    $"-I{cxxInc}", "-fms-compatibility-version=19.20",
                    // libc++ was built with _WIN32/__MINGW32__ undefined so it takes its newlib
                    // (not Win32/MSVCRT) locale + support backends. Consuming TUs must match, or
                    // libc++'s <locale> pulls the Windows backend and fails on MSVC-only types
                    // like _locale_t that picolibc doesn't provide.
                    "-U_WIN32", "-U__MINGW32__",
                    // The newlib locale backend calls picolibc's *_l locale functions
                    // (strtod_l, ...), which picolibc only declares under __GNU_VISIBLE
                    // (_GNU_SOURCE). libcpp's own build enables it via picolibc_prereq.h.
                    "-D_GNU_SOURCE",
                });
        }
        else
        {
            toolArgs.AddRange(new[] { "cc", "-std=c23" });
        }
        toolArgs.AddRange(common);

        var result = await ProcessRunner.RunStreamedAsync(zig, toolArgs, log, ct: ct);

        // Surface (but don't fail on) warnings in the title's own source. Clean RXDK template
        // code produces none, but imported/legacy code warns heavily — most notably -Wformat on
        // DWORD-vs-%u, which is benign on this ILP32 target — while still compiling correctly.
        // Failing the build on those would make importing real projects impractical.
        var combined = (result.StdOut + result.StdErr).Split('\n');
        var sourcePattern = new Regex(Regex.Escape(Path.GetFullPath(source)));
        var warnCount = combined.Count(l => l.Contains(": warning:") && sourcePattern.IsMatch(l));
        if (warnCount > 0 && isCpp)
            log?.Invoke($"Note: {warnCount} warning(s) in {Path.GetFileName(source)} (not fatal)");
        if (!result.Success)
            throw new InvalidOperationException($"Zig compile failed on {source} (exit {result.ExitCode})");
    }

    /// <summary>
    /// Runs the bundler on the project's .rdf resource files. Uses the explicit
    /// <see cref="RxdkProjectManifest.Resources"/> list if present, otherwise auto-discovers
    /// every *.rdf under the project root. The bundler resolves out_header / out_packedresource
    /// paths relative to each .rdf, so outputs land in the project tree (Resource.h next to the
    /// sources, the .xpr under the media/deploy path named in the .rdf).
    /// </summary>
    private static async Task CompileResourcesAsync(
        string projectRoot, RxdkProjectManifest manifest, Action<string>? log, CancellationToken ct)
    {
        var rdfs = new List<string>();
        if (manifest.Resources is { Count: > 0 })
        {
            foreach (var rel in manifest.Resources)
            {
                if (string.IsNullOrWhiteSpace(rel)) continue;
                var p = Path.GetFullPath(Path.Combine(projectRoot, rel.Replace('/', Path.DirectorySeparatorChar)));
                if (!File.Exists(p))
                    throw new FileNotFoundException($"resources: .rdf not found: {p}");
                rdfs.Add(p);
            }
        }
        else
        {
            rdfs.AddRange(Directory.EnumerateFiles(projectRoot, "*.rdf", SearchOption.AllDirectories));
        }

        if (rdfs.Count == 0)
            return;

        var bundler = RxdkPaths.ResolveHostTool("bundler");
        if (!File.Exists(bundler))
            throw new FileNotFoundException(
                $"bundler host tool not found: {bundler}. Update the RXDK tools (the resource pipeline needs the bundler).");

        foreach (var rdf in rdfs)
        {
            log?.Invoke($"Compiling resources: {Path.GetFileName(rdf)}");
            var result = await ProcessRunner.RunStreamedAsync(
                bundler, new[] { rdf, "-q" }, log, Path.GetDirectoryName(rdf), ct);
            if (!result.Success)
                throw new InvalidOperationException(
                    $"bundler failed on {Path.GetFileName(rdf)} (exit {result.ExitCode})");
        }
    }

    // ---- multi-project (library reference) support ----

    private static List<string> GetProjectReferences(string projectRoot, RxdkProjectManifest m)
    {
        var refs = new List<string>();
        foreach (var rel in m.ProjectReferences ?? new())
        {
            if (string.IsNullOrWhiteSpace(rel)) continue;
            var dir = Path.GetFullPath(Path.Combine(projectRoot, rel));
            if (!HasManifest(dir))
                throw new InvalidOperationException(
                    $"projectReferences: no manifest in {dir} " +
                    "(rxdk.project.json, or out\\rxdk.manifest.json from a prior build)");
            refs.Add(dir);
        }
        return refs;
    }

    private static void AddDependencyOrder(string dir, List<string> ordered, Dictionary<string, string> state)
    {
        var key = dir.ToLowerInvariant();
        if (state.TryGetValue(key, out var s))
        {
            if (s == "done") return;
            if (s == "visiting") throw new InvalidOperationException($"Cyclic projectReferences involving {dir}");
        }
        state[key] = "visiting";
        var manifest = ReadManifest(dir);
        foreach (var reference in GetProjectReferences(dir, manifest))
            AddDependencyOrder(reference, ordered, state);
        state[key] = "done";
        ordered.Add(dir);
    }

    /// <summary>Transitive library dependencies, in build (deps-first) order.</summary>
    private static List<string> GetDependencyOrder(string projectRoot, RxdkProjectManifest m)
    {
        var ordered = new List<string>();
        var state = new Dictionary<string, string>();
        foreach (var reference in GetProjectReferences(projectRoot, m))
            AddDependencyOrder(reference, ordered, state);
        return ordered;
    }

    private static List<string> ResolveIncludeArgs(string projectRoot, IReadOnlyList<string>? values, string label)
    {
        var outList = new List<string>();
        foreach (var rel in values ?? new List<string>())
        {
            if (string.IsNullOrWhiteSpace(rel)) continue;
            var dir = Path.GetFullPath(Path.Combine(projectRoot, rel));
            if (!Directory.Exists(dir)) throw new InvalidOperationException($"{label}: not found {dir}");
            outList.Add($"-I{dir}");
        }
        return outList;
    }

    /// <summary>Public includes exported by every transitive library dependency (deduped -I args).</summary>
    private static List<string> GetTransitivePublicIncludeArgs(string projectRoot, RxdkProjectManifest m)
    {
        var seen = new HashSet<string>();
        var outList = new List<string>();
        foreach (var dep in GetDependencyOrder(projectRoot, m))
        {
            var depManifest = ReadManifest(dep);
            foreach (var arg in ResolveIncludeArgs(dep, depManifest.PublicIncludePaths, "publicIncludePaths"))
                if (seen.Add(arg)) outList.Add(arg);
        }
        return outList;
    }

    private static async Task<(List<string> objs, bool usesCpp)> CompileProjectSourcesAsync(
        string projectRoot, RxdkProjectManifest m, string zig, string outDir,
        IReadOnlyList<string> includeArgs, IReadOnlyList<string> defineArgs,
        RxdkOptimizeMode optimize, Action<string>? log, CancellationToken ct)
    {
        var objs = new List<string>();
        var usesCpp = false;
        foreach (var relSrc in m.Sources ?? new())
        {
            var src = Path.Combine(projectRoot, relSrc.Replace('/', Path.DirectorySeparatorChar));
            if (!File.Exists(src)) throw new FileNotFoundException($"Source not found: {src}");
            var obj = Path.Combine(outDir, $"{Path.GetFileNameWithoutExtension(src)}.obj");
            var ext = Path.GetExtension(src).ToLowerInvariant();
            var isCpp = ext is ".cpp" or ".cxx";
            if (isCpp) usesCpp = true;
            await ZigCompileAsync(zig, src, obj, includeArgs, defineArgs, isCpp, optimize, log, ct);
            log?.Invoke($"Compiled {obj}");
            objs.Add(obj);
        }
        return (objs, usesCpp);
    }

    /// <summary>Build one library project to a static .lib and return its path.</summary>
    private static async Task<string> BuildLibraryAsync(
        string libRoot, string zig, string sdkInclude, RxdkOptimizeMode optimize,
        Action<string>? log, CancellationToken ct, RxdkProjectManifest? knownManifest = null)
    {
        // knownManifest is the resolved manifest for a top-level library (native .vcxproj flow,
        // which has no rxdk.project.json on disk); a projectReference dep reads its own.
        var manifest = knownManifest ?? ReadManifest(libRoot);
        if (manifest.Type != RxdkProjectKind.Library)
            throw new InvalidOperationException(
                $"projectReferences must point to type:library projects - {manifest.Name} is not one");
        var outDir = SdkLayout.GetProjectOutDir(libRoot, manifest);
        Directory.CreateDirectory(outDir);

        var includeArgs = new List<string> { "-I", sdkInclude };
        includeArgs.AddRange(ResolveIncludeArgs(libRoot, manifest.IncludePaths, "includePaths"));
        includeArgs.AddRange(ResolveIncludeArgs(libRoot, manifest.PublicIncludePaths, "publicIncludePaths"));
        includeArgs.AddRange(GetTransitivePublicIncludeArgs(libRoot, manifest));
        var defineArgs = ProjectDefineArgs(manifest);

        log?.Invoke($"== Building library {manifest.Name} ==");
        var (objs, _) = await CompileProjectSourcesAsync(
            libRoot, manifest, zig, outDir, includeArgs, defineArgs, optimize, log, ct);
        if (objs.Count == 0)
            throw new InvalidOperationException($"Library {manifest.Name} has no sources to archive");

        var lib = Path.Combine(outDir, $"{manifest.Name}.lib");
        if (File.Exists(lib)) File.Delete(lib);
        var arArgs = new List<string> { "ar", "rcs", lib };
        arArgs.AddRange(objs);
        var ar = await ProcessRunner.RunStreamedAsync(zig, arArgs, log, ct: ct);
        if (!ar.Success) throw new InvalidOperationException($"Archiving {lib} failed (exit {ar.ExitCode})");
        log?.Invoke($"Archived {lib}");
        return lib;
    }

    // ---- main ----

    public static async Task<BuildResult> BuildAsync(BuildOptions opts, CancellationToken ct = default)
    {
        var log = opts.Log;
        try
        {
            var projectRoot = Path.GetFullPath(opts.ProjectRoot);
            var manifest = RxdkManifestLoader.Resolve(projectRoot, opts.ManifestPath);
            var projectName = manifest.Name;
            var outDir = SdkLayout.GetProjectOutDir(projectRoot, manifest);
            Directory.CreateDirectory(outDir);
            var optimize = opts.Optimize;

            // Resource pipeline: compile any .rdf files with the bundler BEFORE the C/C++
            // sources, so the generated Resource.h exists at compile time and the packed .xpr
            // is written (to the out_packedresource path named in the .rdf) for deploy.
            await CompileResourcesAsync(projectRoot, manifest, log, ct);

            var sdkInclude = SdkLayout.GetSdkIncludeDir();
            var sdkLib = SdkLayout.GetSdkLibDir();
            if (!Directory.Exists(sdkInclude))
                throw new DirectoryNotFoundException("Missing sdk/include - run RXDK prerequisites (SDK install)");

            var zig = await ZigRuntime.ResolveZigExecutableAsync(opts.ZigExecutable, ct)
                ?? throw new InvalidOperationException(
                    "Zig not found. Install Zig (install-zig), or add zig to PATH.");

            var configuration = manifest.EffectiveConfiguration;
            var sdkLibDir = SdkLayout.ResolveSdkLibVariantDir(sdkLib, configuration);
            log?.Invoke($"Linking SDK libraries (configuration: {configuration.ToString().ToLowerInvariant()})");
            // Library search dirs: the SDK lib variant dir first, then any user libraryPaths.
            var libSearchDirs = new List<string> { sdkLibDir };
            foreach (var rel in manifest.LibraryPaths ?? new())
            {
                if (string.IsNullOrWhiteSpace(rel)) continue;
                var dir = Path.GetFullPath(Path.Combine(projectRoot, rel.Replace('/', Path.DirectorySeparatorChar)));
                if (Directory.Exists(dir)) libSearchDirs.Add(dir);
                else log?.Invoke($"Warning: libraryPath not found: {dir}");
            }
            string? ResolveLib(string name)
            {
                foreach (var dir in libSearchDirs)
                {
                    var candidate = Path.Combine(dir, name);
                    if (File.Exists(candidate)) return candidate;
                }
                return null;
            }

            // Referenced library projects, in dependency order. If a dep's .lib is already
            // built (native .vcxproj flow: VS builds the child project first via a
            // ProjectReference), link it directly; otherwise build it now (CLI / no VS).
            var depOrder = GetDependencyOrder(projectRoot, manifest);
            var userLibs = new List<string>();
            foreach (var dep in depOrder)
            {
                var depManifest = ReadManifest(dep);
                var prebuilt = Path.Combine(SdkLayout.GetProjectOutDir(dep, depManifest), $"{depManifest.Name}.lib");
                if (File.Exists(prebuilt))
                {
                    log?.Invoke($"Using prebuilt library {prebuilt}");
                    userLibs.Add(prebuilt);
                }
                else
                {
                    userLibs.Add(await BuildLibraryAsync(dep, zig, sdkInclude, optimize, log, ct, depManifest));
                }
            }

            // Explicit prebuilt .lib files (additionalLibraries), linked verbatim alongside deps.
            foreach (var rel in manifest.AdditionalLibraries ?? new())
            {
                if (string.IsNullOrWhiteSpace(rel)) continue;
                var lib = Path.GetFullPath(Path.Combine(projectRoot, rel.Replace('/', Path.DirectorySeparatorChar)));
                if (File.Exists(lib)) { log?.Invoke($"Linking additional library {lib}"); userLibs.Add(lib); }
                else throw new FileNotFoundException($"additionalLibraries: not found: {lib}");
            }

            // A library root builds to a .lib and stops (no link / imagebld / deploy).
            if (manifest.Type == RxdkProjectKind.Library)
            {
                var lib = await BuildLibraryAsync(projectRoot, zig, sdkInclude, optimize, log, ct, manifest);
                log?.Invoke($"OK: library {projectName} build complete -> {lib}");
                return new BuildResult(true, outDir);
            }

            // Compile this executable's own sources.
            var projectIncludeArgs = new List<string> { "-I", sdkInclude };
            projectIncludeArgs.AddRange(ResolveIncludeArgs(projectRoot, manifest.IncludePaths, "includePaths"));
            projectIncludeArgs.AddRange(ResolveIncludeArgs(projectRoot, manifest.PublicIncludePaths, "publicIncludePaths"));
            projectIncludeArgs.AddRange(GetTransitivePublicIncludeArgs(projectRoot, manifest));
            var projectDefines = ProjectDefineArgs(manifest);

            log?.Invoke($"== Building executable {projectName} ==");
            var (objs, _) = await CompileProjectSourcesAsync(
                projectRoot, manifest, zig, outDir, projectIncludeArgs, projectDefines, optimize, log, ct);

            if (opts.CompileOnly)
            {
                log?.Invoke("Compile OK (compileOnly).");
                return new BuildResult(true, outDir);
            }

            // SDK libraries to link: executable's own + every referenced library's, deduped in
            // first-seen order, libkernel forced last so other archives resolve kernel imports.
            var libNames = new List<string>();
            void AddLibName(string n) { if (!string.IsNullOrWhiteSpace(n) && !libNames.Contains(n)) libNames.Add(n); }
            foreach (var n in manifest.Libraries ?? new()) AddLibName(n);
            foreach (var dep in depOrder)
                foreach (var n in ReadManifest(dep).Libraries ?? new()) AddLibName(n);
            if (libNames.Contains("libkernel"))
            {
                libNames.Remove("libkernel");
                libNames.Add("libkernel");
            }

            var isDxt = manifest.Type == RxdkProjectKind.Dxt;
            var entry = isDxt ? "DxtEntry" : libNames.Contains("libxapi") ? "XapiTitleStartup" : "start";

            var linkLibs = new List<string>();
            if (isDxt) linkLibs.Add("-Wl,--dynamicbase"); // DXT keeps its base-reloc table.
            if (userLibs.Count > 0)
            {
                linkLibs.Add("-Wl,--start-group");
                linkLibs.AddRange(userLibs);
                linkLibs.Add("-Wl,--end-group");
            }
            foreach (var libName in libNames)
            {
                var resolved = ResolveLib($"{libName}.lib")
                    ?? (libName == "libkernel" ? ResolveLib("xboxkrnl.lib") : null);
                if (resolved is null)
                    throw new InvalidOperationException(
                        $"Missing library: {libName}.lib under sdk/lib - run RXDK SDK install");
                linkLibs.Add(resolved);
            }

            var exe = Path.GetFullPath(Path.Combine(outDir, $"{projectName}.exe"));
            var linkResult = await XdkLink.LinkAsync(
                zig, objs, linkLibs, exe, entry, sdkLibDir,
                OptimizeMode.KeepsDebugInfo(optimize), log, ct);
            if (!linkResult.Success)
                throw new InvalidOperationException($"Link failed (exit {linkResult.ExitCode})");
            log?.Invoke($"Linked {exe}");

            // A DXT is a raw flat PE, not an XBE.
            if (isDxt)
            {
                var imageBldDxt = RxdkPaths.ResolveHostTool("imagebld");
                if (!File.Exists(imageBldDxt)) throw new FileNotFoundException($"Missing {imageBldDxt}");
                var dxt = await ImageBuild.BuildDxtAsync(
                    exe, Path.GetFullPath(Path.Combine(outDir, $"{projectName}.dxt")), imageBldDxt, log, ct);
                log?.Invoke($"Built {dxt}");
                log?.Invoke($"OK: DXT {projectName} build complete -> {outDir}");
                return new BuildResult(true, outDir);
            }

            var imageBldPath = RxdkPaths.ResolveHostTool("imagebld");
            var xdvdfsPath = RxdkPaths.ResolveHostTool("xdvdfs");
            if (!File.Exists(imageBldPath)) throw new FileNotFoundException($"Missing {imageBldPath}");
            if (!File.Exists(xdvdfsPath)) throw new FileNotFoundException($"Missing {xdvdfsPath}");

            var insertFiles = new List<string>();
            foreach (var item in manifest.Embed ?? new())
            {
                if (string.IsNullOrEmpty(item.Path) || string.IsNullOrEmpty(item.Name)) continue;
                var embedPath = Path.Combine(projectRoot, item.Path.Replace('/', Path.DirectorySeparatorChar));
                if (File.Exists(embedPath))
                {
                    insertFiles.Add($"{Path.GetFullPath(embedPath)},{item.Name},R");
                    log?.Invoke($"Embedding {item.Name} from {embedPath}");
                }
                else
                {
                    log?.Invoke($"Warning: embed path not found: {embedPath}");
                }
            }

            var xbe = await ImageBuild.BuildXbeAsync(exe, imageBldPath, manifest.ImageBuild, insertFiles, projectRoot, log, ct);
            log?.Invoke($"Built {xbe}");

            if (manifest.CreateIso ?? true)
            {
                try
                {
                    var stageFiles = PackXiso.ResolveDeployPaths(projectRoot, manifest.DeployPaths, log);
                    if (stageFiles.Count > 0)
                        log?.Invoke($"Staging {stageFiles.Count} deployPaths file(s) into ISO");
                    var iso = await PackXiso.PackAsync(xbe, projectName, outDir, xdvdfsPath, stageFiles, log, ct);
                    log?.Invoke($"Packed {iso}");
                }
                catch (Exception err)
                {
                    log?.Invoke($"Note: ISO pack skipped ({err.Message})");
                }
            }
            else
            {
                log?.Invoke("ISO creation disabled (createIso=false); .xbe is the final output.");
            }

            log?.Invoke($"OK: {projectName} build complete -> {outDir}");
            return new BuildResult(true, outDir);
        }
        catch (Exception err)
        {
            return new BuildResult(false, "", err.Message);
        }
    }
}
