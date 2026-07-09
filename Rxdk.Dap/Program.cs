using Rxdk.Dap;

// Entry point for the RXDK debug adapter — a stdio DAP server that Visual Studio's
// Debug Adapter Host launches (and that VS Code could also use). It speaks the Debug
// Adapter Protocol on stdin/stdout and translates to the xboxdbg-bridge line-JSON
// protocol. C# port of RXDK-VSCode debug/src/adapter.ts.

using var stdin = Console.OpenStandardInput();
using var stdout = Console.OpenStandardOutput();

var adapter = new XboxDebugAdapter(stdin, stdout);
adapter.Run();
