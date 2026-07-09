using Rxdk.Engine.Model;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Build;

/// <summary>
/// Resolves the staged-SDK include/lib directories and a project's output dir. C# port of
/// the CLI-relevant parts of RXDK-VSCode sdkPath.ts (the extension's bundled-fallback and
/// VS Code settings overrides are dropped — the VS port always uses the staged SDK).
/// </summary>
public static class SdkLayout
{
    /// <summary>Headers dir for compile + IntelliSense (…/RXDK/sdk/include).</summary>
    public static string GetSdkIncludeDir() =>
        Path.Combine(RxdkPaths.GetStagedSdkRoot(), "include");

    /// <summary>Library root for linking (…/RXDK/sdk/lib). See ResolveSdkLibVariantDir for the split.</summary>
    public static string GetSdkLibDir() =>
        Path.Combine(RxdkPaths.GetStagedSdkRoot(), "lib");

    /// <summary>A project's build output directory (manifest outputDir, default "out"), absolute.</summary>
    public static string GetProjectOutDir(string projectRoot, RxdkProjectManifest manifest) =>
        Path.GetFullPath(Path.Combine(projectRoot, string.IsNullOrWhiteSpace(manifest.OutputDir) ? "out" : manifest.OutputDir));

    /// <summary>
    /// Pick the lib dir to link from. A split SDK (lib/debug + lib/release) resolves to the
    /// requested variant subdir; a legacy flat SDK resolves to sdkLib unchanged.
    /// </summary>
    public static string ResolveSdkLibVariantDir(string sdkLib, RxdkConfiguration configuration)
    {
        var variantDir = Path.Combine(sdkLib, configuration == RxdkConfiguration.Debug ? "debug" : "release");
        return Directory.Exists(variantDir) ? variantDir : sdkLib;
    }
}
