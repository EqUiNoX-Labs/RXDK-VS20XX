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
}
