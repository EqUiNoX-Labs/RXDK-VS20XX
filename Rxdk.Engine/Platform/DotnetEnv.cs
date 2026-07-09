namespace Rxdk.Engine.Platform;

/// <summary>
/// Env for spawning the framework-dependent .NET host tools (imagebld, xbcp, xdvdfs,
/// xboxdbg-bridge, xbwatson). C# port of RXDK-VSCode dotnetEnv.ts: if a managed runtime
/// was installed under %USERPROFILE%\.dotnet, point DOTNET_ROOT at it so the tools'
/// apphost can find it. When the runtime is already resolvable globally this is a no-op.
/// </summary>
public static class DotnetEnv
{
    /// <summary>
    /// Returns null when no override is needed (host tools resolve the runtime globally),
    /// otherwise a copy of the current environment with DOTNET_ROOT set to the managed root.
    /// </summary>
    public static IReadOnlyDictionary<string, string>? WithManagedDotnet()
    {
        if (!string.IsNullOrEmpty(Environment.GetEnvironmentVariable("DOTNET_ROOT")))
            return null; // already configured — don't override.

        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var root = Path.Combine(home, ".dotnet");
        if (!Directory.Exists(Path.Combine(root, "shared", "Microsoft.NETCore.App")))
            return null; // no managed install — global runtime is used.

        var env = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (System.Collections.DictionaryEntry kv in Environment.GetEnvironmentVariables())
            env[(string)kv.Key] = kv.Value?.ToString() ?? "";
        env["DOTNET_ROOT"] = root;
        return env;
    }
}
