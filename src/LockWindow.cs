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

        readonly LockTheme theme;
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
            theme = Themes.Find(cfg.ThemeId);

            Win = new Window();
            Win.FontFamily = Ui.FontOf(cfg.FontFamily);
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
                blur.Radius = theme.Background.Blur;
                blur.KernelType = KernelType.Gaussian;
                img.Effect = blur;
                root.Children.Add(img);
                root.Children.Add(DimLayer(theme.Background.Dim));
            }
            else
            {
                Rectangle solid = new Rectangle();
                solid.Fill = Ui.BrushOf(theme.Background.Color, Brushes.Black);
                root.Children.Add(solid);
            }

            StackPanel column = new StackPanel();
            column.HorizontalAlignment = HorizontalAlignment.Center;
            column.VerticalAlignment = VerticalAlignment.Center;

            Brush clockBrush = Ui.BrushOf(theme.Clock.Color, Brushes.White);
            clockText = Ui.Text("", theme.Clock.Size, clockBrush, FontWeight.FromOpenTypeWeight(theme.Clock.Weight));
            clockText.FontFamily = Ui.FontOf(cfg.ClockFontName);
            clockText.HorizontalAlignment = HorizontalAlignment.Center;
            clockText.Visibility = (cfg.ShowClock && theme.Clock.Show) ? Visibility.Visible : Visibility.Collapsed;
            column.Children.Add(clockText);

            dateText = Ui.Text("", theme.Date.Size, Ui.BrushOf(theme.Date.Color, Ui.TextSecondary), FontWeights.Normal);
            dateText.HorizontalAlignment = HorizontalAlignment.Center;
            dateText.Margin = new Thickness(0, 2, 0, theme.Date.Gap);
            dateText.Visibility = clockText.Visibility;
            column.Children.Add(dateText);

            Border card = new Squircle();
            if (theme.Card.Show)
            {
                card.Width = theme.Card.Width;
                card.CornerRadius = new CornerRadius(theme.Card.Radius);
                card.Padding = new Thickness(theme.Card.PadH, theme.Card.PadV, theme.Card.PadH, theme.Card.PadV);
                card.Background = Ui.BrushOf(theme.Card.Bg, Brushes.Transparent);
                card.BorderThickness = new Thickness(1);
                card.BorderBrush = Ui.BrushOf(theme.Card.Border, Brushes.Transparent);
            }
            else
            {
                card.Padding = new Thickness(0);
                card.Background = Brushes.Transparent;
                card.BorderThickness = new Thickness(0);
            }
            if (!interactive) card.Visibility = Visibility.Collapsed;
            column.Children.Add(card);

            StackPanel body = new StackPanel();
            if (!theme.Card.Show && theme.Input.Width > 0) body.Width = theme.Input.Width;
            card.Child = body;

            if (theme.Brand.Show)
            {
                TextBlock brand = Ui.Text(theme.Brand.Text, theme.Brand.Size,
                    Ui.BrushOf(theme.Brand.Color, Ui.TextSecondary), FontWeights.SemiBold);
                brand.Margin = new Thickness(0, 0, 0, theme.Brand.Gap);
                body.Children.Add(brand);
            }

            TextBlock subtitle = Ui.Text(cfg.Subtitle, theme.Subtitle.Size,
                Ui.BrushOf(theme.Subtitle.Color, Ui.TextPrimary), FontWeights.Normal);
            subtitle.Margin = new Thickness(0, 0, 0, theme.Subtitle.Gap);
            if (!cfg.ShowSubtitle || !theme.Subtitle.Show) subtitle.Visibility = Visibility.Collapsed;
            body.Children.Add(subtitle);

            passwordBox = new PasswordBox();
            StyleLockInput(passwordBox);
            passwordBox.KeyDown += OnPasswordKeyDown;
            passwordBox.PasswordChanged += OnPasswordChanged;

            placeholder = Ui.Text("锁屏密码", Math.Max(10, theme.Input.Size - 1),
                Ui.BrushOf(theme.Input.Hint, Ui.TextSecondary), FontWeights.Normal);
            placeholder.Margin = new Thickness(InputTextInset(), 0, 0, 0);
            placeholder.VerticalAlignment = VerticalAlignment.Center;
            placeholder.IsHitTestVisible = false;

            Button unlock = LockButton("解锁");
            unlock.Click += delegate { FireSubmit(); };

            passwordPanel = new StackPanel();
            body.Children.Add(passwordPanel);

            if (theme.Inline)
            {
                Grid row = new Grid();
                row.ColumnDefinitions.Add(new ColumnDefinition());
                ColumnDefinition auto = new ColumnDefinition();
                auto.Width = GridLength.Auto;
                row.ColumnDefinitions.Add(auto);

                Grid held = new Grid();
                held.Children.Add(passwordBox);
                held.Children.Add(placeholder);
                row.Children.Add(held);

                unlock.Height = theme.Input.Height;
                unlock.Width = theme.Button.Width > 0 ? theme.Button.Width : 96;
                unlock.Margin = new Thickness(8, 0, 0, 0);
                Grid.SetColumn(unlock, 1);
                row.Children.Add(unlock);
                passwordPanel.Children.Add(row);
            }
            else
            {
                Grid held = new Grid();
                held.Children.Add(passwordBox);
                held.Children.Add(placeholder);
                passwordPanel.Children.Add(held);

                if (theme.Button.Width > 0) unlock.Width = theme.Button.Width;
                unlock.Margin = new Thickness(0, theme.Button.Gap, 0, 0);
                passwordPanel.Children.Add(unlock);
            }

            TextBlock forgot = Ui.Text("忘记密码？用恢复码解锁", theme.Link.Size,
                Ui.BrushOf(theme.Link.Color, Ui.Accent), FontWeights.Normal);
            forgot.HorizontalAlignment = HorizontalAlignment.Center;
            forgot.Margin = new Thickness(0, theme.Link.Gap, 0, 0);
            forgot.Cursor = Cursors.Hand;
            forgot.MouseLeftButtonUp += delegate { ShowRecovery(true); };
            passwordPanel.Children.Add(forgot);

            recoveryPanel = new StackPanel();
            recoveryPanel.Visibility = Visibility.Collapsed;
            body.Children.Add(recoveryPanel);

            TextBlock recHint = Ui.Text("输入 10 位恢复码（形如 XXXXX-XXXXX）",
                Math.Max(10, theme.Input.Size - 3), Ui.TextSecondary, FontWeights.Normal);
            recHint.Margin = new Thickness(0, 0, 0, 8);
            recoveryPanel.Children.Add(recHint);

            recoveryBox = new PasswordBox();
            StyleLockInput(recoveryBox);
            recoveryBox.KeyDown += OnRecoveryKeyDown;
            recoveryPanel.Children.Add(recoveryBox);

            Button recOk = LockButton("用恢复码解锁");
            recOk.Margin = new Thickness(0, theme.Button.Gap, 0, 0);
            recOk.Click += delegate { FireRecovery(); };
            recoveryPanel.Children.Add(recOk);

            TextBlock back = Ui.Text("返回密码输入", theme.Link.Size,
                Ui.BrushOf(theme.Link.Color, Ui.Accent), FontWeights.Normal);
            back.HorizontalAlignment = HorizontalAlignment.Center;
            back.Margin = new Thickness(0, theme.Link.Gap, 0, 0);
            back.Cursor = Cursors.Hand;
            back.MouseLeftButtonUp += delegate { ShowRecovery(false); };
            recoveryPanel.Children.Add(back);

            // 没有提示时不占位置，卡片底部就不会多出一块空白。
            messageBlock = Ui.Text("", 12, Ui.Error, FontWeights.Normal);
            messageBlock.Height = 18;
            messageBlock.Margin = new Thickness(0, 10, 0, 0);
            messageBlock.TextAlignment = TextAlignment.Center;
            messageBlock.Visibility = Visibility.Collapsed;
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

        FrameworkElement DimLayer(int dim)
        {
            Rectangle rect = new Rectangle();
            System.Windows.Media.Color c;
            if (Ui.TryColor(theme.Background.Color, out c))
            {
                byte alpha = (byte)Math.Max(0, Math.Min(255, dim));
                SolidColorBrush brush = new SolidColorBrush(System.Windows.Media.Color.FromArgb(alpha, c.R, c.G, c.B));
                brush.Freeze();
                rect.Fill = brush;
            }
            else
            {
                rect.Fill = Ui.Fg((byte)Math.Max(0, Math.Min(255, dim)), 8, 10, 16);
            }
            return rect;
        }

        bool Underline
        {
            get { return string.Equals(theme.Input.Kind, "underline", StringComparison.OrdinalIgnoreCase); }
        }

        double InputTextInset()
        {
            return Underline ? 0 : 13;
        }

        void StyleLockInput(PasswordBox box)
        {
            Ui.StyleInput(box, false, theme.Input.Height, theme.Input.Radius,
                Ui.BrushOf(theme.Input.Bg, Ui.InputBg),
                Ui.BrushOf(theme.Input.Border, Ui.InputBorder),
                Ui.BrushOf(theme.Input.Fg, Brushes.White),
                theme.Input.Size);
            if (Underline)
            {
                box.BorderThickness = new Thickness(0, 0, 0, 1);
                box.Background = Brushes.Transparent;
            }
        }

        Button LockButton(string text)
        {
            bool outline = string.Equals(theme.Button.Kind, "outline", StringComparison.OrdinalIgnoreCase);
            bool flat = string.Equals(theme.Button.Kind, "text", StringComparison.OrdinalIgnoreCase);
            Brush bg = flat ? Brushes.Transparent : Ui.BrushOf(theme.Button.Bg, Ui.AccentStrong);
            Brush fg = Ui.BrushOf(theme.Button.Fg, Brushes.White);
            Brush hover = string.IsNullOrEmpty(theme.Button.Hover)
                ? Ui.Shade(theme.Button.Bg, Ui.AccentHover)
                : Ui.BrushOf(theme.Button.Hover, Ui.AccentHover);
            Brush border = outline ? Ui.BrushOf(theme.Button.Border, Ui.ChipBorder) : null;
            return Ui.StyledButton(text, theme.Button.Height, theme.Button.Radius, bg, fg, hover, border, theme.Button.Size);
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
            messageBlock.Visibility = Visibility.Collapsed;
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
            messageBlock.Visibility = string.IsNullOrEmpty(text) ? Visibility.Collapsed : Visibility.Visible;
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
            dateText.Text = now.ToString(theme.DateFormat, new System.Globalization.CultureInfo("zh-CN"));
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
