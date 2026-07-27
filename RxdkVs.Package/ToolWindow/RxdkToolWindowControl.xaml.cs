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
            LoadLogo();
        }

        // Loads the extension icon (deployed next to the DLL as Resources\extension-icon.png)
        // into the header. Best-effort: on any failure the header just shows the "RXDK" title.
        private void LoadLogo()
        {
            try
            {
                var dir = System.IO.Path.GetDirectoryName(GetType().Assembly.Location);
                var path = System.IO.Path.Combine(dir, "Resources", "extension-icon.png");
                if (!System.IO.File.Exists(path)) return;
                var bmp = new System.Windows.Media.Imaging.BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = System.Windows.Media.Imaging.BitmapCacheOption.OnLoad;
                bmp.UriSource = new Uri(path, UriKind.Absolute);
                bmp.EndInit();
                bmp.Freeze();
                LogoImage.Source = bmp;
            }
            catch { /* header shows the title without a logo */ }
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
        // Project
        private void OnImportProject(object sender, RoutedEventArgs e) => Exec(CommandIds.CmdImportProject);
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
        /// Modal wizard for importing a VS2003 XDK project: pick the .vcproj and an output folder.
        /// Returns (vcprojPath, outputDir), or (null, null) if cancelled.
        /// </summary>
        public static (string vcproj, string outDir) PromptForImport()
        {
            var dialog = new Window
            {
                Title = "Import VS2003 XDK Project",
                Width = 560,
                SizeToContent = SizeToContent.Height,
                WindowStartupLocation = WindowStartupLocation.CenterOwner,
                ResizeMode = ResizeMode.NoResize,
            };
            var root = new StackPanel { Margin = new Thickness(12) };

            root.Children.Add(new TextBlock { Text = "VS2003 project (.vcproj):", Margin = new Thickness(0, 0, 0, 4) });
            var vcprojBox = new TextBox();
            var vcprojBrowse = new Button { Content = "Browse…", Width = 78, Margin = new Thickness(6, 0, 0, 0) };
            var row1 = new DockPanel { Margin = new Thickness(0, 0, 0, 10) };
            DockPanel.SetDock(vcprojBrowse, Dock.Right);
            row1.Children.Add(vcprojBrowse);
            row1.Children.Add(vcprojBox);
            root.Children.Add(row1);

            root.Children.Add(new TextBlock { Text = "Output folder:", Margin = new Thickness(0, 0, 0, 4) });
            var outBox = new TextBox();
            var outBrowse = new Button { Content = "Browse…", Width = 78, Margin = new Thickness(6, 0, 0, 0) };
            var row2 = new DockPanel();
            DockPanel.SetDock(outBrowse, Dock.Right);
            row2.Children.Add(outBrowse);
            row2.Children.Add(outBox);
            root.Children.Add(row2);

            root.Children.Add(new TextBlock
            {
                Text = "The RXDK project (.vcxproj + property pages) is written to the output folder next to your sources.",
                TextWrapping = TextWrapping.Wrap, Opacity = 0.7, FontSize = 11, Margin = new Thickness(0, 8, 0, 0),
            });

            var buttons = new StackPanel
            {
                Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right,
                Margin = new Thickness(0, 14, 0, 0),
            };
            var ok = new Button { Content = "Import", Width = 78, IsDefault = true, Margin = new Thickness(0, 0, 6, 0) };
            var cancel = new Button { Content = "Cancel", Width = 78, IsCancel = true };
            buttons.Children.Add(ok);
            buttons.Children.Add(cancel);
            root.Children.Add(buttons);
            dialog.Content = root;

            vcprojBrowse.Click += (_, __) =>
            {
                var ofd = new Microsoft.Win32.OpenFileDialog
                {
                    Filter = "VS2003 project (*.vcproj)|*.vcproj|All files (*.*)|*.*",
                    Title = "Select the VS2003 .vcproj",
                };
                if (ofd.ShowDialog() == true)
                {
                    vcprojBox.Text = ofd.FileName;
                    if (string.IsNullOrEmpty(outBox.Text))
                        outBox.Text = System.IO.Path.GetDirectoryName(ofd.FileName);
                }
            };
            outBrowse.Click += (_, __) =>
            {
                using (var fbd = new System.Windows.Forms.FolderBrowserDialog { Description = "Output folder for the RXDK project" })
                {
                    if (!string.IsNullOrEmpty(outBox.Text)) fbd.SelectedPath = outBox.Text;
                    if (fbd.ShowDialog() == System.Windows.Forms.DialogResult.OK) outBox.Text = fbd.SelectedPath;
                }
            };

            bool okd = false;
            ok.Click += (_, __) =>
            {
                if (string.IsNullOrWhiteSpace(vcprojBox.Text) || string.IsNullOrWhiteSpace(outBox.Text))
                {
                    System.Windows.MessageBox.Show(dialog, "Pick both a .vcproj and an output folder.", "RXDK");
                    return;
                }
                okd = true;
                dialog.DialogResult = true;
            };

            return dialog.ShowDialog() == true && okd ? (vcprojBox.Text.Trim(), outBox.Text.Trim()) : (null, null);
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
