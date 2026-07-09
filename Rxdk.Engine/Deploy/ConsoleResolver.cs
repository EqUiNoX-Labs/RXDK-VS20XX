using System.Text.RegularExpressions;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Deploy;

/// <summary>
/// Resolves the target Xbox devkit address from the Windows Xbox SDK registry
/// (XBSetIP / Xbox Neighborhood). C# port of the Windows path of RXDK-VSCode
/// xboxConsole.ts — the macOS/Linux consoles.json store is dropped (VS is Windows-only).
/// Reads via reg.exe (the same fallback the TS uses) so there's no extra dependency.
/// </summary>
public static class ConsoleResolver
{
    private const string RegKey = @"Software\Microsoft\XboxSDK";
    private const string RegValue = "XboxName";

    /// <summary>The active devkit address for display, or null if none is configured.</summary>
    public static Task<string?> GetActiveXboxAddressAsync(CancellationToken ct = default) =>
        ReadRegistryXboxNameAsync(ct);

    /// <summary>
    /// The value to pass as the tools' -x switch. An explicit override always wins;
    /// otherwise the registry value (its source of truth on Windows).
    /// </summary>
    public static async Task<string?> ResolveConsoleSwitchAsync(string? explicitName = null, CancellationToken ct = default)
    {
        var overrideName = explicitName?.Trim();
        if (!string.IsNullOrEmpty(overrideName))
            return overrideName;
        return await ReadRegistryXboxNameAsync(ct);
    }

    /// <summary>Persist the devkit address to the registry (HKCU XboxName).</summary>
    public static async Task SetActiveXboxAddressAsync(string address, CancellationToken ct = default)
    {
        var trimmed = address?.Trim() ?? "";
        if (string.IsNullOrEmpty(trimmed))
            throw new ArgumentException("Xbox address cannot be empty.");
        var r = await ProcessRunner.RunAsync("reg.exe",
            new[] { "add", $"HKCU\\{RegKey}", "/v", RegValue, "/t", "REG_SZ", "/d", trimmed, "/f" }, ct: ct);
        if (!r.Success)
            throw new InvalidOperationException($"Failed to write {RegValue} to registry: {r.StdErr.Trim()}");
    }

    // ---- registry reads (port of readRegistryXboxName) ----

    private static async Task<string?> ReadRegistryXboxNameAsync(CancellationToken ct)
    {
        // 1. XboxName that resolves to a known address (IP literal or an xbshlext name).
        foreach (var hive in new[] { "HKCU", "HKLM" })
        {
            var xboxName = (await ReadRegValueAsync(hive, RegKey, RegValue, ct))?.Trim();
            if (!string.IsNullOrEmpty(xboxName) && await IsResolvableTargetAsync(xboxName, ct))
                return ResolveTarget(xboxName, await ReadShellExtAddressesAsync(hive, ct));
        }

        // 2. Xbox Neighborhood fallback (first console with a valid address).
        var fromNeighborhood = await ReadNeighborhoodAddressAsync(ct);
        if (fromNeighborhood is not null)
            return fromNeighborhood;

        // 3. Any XboxName at all (even if unresolvable), best-effort.
        foreach (var hive in new[] { "HKCU", "HKLM" })
        {
            var xboxName = (await ReadRegValueAsync(hive, RegKey, RegValue, ct))?.Trim();
            if (!string.IsNullOrEmpty(xboxName))
                return ResolveTarget(xboxName, await ReadShellExtAddressesAsync(hive, ct));
        }
        return null;
    }

    private static Task<Dictionary<string, string>> ReadShellExtAddressesAsync(string hive, CancellationToken ct) =>
        ReadRegKeyValuesAsync($@"{hive}\Software\Microsoft\XboxSDK\xbshlext\Addresses", ct);

    private static string ResolveTarget(string nameOrAddress, IReadOnlyDictionary<string, string> addresses)
    {
        if (IsIpv4(nameOrAddress)) return nameOrAddress;
        return addresses.TryGetValue(nameOrAddress, out var addr) ? addr : nameOrAddress;
    }

    private static async Task<bool> IsResolvableTargetAsync(string nameOrAddress, CancellationToken ct)
    {
        if (IsIpv4(nameOrAddress)) return true;
        foreach (var hive in new[] { "HKCU", "HKLM" })
            if ((await ReadShellExtAddressesAsync(hive, ct)).ContainsKey(nameOrAddress))
                return true;
        return false;
    }

    private static async Task<string?> ReadNeighborhoodAddressAsync(CancellationToken ct)
    {
        foreach (var hive in new[] { "HKCU", "HKLM" })
        {
            var addresses = await ReadShellExtAddressesAsync(hive, ct);
            var consoles = await ReadRegKeyValuesAsync($@"{hive}\Software\Microsoft\XboxSDK\xbshlext\Consoles", ct);
            foreach (var name in consoles.Keys)
            {
                if (name == "(default)") continue;
                if (addresses.TryGetValue(name, out var address) && IsValidAddress(address.Trim()))
                    return address.Trim();
            }

            var rxdk = await ReadRegKeyValuesAsync($@"{hive}\Software\Microsoft\XboxSDK\RXDKNeighborhood\Consoles", ct);
            foreach (var key in rxdk.Keys)
                if (IsValidAddress(key))
                    return key;
        }
        return null;
    }

    // ---- reg.exe helpers ----

    private static async Task<string?> ReadRegValueAsync(string hive, string subkey, string valueName, CancellationToken ct)
    {
        var r = await ProcessRunner.RunAsync("reg.exe", new[] { "query", $"{hive}\\{subkey}", "/v", valueName }, ct: ct);
        if (!r.Success) return null;
        var m = Regex.Match(r.StdOut, $@"{Regex.Escape(valueName)}\s+REG_SZ\s+(.+)", RegexOptions.IgnoreCase);
        var val = m.Success ? m.Groups[1].Value.Trim() : null;
        return string.IsNullOrEmpty(val) ? null : val;
    }

    private static async Task<Dictionary<string, string>> ReadRegKeyValuesAsync(string keyPath, CancellationToken ct)
    {
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var r = await ProcessRunner.RunAsync("reg.exe", new[] { "query", keyPath }, ct: ct);
        if (!r.Success) return values;
        foreach (var line in r.StdOut.Split('\n'))
        {
            var m = Regex.Match(line.Trim(), @"^(\S+)\s+REG_SZ\s+(.+)$", RegexOptions.IgnoreCase);
            if (m.Success) values[m.Groups[1].Value] = m.Groups[2].Value.Trim();
        }
        return values;
    }

    private static bool IsIpv4(string address) =>
        Regex.IsMatch(address, @"^(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d)$");

    /// <summary>Light validation: a non-empty IPv4 literal or hostname.</summary>
    private static bool IsValidAddress(string address) =>
        !string.IsNullOrWhiteSpace(address) &&
        (IsIpv4(address) || Regex.IsMatch(address, @"^[A-Za-z0-9][A-Za-z0-9._-]*$"));
}
