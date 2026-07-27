using System;
using System.ComponentModel.Design;
using System.Windows;
using System.Windows.Controls;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Threading;
using RxdkVs.Package.Commands;

namespace RxdkVs.Package.ToolWindow
{
    /// <summary>
    /// WPF content for the RXDK tool window. Buttons re-use the package's command surface by
    /// invoking the corresponding CommandID on the OleMenuCommandService, so there's exactly one
    /// implementation of each action (in RxdkCommands) whether it's triggered from the menu or here.
    /// The IP label is refreshed from `Rxdk.Cli xbox-ip`.
    /// </summary>
    public partial class RxdkToolWindowControl : UserControl
    {
        private RxdkPackage _package;

        public RxdkToolWindowControl()
        {
            InitializeComponent();
        }

        public void Initialize(RxdkPackage package)
        {
            _package = package;
            _ = RefreshAsync();
        }

        // ---- button handlers: dispatch to the shared command IDs ----

        // Console
        private void OnReboot(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdRebootConsole);
        private void OnSetIp(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdSetXboxIp, refreshAfter: true);
        // Folders
        private void OnOpenSdkFolder(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenSdkFolder);
        private void OnOpenToolsFolder(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenToolsFolder);
        private void OnOpenDocsFolder(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenDocsFolder);
        // Documentation
        private void OnOpenSdkDocs(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenSdkDocs);
        private void OnOpenExtensionDocs(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenExtensionDocs);
        // Tools
        private void OnLaunchXbwatson(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdLaunchXbwatson);
        private void OnLaunchNeighborhoodApp(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdLaunchXbNeighborhood);
        private void OnOpenXboxNeighborhood(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenXboxNeighborhood);
        private void OnCycleGlobals(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdCycleGlobalsScope);
        // Setup
        private void OnFetchSdk(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdFetchLatestSdk);
        private void OnInstallDotNet(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdInstallDotNet);
        private void OnCompleteSetup(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdSetupPrerequisites);
        private void OnSettings(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdOpenSettings);
        private void OnRefresh(object sender, RoutedEventArgs e) => _ = RefreshAsync();

        private void Exec(int commandId, bool refreshAfter = false)
        {
            if (_package == null)
            {
                return;
            }
            _package.JoinableTaskFactory.RunAsync(async () =>
            {
                await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
                var svc = (OleMenuCommandService)await _package.GetServiceAsync(typeof(IMenuCommandService));
                var id = new CommandID(RxdkPackageGuids.CommandSet, commandId);
                svc?.GlobalInvoke(id);
                if (refreshAfter)
                {
                    await RefreshAsync();
                }
            }).FileAndForget("rxdk/toolwindow");
        }

        // ---- IP refresh ----

        private async System.Threading.Tasks.Task RefreshAsync()
        {
            if (_package == null)
            {
                return;
            }
            SetStatus("Querying devkit…");
            string ip = null;
            try
            {
                ip = await ProbeIpAsync();
            }
            catch (Exception ex)
            {
                SetStatus($"IP query failed: {ex.Message}");
            }

            await ThreadHelper.JoinableTaskFactory.SwitchToMainThreadAsync();
            IpText.Text = string.IsNullOrEmpty(ip) ? "(none configured)" : ip;
            SetStatus(string.IsNullOrEmpty(ip)
                ? "No Xbox console configured. Click Set… to enter an IP."
                : "Ready.");
        }

        private void SetStatus(string text)
        {
            if (StatusText != null)
            {
                StatusText.Text = text;
            }
        }

        // ---- a tiny modal input box (VS ships none) ----

        /// <summary>
        /// Shows a minimal modal text-input dialog. Returns the entered string, or null if the
        /// user cancels. Used by the Set Xbox IP command.
        /// </summary>
        public static string PromptForString(string title, string prompt, string initial)
        {
            var dialog = new Window
            {
                Title = title,
                Width = 380,
                Height = 150,
                WindowStartupLocation = WindowStartupLocation.CenterOwner,
                ResizeMode = ResizeMode.NoResize,
                SizeToContent = SizeToContent.Manual,
            };

            var root = new StackPanel { Margin = new Thickness(12) };
            root.Children.Add(new TextBlock { Text = prompt, Margin = new Thickness(0, 0, 0, 6), TextWrapping = TextWrapping.Wrap });

            var input = new TextBox { Text = initial ?? string.Empty };
            root.Children.Add(input);

            var buttons = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Right,
                Margin = new Thickness(0, 12, 0, 0),
            };
            var ok = new Button { Content = "OK", Width = 72, IsDefault = true, Margin = new Thickness(0, 0, 6, 0) };
            var cancel = new Button { Content = "Cancel", Width = 72, IsCancel = true };
            buttons.Children.Add(ok);
            buttons.Children.Add(cancel);
            root.Children.Add(buttons);

            dialog.Content = root;

            string result = null;
            ok.Click += (_, __) => { result = input.Text; dialog.DialogResult = true; };
            input.Focus();
            input.SelectAll();

            return dialog.ShowDialog() == true ? result : null;
        }

        /// <summary>
        /// Runs `Rxdk.Cli.exe xbox-ip` and returns the resolved devkit address, or null. Kept
        /// local to the control so the IP label refreshes without touching the command service.
        /// </summary>
        private static async System.Threading.Tasks.Task<string> ProbeIpAsync()
        {
            var cliPath = Services.ToolLocator.ResolveCli();
            if (cliPath == null)
            {
                return null;
            }
            var psi = new System.Diagnostics.ProcessStartInfo
            {
                FileName = cliPath,
                Arguments = "xbox-ip",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                CreateNoWindow = true,
            };
            using (var p = System.Diagnostics.Process.Start(psi))
            {
                var output = await p.StandardOutput.ReadToEndAsync();
                p.WaitForExit(5000);
                var line = output.Trim();
                if (p.ExitCode != 0 || line.StartsWith("no Xbox", StringComparison.OrdinalIgnoreCase))
                {
                    return null;
                }
                return line;
            }
        }
    }
}
