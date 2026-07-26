using System.Text.Json.Serialization;

namespace Rxdk.Engine.Model;

// C# port of RXDK-VSCode src/projectTypes.ts. The rxdk.project.json manifest is the
// stable contract shared with the VS Code extension, so field names and semantics here
// must match that file exactly. JSON is camelCase to match the on-disk manifests.

/// <summary>Output kind. Omitted = Executable.</summary>
public enum RxdkProjectKind
{
    Executable,
    Library,
    Dxt,
}

/// <summary>
/// Which prebuilt SDK library variant this project links. The staged SDK ships every
/// library in both flavors side by side (lib/debug: Debug -O0 -g, lib/release:
/// ReleaseSmall -Os). Omitted = Release.
/// </summary>
public enum RxdkConfiguration
{
    Debug,
    Release,
}

/// <summary>A file embedded into the XBE at build time (imagebld /insertfile).</summary>
public sealed class RxdkEmbedFile
{
    public string Path { get; set; } = "";
    public string Name { get; set; } = "";
}

/// <summary>Options passed to imagebld (/stack, /debug, /limitmem, …). Omitted keys use RXDK defaults.</summary>
public sealed class RxdkImageBuildOptions
{
    public int? StackSize { get; set; }
    public bool? Debug { get; set; }
    public bool? NoLogo { get; set; }
    public bool? NoLibWarn { get; set; }
    public bool? LimitMemory { get; set; }
    public bool? DontModifyHardDisk { get; set; }
    public bool? DontMountUtilityDrive { get; set; }
    public bool? FormatUtilityDrive { get; set; }

    /// <summary>16384, 32768, or 65536 bytes. Omit or 0 for imagebld default.</summary>
    public int? UtilityDriveClusterSize { get; set; }

    /// <summary>Section names for /nopreload:&lt;section&gt;.</summary>
    public List<string>? NoPreload { get; set; }

    // ---- XBE certificate (imagebld /TEST* switches) ----

    /// <summary>Title ID (/TESTID). Decimal or 0x-hex string.</summary>
    public string? TestId { get; set; }
    /// <summary>Alternate title ID, optionally "number,key" (/TESTALTID).</summary>
    public string? TestAltId { get; set; }
    /// <summary>Allowed regions bitmask (/TESTREGION).</summary>
    public string? TestRegion { get; set; }
    /// <summary>Ratings value (/TESTRATINGS).</summary>
    public string? TestRatings { get; set; }
    /// <summary>Allowed media types bitmask (/TESTMEDIATYPES).</summary>
    public string? TestMediaTypes { get; set; }
    /// <summary>LAN key (/TESTLANKEY).</summary>
    public string? TestLanKey { get; set; }
    /// <summary>Signature key (/TESTSIGNKEY).</summary>
    public string? TestSignKey { get; set; }

    // ---- Title info (imagebld title switches) ----

    /// <summary>Test title name (/TESTNAME).</summary>
    public string? TestName { get; set; }
    /// <summary>Test version number (/TESTVERSION).</summary>
    public string? TestVersion { get; set; }
    /// <summary>Project-relative title info file (/TITLEINFO).</summary>
    public string? TitleInfo { get; set; }
    /// <summary>Project-relative title image, XPR format (/TITLEIMAGE).</summary>
    public string? TitleImage { get; set; }
    /// <summary>Project-relative default save image, XPR format (/DEFAULTSAVEIMAGE).</summary>
    public string? DefaultSaveImage { get; set; }
}

/// <summary>A prebuilt-XBE project references existing artifacts in place (no compile step).</summary>
public sealed class RxdkPrebuiltConfig
{
    /// <summary>Absolute local path to the .xbe.</summary>
    public string Xbe { get; set; } = "";

    /// <summary>Absolute local path to the .pdb (symbols).</summary>
    public string? Pdb { get; set; }

    /// <summary>Absolute local path to the .map (globals).</summary>
    public string? Map { get; set; }

    /// <summary>Optional host PE .exe; used for image size, falls back to the XBE header.</summary>
    public string? Exe { get; set; }

    /// <summary>Optional source root for PDBs built on another machine.</summary>
    public string? SrcRoot { get; set; }

    /// <summary>Remote folder name under xe:\\ for deploy/launch.</summary>
    public string RemoteName { get; set; } = "";
}

public sealed class RxdkProjectManifest
{
    public string Name { get; set; } = "";

    /// <summary>Output kind. Omitted = Executable.</summary>
    public RxdkProjectKind? Type { get; set; }

    /// <summary>Which SDK library variant to link (lib/debug or lib/release). Omitted = Release.</summary>
    public RxdkConfiguration? Configuration { get; set; }

    public List<string>? Sources { get; set; }
    public List<string>? Libraries { get; set; }

    /// <summary>
    /// Project-relative paths to library projects (folders containing an rxdk.project.json with
    /// type:"library") this project links. Resolved transitively, built in dependency order to
    /// static .libs, then linked. Their PublicIncludePaths are added to this project's compile
    /// include path automatically.
    /// </summary>
    public List<string>? ProjectReferences { get; set; }

    /// <summary>When set, this is a prebuilt-XBE project (deploy + debug, no build).</summary>
    public RxdkPrebuiltConfig? Prebuilt { get; set; }

    public string? OutputDir { get; set; }

    /// <summary>Project-relative directories copied recursively on deploy (e.g. "media" -> xe:\\&lt;name&gt;\\media).</summary>
    public List<string>? DeployPaths { get; set; }

    /// <summary>Files embedded into the XBE at build time (imagebld /insertfile).</summary>
    public List<RxdkEmbedFile>? Embed { get; set; }

    /// <summary>Pack the build output into an .iso via xdvdfs (default true). When false the build
    /// stops at the .xbe (plus any deployPaths staged into out\Build), skipping ISO creation.</summary>
    public bool? CreateIso { get; set; }

    /// <summary>imagebld.exe switches for the PE -> XBE step.</summary>
    public RxdkImageBuildOptions? ImageBuild { get; set; }

    /// <summary>Extra project-relative include directories (passed as cl /I after sdk/include).</summary>
    public List<string>? IncludePaths { get; set; }

    /// <summary>
    /// Include directories a library project exports to referencing projects (added to their
    /// compile include path). For an executable this behaves like an extra local include path.
    /// </summary>
    public List<string>? PublicIncludePaths { get; set; }

    /// <summary>Extra preprocessor defines (cl /D), appended after RXDK defaults.</summary>
    public List<string>? Defines { get; set; }

    // ---- Derived helpers (port of the free functions in projectTypes.ts) ----

    [JsonIgnore]
    public RxdkProjectKind EffectiveType => Type ?? RxdkProjectKind.Executable;

    [JsonIgnore]
    public RxdkConfiguration EffectiveConfiguration => Configuration ?? RxdkConfiguration.Release;

    [JsonIgnore]
    public bool IsPrebuilt => Prebuilt is not null && !string.IsNullOrEmpty(Prebuilt.Xbe);

    [JsonIgnore]
    public bool IsLibrary => Type == RxdkProjectKind.Library;

    /// <summary>
    /// True for a DXT (debug-monitor extension) project. Builds a raw flat .dxt (entry
    /// DxtEntry, via imagebld /DXT) instead of an XBE; deploys to xe:\dxt and loads on a warm
    /// reboot. Not debuggable via attach (it runs inside the debug monitor).
    /// </summary>
    [JsonIgnore]
    public bool IsDxt => Type == RxdkProjectKind.Dxt;

    [JsonIgnore]
    public bool UsesCpp =>
        Sources?.Any(s => System.Text.RegularExpressions.Regex.IsMatch(s, @"\.(cpp|cxx|cc)$", System.Text.RegularExpressions.RegexOptions.IgnoreCase)) ?? false;

    /// <summary>True when the project has compilable sources that need C/C++ IntelliSense.</summary>
    [JsonIgnore]
    public bool NeedsIntelliSense =>
        !IsPrebuilt &&
        (Sources?.Any(s => System.Text.RegularExpressions.Regex.IsMatch(s, @"\.(c|cpp|cxx|cc|h|hpp)$", System.Text.RegularExpressions.RegexOptions.IgnoreCase)) ?? false);
}
