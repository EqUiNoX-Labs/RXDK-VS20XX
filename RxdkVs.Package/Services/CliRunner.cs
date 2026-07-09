using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;

namespace RxdkVs.Package.Services
{
    /// <summary>
    /// Shared helper that shells out to <c>Rxdk.Cli.exe &lt;verb&gt; …</c>, streams stdout/stderr
    /// into the "RXDK" VS Output pane, and turns compiler/engine diagnostics into Error List
    /// entries. This is the pure-.NET analog of RXDK-VSCode's buildRunner/task output plumbing.
    ///
    /// The CLI is net8 while this package is .NET Framework, so we cross the runtime boundary
    /// by process, injecting DOTNET_ROOT (mirroring RXDK-VSCode's dotnetEnv.ts) so the managed
    /// host resolves even when a global .NET isn't on PATH.
    /// </summary>
    internal sealed class CliRunner
    {
        // Output pane GUID (stable so the pane persists across runs). Distinct from any command GUID.
        private static readonly Guid OutputPaneGuid = new Guid("2b6b0c4e-4a2f-4d8c-9d51-2f4f0c9b7a10");
        private const string OutputPaneTitle = "RXDK";

        private readonly AsyncPackage _package;

        // gcc/clang-style diagnostic:  path:line:col: error: message   (matches problemMatcher: ['$gcc'])
        private static readonly Regex GccDiagnostic = new Regex(
            @"^(?<file>[^:]+):(?<line>\d+):(?<col>\d+):\s*(?<sev>error|warning|note):\s*(?<msg>.*)$",
            RegexOptions.Compiled | RegexOptions.IgnoreCase);

        // Engine-level failures the CLI prints, e.g. "build failed: <reason>" / "error: <reason>".
        private static readonly Regex EngineError = new Regex(
            @"^(?:error:\s*|(?:build|deploy|run|reboot|set-ip)\s+failed:\s*)(?<msg>.*)$",
            RegexOptions.Compiled | RegexOptions.IgnoreCase);

        public CliRunner(AsyncPackage package)
        {
            _package = package;
        }

        /// <summary>
        /// Runs the CLI with the given argument list against the given working directory.
        /// Returns the process exit code (non-zero = failure). Never throws for a failed build;
        /// callers inspect the return code. Throws only if the CLI exe can't be found.
        /// </summary>
        public async Task<int> RunAsync(IEnumerable<string> args, string workingDirectory, CancellationToken cancellationToken = default)
        {
            var cliPath = ToolLocator.ResolveCli();
            if (cliPath == null)
            {
                await WriteLineAsync($"[RXDK] Could not locate {ToolLocator.CliExeName}. " +
                    "Set RXDK_TOOLS_DIR, or install the engine (see README).");
                throw new FileNotFoundExceptionLite($"{ToolLocator.CliExeName} not found");
            }

            var quotedArgs = string.Join(" ", QuoteAll(args));
            await ClearErrorListAsync();
            await WriteLineAsync($"[RXDK] > {cliPath} {quotedArgs}");

            var psi = new ProcessStartInfo
            {
                FileName = cliPath,
                Arguments = quotedArgs,
                WorkingDirectory = workingDirectory ?? Environment.CurrentDirectory,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
            };
            InjectDotnetRoot(psi);

            var errors = new List<DiagnosticEntry>();

            using (var process = new Process { StartInfo = psi, EnableRaisingEvents = true })
            {
                var tcs = new TaskCompletionSource<int>();
                process.Exited += (_, __) => tcs.TrySetResult(process.ExitCode);

                process.OutputDataReceived += (_, e) => HandleLine(e.Data, errors);
                process.ErrorDataReceived += (_, e) => HandleLine(e.Data, errors);

                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                using (cancellationToken.Register(() => TryKill(process)))
                {
                    var exitCode = await tcs.Task;
                    await PublishErrorsAsync(errors);
                    await WriteLineAsync($"[RXDK] exit code {exitCode}");
                    return exitCode;
                }
            }
        }

        private void HandleLine(string line, List<DiagnosticEntry> errors)
        {
            if (line == null)
            {
                return;
            }

            // Fire-and-forget the UI write; ordering within the pane is preserved because we
            // marshal to the UI thread via the JoinableTaskFactory queue.
            _ = WriteLineAsync(line);

            var gcc = GccDiagnostic.Match(line);
            if (gcc.Success)
            {
                errors.Add(new DiagnosticEntry
                {
                    File = gcc.Groups["file"].Value.Trim(),
                    Line = int.Parse(gcc.Groups["line"].Value),
                    Column = int.Parse(gcc.Groups["col"].Value),
                    Message = gcc.Groups["msg"].Value.Trim(),
                    IsWarning = gcc.Groups["sev"].Value.Equals("warning", StringComparison.OrdinalIgnoreCase)
                                || gcc.Groups["sev"].Value.Equals("note", StringComparison.OrdinalIgnoreCase),
                });
                return;
            }

            var eng = EngineError.Match(line);
            if (eng.Success)
            {
                errors.Add(new DiagnosticEntry { Message = eng.Groups["msg"].Value.Trim(), IsWarning = false });
            }
        }

        // ---- VS Output pane ----

        private IVsOutputWindowPane _pane;

        private async Task<IVsOutputWindowPane> GetPaneAsync()
        {
            if (_pane != null)
            {
                return _pane;
            }
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var outputWindow = (IVsOutputWindow)await _package.GetServiceAsync(typeof(SVsOutputWindow));
            var paneGuid = OutputPaneGuid;
            // CreatePane is idempotent for a given guid.
            outputWindow.CreatePane(ref paneGuid, OutputPaneTitle, fInitVisible: 1, fClearWithSolution: 0);
            outputWindow.GetPane(ref paneGuid, out _pane);
            return _pane;
        }

        private async Task WriteLineAsync(string text)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var pane = await GetPaneAsync();
            pane?.OutputStringThreadSafe(text + Environment.NewLine);
            pane?.Activate();
        }

        // ---- Error List ----

        private ErrorListProvider _errorList;

        private async Task<ErrorListProvider> GetErrorListAsync()
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            // AsyncPackage implements System.IServiceProvider synchronously on the UI thread.
            return _errorList ?? (_errorList = new ErrorListProvider(_package) { ProviderName = "RXDK" });
        }

        private async Task ClearErrorListAsync()
        {
            var list = await GetErrorListAsync();
            list.Tasks.Clear();
        }

        private async Task PublishErrorsAsync(List<DiagnosticEntry> errors)
        {
            if (errors.Count == 0)
            {
                return;
            }
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var list = await GetErrorListAsync();
            foreach (var e in errors)
            {
                var task = new ErrorTask
                {
                    Category = TaskCategory.BuildCompile,
                    ErrorCategory = e.IsWarning ? TaskErrorCategory.Warning : TaskErrorCategory.Error,
                    Text = e.Message,
                    Document = e.File ?? string.Empty,
                    Line = Math.Max(0, e.Line - 1),      // Error List is 0-based.
                    Column = Math.Max(0, e.Column - 1),
                };
                list.Tasks.Add(task);
            }
            list.Show();
        }

        // ---- helpers ----

        private static void InjectDotnetRoot(ProcessStartInfo psi)
        {
            // Mirror RXDK-VSCode dotnetEnv.ts: if a private .NET root exists, hand it to the
            // child so a self-contained/framework-dependent net8 exe resolves the runtime.
            if (psi.EnvironmentVariables.ContainsKey("DOTNET_ROOT"))
            {
                return;
            }
            var candidates = new[]
            {
                Environment.ExpandEnvironmentVariables(@"%ProgramFiles%\dotnet"),
                Environment.ExpandEnvironmentVariables(@"%ProgramData%\RXDK\dotnet"),
                System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".dotnet"),
            };
            foreach (var c in candidates)
            {
                if (System.IO.Directory.Exists(c))
                {
                    psi.EnvironmentVariables["DOTNET_ROOT"] = c;
                    break;
                }
            }
        }

        private static IEnumerable<string> QuoteAll(IEnumerable<string> args)
        {
            foreach (var a in args)
            {
                yield return a.IndexOf(' ') >= 0 ? "\"" + a + "\"" : a;
            }
        }

        private static void TryKill(Process p)
        {
            try { if (!p.HasExited) { p.Kill(); } } catch { /* best effort */ }
        }

        private sealed class DiagnosticEntry
        {
            public string File;
            public int Line;
            public int Column;
            public string Message;
            public bool IsWarning;
        }

        // Avoids a using for System.IO just to throw a typed not-found error.
        private sealed class FileNotFoundExceptionLite : Exception
        {
            public FileNotFoundExceptionLite(string message) : base(message) { }
        }
    }
}
