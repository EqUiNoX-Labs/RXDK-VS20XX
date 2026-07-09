using Rxdk.Engine.Bootstrap;
using Rxdk.Engine.Model;
using Rxdk.Engine.Platform;

// Thin CLI over Rxdk.Engine — the pure-.NET replacement for RXDK-VSCode's cli.ts.
// Grows subcommands (build/deploy/run/reboot) as the engine is ported. For now it
// carries `info`, which parses an rxdk.project.json and prints the resolved model —
// a smoke test that the manifest port matches the on-disk contract.

if (args.Length == 0)
{
    Console.Error.WriteLine("usage: rxdk <command> [options]");
    Console.Error.WriteLine("commands:");
    Console.Error.WriteLine("  info --project-root <dir>   Parse rxdk.project.json and print the resolved model");
    Console.Error.WriteLine("  install-tools [--tools-tag <t>] [--xdvdfs-tag <t>]   Download host tools into the staged root");
    Console.Error.WriteLine("  tools-status                Report whether host tools are installed");
    Console.Error.WriteLine("  install-sdk                 Clone/update RXDK-SDK (headers + libs)");
    Console.Error.WriteLine("  sdk-status                  Report staged SDK presence");
    Console.Error.WriteLine("  install-zig                 Download the pinned Zig toolchain");
    Console.Error.WriteLine("  zig-status                  Report the resolved Zig toolchain");
    return 2;
}

var command = args[0];
var opts = ParseArgs(args.Skip(1));

switch (command)
{
    case "info":
        return CmdInfo(opts);
    case "install-tools":
        return await CmdInstallTools(opts);
    case "tools-status":
        return CmdToolsStatus();
    case "install-sdk":
        return await CmdInstallSdk();
    case "sdk-status":
        return CmdSdkStatus();
    case "install-zig":
        return await CmdInstallZig();
    case "zig-status":
        return await CmdZigStatus();
    default:
        Console.Error.WriteLine($"unknown command: {command}");
        return 2;
}

static async Task<int> CmdInstallTools(Dictionary<string, string> opts)
{
    opts.TryGetValue("tools-tag", out var toolsTag);
    opts.TryGetValue("xdvdfs-tag", out var xdvdfsTag);
    try
    {
        var root = await HostToolsInstaller.InstallAsync(
            hostToolsTag: string.IsNullOrEmpty(toolsTag) ? null : toolsTag,
            xdvdfsTag: string.IsNullOrEmpty(xdvdfsTag) ? null : xdvdfsTag,
            log: msg => Console.WriteLine(msg));
        Console.WriteLine($"Host tools installed at: {root}");
        return 0;
    }
    catch (Exception ex)
    {
        Console.Error.WriteLine($"install-tools failed: {ex.Message}");
        return 1;
    }
}

static int CmdToolsStatus()
{
    var root = RxdkPaths.GetStagedToolsRoot();
    Console.WriteLine($"staged tools root: {root}");
    var installed = HostToolsInstaller.IsInstalled();
    foreach (var tool in HostToolsInstaller.RequiredHostTools)
    {
        var path = System.IO.Path.Combine(root, RxdkPaths.HostToolExecutableName(tool));
        Console.WriteLine($"  [{(System.IO.File.Exists(path) ? "x" : " ")}] {tool}");
    }
    Console.WriteLine($"installed: {installed}");
    return installed ? 0 : 1;
}

static async Task<int> CmdInstallSdk()
{
    try
    {
        var root = await SdkStaging.EnsureAsync(log: msg => Console.WriteLine(msg));
        Console.WriteLine($"SDK staged at: {root}");
        return 0;
    }
    catch (Exception ex)
    {
        Console.Error.WriteLine($"install-sdk failed: {ex.Message}");
        return 1;
    }
}

static int CmdSdkStatus()
{
    Console.WriteLine($"staged SDK root: {RxdkPaths.GetStagedSdkRoot()}");
    var headers = SdkStaging.IsStagedSdkPresent();
    var libs = SdkStaging.IsStagedSdkLibPresent();
    Console.WriteLine($"  headers (include/d3d8.h): {headers}");
    Console.WriteLine($"  libs (linkable marker):   {libs}");
    return headers && libs ? 0 : 1;
}

static async Task<int> CmdInstallZig()
{
    try
    {
        var zig = await ZigRuntime.InstallAsync(log: msg => Console.WriteLine(msg));
        Console.WriteLine($"Zig ready: {zig}");
        return 0;
    }
    catch (Exception ex)
    {
        Console.Error.WriteLine($"install-zig failed: {ex.Message}");
        return 1;
    }
}

static async Task<int> CmdZigStatus()
{
    var zig = await ZigRuntime.ResolveZigExecutableAsync();
    if (zig is null)
    {
        Console.WriteLine("zig: not found (run install-zig)");
        return 1;
    }
    var version = await ZigRuntime.GetVersionLineAsync();
    Console.WriteLine($"zig: {zig}");
    Console.WriteLine($"version: {version} (pinned {ZigRuntime.ZigVersion})");
    return 0;
}

static int CmdInfo(Dictionary<string, string> opts)
{
    if (!opts.TryGetValue("project-root", out var root) || string.IsNullOrEmpty(root))
    {
        Console.Error.WriteLine("missing required --project-root");
        return 2;
    }

    var manifest = RxdkManifestLoader.TryLoad(root);
    if (manifest is null)
    {
        Console.Error.WriteLine($"no valid {RxdkManifestLoader.ManifestFileName} under {root}");
        return 1;
    }

    Console.WriteLine($"name:           {manifest.Name}");
    Console.WriteLine($"type:           {manifest.EffectiveType}");
    Console.WriteLine($"configuration:  {manifest.EffectiveConfiguration}");
    Console.WriteLine($"sources:        {manifest.Sources?.Count ?? 0}");
    Console.WriteLine($"libraries:      {string.Join(", ", manifest.Libraries ?? new())}");
    Console.WriteLine($"projectRefs:    {string.Join(", ", manifest.ProjectReferences ?? new())}");
    Console.WriteLine($"usesCpp:        {manifest.UsesCpp}");
    Console.WriteLine($"needsIntelliSense: {manifest.NeedsIntelliSense}");
    Console.WriteLine($"isPrebuilt:     {manifest.IsPrebuilt}");
    Console.WriteLine($"isLibrary:      {manifest.IsLibrary}");
    Console.WriteLine($"isDxt:          {manifest.IsDxt}");
    if (manifest.IsPrebuilt)
    {
        Console.WriteLine($"prebuilt.xbe:   {manifest.Prebuilt!.Xbe}");
        Console.WriteLine($"prebuilt.remote:{manifest.Prebuilt!.RemoteName}");
    }
    return 0;
}

static Dictionary<string, string> ParseArgs(IEnumerable<string> argv)
{
    var result = new Dictionary<string, string>();
    var list = argv.ToList();
    for (var i = 0; i < list.Count; i++)
    {
        if (!list[i].StartsWith("--")) continue;
        var key = list[i][2..];
        if (i + 1 < list.Count && !list[i + 1].StartsWith("--"))
        {
            result[key] = list[i + 1];
            i++;
        }
        else
        {
            result[key] = "true";
        }
    }
    return result;
}
