using Rxdk.Engine.Model;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Build;

/// <summary>
/// Wraps the imagebld host tool: PE .exe → Xbox .xbe, or → flat .dxt. C# port of
/// RXDK-VSCode imageBuild.ts.
/// </summary>
public static class ImageBuild
{
    private sealed class ResolvedSettings
    {
        public int StackSize = 65536;
        public bool Debug = true;
        public bool NoLogo = true;
        public bool NoLibWarn = true;
        public bool LimitMemory;
        public bool DontModifyHardDisk;
        public bool DontMountUtilityDrive;
        public bool FormatUtilityDrive;
        public int UtilityDriveClusterSize;
        public List<string> NoPreload = new();
    }

    private static ResolvedSettings Resolve(RxdkImageBuildOptions? o)
    {
        var s = new ResolvedSettings();
        if (o is null) return s;
        if (o.StackSize is { } v) s.StackSize = v;
        if (o.Debug is { } d) s.Debug = d;
        if (o.NoLogo is { } nl) s.NoLogo = nl;
        if (o.NoLibWarn is { } nlw) s.NoLibWarn = nlw;
        if (o.LimitMemory is { } lm) s.LimitMemory = lm;
        if (o.DontModifyHardDisk is { } dmh) s.DontModifyHardDisk = dmh;
        if (o.DontMountUtilityDrive is { } dmu) s.DontMountUtilityDrive = dmu;
        if (o.FormatUtilityDrive is { } fu) s.FormatUtilityDrive = fu;
        if (o.UtilityDriveClusterSize is { } uc) s.UtilityDriveClusterSize = uc;
        if (o.NoPreload is { } np) s.NoPreload = np;
        return s;
    }

    /// <summary>Convert a linked Win32 PE .exe into an Xbox .xbe. Returns the .xbe path.</summary>
    public static async Task<string> BuildXbeAsync(
        string inputExe, string toolPath, RxdkImageBuildOptions? imageBuild,
        IReadOnlyList<string>? insertFiles = null, Action<string>? log = null,
        CancellationToken ct = default)
    {
        var input = Path.GetFullPath(inputExe);
        if (!File.Exists(input)) throw new FileNotFoundException($"imagebld: input not found: {input}");
        if (!File.Exists(toolPath)) throw new FileNotFoundException($"imagebld: tool not found: {toolPath}");

        var output = Path.GetFullPath(System.Text.RegularExpressions.Regex.Replace(input, @"\.exe$", ".xbe",
            System.Text.RegularExpressions.RegexOptions.IgnoreCase));
        var cfg = Resolve(imageBuild);
        if (cfg.FormatUtilityDrive && cfg.DontMountUtilityDrive)
            throw new InvalidOperationException(
                "imageBuild: formatUtilityDrive and dontMountUtilityDrive cannot both be true");

        var args = new List<string> { $"/in:{input}", $"/out:{output}" };
        if (cfg.NoLogo) args.Add("/nologo");
        if (cfg.StackSize > 0) args.Add($"/stack:{cfg.StackSize}");
        if (cfg.Debug) args.Add("/debug");
        if (cfg.NoLibWarn) args.Add("/nolibwarn");
        if (cfg.LimitMemory) args.Add("/limitmem");
        if (cfg.DontModifyHardDisk) args.Add("/dontmodifyhd");
        if (cfg.DontMountUtilityDrive) args.Add("/dontmountud");
        if (cfg.FormatUtilityDrive) args.Add("/formatud");
        if (cfg.UtilityDriveClusterSize > 0) args.Add($"/udcluster:{cfg.UtilityDriveClusterSize}");
        foreach (var section in cfg.NoPreload.Where(s => !string.IsNullOrEmpty(s)))
            args.Add($"/nopreload:{section}");
        foreach (var insert in (insertFiles ?? Array.Empty<string>()).Where(s => !string.IsNullOrEmpty(s)))
            args.Add($"/INSERTFILE:{insert}");

        var r = await ProcessRunner.RunStreamedAsync(toolPath, args, log, ct: ct);
        if (!r.Success) throw new InvalidOperationException($"imagebld failed (exit {r.ExitCode})");
        return output;
    }

    /// <summary>Convert a linked PE .exe into a flat .dxt via `imagebld /DXT`. Returns the .dxt path.</summary>
    public static async Task<string> BuildDxtAsync(
        string inputExe, string outputDxt, string toolPath, Action<string>? log = null,
        CancellationToken ct = default)
    {
        var input = Path.GetFullPath(inputExe);
        if (!File.Exists(input)) throw new FileNotFoundException($"imagebld: input not found: {input}");
        if (!File.Exists(toolPath)) throw new FileNotFoundException($"imagebld: tool not found: {toolPath}");
        var output = Path.GetFullPath(outputDxt);

        var r = await ProcessRunner.RunStreamedAsync(toolPath, new[] { "/DXT", input, output }, log, ct: ct);
        if (!r.Success) throw new InvalidOperationException($"imagebld /DXT failed (exit {r.ExitCode})");
        return output;
    }
}
