using Rxdk.Engine.Build;
using Rxdk.Engine.Model;
using Rxdk.Engine.Platform;

namespace Rxdk.Engine.Deploy;

public sealed record DeployResult(bool Ok, IReadOnlyList<string> Deployed, string? Error = null)
{
    public static DeployResult Fail(string error) => new(false, Array.Empty<string>(), error);
}

/// <summary>
/// Copies build output to the devkit via xbcp, and removes DXTs via xbdel. C# port of
/// RXDK-VSCode xboxDeploy.ts. Uses '-' switches (not '/') so the tools don't misparse args.
/// </summary>
public static class XboxDeploy
{
    public sealed class DeployOptions
    {
        public required string ProjectRoot { get; init; }
        public string? ProjectName { get; init; }
        public string? LocalDir { get; init; }
        public string? RemoteDir { get; init; }
        public string? ConsoleName { get; init; }
        /// <summary>Filename patterns for the project's own output. Default: *.xbe, *.pdb, *.map.</summary>
        public IReadOnlyList<string>? Files { get; init; }
        public bool Quiet { get; init; }
        public Action<string>? Log { get; init; }
    }

    public static async Task<DeployResult> DeployProjectAsync(DeployOptions opts, CancellationToken ct = default)
    {
        try
        {
            var projectRoot = Path.GetFullPath(opts.ProjectRoot);
            var manifest = RxdkManifestLoader.Load(projectRoot);
            var projectName = opts.ProjectName ?? manifest.Name;
            var localDir = Path.GetFullPath(opts.LocalDir ?? SdkLayout.GetProjectOutDir(projectRoot, manifest));
            if (!Directory.Exists(localDir))
                return DeployResult.Fail($"Deploy source directory not found: {localDir}");

            // A DXT deploys to xe:\dxt (xbdm scans E:\dxt\*.DXT non-recursively), not xe:\<name>.
            var isDxt = manifest.Type == RxdkProjectKind.Dxt;
            var remoteDir = isDxt ? @"xe:\dxt" : NormalizeRemoteDir(opts.RemoteDir ?? "", projectName);
            var xbcp = RxdkPaths.ResolveHostTool("xbcp");
            var displayAddr = string.IsNullOrWhiteSpace(opts.ConsoleName)
                ? await ConsoleResolver.GetActiveXboxAddressAsync(ct)
                : opts.ConsoleName.Trim();
            var consoleSwitch = await ConsoleResolver.ResolveConsoleSwitchAsync(opts.ConsoleName, ct);
            opts.Log?.Invoke(displayAddr is not null
                ? $"Deploying to Xbox '{displayAddr}' -> {remoteDir}"
                : $"Deploying to default Xbox -> {remoteDir}");

            var defaultPatterns = isDxt ? new[] { "*.dxt" } : new[] { "*.xbe", "*.pdb", "*.map" };
            var patterns = opts.Files is { Count: > 0 } ? opts.Files : defaultPatterns;
            var sent = new List<string>();
            foreach (var pattern in patterns)
            {
                foreach (var name in ListFilesMatching(localDir, pattern))
                {
                    var dest = $@"{remoteDir}\{name}";
                    if (!opts.Quiet) opts.Log?.Invoke($"{name} -> {dest}");
                    await XbcpCopyAsync(xbcp, Path.Combine(localDir, name), dest, consoleSwitch, opts.Log, ct);
                    sent.Add(name);
                }
            }
            if (sent.Count == 0)
                return DeployResult.Fail($"No files matched in {localDir} (patterns: {string.Join(", ", patterns)})");

            // deployPaths: project-relative files/dirs copied next to the output on the console.
            // Copied per-file with an explicit destination (xbcp's own recursive copy misbehaves
            // on a plain local folder source — see xboxDeploy.ts).
            var deployFiles = PackXiso.ResolveDeployPaths(projectRoot, manifest.DeployPaths, opts.Log);
            foreach (var entry in deployFiles)
            {
                var dest = $@"{remoteDir}\{entry.RelativeDest.Replace('/', '\\')}";
                if (!opts.Quiet) opts.Log?.Invoke($"{entry.Source} -> {dest}");
                await XbcpCopyAsync(xbcp, entry.Source, dest, consoleSwitch, opts.Log, ct);
            }

            var summary = $"Deployed: {string.Join(", ", sent)} -> {remoteDir}";
            if (deployFiles.Count > 0) summary += $"; deployPaths: {deployFiles.Count} file(s)";
            opts.Log?.Invoke(summary);
            return new DeployResult(true, sent);
        }
        catch (Exception err)
        {
            return DeployResult.Fail(err.Message);
        }
    }

    public sealed class DeployPrebuiltOptions
    {
        public required string XbePath { get; init; }
        public string? PdbPath { get; init; }
        public string? MapPath { get; init; }
        public string? RemoteName { get; init; }
        public string? ConsoleName { get; init; }
        public bool Quiet { get; init; }
        public Action<string>? Log { get; init; }
    }

    /// <summary>Manifest-less deploy of an explicit prebuilt XBE (+ optional PDB/MAP).</summary>
    public static async Task<DeployResult> DeployPrebuiltAsync(DeployPrebuiltOptions opts, CancellationToken ct = default)
    {
        try
        {
            var xbePath = Path.GetFullPath(opts.XbePath);
            if (!File.Exists(xbePath))
                return DeployResult.Fail($"XBE not found: {xbePath}");
            var remoteName = opts.RemoteName ?? Path.GetFileNameWithoutExtension(xbePath);
            var remoteDir = $@"xe:\{remoteName}".TrimEnd('\\');

            var xbcp = RxdkPaths.ResolveHostTool("xbcp");
            var displayAddr = string.IsNullOrWhiteSpace(opts.ConsoleName)
                ? await ConsoleResolver.GetActiveXboxAddressAsync(ct)
                : opts.ConsoleName.Trim();
            var consoleSwitch = await ConsoleResolver.ResolveConsoleSwitchAsync(opts.ConsoleName, ct);
            opts.Log?.Invoke(displayAddr is not null
                ? $"Deploying to Xbox '{displayAddr}' -> {remoteDir}"
                : $"Deploying to default Xbox -> {remoteDir}");

            var toCopy = new List<string> { xbePath };
            if (!string.IsNullOrEmpty(opts.PdbPath)) toCopy.Add(Path.GetFullPath(opts.PdbPath));
            if (!string.IsNullOrEmpty(opts.MapPath)) toCopy.Add(Path.GetFullPath(opts.MapPath));

            var sent = new List<string>();
            foreach (var file in toCopy)
            {
                if (!File.Exists(file)) { opts.Log?.Invoke($"Warning: skip missing file: {file}"); continue; }
                var name = Path.GetFileName(file);
                var dest = $@"{remoteDir}\{name}";
                if (!opts.Quiet) opts.Log?.Invoke($"{name} -> {dest}");
                await XbcpCopyAsync(xbcp, file, dest, consoleSwitch, opts.Log, ct);
                sent.Add(name);
            }
            if (sent.Count == 0)
                return DeployResult.Fail($"No files deployed for {xbePath}");
            opts.Log?.Invoke($"Deployed: {string.Join(", ", sent)} -> {remoteDir}");
            return new DeployResult(true, sent);
        }
        catch (Exception err)
        {
            return DeployResult.Fail(err.Message);
        }
    }

    /// <summary>Delete a DXT from the console's E:\dxt via xbdel (pair with a warm reboot).</summary>
    public static async Task<DeployResult> RemoveDxtAsync(
        string projectRoot, string? projectName = null, string? consoleName = null,
        Action<string>? log = null, CancellationToken ct = default)
    {
        try
        {
            var manifest = RxdkManifestLoader.Load(Path.GetFullPath(projectRoot));
            var name = projectName ?? manifest.Name;
            var xbdel = RxdkPaths.ResolveHostTool("xbdel");
            if (!File.Exists(xbdel))
                return DeployResult.Fail($"xbdel not found at {xbdel}. Update the RXDK host tools.");
            var displayAddr = string.IsNullOrWhiteSpace(consoleName)
                ? await ConsoleResolver.GetActiveXboxAddressAsync(ct)
                : consoleName.Trim();
            var consoleSwitch = await ConsoleResolver.ResolveConsoleSwitchAsync(consoleName, ct);
            var remote = $@"xe:\dxt\{name}.dxt";
            log?.Invoke(displayAddr is not null
                ? $"Removing {remote} from '{displayAddr}'"
                : $"Removing {remote} from default Xbox");

            var args = new List<string> { "-f", remote };
            if (consoleSwitch is not null) { args.Add("-x"); args.Add(consoleSwitch); }
            var r = await ProcessRunner.RunStreamedAsync(xbdel, args, log, ct: ct);
            if (!r.Success)
                return DeployResult.Fail($"xbdel failed (exit {r.ExitCode}) — was {remote} present?");
            return new DeployResult(true, new[] { $"{name}.dxt (removed)" });
        }
        catch (Exception err)
        {
            return DeployResult.Fail(err.Message);
        }
    }

    // ---- helpers ----

    private static async Task XbcpCopyAsync(
        string xbcp, string localFile, string remoteDest, string? console, Action<string>? log, CancellationToken ct)
    {
        var args = new List<string> { "-y", "-t", "-q" };
        if (console is not null) { args.Add("-x"); args.Add(console); }
        args.Add(localFile);
        args.Add(remoteDest);
        var r = await ProcessRunner.RunStreamedAsync(xbcp, args, log, ct: ct);
        if (!r.Success)
            throw new InvalidOperationException($"xbcp failed copying {Path.GetFileName(localFile)} (exit {r.ExitCode})");
    }

    private static string NormalizeRemoteDir(string remoteDir, string defaultName)
    {
        var dir = string.IsNullOrEmpty(remoteDir) ? $@"xe:\{defaultName}" : remoteDir;
        if (System.Text.RegularExpressions.Regex.IsMatch(dir, @"^x[edc]:\\", System.Text.RegularExpressions.RegexOptions.IgnoreCase))
            return dir.TrimEnd('\\');
        return $@"xe:\{dir}".TrimEnd('\\');
    }

    /// <summary>Only `*.ext`-shaped patterns are used by callers — not a full glob engine.</summary>
    private static IEnumerable<string> ListFilesMatching(string dir, string pattern)
    {
        if (!Directory.Exists(dir)) yield break;
        foreach (var file in Directory.EnumerateFiles(dir))
        {
            var name = Path.GetFileName(file);
            var matches = pattern.StartsWith("*.", StringComparison.Ordinal)
                ? name.EndsWith(pattern[1..], StringComparison.OrdinalIgnoreCase)
                : string.Equals(name, pattern, StringComparison.OrdinalIgnoreCase);
            if (matches) yield return name;
        }
    }
}
