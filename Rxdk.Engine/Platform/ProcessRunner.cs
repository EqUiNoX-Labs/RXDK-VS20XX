using System.Diagnostics;
using System.Text;

namespace Rxdk.Engine.Platform;

public sealed record ProcessResult(int ExitCode, string StdOut, string StdErr)
{
    public bool Success => ExitCode == 0;
}

/// <summary>
/// Runs a child process and captures its output. The pure-.NET analog of the
/// child_process execFile/spawn calls scattered across RXDK-VSCode. Used for git
/// (SDK staging) and, later, the Zig/imagebld/xbcp build+deploy pipeline.
/// </summary>
public static class ProcessRunner
{
    /// <summary>Run <paramref name="fileName"/> with args, capturing stdout/stderr.</summary>
    public static async Task<ProcessResult> RunAsync(
        string fileName,
        IEnumerable<string> args,
        string? workingDirectory = null,
        IReadOnlyDictionary<string, string>? env = null,
        Action<string>? onStdErrLine = null,
        Action<string>? onOutputLine = null,
        CancellationToken ct = default)
    {
        var psi = new ProcessStartInfo
        {
            FileName = fileName,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = workingDirectory ?? "",
        };
        foreach (var a in args)
            psi.ArgumentList.Add(a);
        if (env is not null)
            foreach (var kv in env)
                psi.Environment[kv.Key] = kv.Value;

        using var process = new Process { StartInfo = psi };
        var stdout = new StringBuilder();
        var stderr = new StringBuilder();

        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data is null) return;
            stdout.AppendLine(e.Data);
            if (e.Data.Length > 0) onOutputLine?.Invoke(e.Data);
        };
        process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data is null) return;
            stderr.AppendLine(e.Data);
            onStdErrLine?.Invoke(e.Data);
            if (e.Data.Length > 0) onOutputLine?.Invoke(e.Data);
        };

        if (!process.Start())
            throw new InvalidOperationException($"Failed to start process: {fileName}");
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        await process.WaitForExitAsync(ct);
        return new ProcessResult(process.ExitCode, stdout.ToString(), stderr.ToString());
    }

    /// <summary>
    /// Run a tool, echoing the command line and streaming every output line to
    /// <paramref name="log"/>. C# analog of RXDK-VSCode processRunner.ts runStreamed:
    /// never throws on a non-zero exit (the caller interprets the code — some host tools
    /// use non-zero for non-fatal conditions), only on spawn failure. Injects the managed
    /// .NET runtime env so framework-dependent host tools find their runtime.
    /// </summary>
    public static Task<ProcessResult> RunStreamedAsync(
        string command,
        IReadOnlyList<string> args,
        Action<string>? log = null,
        string? workingDirectory = null,
        CancellationToken ct = default)
    {
        if (log is not null)
        {
            var shown = string.Join(' ', args.Select(a => a.Contains(' ') ? $"\"{a}\"" : a));
            log($"$ {command} {shown}");
        }
        return RunAsync(
            command, args,
            workingDirectory: workingDirectory,
            env: DotnetEnv.WithManagedDotnet(),
            onOutputLine: log,
            ct: ct);
    }
}
