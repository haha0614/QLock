using System;
using System.IO;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace QLock
{
    public class UpdateWindow
    {
        public Window Win;

        readonly QLockApp app;
        readonly ReleaseInfo info;
        readonly Action<string> onSkip;

        TextBlock status;
        ProgressBar bar;
        Button upgrade, later, skip;

        public UpdateWindow(QLockApp app, ReleaseInfo info, Action<string> onSkip)
        {
            this.app = app;
            this.info = info;
            this.onSkip = onSkip;

            string remote = info.Tag == null ? "" : info.Tag.Trim();
            if (remote.StartsWith("v", StringComparison.OrdinalIgnoreCase)) remote = remote.Substring(1);

            Win = new Window();
            Win.Title = "QLock 更新";
            Win.Width = 560;
            Win.Height = 480;
            Win.ResizeMode = ResizeMode.NoResize;
            Win.WindowStartupLocation = WindowStartupLocation.CenterScreen;
            Win.Background = Ui.Surface;
            Win.FontFamily = Ui.Font;
            Win.Foreground = Ui.TextPrimary;

            StackPanel root = new StackPanel();
            root.Margin = new Thickness(Design.Gap20, Design.Gap20, Design.Gap20, Design.Gap16);

            TextBlock title = Ui.Text("发现新版本 " + remote, Design.FontTitle, Ui.TextPrimary, FontWeights.SemiBold);
            root.Children.Add(title);

            TextBlock intro = Ui.Text("当前版本 " + Updater.VersionText(Updater.CurrentVersion()) + "，可以升级到 " + remote + "。升级只替换程序本身，配置和密码都保留。", Design.FontSmall, Ui.TextSecondary, FontWeights.Normal);
            intro.TextWrapping = TextWrapping.Wrap;
            intro.Margin = new Thickness(0, Design.Gap8, 0, Design.Gap16);
            root.Children.Add(intro);

            root.Children.Add(Ui.Label("更新内容"));

            TextBox log = new TextBox();
            log.IsReadOnly = true;
            log.AcceptsReturn = true;
            log.TextWrapping = TextWrapping.Wrap;
            log.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;
            log.Height = 220;
            log.Margin = new Thickness(0, Design.Gap8, 0, 0);
            log.Text = string.IsNullOrEmpty(info.Body) ? "（这次发布没有写更新说明）" : info.Body.Replace("\r\n", "\n").Trim();
            Ui.StyleInput(log, true);
            root.Children.Add(log);

            bar = new ProgressBar();
            bar.Height = 6;
            bar.Margin = new Thickness(0, Design.Gap16, 0, 0);
            bar.Minimum = 0;
            bar.Maximum = 100;
            bar.Visibility = Visibility.Collapsed;
            root.Children.Add(bar);

            status = Ui.Text("", Design.FontSmall, Ui.TextSecondary, FontWeights.Normal);
            status.TextWrapping = TextWrapping.Wrap;
            status.Margin = new Thickness(0, Design.Gap8, 0, 0);
            root.Children.Add(status);

            StackPanel buttons = new StackPanel();
            buttons.Orientation = Orientation.Horizontal;
            buttons.Margin = new Thickness(0, Design.Gap16, 0, 0);
            root.Children.Add(buttons);

            upgrade = Ui.Button("下载并升级", true);
            upgrade.Width = 120;
            upgrade.Click += delegate { StartUpgrade(); };
            buttons.Children.Add(upgrade);

            later = Ui.Button("稍后", false);
            later.Width = 84;
            later.Margin = new Thickness(Design.Gap8, 0, 0, 0);
            later.Click += delegate { Win.Close(); };
            buttons.Children.Add(later);

            skip = Ui.Button("跳过这个版本", false);
            skip.Width = 120;
            skip.Margin = new Thickness(Design.Gap8, 0, 0, 0);
            skip.Click += delegate
            {
                if (onSkip != null) onSkip(info.Tag);
                Win.Close();
            };
            buttons.Children.Add(skip);

            Win.Content = root;
        }

        void SetStatus(string text, bool error)
        {
            status.Text = text;
            status.Foreground = error ? Ui.Error : Ui.TextSecondary;
        }

        void StartUpgrade()
        {
            upgrade.IsEnabled = false;
            later.IsEnabled = false;
            skip.IsEnabled = false;
            bar.Visibility = Visibility.Visible;
            bar.IsIndeterminate = true;
            SetStatus("正在下载…", false);

            string exeUrl = Updater.ExeDownloadUrl(info);
            string sumsUrl = Updater.SumsDownloadUrl(info);
            string target = Path.Combine(Updater.DownloadDirectory(), Updater.ExeAssetName);
            Log.Write("update: downloading " + exeUrl);

            Thread worker = new Thread(delegate ()
            {
                try
                {
                    Updater.Download(exeUrl, target, delegate (long got, long total)
                    {
                        long gotSnapshot = got;
                        long totalSnapshot = total;
                        Win.Dispatcher.BeginInvoke(new Action(delegate
                        {
                            if (totalSnapshot <= 0)
                            {
                                bar.IsIndeterminate = true;
                                return;
                            }
                            bar.IsIndeterminate = false;
                            bar.Value = Math.Min(100, (int)(gotSnapshot * 100 / totalSnapshot));
                            SetStatus("正在下载… " + (gotSnapshot / 1024) + " / " + (totalSnapshot / 1024) + " KB", false);
                        }));
                    });

                    Win.Dispatcher.BeginInvoke(new Action(delegate
                    {
                        bar.IsIndeterminate = true;
                        SetStatus("正在校验文件…", false);
                    }));

                    string sums = Updater.FetchText(sumsUrl);
                    string expected = Updater.ExpectedHash(sums);
                    if (expected == null) throw new Exception("发布页里没找到 SHA256SUMS.txt，放弃这次升级");
                    string actual = Updater.Sha256File(target);
                    if (!string.Equals(expected, actual, StringComparison.OrdinalIgnoreCase))
                        throw new Exception("下载的文件和发布页的校验值不一致，已放弃这次升级");

                    Updater.Install(target);
                    Log.Write("update: installed, restarting");

                    Win.Dispatcher.BeginInvoke(new Action(delegate
                    {
                        bar.IsIndeterminate = false;
                        bar.Value = 100;
                        SetStatus("已经装好，正在重启 QLock…", false);
                        Win.Dispatcher.BeginInvoke(new Action(delegate { app.RestartForUpdate(); }), DispatcherPriority.Background);
                    }));
                }
                catch (Exception ex)
                {
                    Log.Write("update failed: " + ex.Message);
                    Win.Dispatcher.BeginInvoke(new Action(delegate
                    {
                        bar.Visibility = Visibility.Collapsed;
                        upgrade.IsEnabled = true;
                        later.IsEnabled = true;
                        skip.IsEnabled = true;
                        SetStatus("升级失败：" + ex.Message, true);
                    }));
                }
            });
            worker.IsBackground = true;
            worker.Start();
        }
    }
}
