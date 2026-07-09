using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Deploy;

/// <summary>Launch/reboot outcome. NoConsoleConfigured mirrors xbox-launch's exit code 2.</summary>
public sealed record LaunchResult(bool Ok, bool NoConsoleConfigured = false, string? Error = null)
{
    public static readonly LaunchResult Success = new(true);
    public static readonly LaunchResult NoConsole = new(false, NoConsoleConfigured: true);
    public static LaunchResult Fail(string error) => new(false, Error: error);
}

/// <summary>
/// Launches a deployed title / warm-reboots the devkit via the xbox-launch host tool.
/// C# port of RXDK-VSCode xboxLaunch.ts. xbox-launch exits 2 for "no console configured"
/// (not a hard failure), which surfaces as NoConsoleConfigured.
/// </summary>
public static class XboxLaunch
{
    /// <summary>
    /// Warm-reboot the console via `xbox-launch -rebootonly` (no title). A DXT in E:\dxt loads
    /// on the next boot (xbdm re-scans E:\dxt at debug-monitor init).
    /// </summary>
    public static async Task<LaunchResult> RebootConsoleAsync(
        string? consoleName = null, Action<string>? log = null, CancellationToken ct = default)
    {
        try
        {
            var launcher = RxdkPaths.ResolveHostTool("xbox-launch");
            var args = new List<string> { "-rebootonly" };
            var consoleSwitch = await ConsoleResolver.ResolveConsoleSwitchAsync(consoleName, ct);
            if (consoleSwitch is not null) { args.Add("-x"); args.Add(consoleSwitch); }

            var r = await ProcessRunner.RunStreamedAsync(launcher, args, log, ct: ct);
            if (r.ExitCode == 2)
            {
                log?.Invoke("Warning: No Xbox console configured (set the devkit IP, or Xbox Neighborhood).");
                return LaunchResult.NoConsole;
            }
            if (!r.Success)
                return LaunchResult.Fail($"xbox-launch -rebootonly failed (exit {r.ExitCode})");
            return LaunchResult.Success;
        }
        catch (Exception err)
        {
            return LaunchResult.Fail(err.Message);
        }
    }

    public sealed class LaunchOptions
    {
        public required string ProjectName { get; init; }
        public string? RemoteDir { get; init; }
        public string? Title { get; init; }
        public string? ConsoleName { get; init; }
        public string? CmdLine { get; init; }
        public bool Reboot { get; init; }
        public int TimeoutMs { get; init; } = 120000;
        public Action<string>? Log { get; init; }
    }

    /// <summary>Launch a deployed Xbox title via xbox-launch.</summary>
    public static async Task<LaunchResult> LaunchProjectAsync(LaunchOptions opts, CancellationToken ct = default)
    {
        try
        {
            var remoteDir = opts.RemoteDir ?? $@"xe:\{opts.ProjectName}";
            var title = opts.Title ?? $"{opts.ProjectName}.xbe";

            var launcher = RxdkPaths.ResolveHostTool("xbox-launch");
            var args = new List<string> { "-dir", remoteDir, "-title", title, "-timeout", opts.TimeoutMs.ToString() };
            if (!string.IsNullOrEmpty(opts.CmdLine)) { args.Add("-cmd"); args.Add(opts.CmdLine); }
            var consoleSwitch = await ConsoleResolver.ResolveConsoleSwitchAsync(opts.ConsoleName, ct);
            if (consoleSwitch is not null) { args.Add("-x"); args.Add(consoleSwitch); }
            if (opts.Reboot) args.Add("-reboot");

            var r = await ProcessRunner.RunStreamedAsync(launcher, args, opts.Log, ct: ct);
            if (r.ExitCode == 2)
            {
                opts.Log?.Invoke("Warning: No Xbox console configured (set the devkit IP, or Xbox Neighborhood).");
                return LaunchResult.NoConsole;
            }
            if (!r.Success)
                return LaunchResult.Fail($"xbox-launch failed (exit {r.ExitCode})");
            return LaunchResult.Success;
        }
        catch (Exception err)
        {
            return LaunchResult.Fail(err.Message);
        }
    }
}
