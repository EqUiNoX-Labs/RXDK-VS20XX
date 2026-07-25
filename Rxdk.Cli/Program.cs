using Rxdk.Engine.Bootstrap;
using Rxdk.Engine.Build;
using Rxdk.Engine.Deploy;
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
    Console.Error.WriteLine("  build --project-root <dir> [--optimize <mode>] [--compile-only]   Compile+link to .xbe");
    Console.Error.WriteLine("  deploy --project-root <dir> [--console <ip>]     Copy build output to the devkit");
    Console.Error.WriteLine("  run --project-root <dir> [--console <ip>] [--reboot]   Launch the deployed title");
    Console.Error.WriteLine("  reboot [--console <ip>]     Warm-reboot the devkit");
    Console.Error.WriteLine("  set-ip --address <ip>       Set the devkit IP/hostname (registry)");
    Console.Error.WriteLine("  xbox-ip                     Print the resolved devkit address");
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
    case "build":
        return await CmdBuild(opts);
    case "deploy":
        return await CmdDeploy(opts);
    case "run":
        return await CmdRun(opts);
    case "reboot":
        return await CmdReboot(opts);
    case "set-ip":
        return await CmdSetIp(opts);
    case "xbox-ip":
        return await CmdXboxIp();
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

static async Task<int> CmdBuild(Dictionary<string, string> opts)
{
    if (!opts.TryGetValue("project-root", out var root) || string.IsNullOrEmpty(root))
    {
        Console.Error.WriteLine("missing required --project-root");
        return 2;
    }
    var optimize = RxdkOptimizeMode.Debug;
    if (opts.TryGetValue("optimize", out var opt) && !string.IsNullOrEmpty(opt)
        && !OptimizeMode.TryParse(opt, out optimize))
    {
        Console.Error.WriteLine($"invalid --optimize '{opt}' (Debug|ReleaseSafe|ReleaseFast|ReleaseSmall)");
        return 2;
    }

    opts.TryGetValue("manifest", out var manifestPath);
    var result = await XboxBuild.BuildAsync(new BuildOptions
    {
        ProjectRoot = root,
        Optimize = optimize,
        CompileOnly = opts.ContainsKey("compile-only"),
        ManifestPath = string.IsNullOrEmpty(manifestPath) ? null : manifestPath,
        Log = msg => Console.WriteLine(msg),
    });
    if (!result.Ok)
    {
        Console.Error.WriteLine($"build failed: {result.Error}");
        return 1;
    }
    Console.WriteLine($"build OK -> {result.OutDir}");
    return 0;
}

static async Task<int> CmdDeploy(Dictionary<string, string> opts)
{
    if (!opts.TryGetValue("project-root", out var root) || string.IsNullOrEmpty(root))
    {
        Console.Error.WriteLine("missing required --project-root");
        return 2;
    }
    opts.TryGetValue("console", out var console);
    opts.TryGetValue("manifest", out var deployManifest);
    var result = await XboxDeploy.DeployProjectAsync(new XboxDeploy.DeployOptions
    {
        ProjectRoot = root,
        ConsoleName = string.IsNullOrEmpty(console) ? null : console,
        ManifestPath = string.IsNullOrEmpty(deployManifest) ? null : deployManifest,
        Log = msg => Console.WriteLine(msg),
    });
    if (!result.Ok)
    {
        Console.Error.WriteLine($"deploy failed: {result.Error}");
        return 1;
    }
    return 0;
}

static async Task<int> CmdRun(Dictionary<string, string> opts)
{
    if (!opts.TryGetValue("project-root", out var root) || string.IsNullOrEmpty(root))
    {
        Console.Error.WriteLine("missing required --project-root");
        return 2;
    }
    opts.TryGetValue("manifest", out var runManifest);
    RxdkProjectManifest? manifest;
    try { manifest = RxdkManifestLoader.Resolve(root, string.IsNullOrEmpty(runManifest) ? null : runManifest); }
    catch { manifest = null; }
    if (manifest is null)
    {
        Console.Error.WriteLine($"no valid manifest for {root}");
        return 1;
    }
    opts.TryGetValue("console", out var console);
    var result = await XboxLaunch.LaunchProjectAsync(new XboxLaunch.LaunchOptions
    {
        ProjectName = manifest.Name,
        ConsoleName = string.IsNullOrEmpty(console) ? null : console,
        Reboot = opts.ContainsKey("reboot"),
        Log = msg => Console.WriteLine(msg),
    });
    if (result.NoConsoleConfigured)
    {
        Console.Error.WriteLine("no Xbox console configured (set-ip, or Xbox Neighborhood)");
        return 2;
    }
    if (!result.Ok)
    {
        Console.Error.WriteLine($"run failed: {result.Error}");
        return 1;
    }
    return 0;
}

static async Task<int> CmdReboot(Dictionary<string, string> opts)
{
    opts.TryGetValue("console", out var console);
    var result = await XboxLaunch.RebootConsoleAsync(
        string.IsNullOrEmpty(console) ? null : console, msg => Console.WriteLine(msg));
    if (result.NoConsoleConfigured) { Console.Error.WriteLine("no Xbox console configured"); return 2; }
    if (!result.Ok) { Console.Error.WriteLine($"reboot failed: {result.Error}"); return 1; }
    return 0;
}

static async Task<int> CmdSetIp(Dictionary<string, string> opts)
{
    if (!opts.TryGetValue("address", out var addr) || string.IsNullOrEmpty(addr))
    {
        Console.Error.WriteLine("missing required --address");
        return 2;
    }
    try
    {
        await ConsoleResolver.SetActiveXboxAddressAsync(addr);
        Console.WriteLine($"Xbox address set to {addr}");
        return 0;
    }
    catch (Exception ex)
    {
        Console.Error.WriteLine($"set-ip failed: {ex.Message}");
        return 1;
    }
}

static async Task<int> CmdXboxIp()
{
    var addr = await ConsoleResolver.GetActiveXboxAddressAsync();
    Console.WriteLine(addr is null ? "no Xbox console configured" : addr);
    return addr is null ? 1 : 0;
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
