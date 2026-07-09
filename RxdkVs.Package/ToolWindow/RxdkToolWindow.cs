using System;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Shell;

namespace RxdkVs.Package.ToolWindow
{
    /// <summary>
    /// The RXDK tool window — the VS analog of RXDK-VSCode's activity-bar sidebar. Hosts a WPF
    /// control (<see cref="RxdkToolWindowControl"/>) with the current Xbox IP and Build/Deploy/
    /// Run/Debug/Set-IP/New-Project buttons. Registered via [ProvideToolWindow] on RxdkPackage and
    /// shown by the rxdk.showSidebar command.
    /// </summary>
    [Guid(RxdkPackageGuids.ToolWindowGuidString)]
    public sealed class RxdkToolWindow : ToolWindowPane
    {
        public RxdkToolWindow() : base(null)
        {
            Caption = "RXDK";
            // The WPF control is the window content. It talks to the package's command service so
            // its buttons execute exactly the same handlers as the menu commands.
            Content = new RxdkToolWindowControl();
        }

        /// <summary>Called after the frame is created; hand the control our service provider.</summary>
        public override void OnToolWindowCreated()
        {
            base.OnToolWindowCreated();
            if (Content is RxdkToolWindowControl control)
            {
                control.Initialize(this.Package as RxdkPackage);
            }
        }
    }
}
