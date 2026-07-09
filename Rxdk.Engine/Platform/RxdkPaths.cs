namespace Rxdk.Engine.Platform;

/// <summary>
/// Platform paths for host tools, the staged SDK, and the managed Zig install. C# port of
/// the path logic in RXDK-VSCode (bridgePath.ts, hostTools.ts, sdkStaging.ts, zigRuntime.ts),
/// reduced to Windows only: Visual Studio 2022/2026 runs on Windows exclusively. The on-disk
/// layout still matches the VS Code extension so both toolchains share one …/RXDK tree.
/// </summary>
public static class RxdkPaths
{
    /// <summary>Host-tools RID. Always win-x64 for the VS port.</summary>
    public const string ToolRid = "win-x64";

    /// <summary>Append the Windows executable extension.</summary>
    public static string HostToolExecutableName(string baseName) => $"{baseName}.exe";

    private static string ProgramData()
    {
        var programData = Environment.GetEnvironmentVariable("ProgramData");
        return string.IsNullOrEmpty(programData) ? @"C:\ProgramData" : programData;
    }

    private static string LocalAppData() =>
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

    // ---- Staged host tools (…/RXDK/tools) ----

    public static string GetDefaultStagedToolsRoot() =>
        Path.Combine(ProgramData(), "RXDK", "tools");

    /// <summary>Effective staged tools root, honoring the RXDK_STAGED_TOOLS override.</summary>
    public static string GetStagedToolsRoot() =>
        EnvOverride("RXDK_STAGED_TOOLS") ?? GetDefaultStagedToolsRoot();

    /// <summary>Absolute path to a host tool in the staged tools root (may not exist yet).</summary>
    public static string ResolveHostTool(string baseName) =>
        Path.Combine(GetStagedToolsRoot(), HostToolExecutableName(baseName));

    // ---- Staged SDK (headers + libs, …/RXDK/sdk) ----

    public static string GetDefaultStagedSdkRoot() =>
        Path.Combine(ProgramData(), "RXDK", "sdk");

    /// <summary>Effective staged SDK root, honoring the RXDK_STAGED_SDK override.</summary>
    public static string GetStagedSdkRoot() =>
        EnvOverride("RXDK_STAGED_SDK") ?? GetDefaultStagedSdkRoot();

    // ---- Managed Zig install (…/RXDK/zig under LocalAppData) ----

    /// <summary>Persistent Zig install root.</summary>
    public static string GetZigInstallRoot() =>
        Path.Combine(LocalAppData(), "RXDK", "zig");

    private static string? EnvOverride(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return string.IsNullOrWhiteSpace(value) ? null : Path.GetFullPath(value.Trim());
    }
}
