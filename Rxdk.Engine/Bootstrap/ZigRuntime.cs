using System.IO.Compression;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Bootstrap;

/// <summary>
/// Manages the RXDK-pinned Zig toolchain used to build titles. C# port of the Windows
/// path of RXDK-VSCode zigRuntime.ts. Visual Studio is Windows-only, so only the Windows
/// (.zip) archive is handled — no tar.xz/bsdtar/xz plumbing. The SDK libraries are built
/// and tested against exactly ZIG_VERSION, so the managed install is preferred over any
/// zig on PATH (a different Clang can diverge in codegen/predefined macros).
/// </summary>
public static class ZigRuntime
{
    public const string ZigVersion = "0.16.0";
    private const string ZigDownloadPage = "https://ziglang.org/download/";

    // Zig release archives are named arch-first: zig-x86_64-windows-<version>.
    private static string ArchiveBaseName => $"zig-x86_64-windows-{ZigVersion}";
    private static string ArchiveFileName => $"{ArchiveBaseName}.zip";
    private const string ZigExe = "zig.exe";

    private static IEnumerable<string> InstalledZigCandidates()
    {
        var root = RxdkPaths.GetZigInstallRoot();
        yield return Path.Combine(root, ZigVersion, ArchiveBaseName, ZigExe);
        yield return Path.Combine(root, ZigVersion, ZigExe);
    }

    /// <summary>
    /// Resolve the zig.exe to use for a build. Order: explicit override → RXDK_ZIG env →
    /// managed pinned install → `zig` on PATH. Returns null if none is available.
    /// </summary>
    public static async Task<string?> ResolveZigExecutableAsync(
        string? overridePath = null, CancellationToken ct = default)
    {
        if (!string.IsNullOrWhiteSpace(overridePath))
        {
            var resolved = Path.GetFullPath(overridePath);
            if (!File.Exists(resolved))
                throw new FileNotFoundException($"Zig not found: {resolved}");
            return resolved;
        }

        var env = Environment.GetEnvironmentVariable("RXDK_ZIG");
        if (!string.IsNullOrWhiteSpace(env))
        {
            var resolved = Path.GetFullPath(env.Trim());
            if (!File.Exists(resolved))
                throw new FileNotFoundException($"RXDK_ZIG points to missing file: {resolved}");
            return resolved;
        }

        foreach (var candidate in InstalledZigCandidates())
            if (File.Exists(candidate))
                return candidate;

        // Fallback: `zig` on PATH (only when the managed install is absent).
        var probe = await ProcessRunner.RunAsync("zig", new[] { "version" }, ct: ct);
        return probe.Success ? "zig" : null;
    }

    public static async Task<bool> IsInstalledAsync(CancellationToken ct = default) =>
        await ResolveZigExecutableAsync(ct: ct) is not null;

    public static async Task<string?> GetVersionLineAsync(CancellationToken ct = default)
    {
        var zig = await ResolveZigExecutableAsync(ct: ct);
        if (zig is null) return null;
        var r = await ProcessRunner.RunAsync(zig, new[] { "version" }, ct: ct);
        return r.Success ? r.StdOut.Trim().Split('\n')[0].Trim() : null;
    }

    /// <summary>
    /// Download + install the pinned Zig into the managed root. Returns the resolved zig.exe.
    /// Does not mutate PATH — the build resolves Zig by absolute path; a PATH entry for user
    /// terminals is a UI concern the VS extension can add separately.
    /// </summary>
    public static async Task<string> InstallAsync(Action<string>? log = null, CancellationToken ct = default)
    {
        var url = $"{ZigDownloadPage}{ZigVersion}/{ArchiveFileName}";
        var installRoot = Path.Combine(RxdkPaths.GetZigInstallRoot(), ZigVersion);
        var extractDir = Path.Combine(installRoot, "extract");
        var archivePath = Path.Combine(Path.GetTempPath(), $"rxdk-{ArchiveFileName}");

        Directory.CreateDirectory(installRoot);
        if (Directory.Exists(extractDir))
            Directory.Delete(extractDir, recursive: true);

        log?.Invoke($"RXDK: downloading Zig {ZigVersion} from {url}");
        await DownloadFile.DownloadToPathAsync(url, archivePath, progress: null, ct: ct);

        log?.Invoke($"RXDK: extracting Zig to {installRoot}");
        Directory.CreateDirectory(extractDir);
        ZipFile.ExtractToDirectory(archivePath, extractDir, overwriteFiles: true);

        // The archive contains a nested zig-x86_64-windows-<ver>/ folder; move it into place.
        var nestedDir = Path.Combine(extractDir, ArchiveBaseName);
        var binDir = Path.Combine(installRoot, ArchiveBaseName);
        if (File.Exists(Path.Combine(nestedDir, ZigExe)))
        {
            if (Directory.Exists(binDir))
                Directory.Delete(binDir, recursive: true);
            Directory.Move(nestedDir, binDir);
        }
        else if (File.Exists(Path.Combine(extractDir, ZigExe)))
        {
            File.Copy(Path.Combine(extractDir, ZigExe), Path.Combine(installRoot, ZigExe), overwrite: true);
        }
        else
        {
            throw new InvalidDataException("Zig archive did not contain the expected executable layout.");
        }

        try { Directory.Delete(extractDir, recursive: true); } catch { /* ignore */ }
        try { File.Delete(archivePath); } catch { /* ignore */ }

        var zig = await ResolveZigExecutableAsync(ct: ct)
            ?? throw new InvalidOperationException(
                $"Zig {ZigVersion} was not detected after installation.");
        log?.Invoke($"RXDK: Zig {ZigVersion} ready ({zig})");
        return zig;
    }
}
