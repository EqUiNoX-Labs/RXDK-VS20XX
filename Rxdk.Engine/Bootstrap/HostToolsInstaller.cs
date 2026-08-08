using System.IO.Compression;
using System.Text.RegularExpressions;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Bootstrap;

/// <summary>
/// Downloads the RXDK host tools into the staged tools root. C# port of RXDK-VSCode
/// hostTools.ts installHostTools. Pulls the RXDK-Tools managed bundle (imagebld, xbcp,
/// xbox-launch, xboxdbg-bridge, xbwatson) plus xdvdfs (separate repo) for the current
/// platform. Layout matches the VS Code extension so both share one …/RXDK/tools.
/// </summary>
public static partial class HostToolsInstaller
{
    private const string RxdkToolsRepo = "Team-Resurgent/RXDK-Tools";
    private const string XdvdfsRepo = "Team-Resurgent/xdvdfs";

    /// <summary>
    /// Host tools that must be present for RXDK to build, pack, and deploy. Mirrors
    /// scripts/required-tools.txt / hostTools.ts REQUIRED_HOST_TOOLS.
    /// </summary>
    public static readonly string[] RequiredHostTools =
    {
        "imagebld", "bundler", "xactbld", "xsasm", "xbcp", "xbox-launch", "xboxdbg-bridge", "xbwatson", "xdvdfs",
    };

    /// <summary>True when every required tool exists in the staged tools root.</summary>
    public static bool IsInstalled()
    {
        var root = RxdkPaths.GetStagedToolsRoot();
        return RequiredHostTools.All(t =>
            File.Exists(Path.Combine(root, RxdkPaths.HostToolExecutableName(t))));
    }

    [GeneratedRegex(@"(^|/)tools/[^/]+$")]
    private static partial Regex ToolsEntryRegex();

    // Visual Studio is Windows-only, so the platform asset is always the Windows variant.
    private const string XdvdfsAssetPrefix = "xdvdfs-windows-";

    /// <summary>
    /// Download + extract host tools into the staged root. <paramref name="hostToolsTag"/> /
    /// <paramref name="xdvdfsTag"/> pin releases (null = latest). Returns the tools root.
    /// </summary>
    public static async Task<string> InstallAsync(
        string? hostToolsTag = null,
        string? xdvdfsTag = null,
        Action<string>? log = null,
        CancellationToken ct = default)
    {
        var root = RxdkPaths.GetStagedToolsRoot();
        Directory.CreateDirectory(root);

        // 1. RXDK-Tools managed bundle.
        log?.Invoke("Resolving RXDK-Tools release…");
        var toolsRelease = await GitHubReleases.FetchReleaseAsync(RxdkToolsRepo, hostToolsTag, ct);
        var toolsAsset = GitHubReleases.RequireAsset(
            toolsRelease, $"rxdk-managed-{RxdkPaths.ToolRid}.zip", RxdkToolsRepo);
        log?.Invoke($"RXDK: host tools {toolsRelease.TagName} → {root}");
        var wrote = await DownloadAndExtractAsync(
            toolsAsset.BrowserDownloadUrl, root,
            // Files directly under a tools/ dir inside the archive (dist/rxdk-managed-<rid>/tools/*).
            name => ToolsEntryRegex().IsMatch(name),
            "RXDK-Tools", log, ct);
        log?.Invoke($"RXDK: extracted {wrote} file(s) from RXDK-Tools");

        // 2. xdvdfs (separate repo).
        log?.Invoke("Resolving xdvdfs release…");
        var xdvdfsRelease = await GitHubReleases.FetchReleaseAsync(XdvdfsRepo, xdvdfsTag, ct);
        var prefix = XdvdfsAssetPrefix;
        var xdvdfsAsset = xdvdfsRelease.Assets
            .Where(a => a.Name.StartsWith(prefix, StringComparison.Ordinal)
                        && a.Name.EndsWith(".zip", StringComparison.Ordinal)
                        && !a.Name.StartsWith("xdvdfs-fsd-", StringComparison.Ordinal))
            .OrderByDescending(a => a.Name, StringComparer.Ordinal)
            .FirstOrDefault()
            ?? throw new InvalidOperationException(
                $"xdvdfs {xdvdfsRelease.TagName} has no asset matching \"{prefix}*.zip\"");
        log?.Invoke($"RXDK: xdvdfs {xdvdfsRelease.TagName} ({xdvdfsAsset.Name})");
        var xdvdfsName = RxdkPaths.HostToolExecutableName("xdvdfs");
        await DownloadAndExtractAsync(
            xdvdfsAsset.BrowserDownloadUrl, root,
            name => PosixBasename(name) == xdvdfsName,
            "xdvdfs", log, ct);

        var missing = RequiredHostTools
            .Where(t => !File.Exists(Path.Combine(root, RxdkPaths.HostToolExecutableName(t))))
            .ToList();
        if (missing.Count > 0)
            throw new InvalidOperationException(
                $"Host tools install incomplete — missing: {string.Join(", ", missing)}");

        log?.Invoke($"RXDK: host tools ready at {root}");
        return root;
    }

    private static async Task<int> DownloadAndExtractAsync(
        string url, string destRoot, Func<string, bool> pick, string label,
        Action<string>? log, CancellationToken ct)
    {
        var tmp = Path.Combine(Path.GetTempPath(), $"rxdk-hosttool-{Guid.NewGuid():N}.zip");
        try
        {
            log?.Invoke($"Downloading {label}…");
            await DownloadFile.DownloadToPathAsync(url, tmp, progress: null, ct: ct);

            log?.Invoke($"Extracting {label}…");
            var wrote = 0;
            using (var archive = ZipFile.OpenRead(tmp))
            {
                foreach (var entry in archive.Entries)
                {
                    // Directory entries have empty Name; entry.FullName is forward-slashed.
                    if (string.IsNullOrEmpty(entry.Name)) continue;
                    var normalized = entry.FullName.Replace('\\', '/');
                    if (!pick(normalized)) continue;

                    var target = Path.Combine(destRoot, PosixBasename(normalized));
                    entry.ExtractToFile(target, overwrite: true);
                    wrote++;
                }
            }
            if (wrote == 0)
                throw new InvalidOperationException($"No matching files found inside the {label} archive");
            return wrote;
        }
        finally
        {
            try { File.Delete(tmp); } catch { /* ignore */ }
        }
    }

    private static string PosixBasename(string p)
    {
        var slash = p.LastIndexOf('/');
        return slash >= 0 ? p[(slash + 1)..] : p;
    }
}
