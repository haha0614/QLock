using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace QLock
{
    public class LockWindow
    {
        public Window Win;
        public readonly bool Interactive;

        readonly PasswordBox passwordBox;
        readonly PasswordBox recoveryBox;
        readonly TextBlock placeholder;
        readonly TextBlock messageBlock;
        readonly StackPanel passwordPanel;
        readonly StackPanel recoveryPanel;
        readonly TextBlock clockText;
        readonly TextBlock dateText;
        readonly DispatcherTimer clockTimer;
        readonly DispatcherTimer guardTimer;
        readonly int screenX, screenY, screenW, screenH;

        bool allowClose;
        bool recovering;
        bool retired;

        public event Action<string> SubmitPassword;
        public event Action<string> SubmitRecovery;

        public LockWindow(System.Drawing.Rectangle bounds, BitmapSource background, AppConfig cfg, bool interactive)
        {
            Interactive = interactive;
            screenX = bounds.X;
            screenY = bounds.Y;
            screenW = bounds.Width;
            screenH = bounds.Height;

            Win = new Window();
            Win.WindowStyle = WindowStyle.None;
            Win.ResizeMode = ResizeMode.NoResize;
            Win.ShowInTaskbar = false;
            Win.Topmost = true;
            Win.Background = Brushes.Black;
            Win.WindowStartupLocation = WindowStartupLocation.Manual;
            Win.ShowActivated = interactive;
            Win.Left = 0;
            Win.Top = 0;
            Win.Width = 640;
            Win.Height = 480;
            Win.Closing += OnClosing;
            Win.Deactivated += delegate { if (Interactive && !allowClose && !retired) Reclaim(); };

            Grid root = new Grid();

            if (background != null)
            {
                Image img = new Image();
                img.Source = background;
                img.Stretch = Stretch.Fill;
                BlurEffect blur = new BlurEffect();
                blur.Radius = 26;
                blur.KernelType = KernelType.Gaussian;
                img.Effect = blur;
                root.Children.Add(img);
            }

            Rectangle dim = new Rectangle();
            dim.Fill = Ui.Fg(150, 8, 10, 16);
            root.Children.Add(dim);

            StackPanel column = new StackPanel();
            column.HorizontalAlignment = HorizontalAlignment.Center;
            column.VerticalAlignment = VerticalAlignment.Center;

            clockText = Ui.Text("", 74, Brushes.White, FontWeights.Light);
            clockText.HorizontalAlignment = HorizontalAlignment.Center;
            clockText.Visibility = cfg.ShowClock ? Visibility.Visible : Visibility.Collapsed;
            column.Children.Add(clockText);

            dateText = Ui.Text("", 17, Ui.Fg(190, 200, 215, 235), FontWeights.Normal);
            dateText.HorizontalAlignment = HorizontalAlignment.Center;
            dateText.Margin = new Thickness(0, 2, 0, 26);
            dateText.Visibility = cfg.ShowClock ? Visibility.Visible : Visibility.Collapsed;
            column.Children.Add(dateText);

            Border card = new Border();
            card.Width = 380;
            card.CornerRadius = new CornerRadius(14);
            card.Padding = new Thickness(26, 20, 26, 20);
            card.Background = Ui.Fg(216, 24, 28, 38);
            card.BorderThickness = new Thickness(1);
            card.BorderBrush = Ui.Fg(55, 255, 255, 255);
            if (!interactive) card.Visibility = Visibility.Collapsed;
            column.Children.Add(card);

            StackPanel body = new StackPanel();
            card.Child = body;

            TextBlock brand = Ui.Text("Q L O C K", 11, Ui.Fg(255, 120, 160, 230), FontWeights.SemiBold);
            brand.Margin = new Thickness(0, 0, 0, 6);
            body.Children.Add(brand);

            TextBlock subtitle = Ui.Text(cfg.Subtitle, 14.5, Ui.TextPrimary, FontWeights.Normal);
            subtitle.Margin = new Thickness(0, 0, 0, 14);
            body.Children.Add(subtitle);

            passwordPanel = new StackPanel();
            body.Children.Add(passwordPanel);

            Grid inputHolder = new Grid();
            passwordBox = new PasswordBox();
            passwordBox.Height = 40;
            passwordBox.FontSize = 15;
            Ui.StyleInput(passwordBox);
            passwordBox.Height = 40;
            passwordBox.KeyDown += OnPasswordKeyDown;
            passwordBox.PasswordChanged += OnPasswordChanged;
            inputHolder.Children.Add(passwordBox);

            placeholder = Ui.Text("锁屏密码", 14, Ui.Fg(105, 255, 255, 255), FontWeights.Normal);
            placeholder.Margin = new Thickness(13, 0, 0, 0);
            placeholder.VerticalAlignment = VerticalAlignment.Center;
            placeholder.IsHitTestVisible = false;
            inputHolder.Children.Add(placeholder);
            passwordPanel.Children.Add(inputHolder);

            Button unlock = Ui.Button("解锁", true);
            unlock.Height = 40;
            unlock.FontSize = 15;
            unlock.Margin = new Thickness(0, 12, 0, 0);
            unlock.Click += delegate { FireSubmit(); };
            passwordPanel.Children.Add(unlock);

            TextBlock forgot = Ui.Text("忘记密码？用恢复码解锁", 12, Ui.Fg(255, 140, 180, 240), FontWeights.Normal);
            forgot.HorizontalAlignment = HorizontalAlignment.Center;
            forgot.Margin = new Thickness(0, 12, 0, 0);
            forgot.Cursor = Cursors.Hand;
            forgot.MouseLeftButtonUp += delegate { ShowRecovery(true); };
            passwordPanel.Children.Add(forgot);

            recoveryPanel = new StackPanel();
            recoveryPanel.Visibility = Visibility.Collapsed;
            body.Children.Add(recoveryPanel);

            TextBlock recHint = Ui.Text("输入 10 位恢复码（形如 XXXXX-XXXXX）", 12, Ui.TextSecondary, FontWeights.Normal);
            recHint.Margin = new Thickness(0, 0, 0, 8);
            recoveryPanel.Children.Add(recHint);

            recoveryBox = new PasswordBox();
            recoveryBox.Height = 40;
            recoveryBox.FontSize = 15;
            Ui.StyleInput(recoveryBox);
            recoveryBox.Height = 40;
            recoveryBox.KeyDown += OnRecoveryKeyDown;
            recoveryPanel.Children.Add(recoveryBox);

            Button recOk = Ui.Button("用恢复码解锁", true);
            recOk.Height = 40;
            recOk.Margin = new Thickness(0, 12, 0, 0);
            recOk.Click += delegate { FireRecovery(); };
            recoveryPanel.Children.Add(recOk);

            TextBlock back = Ui.Text("返回密码输入", 12, Ui.Fg(255, 140, 180, 240), FontWeights.Normal);
            back.HorizontalAlignment = HorizontalAlignment.Center;
            back.Margin = new Thickness(0, 12, 0, 0);
            back.Cursor = Cursors.Hand;
            back.MouseLeftButtonUp += delegate { ShowRecovery(false); };
            recoveryPanel.Children.Add(back);

            messageBlock = Ui.Text("", 12, Ui.Error, FontWeights.Normal);
            messageBlock.Height = 18;
            messageBlock.Margin = new Thickness(0, 10, 0, 0);
            messageBlock.TextAlignment = TextAlignment.Center;
            body.Children.Add(messageBlock);

            root.Children.Add(column);
            Win.Content = root;

            clockTimer = new DispatcherTimer();
            clockTimer.Interval = TimeSpan.FromSeconds(2);
            clockTimer.Tick += delegate { UpdateClock(); };

            guardTimer = new DispatcherTimer();
            guardTimer.Interval = TimeSpan.FromMilliseconds(350);
            guardTimer.Tick += delegate { Guard(); };

            UpdateClock();
            clockTimer.Start();

        }

        public void ShowLock()
        {
            Win.Show();
            IntPtr handle = new WindowInteropHelper(Win).Handle;
            Native.AddExStyle(handle, Native.WS_EX_TOOLWINDOW);
            if (!Interactive) Native.AddExStyle(handle, Native.WS_EX_NOACTIVATE);
            Native.SetWindowPos(handle, Native.HWND_TOPMOST, screenX, screenY, screenW, screenH, Native.SWP_SHOWWINDOW);

            retired = false;
            guardTimer.Start();
            clockTimer.Start();
            if (Interactive)
            {
                Win.Activate();
                Native.SetForegroundWindow(handle);
                passwordBox.Focus();
            }
        }

        void OnClosing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (!allowClose) e.Cancel = true;
        }

        void OnPasswordChanged(object sender, RoutedEventArgs e)
        {
            placeholder.Visibility = passwordBox.Password.Length == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        void OnPasswordKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                e.Handled = true;
                FireSubmit();
            }
        }

        void OnRecoveryKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                e.Handled = true;
                FireRecovery();
            }
        }

        void FireSubmit()
        {
            if (SubmitPassword != null) SubmitPassword(passwordBox.Password);
        }

        void FireRecovery()
        {
            if (SubmitRecovery != null) SubmitRecovery(recoveryBox.Password);
        }

        void ShowRecovery(bool on)
        {
            recovering = on;
            passwordPanel.Visibility = on ? Visibility.Collapsed : Visibility.Visible;
            recoveryPanel.Visibility = on ? Visibility.Visible : Visibility.Collapsed;
            messageBlock.Text = "";
            Win.Dispatcher.BeginInvoke(DispatcherPriority.Input, new Action(FocusActiveInput));
        }

        void FocusActiveInput()
        {
            if (recovering) recoveryBox.Focus();
            else passwordBox.Focus();
        }

        public void SetMessage(string text, bool isError)
        {
            messageBlock.Text = text;
            messageBlock.Foreground = isError ? Ui.Error : Ui.Good;
        }

        public void ClearInput()
        {
            passwordBox.Password = "";
            recoveryBox.Password = "";
            FocusActiveInput();
        }

        public void SetInputEnabled(bool enabled)
        {
            passwordBox.IsEnabled = enabled;
            recoveryBox.IsEnabled = enabled;
        }

        void UpdateClock()
        {
            if (clockText.Visibility != Visibility.Visible) return;
            DateTime now = DateTime.Now;
            clockText.Text = now.ToString("HH:mm");
            dateText.Text = now.ToString("yyyy年M月d日 dddd", new System.Globalization.CultureInfo("zh-CN"));
        }

        public System.Drawing.Rectangle ScreenBounds
        {
            get { return new System.Drawing.Rectangle(screenX, screenY, screenW, screenH); }
        }

        public void Retire()
        {
            retired = true;
            guardTimer.Stop();
            clockTimer.Stop();
        }

        public void ForceGuard()
        {
            Guard();
        }

        public bool HoldsForeground()
        {
            IntPtr handle = new WindowInteropHelper(Win).Handle;
            return handle != IntPtr.Zero && Native.GetForegroundWindow() == handle;
        }

        public void Reclaim()
        {
            IntPtr handle = new WindowInteropHelper(Win).Handle;
            if (handle == IntPtr.Zero) return;
            Native.RaiseTopmost(handle);
            Native.ForceForeground(handle);
            Win.Activate();
            FocusActiveInput();
        }

        void Guard()
        {
            if (retired) return;
            try
            {
                IntPtr handle = new WindowInteropHelper(Win).Handle;
                if (handle == IntPtr.Zero) return;
                if (Native.IsIconic(handle))
                {
                    Log.Write("lock window was minimized, restoring");
                    Native.ShowWindow(handle, Native.SW_RESTORE);
                }
                else if (!Native.IsWindowVisible(handle))
                {
                    Log.Write("lock window was hidden, restoring");
                    Native.ShowWindow(handle, Native.SW_SHOW);
                }
                Native.SetWindowPos(handle, Native.HWND_TOPMOST, screenX, screenY, screenW, screenH,
                    Native.SWP_SHOWWINDOW | Native.SWP_NOACTIVATE);
                Native.RaiseTopmost(handle);
                if (!Interactive) return;
                if (!HoldsForeground())
                {
                    Log.Write("foreground reclaimed");
                    Native.ForceForeground(handle);
                    Win.Activate();
                    FocusActiveInput();
                }
            }
            catch (Exception ex)
            {
                Log.Write("guard failed: " + ex.Message);
            }
        }

        public void Close()
        {
            allowClose = true;
            clockTimer.Stop();
            guardTimer.Stop();
            try { Win.Hide(); } catch { }
            try { Win.Close(); } catch (Exception ex) { Log.Write("lock window close failed: " + ex.Message); }
        }
    }
}
