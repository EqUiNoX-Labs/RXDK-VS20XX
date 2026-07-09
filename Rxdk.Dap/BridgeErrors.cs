namespace Rxdk.Dap;

/// <summary>
/// Maps a bridge error code to a human-actionable hint. C# port of bridgeClient.ts
/// formatBridgeError.
/// </summary>
public static class BridgeErrors
{
    private static readonly Dictionary<string, string> Hints = new()
    {
        ["launchTimeout"] = "Timed out waiting for the title to stop at entry. The kit may be offline, the title may have crashed, or reboot took too long.",
        ["titleRebooted"] = "The title loaded but exited or rebooted to the dashboard before the debugger could stop at entry. Check DM_EXCEPTION/DM_DEBUGSTR in the bridge log.",
        ["pendingExec"] = "Could not reboot the devkit into pending-exec state (required for initial breakpoint).",
        ["initialBreakpoint"] = "DmSetInitialBreakpoint failed. The console must be in pending-exec before launch.",
        ["connectDebugger"] = "DmConnectDebugger failed. The title may not be debuggable (wrong module stopped, or crash).",
        ["memberNotFound"] = "Struct member not found in PDB. Expand the struct under Locals, or use a simple name like d3pp.SwapEffect.",
        ["symbolNotFound"] = "Symbol not found. Use exact PDB names (e.g. g_pD3D, d3pp.SwapEffect).",
        ["readFailed"] = "Could not read Xbox memory at the resolved address.",
        ["installBreakpoint"] = "Line resolved but the devkit rejected the breakpoint (address not executable).",
        ["badAddress"] = "Breakpoint address is outside the loaded title image. Wait for launch to finish or set the BP again after the module loads.",
        ["hwBpFull"] = "Rare hardware-breakpoint fallback failed (Xbox allows 4 HW execute slots). Soft INT3 breakpoints should be used normally.",
        ["resolveLine"] = "No PDB code mapping for that source line. Use a line with a statement.",
        ["stepTimeout"] = "Single-step did not complete in time. The bridge issued STOP to resync — press Step Over again.",
        ["go"] = "Target is already running (prior step may have timed out). Use Stop, then Continue or Step again.",
        ["stillStopped"] = "Thread still stopped on the devkit. For launch: check bridge logs for stop reason. For Continue: try Stop, then Continue again.",
        ["continueThread"] = "Could not CONTINUE the stopped thread on the devkit.",
    };

    public static string Format(string line, BridgeMessage msg)
    {
        var err = msg.GetString("error") ?? "";
        return Hints.TryGetValue(err, out var hint)
            ? $"{hint} ({err})"
            : $"bridge command failed: {line}";
    }
}
