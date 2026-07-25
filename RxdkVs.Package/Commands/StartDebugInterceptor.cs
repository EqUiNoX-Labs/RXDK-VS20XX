using System;
using System.Threading.Tasks;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using RxdkVs.Package.Services;
using Task = System.Threading.Tasks.Task;

namespace RxdkVs.Package.Commands
{
    /// <summary>
    /// Makes F5 / the green Run button debug Xbox titles. VS won't natively route a Makefile
    /// project's F5 to a DAP adapter — it tries to run the .xbe as a local Windows process
    /// ("not a valid Win32 application"). So we register a priority command target that gets
    /// first crack at Debug.Start (F5): if the startup project is an RXDK Xbox project we launch
    /// the Xbox debug adapter and mark the command handled; otherwise we return NOTSUPPORTED so
    /// VS's normal debugging is completely unaffected for every other project type.
    /// </summary>
    internal sealed class StartDebugInterceptor : IOleCommandTarget
    {
        private readonly RxdkPackage _package;
        private readonly CliRunner _cli;

        private StartDebugInterceptor(RxdkPackage package, CliRunner cli)
        {
            _package = package;
            _cli = cli;
        }

        public static async Task RegisterAsync(RxdkPackage package, CliRunner cli)
        {
            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            var register = (IVsRegisterPriorityCommandTarget)await package.GetServiceAsync(typeof(SVsRegisterPriorityCommandTarget));
            if (register == null) return;
            register.RegisterPriorityCommandTarget(0, new StartDebugInterceptor(package, cli), out _);
            // Cookie intentionally not stored: the interceptor lives for the VS session.
        }

        private static bool IsStart(ref Guid group, uint cmdId) =>
            group == VSConstants.GUID_VSStandardCommandSet97 && cmdId == (uint)VSConstants.VSStd97CmdID.Start;

        public int QueryStatus(ref Guid pguidCmdGroup, uint cCmds, OLECMD[] prgCmds, IntPtr pCmdText)
        {
            // Never override enablement/label — let VS's default decide. We only act at Exec.
            return (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED;
        }

        public int Exec(ref Guid pguidCmdGroup, uint nCmdID, uint nCmdexecopt, IntPtr pvaIn, IntPtr pvaOut)
        {
            if (IsStart(ref pguidCmdGroup, nCmdID) && !System.Diagnostics.Debugger.IsAttached)
            {
                bool isXbox;
                try
                {
                    // We're on the UI thread here; the check is UI-bound and quick.
                    isXbox = ThreadHelper.JoinableTaskFactory.Run(() =>
                        XboxDebugLauncher.IsXboxStartupProjectAsync(_package));
                }
                catch
                {
                    isXbox = false;
                }

                if (isXbox)
                {
                    _package.JoinableTaskFactory
                        .RunAsync(() => XboxDebugLauncher.LaunchAsync(_package, _cli))
                        .FileAndForget("rxdk/f5-debug");
                    return VSConstants.S_OK; // handled — VS does not run the Local Windows Debugger.
                }
            }
            return (int)Microsoft.VisualStudio.OLE.Interop.Constants.OLECMDERR_E_NOTSUPPORTED; // pass through to VS.
        }
    }
}
