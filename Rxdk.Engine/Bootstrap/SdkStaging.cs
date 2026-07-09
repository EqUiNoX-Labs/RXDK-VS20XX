using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Bootstrap;

/// <summary>
/// Clones/updates the RXDK-SDK (headers + libs) into the staged SDK root. C# port of the
/// headless core of RXDK-VSCode sdkStaging.ts (the VS Code progress/notification wrappers
/// are omitted). Layout matches the extension so both share one …/RXDK/sdk.
/// </summary>
public static class SdkStaging
{
    public const string DefaultSdkGitUrl = "https://github.com/Team-Resurgent/RXDK-SDK.git";

    /// <summary>SDK clone URL, honoring the RXDK_SDK_GIT_URL override.</summary>
    public static string GetSdkGitUrl()
    {
        var env = Environment.GetEnvironmentVariable("RXDK_SDK_GIT_URL");
        return !string.IsNullOrWhiteSpace(env) ? env.Trim() : DefaultSdkGitUrl;
    }

    /// <summary>Optional branch/tag to clone, from RXDK_SDK_GIT_REF (null = default branch).</summary>
    public static string? GetSdkGitRef()
    {
        var env = Environment.GetEnvironmentVariable("RXDK_SDK_GIT_REF");
        return string.IsNullOrWhiteSpace(env) ? null : env.Trim();
    }

    /// <summary>Headers present (include/d3d8.h exists).</summary>
    public static bool IsStagedSdkPresent() =>
        File.Exists(Path.Combine(RxdkPaths.GetStagedSdkRoot(), "include", "d3d8.h"));

    /// <summary>
    /// A linkable library is staged. Libs ship either flat (lib/libc.lib) or split by
    /// configuration (lib/release + lib/debug); a marker in any counts. Mirrors
    /// sdkStaging.ts isStagedSdkLibPresent.
    /// </summary>
    public static bool IsStagedSdkLibPresent()
    {
        var lib = Path.Combine(RxdkPaths.GetStagedSdkRoot(), "lib");
        string[] markers = { "libkernel.lib", "libc.lib", "xboxkrnl.lib", "libcmt.lib" };
        string[] dirs = { lib, Path.Combine(lib, "release"), Path.Combine(lib, "debug") };
        return dirs.Any(dir => markers.Any(m => File.Exists(Path.Combine(dir, m))));
    }

    private static bool IsGitRepo(string dir) => Directory.Exists(Path.Combine(dir, ".git"));

    /// <summary>
    /// Ensure the staged SDK is present and up to date: pull if it's a git checkout, else
    /// clone. Never silently overwrites a non-git folder. Returns the staged root.
    /// </summary>
    public static async Task<string> EnsureAsync(
        Action<string>? log = null, CancellationToken ct = default)
    {
        var staged = RxdkPaths.GetStagedSdkRoot();
        var url = GetSdkGitUrl();
        var gitRef = GetSdkGitRef();

        if (IsGitRepo(staged))
        {
            var branch = gitRef ?? await CurrentBranchAsync(staged, ct) ?? "main";
            log?.Invoke($"RXDK: fetching latest RXDK-SDK ({branch}) → {staged}");
            await Git(new[] { "fetch", "--progress", "--depth", "1", "origin", branch },
                cwd: staged, log: log, ct: ct);
            await Git(new[] { "-C", staged, "reset", "--hard", $"origin/{branch}" }, log: log, ct: ct);
            log?.Invoke($"RXDK: SDK updated at {staged}");
            return staged;
        }

        if (Directory.Exists(staged) && Directory.EnumerateFileSystemEntries(staged).Any())
            throw new InvalidOperationException(
                $"SDK folder exists but is not a git checkout — refusing to overwrite: {staged}. " +
                "Delete it to re-clone.");

        log?.Invoke($"RXDK: cloning {url}{(gitRef is null ? "" : $" ({gitRef})")} → {staged}");
        Directory.CreateDirectory(Path.GetDirectoryName(staged)!);
        var args = new List<string> { "clone", "--progress", "--depth", "1" };
        if (gitRef is not null) { args.Add("--branch"); args.Add(gitRef); }
        args.Add(url);
        args.Add(staged);
        await Git(args, log: log, ct: ct);

        if (!IsStagedSdkPresent())
            throw new InvalidOperationException(
                $"SDK clone completed but include/d3d8.h is missing under {staged}.");
        log?.Invoke($"RXDK: SDK cloned to {staged}");
        return staged;
    }

    private static async Task<string?> CurrentBranchAsync(string repoDir, CancellationToken ct)
    {
        var r = await ProcessRunner.RunAsync(
            "git", new[] { "-C", repoDir, "rev-parse", "--abbrev-ref", "HEAD" }, ct: ct);
        var branch = r.StdOut.Trim();
        return r.Success && branch.Length > 0 && branch != "HEAD" ? branch : null;
    }

    private static async Task Git(
        IEnumerable<string> args, string? cwd = null, Action<string>? log = null,
        CancellationToken ct = default)
    {
        // Surface "Receiving objects: NN%" lines so callers can show clone progress.
        var r = await ProcessRunner.RunAsync(
            "git", args, workingDirectory: cwd,
            onStdErrLine: line =>
            {
                if (line.Contains("Receiving objects:", StringComparison.Ordinal))
                    log?.Invoke($"  {line.Trim()}");
            },
            ct: ct);
        if (!r.Success)
            throw new InvalidOperationException(
                $"git failed ({r.ExitCode}): {r.StdErr.Trim()}".Trim());
    }
}
