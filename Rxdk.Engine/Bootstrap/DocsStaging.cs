using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Bootstrap;

/// <summary>
/// Clones/updates RXDK-Docs (the Xbox SDK help set + the extension docs) into the staged docs
/// root (…/RXDK/docs). Mirrors <see cref="SdkStaging"/>: pull if it's a git checkout, else clone,
/// never overwriting a non-git folder. The RXDK tool window's documentation commands read from
/// this same root (docs\xboxsdk, docs\rxdk).
/// </summary>
public static class DocsStaging
{
    public const string DefaultDocsGitUrl = "https://github.com/Team-Resurgent/RXDK-Docs.git";

    /// <summary>Docs clone URL, honoring the RXDK_DOCS_GIT_URL override.</summary>
    public static string GetDocsGitUrl()
    {
        var env = Environment.GetEnvironmentVariable("RXDK_DOCS_GIT_URL");
        return !string.IsNullOrWhiteSpace(env) ? env.Trim() : DefaultDocsGitUrl;
    }

    /// <summary>Optional branch/tag to clone, from RXDK_DOCS_GIT_REF (null = default branch).</summary>
    public static string? GetDocsGitRef()
    {
        var env = Environment.GetEnvironmentVariable("RXDK_DOCS_GIT_REF");
        return string.IsNullOrWhiteSpace(env) ? null : env.Trim();
    }

    /// <summary>Docs present (rxdk/toc.json exists under the staged docs root).</summary>
    public static bool IsStagedDocsPresent() =>
        File.Exists(Path.Combine(RxdkPaths.GetStagedDocsRoot(), "rxdk", "toc.json"));

    private static bool IsGitRepo(string dir) => Directory.Exists(Path.Combine(dir, ".git"));

    /// <summary>
    /// Ensure the staged docs are present and up to date: pull if it's a git checkout, else clone.
    /// Never silently overwrites a non-git folder. Returns the staged root.
    /// </summary>
    public static async Task<string> EnsureAsync(
        Action<string>? log = null, CancellationToken ct = default)
    {
        var staged = RxdkPaths.GetStagedDocsRoot();
        var url = GetDocsGitUrl();
        var gitRef = GetDocsGitRef();

        if (IsGitRepo(staged))
        {
            var branch = gitRef ?? await CurrentBranchAsync(staged, ct) ?? "main";
            log?.Invoke($"RXDK: fetching latest RXDK-Docs ({branch}) → {staged}");
            await Git(new[] { "fetch", "--progress", "--depth", "1", "origin", branch },
                cwd: staged, log: log, ct: ct);
            await Git(new[] { "-C", staged, "reset", "--hard", $"origin/{branch}" }, log: log, ct: ct);
            log?.Invoke($"RXDK: docs updated at {staged}");
            return staged;
        }

        if (Directory.Exists(staged) && Directory.EnumerateFileSystemEntries(staged).Any())
            throw new InvalidOperationException(
                $"Docs folder exists but is not a git checkout — refusing to overwrite: {staged}. " +
                "Delete it to re-clone.");

        log?.Invoke($"RXDK: cloning {url}{(gitRef is null ? "" : $" ({gitRef})")} → {staged}");
        Directory.CreateDirectory(Path.GetDirectoryName(staged)!);
        var args = new List<string> { "clone", "--progress", "--depth", "1" };
        if (gitRef is not null) { args.Add("--branch"); args.Add(gitRef); }
        args.Add(url);
        args.Add(staged);
        await Git(args, log: log, ct: ct);

        if (!IsStagedDocsPresent())
            throw new InvalidOperationException(
                $"Docs clone completed but rxdk/toc.json is missing under {staged}.");
        log?.Invoke($"RXDK: docs cloned to {staged}");
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
