using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;

namespace QLock
{
    public class SettingsWindow
    {
        public Window Win;

        readonly AppConfig cfg;
        readonly bool firstRun;
        readonly Action<AppConfig> onSaved;

        PasswordBox pwd1;
        TextBox pwdReveal;
        bool revealPwd;
        Line eyeSlash;
        TextBox recOut;
        TextBlock recStatus;
        TextBlock statusText;
        CheckBox cbWinL, cbHotkey, cbLockOnStartup, cbShowClock, cbSnapshot;
        CheckBox cbBlockHotkeys, cbAutostart, cbElevated;
        TextBox idleBox;
        TextBox subtitleBox;
        string pendingRecovery;

        public SettingsWindow(AppConfig current, bool firstRun, Action<AppConfig> onSaved)
        {
            cfg = current.Clone();
            this.firstRun = firstRun;
            this.onSaved = onSaved;
            pendingRecovery = null;

            Win = new Window();
            Win.Title = firstRun ? "QLock 首次设置" : "QLock 设置";
            Win.Width = 600;
            Win.Height = 800;
            Win.ResizeMode = ResizeMode.CanMinimize;
            Win.WindowStartupLocation = WindowStartupLocation.CenterScreen;
            Win.Background = new SolidColorBrush(Color.FromRgb(24, 27, 35));
            Win.FontFamily = Ui.Font;
            Win.Foreground = Ui.TextPrimary;

            Grid root = new Grid();
            root.RowDefinitions.Add(new RowDefinition());
            root.RowDefinitions.Add(new RowDefinition());
            root.RowDefinitions[1].Height = GridLength.Auto;

            ScrollViewer scroll = new ScrollViewer();
            scroll.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;
            scroll.Padding = new Thickness(24, 20, 24, 8);
            root.Children.Add(scroll);

            StackPanel stack = new StackPanel();
            scroll.Content = stack;

            TextBlock title = Ui.Text(firstRun ? "欢迎使用 QLock" : "QLock 设置", 20, Ui.TextPrimary, FontWeights.SemiBold);
            title.Margin = new Thickness(0, 0, 0, 4);
            stack.Children.Add(title);

            TextBlock intro = Ui.Text(firstRun
                ? "先设置一个解锁密码。这个密码只属于 QLock，与 Windows 账户完全无关，输错也不会导致账户锁定。"
                : "密码与 Windows 账户无关，只用于解锁 QLock 自己的锁屏。", 12.5, Ui.TextSecondary, FontWeights.Normal);
            intro.TextWrapping = TextWrapping.Wrap;
            intro.Margin = new Thickness(0, 0, 0, 16);
            stack.Children.Add(intro);

            // ---- password ----
            StackPanel pwdSection = Ui.Section("解锁密码");
            stack.Children.Add(Ui.Card(pwdSection, 0));

            pwdSection.Children.Add(Ui.Label(cfg.HasPassword ? "修改密码（留空则保持不变）" : "设置密码"));
            Grid pwdHost = new Grid();
            pwdHost.Width = 340;
            pwdHost.Margin = new Thickness(0, 6, 0, 0);

            pwd1 = new PasswordBox();
            Ui.StyleInput(pwd1);
            pwdHost.Children.Add(pwd1);

            pwdReveal = new TextBox();
            Ui.StyleInput(pwdReveal);
            pwdReveal.Visibility = Visibility.Collapsed;
            pwdHost.Children.Add(pwdReveal);

            Button eyeBtn = EyeButton();
            eyeBtn.HorizontalAlignment = HorizontalAlignment.Right;
            eyeBtn.VerticalAlignment = VerticalAlignment.Center;
            eyeBtn.Margin = new Thickness(0, 0, 4, 0);
            pwdHost.Children.Add(eyeBtn);

            pwdSection.Children.Add(pwdHost);
            TextBlock pwdHint = Ui.Text("锁屏密码，至少 4 位。只用于解锁 QLock；忘记后只能用恢复码打开。", 11.5, Ui.TextSecondary, FontWeights.Normal);
            pwdHint.Margin = new Thickness(0, 6, 0, 0);
            pwdSection.Children.Add(pwdHint);

            // ---- recovery ----
            StackPanel recSection = Ui.Section("恢复码");
            stack.Children.Add(Ui.Card(recSection, 12));

            recStatus = Ui.Text(!string.IsNullOrEmpty(cfg.RecoveryHash) ? "已设置恢复码。" : "尚未设置恢复码。", 12.5, Ui.TextSecondary, FontWeights.Normal);
            recSection.Children.Add(recStatus);

            TextBlock recHint = Ui.Text("忘记密码时用恢复码解锁。恢复码只显示一次，请抄下来放在安全的地方。", 11.5, Ui.TextSecondary, FontWeights.Normal);
            recHint.TextWrapping = TextWrapping.Wrap;
            recHint.Margin = new Thickness(0, 4, 0, 8);
            recSection.Children.Add(recHint);

            Button genRec = Ui.Button("生成新的恢复码", false);
            genRec.Width = 150;
            genRec.HorizontalAlignment = HorizontalAlignment.Left;
            genRec.Click += delegate { GenerateRecovery(); };
            recSection.Children.Add(genRec);

            recOut = new TextBox();
            recOut.IsReadOnly = true;
            recOut.Width = 300;
            recOut.Height = 34;
            recOut.FontFamily = new FontFamily("Consolas, Microsoft YaHei UI");
            Ui.StyleInput(recOut);
            recOut.HorizontalAlignment = HorizontalAlignment.Left;
            recOut.Margin = new Thickness(0, 10, 0, 0);
            recOut.Visibility = Visibility.Collapsed;
            recSection.Children.Add(recOut);

            // ---- triggers ----
            StackPanel trig = Ui.Section("锁定触发方式");
            stack.Children.Add(Ui.Card(trig, 12));

            cbWinL = Ui.Check(cfg.InterceptWinL, "接管 Win+L：按 Win+L 时由 QLock 锁定（不触发 Windows 锁屏）", 0);
            trig.Children.Add(cbWinL);
            cbHotkey = Ui.Check(cfg.CustomHotkeyEnabled, "自定义热键：Ctrl+Alt+L 锁定", 8);
            trig.Children.Add(cbHotkey);
            cbLockOnStartup = Ui.Check(cfg.LockOnStartup, "程序启动后立即锁定", 8);
            trig.Children.Add(cbLockOnStartup);

            StackPanel idleRow = new StackPanel();
            idleRow.Orientation = Orientation.Horizontal;
            idleRow.Margin = new Thickness(0, 10, 0, 0);
            TextBlock idleLabel = Ui.Label("空闲自动锁定：");
            idleLabel.VerticalAlignment = VerticalAlignment.Center;
            idleRow.Children.Add(idleLabel);
            idleBox = new TextBox();
            idleBox.Width = 60;
            idleBox.Text = cfg.IdleLockMinutes.ToString();
            Ui.StyleInput(idleBox);
            idleBox.TextAlignment = TextAlignment.Center;
            idleRow.Children.Add(idleBox);
            TextBlock idleUnit = Ui.Label(" 分钟内无输入则锁定（0 = 关闭）");
            idleUnit.VerticalAlignment = VerticalAlignment.Center;
            idleRow.Children.Add(idleUnit);
            trig.Children.Add(idleRow);

            // ---- appearance ----
            StackPanel look = Ui.Section("外观");
            stack.Children.Add(Ui.Card(look, 12));

            cbShowClock = Ui.Check(cfg.ShowClock, "锁屏界面显示时钟和日期", 0);
            look.Children.Add(cbShowClock);
            cbSnapshot = Ui.Check(cfg.BackgroundSnapshot, "用当前屏幕截图做模糊背景", 8);
            look.Children.Add(cbSnapshot);

            StackPanel subRow = new StackPanel();
            subRow.Orientation = Orientation.Horizontal;
            subRow.Margin = new Thickness(0, 10, 0, 0);
            TextBlock subLabel = Ui.Label("提示文字：");
            subLabel.VerticalAlignment = VerticalAlignment.Center;
            subRow.Children.Add(subLabel);
            subtitleBox = new TextBox();
            subtitleBox.Width = 260;
            subtitleBox.Text = cfg.Subtitle;
            Ui.StyleInput(subtitleBox);
            subRow.Children.Add(subtitleBox);
            look.Children.Add(subRow);

            // ---- hardening ----
            StackPanel hard = Ui.Section("加固（可选，默认关闭的部分请自行判断风险）");
            stack.Children.Add(Ui.Card(hard, 12));

            cbBlockHotkeys = Ui.Check(cfg.BlockSystemHotkeys, "锁定时拦截系统热键（Alt+Tab / Win / Alt+F4 / Ctrl+Esc / Ctrl+Shift+Esc）", 0);
            hard.Children.Add(cbBlockHotkeys);
            TextBlock tmHint = Ui.Text("提示：这里只挡顺手。想让人无法用任务管理器结束本程序，请在下方「开机自启」里勾选「以管理员身份启动」。Ctrl+Alt+Del 始终有效。", 11.5, Ui.TextSecondary, FontWeights.Normal);
            tmHint.TextWrapping = TextWrapping.Wrap;
            tmHint.Margin = new Thickness(22, 4, 0, 8);
            hard.Children.Add(tmHint);

            // ---- startup ----
            StackPanel start = Ui.Section("开机自启");
            stack.Children.Add(Ui.Card(start, 12));

            cbAutostart = Ui.Check(cfg.Autostart == "runkey" || cfg.Autostart == "task", "登录时自动启动（写入当前用户启动项）", 0);
            start.Children.Add(cbAutostart);
            cbElevated = Ui.Check(cfg.RunElevated, "以管理员身份启动（每次启动弹一次 UAC）", 6);
            start.Children.Add(cbElevated);

            TextBlock elevHint = Ui.Text("管理员模式下普通权限无法结束本进程（任务管理器会提示拒绝访问）；取消 UAC 授权时以普通权限继续运行。", 11.5, Ui.TextSecondary, FontWeights.Normal);
            elevHint.TextWrapping = TextWrapping.Wrap;
            elevHint.Margin = new Thickness(22, 4, 0, 2);
            start.Children.Add(elevHint);

            TextBlock autoHint = Ui.Text("当前启动项：" + (Autostart.DetectCurrent() == "runkey" ? "已启用" : "未启用") + "。启动时若取消 UAC 授权，会以普通权限继续运行。", 11.5, Ui.TextSecondary, FontWeights.Normal);
            autoHint.TextWrapping = TextWrapping.Wrap;
            autoHint.Margin = new Thickness(22, 4, 0, 2);
            start.Children.Add(autoHint);

            string env = "当前状态：" + (Native.IsElevated() ? "管理员权限运行" : "普通权限运行");
            TextBlock envText = Ui.Text(env + "　|　配置文件：" + ConfigStore.FilePath, 11.5, Ui.TextSecondary, FontWeights.Normal);
            envText.TextWrapping = TextWrapping.Wrap;
            envText.Margin = new Thickness(0, 10, 0, 0);
            start.Children.Add(envText);

            // ---- footer ----
            Border footer = new Border();
            footer.Background = new SolidColorBrush(Color.FromRgb(20, 23, 30));
            footer.Padding = new Thickness(24, 12, 24, 12);
            footer.BorderThickness = new Thickness(0, 1, 0, 0);
            footer.BorderBrush = Ui.Fg(40, 255, 255, 255);
            Grid.SetRow(footer, 1);
            root.Children.Add(footer);

            Grid footerGrid = new Grid();
            footerGrid.ColumnDefinitions.Add(new ColumnDefinition());
            footerGrid.ColumnDefinitions.Add(new ColumnDefinition());
            footerGrid.ColumnDefinitions[1].Width = GridLength.Auto;
            footer.Child = footerGrid;

            statusText = Ui.Text("", 12.5, Ui.TextSecondary, FontWeights.Normal);
            statusText.VerticalAlignment = VerticalAlignment.Center;
            statusText.TextWrapping = TextWrapping.Wrap;
            footerGrid.Children.Add(statusText);

            StackPanel buttons = new StackPanel();
            buttons.Orientation = Orientation.Horizontal;
            Grid.SetColumn(buttons, 1);
            footerGrid.Children.Add(buttons);

            Button save = Ui.Button(firstRun ? "保存并完成" : "保存", true);
            save.Width = 110;
            save.Click += delegate { OnSave(); };
            buttons.Children.Add(save);

            Button close = Ui.Button(firstRun ? "取消" : "关闭", false);
            close.Width = 84;
            close.Margin = new Thickness(10, 0, 0, 0);
            close.Click += delegate { Win.Close(); };
            buttons.Children.Add(close);

            Win.Content = root;
            Win.PreviewKeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.Key == Key.Escape) Win.Close();
            };
        }

        void GenerateRecovery()
        {
            pendingRecovery = Auth.NewRecoveryCode();
            Auth.SetRecovery(cfg, pendingRecovery);
            recOut.Text = pendingRecovery;
            recOut.Visibility = Visibility.Visible;
            recStatus.Text = "新恢复码已生成并复制到剪贴板，点保存后生效，之后不会再显示。";
            recStatus.Foreground = Ui.Good;
            try { Clipboard.SetText(pendingRecovery); } catch { }
        }

        void SetStatus(string text, bool error)
        {
            statusText.Text = text;
            statusText.Foreground = error ? Ui.Error : Ui.Good;
        }

        Button EyeButton()
        {
            Canvas icon = new Canvas();
            icon.Width = 18;
            icon.Height = 18;

            Path eye = new Path();
            eye.Data = Geometry.Parse("M 1,9 C 4.5,4.2 13.5,4.2 17,9 C 13.5,13.8 4.5,13.8 1,9 Z");
            eye.Stroke = Ui.TextSecondary;
            eye.StrokeThickness = 1.3;
            icon.Children.Add(eye);

            Ellipse pupil = new Ellipse();
            pupil.Width = 4.4;
            pupil.Height = 4.4;
            pupil.Fill = Ui.TextSecondary;
            Canvas.SetLeft(pupil, 6.8);
            Canvas.SetTop(pupil, 6.8);
            icon.Children.Add(pupil);

            eyeSlash = new Line();
            eyeSlash.X1 = 1.5;
            eyeSlash.Y1 = 15.5;
            eyeSlash.X2 = 16.5;
            eyeSlash.Y2 = 2.5;
            eyeSlash.Stroke = Ui.Accent;
            eyeSlash.StrokeThickness = 1.4;
            icon.Children.Add(eyeSlash);

            Button btn = new Button();
            btn.Content = icon;
            btn.Width = 26;
            btn.Height = 26;
            btn.Cursor = Cursors.Hand;
            btn.Focusable = false;
            btn.ToolTip = "显示 / 隐藏密码";
            btn.Template = FlatTemplate();
            btn.Click += delegate { ToggleReveal(); };
            return btn;
        }

        static ControlTemplate FlatTemplate()
        {
            ControlTemplate template = new ControlTemplate(typeof(Button));
            FrameworkElementFactory border = new FrameworkElementFactory(typeof(Border));
            border.SetValue(Border.BackgroundProperty, Brushes.Transparent);
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(6));
            FrameworkElementFactory content = new FrameworkElementFactory(typeof(ContentPresenter));
            content.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            content.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            border.AppendChild(content);
            template.VisualTree = border;
            return template;
        }

        void ToggleReveal()
        {
            revealPwd = !revealPwd;
            if (revealPwd)
            {
                pwdReveal.Text = pwd1.Password;
                pwd1.Visibility = Visibility.Collapsed;
                pwdReveal.Visibility = Visibility.Visible;
                pwdReveal.Focus();
                pwdReveal.CaretIndex = pwdReveal.Text.Length;
            }
            else
            {
                pwd1.Password = pwdReveal.Text == null ? "" : pwdReveal.Text;
                pwdReveal.Visibility = Visibility.Collapsed;
                pwd1.Visibility = Visibility.Visible;
                pwd1.Focus();
            }
            if (eyeSlash != null) eyeSlash.Visibility = revealPwd ? Visibility.Collapsed : Visibility.Visible;
        }

        string CurrentPassword()
        {
            if (revealPwd) return pwdReveal.Text == null ? "" : pwdReveal.Text;
            return pwd1.Password;
        }

        void OnSave()
        {
            string p1 = CurrentPassword();

            if (p1.Length > 0)
            {
                if (p1.Length < 4)
                {
                    SetStatus("密码至少 4 位", true);
                    return;
                }
            }
            else if (!cfg.HasPassword)
            {
                SetStatus("请设置解锁密码", true);
                return;
            }

            int minutes;
            if (!int.TryParse((idleBox.Text == null ? "" : idleBox.Text).Trim(), out minutes) || minutes < 0 || minutes > 600)
            {
                SetStatus("空闲分钟数需为 0 到 600 之间的整数", true);
                return;
            }

            if (p1.Length > 0) Auth.SetPassword(cfg, p1);

            cfg.InterceptWinL = cbWinL.IsChecked == true;
            cfg.CustomHotkeyEnabled = cbHotkey.IsChecked == true;
            cfg.LockOnStartup = cbLockOnStartup.IsChecked == true;
            cfg.IdleLockMinutes = minutes;
            cfg.ShowClock = cbShowClock.IsChecked == true;
            cfg.BackgroundSnapshot = cbSnapshot.IsChecked == true;
            cfg.BlockSystemHotkeys = cbBlockHotkeys.IsChecked == true;
            cfg.RunElevated = cbElevated.IsChecked == true;
            cfg.Subtitle = string.IsNullOrEmpty(subtitleBox.Text) ? "输入密码解锁" : subtitleBox.Text.Trim();
            cfg.Autostart = cbAutostart.IsChecked == true ? "runkey" : "none";

            try
            {
                ConfigStore.Save(cfg);
            }
            catch (Exception ex)
            {
                Log.Write("save failed: " + ex);
                SetStatus("保存失败：" + ex.Message, true);
                return;
            }

            if (onSaved != null) onSaved(cfg);

            string err;
            bool autoOk = Autostart.Apply(cfg.Autostart == "runkey", out err);
            if (!autoOk)
            {
                SetStatus("设置已保存。" + (err == null ? "" : (" 自启提示：" + err)), true);
                return;
            }

            SetStatus("已保存" + (pendingRecovery == null ? "。" : "，恢复码已更新。"), false);
            if (firstRun) Win.Close();
        }
    }
}
