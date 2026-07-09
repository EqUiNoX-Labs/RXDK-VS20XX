namespace Rxdk.Engine.Build;

/// <summary>
/// Build type selector, named after Zig's optimize modes. C# port of RXDK-VSCode
/// optimizeMode.ts. The pipeline shells out to `zig cc`/`zig c++` per file, so these map
/// to the closest raw clang flags rather than a single -Doptimize knob.
/// </summary>
public enum RxdkOptimizeMode
{
    Debug,
    ReleaseSafe,
    ReleaseFast,
    ReleaseSmall,
}

public static class OptimizeMode
{
    public static bool TryParse(string value, out RxdkOptimizeMode mode) =>
        Enum.TryParse(value, ignoreCase: false, out mode)
        && Enum.IsDefined(mode);

    /// <summary>Compile-time flags per mode (see optimizeMode.ts for the rationale).</summary>
    public static string[] CompileFlags(RxdkOptimizeMode mode) => mode switch
    {
        RxdkOptimizeMode.Debug => new[] { "-O0", "-g", "-fno-sanitize=undefined" },
        RxdkOptimizeMode.ReleaseSafe => new[] { "-O2", "-g", "-fsanitize=undefined", "-fsanitize-trap=undefined" },
        RxdkOptimizeMode.ReleaseFast => new[] { "-O3", "-fno-sanitize=undefined" },
        RxdkOptimizeMode.ReleaseSmall => new[] { "-Os", "-fno-sanitize=undefined" },
        _ => new[] { "-O0", "-g", "-fno-sanitize=undefined" },
    };

    /// <summary>Whether the linked .exe carries debug info (-g) for PDB/symbol generation.</summary>
    public static bool KeepsDebugInfo(RxdkOptimizeMode mode) =>
        mode is RxdkOptimizeMode.Debug or RxdkOptimizeMode.ReleaseSafe;
}
