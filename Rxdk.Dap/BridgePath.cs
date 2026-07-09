using Rxdk.Engine.Platform;

namespace Rxdk.Dap;

/// <summary>
/// Resolves the xboxdbg-bridge host executable. C# port of bridgePath.ts, simplified: the VS
/// port keeps the bridge in the staged tools root (…/RXDK/tools), so resolution is that path,
/// with a launch-arg override and the XBOX_BRIDGE_PATH env as escape hatches.
/// </summary>
public static class BridgePath
{
    public static string Resolve(string? overridePath = null)
    {
        if (!string.IsNullOrWhiteSpace(overridePath) && File.Exists(overridePath))
            return overridePath;

        var env = Environment.GetEnvironmentVariable("XBOX_BRIDGE_PATH");
        if (!string.IsNullOrWhiteSpace(env) && File.Exists(env))
            return env;

        var staged = RxdkPaths.ResolveHostTool("xboxdbg-bridge");
        if (File.Exists(staged))
            return staged;

        throw new FileNotFoundException(
            $"{RxdkPaths.HostToolExecutableName("xboxdbg-bridge")} not found. Install the RXDK host tools " +
            "(install-tools), or set XBOX_BRIDGE_PATH.");
    }
}
