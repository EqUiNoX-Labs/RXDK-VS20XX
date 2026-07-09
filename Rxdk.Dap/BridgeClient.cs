using System.Diagnostics;
using System.Text;
using System.Text.Json;
using Rxdk.Engine.Platform;

namespace Rxdk.Dap;

/// <summary>A line of JSON from the bridge: either an async "event" or a "result" for a request id.</summary>
public sealed class BridgeMessage
{
    public string? Type { get; init; }
    public string? Event { get; init; }
    public int? Id { get; init; }
    public bool Success { get; init; }
    /// <summary>All fields of the raw JSON object, for ad-hoc access (threadId, address, variables, …).</summary>
    public JsonElement Raw { get; init; }

    public string? GetString(string name) =>
        Raw.ValueKind == JsonValueKind.Object && Raw.TryGetProperty(name, out var v)
            ? v.ValueKind == JsonValueKind.String ? v.GetString() : v.ToString()
            : null;

    public bool GetBool(string name) =>
        Raw.ValueKind == JsonValueKind.Object && Raw.TryGetProperty(name, out var v)
        && v.ValueKind == JsonValueKind.True;

    public double GetNumber(string name)
    {
        if (Raw.ValueKind == JsonValueKind.Object && Raw.TryGetProperty(name, out var v))
        {
            if (v.ValueKind == JsonValueKind.Number) return v.GetDouble();
            if (v.ValueKind == JsonValueKind.String && double.TryParse(v.GetString(), out var d)) return d;
        }
        return 0;
    }

    public bool TryGet(string name, out JsonElement value)
    {
        if (Raw.ValueKind == JsonValueKind.Object && Raw.TryGetProperty(name, out value))
            return true;
        value = default;
        return false;
    }
}

/// <summary>
/// Drives the xboxdbg-bridge host process over line-delimited JSON. C# port of the debug
/// adapter's bridgeClient.ts: newline-framed request/response with an id→pending map, plus
/// async 'event'/'log'/'exit' notifications. The bridge is a framework-dependent .NET app,
/// so the managed-runtime env (DotnetEnv) is injected, and its own dir + parent are on PATH.
/// </summary>
public sealed class BridgeClient : IDisposable
{
    private readonly string _bridgePath;
    private Process? _proc;
    private StreamWriter? _stdin;
    private int _nextId = 1;
    private readonly object _gate = new();
    private readonly Dictionary<int, TaskCompletionSource<BridgeMessage>> _pending = new();

    public event Action<BridgeMessage>? BridgeEvent;
    public event Action<string>? Log;
    public event Action<int?>? Exited;

    public BridgeClient(string bridgePath) => _bridgePath = bridgePath;

    public void Start()
    {
        if (_proc is not null) return;

        var bridgeDir = Path.GetDirectoryName(_bridgePath)!;
        var sdkToolsDir = Path.GetFullPath(Path.Combine(bridgeDir, ".."));
        var pathEnv = $"{sdkToolsDir};{bridgeDir};{Environment.GetEnvironmentVariable("PATH")}";

        var psi = new ProcessStartInfo
        {
            FileName = _bridgePath,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = bridgeDir,
            StandardOutputEncoding = Encoding.UTF8,
        };
        var managed = DotnetEnv.WithManagedDotnet();
        if (managed is not null)
            foreach (var kv in managed) psi.Environment[kv.Key] = kv.Value;
        psi.Environment["PATH"] = pathEnv;

        _proc = new Process { StartInfo = psi, EnableRaisingEvents = true };
        _proc.OutputDataReceived += (_, e) => { if (e.Data is not null) OnLine(e.Data); };
        _proc.ErrorDataReceived += (_, e) => { if (e.Data is not null) Log?.Invoke(e.Data); };
        _proc.Exited += (_, _) =>
        {
            Exited?.Invoke(_proc?.ExitCode);
            FailAllPending(new IOException("bridge exited"));
        };
        _proc.Start();
        _stdin = _proc.StandardInput;
        _proc.BeginOutputReadLine();
        _proc.BeginErrorReadLine();
    }

    /// <summary>Send a command and await its result. Throws on bridge-reported failure or timeout.</summary>
    public async Task<BridgeMessage> RequestAsync(
        string cmd, IReadOnlyDictionary<string, object?>? args = null, int timeoutMs = 180000,
        CancellationToken ct = default)
    {
        if (_stdin is null) throw new InvalidOperationException("bridge not started");

        int id;
        var tcs = new TaskCompletionSource<BridgeMessage>(TaskCreationOptions.RunContinuationsAsynchronously);
        lock (_gate)
        {
            id = _nextId++;
            _pending[id] = tcs;
        }

        var payload = new Dictionary<string, object?> { ["cmd"] = cmd, ["id"] = id };
        if (args is not null)
            foreach (var kv in args) payload[kv.Key] = kv.Value;
        var line = JsonSerializer.Serialize(payload);

        lock (_gate) { _stdin.WriteLine(line); _stdin.Flush(); }

        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        timeoutCts.CancelAfter(timeoutMs);
        using (timeoutCts.Token.Register(() =>
        {
            lock (_gate) { if (_pending.Remove(id)) tcs.TrySetException(new TimeoutException($"bridge command timed out: {cmd}")); }
        }))
        {
            return await tcs.Task;
        }
    }

    public async Task ShutdownAsync(bool rebootDashboard = true)
    {
        if (_proc is null) return;
        try
        {
            await RequestAsync("shutdown", new Dictionary<string, object?> { ["rebootDashboard"] = rebootDashboard }, 30000);
        }
        catch (Exception e) { Log?.Invoke($"shutdown: {e.Message}\n"); }
        await Task.Delay(500);
        try { _proc?.Kill(entireProcessTree: true); } catch { /* ignore */ }
        _proc = null;
    }

    private void OnLine(string line)
    {
        BridgeMessage msg;
        try
        {
            using var doc = JsonDocument.Parse(line);
            var root = doc.RootElement.Clone();
            msg = new BridgeMessage
            {
                Type = root.TryGetProperty("type", out var t) ? t.GetString() : null,
                Event = root.TryGetProperty("event", out var e) ? e.GetString() : null,
                Id = root.TryGetProperty("id", out var i) && i.ValueKind == JsonValueKind.Number ? i.GetInt32() : null,
                Success = root.TryGetProperty("success", out var s) && s.ValueKind == JsonValueKind.True,
                Raw = root,
            };
        }
        catch
        {
            Log?.Invoke($"bridge parse error: {line}\n");
            return;
        }

        if (msg.Type == "event")
        {
            BridgeEvent?.Invoke(msg);
            return;
        }
        if (msg.Type == "result" && msg.Id is int id)
        {
            TaskCompletionSource<BridgeMessage>? tcs;
            lock (_gate) { _pending.Remove(id, out tcs); }
            if (tcs is null) return;
            if (msg.Success) tcs.TrySetResult(msg);
            else tcs.TrySetException(new BridgeException(BridgeErrors.Format(line, msg)));
        }
    }

    private void FailAllPending(Exception ex)
    {
        lock (_gate)
        {
            foreach (var tcs in _pending.Values) tcs.TrySetException(ex);
            _pending.Clear();
        }
    }

    public void Dispose()
    {
        try { _proc?.Kill(entireProcessTree: true); } catch { /* ignore */ }
        _proc?.Dispose();
    }
}

public sealed class BridgeException : Exception
{
    public BridgeException(string message) : base(message) { }
}
