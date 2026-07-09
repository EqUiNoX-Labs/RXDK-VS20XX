namespace Rxdk.Engine.Bootstrap;

/// <summary>
/// Streams a URL to a file. C# port of RXDK-VSCode downloadFile.ts (minus the VS Code
/// progress plumbing, which the caller can layer on via the optional progress callback).
/// </summary>
public static class DownloadFile
{
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromMinutes(30) };

    public static string FormatBytes(long bytes) => bytes switch
    {
        < 1024 => $"{bytes} B",
        < 1024 * 1024 => $"{bytes / 1024.0:F1} KB",
        < 1024L * 1024 * 1024 => $"{bytes / (1024.0 * 1024):F1} MB",
        _ => $"{bytes / (1024.0 * 1024 * 1024):F2} GB",
    };

    /// <summary>Download <paramref name="url"/> to <paramref name="dest"/>, streaming to disk.</summary>
    public static async Task DownloadToPathAsync(
        string url, string dest,
        IProgress<(long received, long? total)>? progress = null,
        CancellationToken ct = default)
    {
        using var response = await Http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, ct);
        if (!response.IsSuccessStatusCode)
            throw new InvalidOperationException($"Download failed ({(int)response.StatusCode}): {url}");

        var total = response.Content.Headers.ContentLength;
        Directory.CreateDirectory(Path.GetDirectoryName(dest)!);

        await using var httpStream = await response.Content.ReadAsStreamAsync(ct);
        await using var fileStream = new FileStream(
            dest, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16, useAsync: true);

        var buffer = new byte[1 << 16];
        long received = 0;
        int read;
        while ((read = await httpStream.ReadAsync(buffer, ct)) > 0)
        {
            await fileStream.WriteAsync(buffer.AsMemory(0, read), ct);
            received += read;
            progress?.Report((received, total));
        }
    }
}
