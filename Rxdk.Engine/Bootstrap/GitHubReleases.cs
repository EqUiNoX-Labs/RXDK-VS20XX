using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Rxdk.Engine.Bootstrap;

public sealed class GitHubAsset
{
    [JsonPropertyName("name")] public string Name { get; set; } = "";
    [JsonPropertyName("browser_download_url")] public string BrowserDownloadUrl { get; set; } = "";
}

public sealed class GitHubRelease
{
    [JsonPropertyName("tag_name")] public string TagName { get; set; } = "";
    [JsonPropertyName("assets")] public List<GitHubAsset> Assets { get; set; } = new();
}

/// <summary>
/// GitHub Releases lookup. C# port of hostTools.ts fetchRelease — resolves latest or a
/// pinned tag, forwards GITHUB_TOKEN/GH_TOKEN, and surfaces rate-limit (403/429) clearly.
/// </summary>
public static class GitHubReleases
{
    private static readonly HttpClient Http = CreateClient();

    private static HttpClient CreateClient()
    {
        var client = new HttpClient();
        // GitHub requires a User-Agent; also send the versioned Accept header.
        client.DefaultRequestHeaders.UserAgent.ParseAdd("RXDK-VS20XX");
        client.DefaultRequestHeaders.Accept.Add(
            new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        client.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
        client.Timeout = TimeSpan.FromMinutes(5);
        return client;
    }

    public static async Task<GitHubRelease> FetchReleaseAsync(
        string repo, string? tag, CancellationToken ct = default)
    {
        var url = !string.IsNullOrEmpty(tag) && tag != "latest"
            ? $"https://api.github.com/repos/{repo}/releases/tags/{Uri.EscapeDataString(tag)}"
            : $"https://api.github.com/repos/{repo}/releases/latest";

        using var request = new HttpRequestMessage(HttpMethod.Get, url);
        var token = Environment.GetEnvironmentVariable("GITHUB_TOKEN")
                    ?? Environment.GetEnvironmentVariable("GH_TOKEN");
        if (!string.IsNullOrEmpty(token))
            request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);

        using var response = await Http.SendAsync(request, ct);
        if (response.StatusCode is HttpStatusCode.Forbidden or HttpStatusCode.TooManyRequests)
        {
            throw new InvalidOperationException(
                $"GitHub API rate limit reached fetching {repo}. Set GITHUB_TOKEN, or pin a " +
                "release tag, then retry.");
        }
        if (!response.IsSuccessStatusCode)
            throw new InvalidOperationException($"GitHub API error {(int)response.StatusCode} for {repo}");

        var json = await response.Content.ReadAsStringAsync(ct);
        return JsonSerializer.Deserialize<GitHubRelease>(json)
               ?? throw new InvalidDataException($"Empty release JSON for {repo}");
    }

    /// <summary>Find an asset by exact name, or throw with the release tag for context.</summary>
    public static GitHubAsset RequireAsset(GitHubRelease release, string assetName, string repo)
    {
        var asset = release.Assets.FirstOrDefault(a => a.Name == assetName);
        return asset ?? throw new InvalidOperationException(
            $"{repo} {release.TagName} has no asset \"{assetName}\"");
    }
}
