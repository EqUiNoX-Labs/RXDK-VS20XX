using System.Runtime.InteropServices;

namespace Rxdk.Engine.Platform;

/// <summary>
/// Platform paths and RIDs for host tools and staged SDK. C# port of the path logic in
/// RXDK-VSCode's bridgePath.ts (platformToolRid / hostToolExecutableName) and
/// hostTools.ts (getDefaultStagedToolsRoot / getStagedToolsRoot / resolveHostTool).
/// The on-disk layout must match the VS Code extension so both share one …/RXDK/tools.
/// </summary>
public static class RxdkPaths
{
    /// <summary>.NET RID for the current platform, matching the host-tools asset naming.</summary>
    public static string PlatformToolRid()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            return "win-x64";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
            return "linux-x64";
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return RuntimeInformation.ProcessArchitecture == Architecture.Arm64 ? "osx-arm64" : "osx-x64";
        return "win-x64";
    }

    /// <summary>Append ".exe" on Windows.</summary>
    public static string HostToolExecutableName(string baseName) =>
        RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? $"{baseName}.exe" : baseName;

    /// <summary>Default persistent tools root, a sibling of the staged SDK (…/RXDK/tools).</summary>
    public static string GetDefaultStagedToolsRoot()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            var programData = Environment.GetEnvironmentVariable("ProgramData");
            if (string.IsNullOrEmpty(programData))
                programData = @"C:\ProgramData";
            return Path.Combine(programData, "RXDK", "tools");
        }
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
        {
            return Path.Combine(HomeDir(), "Library", "Application Support", "RXDK", "tools");
        }
        var xdg = Environment.GetEnvironmentVariable("XDG_DATA_HOME");
        if (string.IsNullOrWhiteSpace(xdg))
            xdg = Path.Combine(HomeDir(), ".local", "share");
        return Path.Combine(xdg, "rxdk", "tools");
    }

    /// <summary>
    /// Effective staged tools root. Honors the RXDK_STAGED_TOOLS override (the VS host passes
    /// an explicit path/env rather than reading VS Code's rxdk.stagedToolsPath setting).
    /// </summary>
    public static string GetStagedToolsRoot()
    {
        var overridePath = Environment.GetEnvironmentVariable("RXDK_STAGED_TOOLS");
        if (!string.IsNullOrWhiteSpace(overridePath))
            return Path.GetFullPath(overridePath.Trim());
        return GetDefaultStagedToolsRoot();
    }

    /// <summary>Absolute path to a host tool in the staged tools root (may not exist yet).</summary>
    public static string ResolveHostTool(string baseName) =>
        Path.Combine(GetStagedToolsRoot(), HostToolExecutableName(baseName));

    private static string HomeDir() =>
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
}
