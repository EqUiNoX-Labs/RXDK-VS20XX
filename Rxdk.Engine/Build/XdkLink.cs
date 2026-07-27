using System.Linq;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Build;

/// <summary>
/// Links Xbox title objects with Zig, mirroring the SDK's own title link (build/link_pe.zig).
/// C# port of RXDK-VSCode xdkLink.ts. A title is: its objects + SDK libs, linked
/// -nostdlib -nostartfiles at the XBE image base, with compiler-rt and an explicit entry.
/// libcompat.lib is always force-linked whole-archive ahead of everything to win the
/// compiler-rt/picolibc comdat tie-break (see xdkLink.ts for the full hardware rationale).
/// </summary>
public static class XdkLink
{
    private const string ComdatFixLib = "libcompat.lib";

    public static async Task<ProcessResult> LinkAsync(
        string zig,
        IReadOnlyList<string> objs,
        IReadOnlyList<string> libs,
        string outExe,
        string entry = "start",
        string? libDir = null,
        bool debugInfo = true,
        Action<string>? log = null,
        CancellationToken ct = default)
    {
        var args = new List<string> { "cc" };

        // C++ exception unwinding: libcpp.lib bundles libunwind, whose baremetal frame lookup
        // reads &__eh_frame_start / &__eh_frame_end to find the merged .eh_frame. Those bounds
        // aren't linker-provided on this PE target, so we bracket the section with two tiny CRT
        // marker objects — begin linked first, end linked last — exactly like the SDK's own title
        // link (build/link_pe.zig). Only needed when libcpp is in the link (STL / exceptions).
        var linksLibcpp = libs.Any(l => Path.GetFileName(l).Contains("libcpp", StringComparison.OrdinalIgnoreCase));
        string? ehBegin = null, ehEnd = null;
        if (linksLibcpp)
            (ehBegin, ehEnd) = await CompileEhBracketsAsync(zig, Path.GetDirectoryName(Path.GetFullPath(outExe))!, log, ct);
        if (ehBegin is not null) args.Add(ehBegin);

        args.AddRange(objs);

        if (libDir is not null)
        {
            var comdatFix = Path.Combine(libDir, ComdatFixLib);
            if (File.Exists(comdatFix))
            {
                args.Add("-Wl,--whole-archive");
                args.Add(comdatFix);
                args.Add("-Wl,--no-whole-archive");
            }
            else
            {
                log?.Invoke(
                    $"Warning: Missing {comdatFix} — SDK predates the compiler-rt comdat fix; " +
                    "picolibc's memmove/fabs/etc. may lose to zig's compiler-rt on real hardware. " +
                    "Reinstall/update the RXDK SDK.");
            }
        }

        args.AddRange(libs);
        if (ehEnd is not null) args.Add(ehEnd); // ___eh_frame_end must follow every .eh_frame contributor
        args.AddRange(new[]
        {
            "-target", "x86-windows-gnu",
            "-nostdlib", "-nostartfiles",
            "-Wl,--image-base=0x10000",
            "-O0",
        });
        if (debugInfo) args.Add("-g");
        args.AddRange(new[] { "-rtlib=compiler-rt", "-e", string.IsNullOrEmpty(entry) ? "start" : entry, "-o", outExe });

        return await ProcessRunner.RunStreamedAsync(zig, args, log, ct: ct);
    }

    // The two .eh_frame bracket markers (i386 COFF mangles C __eh_frame_start -> ___eh_frame_start).
    // Kept in-engine (rather than shipped in the SDK) because they're a pure link-time concern.
    private const string EhBeginAsm =
        ".section .eh_frame,\"dr\"\n.globl ___eh_frame_start\n___eh_frame_start:\n";
    private const string EhEndAsm =
        ".section .eh_frame,\"dr\"\n.globl ___eh_frame_end\n___eh_frame_end:\n";

    /// <summary>
    /// Writes and compiles the two .eh_frame bracket markers next to the output. Returns
    /// (beginObj, endObj) to place first/last in the link, or (null, null) if compilation fails
    /// (the link then surfaces the missing-symbol error, which is the actionable diagnostic).
    /// </summary>
    private static async Task<(string?, string?)> CompileEhBracketsAsync(
        string zig, string outDir, Action<string>? log, CancellationToken ct)
    {
        try
        {
            Directory.CreateDirectory(outDir);
            async Task<string?> CompileAsync(string stem, string asm)
            {
                var src = Path.Combine(outDir, stem + ".S");
                var obj = Path.Combine(outDir, stem + ".o");
                await File.WriteAllTextAsync(src, asm, ct);
                var r = await ProcessRunner.RunStreamedAsync(
                    zig, new[] { "cc", "-target", "x86-windows-gnu", "-march=pentium3", "-c", src, "-o", obj }, log, ct: ct);
                return r.Success ? obj : null;
            }
            var begin = await CompileAsync("rxdk_eh_begin", EhBeginAsm);
            var end = await CompileAsync("rxdk_eh_end", EhEndAsm);
            if (begin is null || end is null)
            {
                log?.Invoke("Warning: could not build .eh_frame brackets; C++ exception unwinding may fail to link.");
                return (null, null);
            }
            return (begin, end);
        }
        catch (Exception ex)
        {
            log?.Invoke($"Warning: .eh_frame bracket setup failed ({ex.Message}); continuing without it.");
            return (null, null);
        }
    }
}
