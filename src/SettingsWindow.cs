using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using Microsoft.Win32;

namespace QLock
{
    public class SettingsWindow
    {
        public Window Win;

        readonly AppConfig cfg;
        readonly bool firstRun;
        readonly Action<AppConfig> onSaved;
        readonly Action onCheckUpdate;

        TextBlock statusText;
        TextBlock headerTitle;
        ScrollViewer scroller;
        string currentPane;

        PasswordBox pwd1;
        TextBox pwdReveal;
        bool revealPwd;
        Line eyeSlash;

        TextBlock recStatus;
        TextBox recOut;

        TextBox idleBox;
        TextBox subtitleBox;
        WrapPanel themeWrap;
        Border previewHost;
        readonly Dictionary<string, Button> themeButtons = new Dictionary<string, Button>();
        readonly Dictionary<string, Button> navButtons = new Dictionary<string, Button>();
        readonly Dictionary<string, FrameworkElement> panes = new Dictionary<string, FrameworkElement>();

        public SettingsWindow(AppConfig current, bool firstRun, Action<AppConfig> onSaved, Action onCheckUpdate)
        {
            cfg = current.Clone();
            wallpaperTried = false;
            wallpaper = null;
            this.firstRun = firstRun;
            this.onSaved = onSaved;
            this.onCheckUpdate = onCheckUpdate;

            Win = new Window();
            Win.Title = firstRun ? "QLock 首次设置" : "QLock 设置";
            Win.Width = 780;
            Win.Height = 640;
            Win.ResizeMode = ResizeMode.CanMinimize;
            Win.WindowStartupLocation = WindowStartupLocation.CenterScreen;
            Win.Background = Ui.Surface;
            Win.FontFamily = Ui.Font;
            Win.Foreground = Ui.TextPrimary;
            Win.Closing += OnClosing;
            Win.PreviewKeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.Key == Key.Escape) Win.Close();
            };

            Grid root = new Grid();
            root.ColumnDefinitions.Add(new ColumnDefinition());
            root.ColumnDefinitions.Add(new ColumnDefinition());
            root.ColumnDefinitions[0].Width = new GridLength(Design.NavWidth);

            Border nav = new Border();
            nav.Background = Ui.SurfaceNav;
            nav.BorderThickness = new Thickness(0, 0, 1, 0);
            nav.BorderBrush = Ui.Line;
            root.Children.Add(nav);

            StackPanel navStack = new StackPanel();
            navStack.Margin = new Thickness(Design.Gap16, Design.Gap20, Design.Gap16, Design.Gap16);
            TextBlock brand = Ui.Text("QLock", Design.FontBrand, Ui.TextPrimary, FontWeights.SemiBold);
            brand.Margin = new Thickness(Design.Gap12, 0, 0, Design.Gap16);
            navStack.Children.Add(brand);

            AddNav(navStack, "unlock", "解锁方式");
            AddNav(navStack, "look", "外观");
            AddNav(navStack, "start", "启动与锁定");
            AddNav(navStack, "hard", "加固");
            AddNav(navStack, "about", "关于");

            nav.Child = navStack;

            Grid body = new Grid();
            body.RowDefinitions.Add(new RowDefinition());
            body.RowDefinitions.Add(new RowDefinition());
            body.RowDefinitions.Add(new RowDefinition());
            body.RowDefinitions[0].Height = GridLength.Auto;
            body.RowDefinitions[2].Height = GridLength.Auto;
            Grid.SetColumn(body, 1);
            root.Children.Add(body);

            StackPanel head = new StackPanel();
            head.Margin = new Thickness(Design.PanePad, Design.Gap20, Design.PanePad, Design.Gap12);
            body.Children.Add(head);

            headerTitle = Ui.Text("", Design.FontTitle, Ui.TextPrimary, FontWeights.SemiBold);
            head.Children.Add(headerTitle);

            scroller = new ScrollViewer();
            scroller.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;
            scroller.HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled;
            scroller.Padding = new Thickness(Design.PanePad, Design.Gap4, Design.PanePad, Design.Gap8);
            Grid.SetRow(scroller, 1);
            body.Children.Add(scroller);

            StackPanel host = new StackPanel();
            scroller.Content = host;

            panes["unlock"] = BuildUnlockPane();
            panes["look"] = BuildLookPane();
            panes["start"] = BuildStartPane();
            panes["hard"] = BuildHardPane();
            panes["about"] = BuildAboutPane();
            foreach (KeyValuePair<string, FrameworkElement> kv in panes) host.Children.Add(kv.Value);

            Border footer = new Border();
            footer.Background = Ui.SurfaceFooter;
            footer.BorderThickness = new Thickness(0, 1, 0, 0);
            footer.BorderBrush = Ui.Line;
            footer.Padding = new Thickness(Design.PanePad, Design.Gap12, Design.PanePad, Design.Gap12);
            Grid.SetRow(footer, 2);
            body.Children.Add(footer);

            Grid foot = new Grid();
            foot.ColumnDefinitions.Add(new ColumnDefinition());
            foot.ColumnDefinitions.Add(new ColumnDefinition());
            foot.ColumnDefinitions[1].Width = GridLength.Auto;
            footer.Child = foot;

            statusText = Ui.Text("", Design.FontSmall, Ui.TextSecondary, FontWeights.Normal);
            statusText.VerticalAlignment = VerticalAlignment.Center;
            statusText.TextWrapping = TextWrapping.Wrap;
            foot.Children.Add(statusText);

            StackPanel footButtons = new StackPanel();
            footButtons.Orientation = Orientation.Horizontal;
            Grid.SetColumn(footButtons, 1);
            foot.Children.Add(footButtons);

            Button done = Ui.Button(firstRun ? "完成" : "关闭", true);
            done.Width = 96;
            done.Click += delegate { Win.Close(); };
            footButtons.Children.Add(done);

            Win.Content = root;
            SwitchTo("unlock");
        }

        // ---------- shell ----------

        void AddNav(StackPanel host, string id, string label)
        {
            Button b = new Button();
            b.Content = label;
            b.Height = Design.NavItemHeight;
            b.Margin = new Thickness(0, 2, 0, 2);
            b.FontSize = Design.FontBody;
            b.FontFamily = Ui.Font;
            b.Cursor = Cursors.Hand;
            b.BorderThickness = new Thickness(0);
            b.Background = Brushes.Transparent;
            b.Foreground = Ui.TextSecondary;
            b.HorizontalContentAlignment = HorizontalAlignment.Left;
            b.Template = NavTemplate();
            string target = id;
            b.Click += delegate { SwitchTo(target); };
            navButtons[id] = b;
            host.Children.Add(b);
        }

        static ControlTemplate NavTemplate()
        {
            ControlTemplate template = new ControlTemplate(typeof(Button));
            FrameworkElementFactory frame = new FrameworkElementFactory(typeof(Squircle));
            frame.SetValue(Border.CornerRadiusProperty, new CornerRadius(Design.RadiusControl));
            frame.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            frame.SetValue(Border.PaddingProperty, new Thickness(Design.Gap12, 0, Design.Gap12, 0));
            FrameworkElementFactory presenter = new FrameworkElementFactory(typeof(ContentPresenter));
            presenter.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Left);
            presenter.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            frame.AppendChild(presenter);
            template.VisualTree = frame;
            return template;
        }

        void SwitchTo(string id)
        {
            currentPane = id;
            foreach (KeyValuePair<string, FrameworkElement> kv in panes)
                kv.Value.Visibility = kv.Key == id ? Visibility.Visible : Visibility.Collapsed;
            foreach (KeyValuePair<string, Button> kv in navButtons)
            {
                bool on = kv.Key == id;
                kv.Value.Background = on ? Ui.AccentSoft : Brushes.Transparent;
                kv.Value.Foreground = on ? Ui.Accent : Ui.TextSecondary;
            }
            if (id == "unlock") headerTitle.Text = "解锁方式";
            else if (id == "look") headerTitle.Text = "外观";
            else if (id == "start") headerTitle.Text = "启动与锁定";
            else if (id == "hard") headerTitle.Text = "加固";
            else headerTitle.Text = "关于";
            if (scroller != null) scroller.ScrollToTop();
        }

        // ---------- rows ----------

        Border Row(string title, FrameworkElement control)
        {
            Grid g = new Grid();
            g.ColumnDefinitions.Add(new ColumnDefinition());
            g.ColumnDefinitions.Add(new ColumnDefinition());
            g.ColumnDefinitions[1].Width = GridLength.Auto;

            StackPanel texts = new StackPanel();
            texts.VerticalAlignment = VerticalAlignment.Center;
            texts.Margin = new Thickness(0, 0, Design.Gap16, 0);
            texts.Children.Add(Ui.Text(title, Design.FontBody, Ui.TextPrimary, FontWeights.Normal));
            g.Children.Add(texts);

            NameControl(control, title);
            control.VerticalAlignment = VerticalAlignment.Center;
            control.HorizontalAlignment = HorizontalAlignment.Right;
            Grid.SetColumn(control, 1);
            g.Children.Add(control);

            Border line = new Border();
            line.Padding = new Thickness(0, Design.Gap12, 0, Design.Gap12);
            line.BorderThickness = new Thickness(0, 0, 0, 1);
            line.BorderBrush = Ui.RowLine;
            line.Child = g;
            return line;
        }

        Border Block(string title, FrameworkElement control)
        {
            StackPanel s = new StackPanel();
            s.Children.Add(Ui.Text(title, Design.FontBody, Ui.TextPrimary, FontWeights.Normal));
            NameControl(control, title);
            control.Margin = new Thickness(0, Design.Gap12, 0, 0);
            control.HorizontalAlignment = HorizontalAlignment.Left;
            s.Children.Add(control);

            Border line = new Border();
            line.Padding = new Thickness(0, Design.Gap12, 0, Design.Gap12);
            line.BorderThickness = new Thickness(0, 0, 0, 1);
            line.BorderBrush = Ui.RowLine;
            line.Child = s;
            return line;
        }

        // 让无文字控件的无障碍名称跟随所在行标题，屏幕阅读器和 UIA 脚本都能定位。
        static void NameControl(FrameworkElement control, string title)
        {
            if (control is CheckBox || control is TextBox || control is PasswordBox)
                System.Windows.Automation.AutomationProperties.SetName(control, title);
        }

        CheckBox Toggle(bool value)
        {
            CheckBox cb = Ui.Switch(value);
            cb.HorizontalAlignment = HorizontalAlignment.Right;
            return cb;
        }

        void Bind(CheckBox cb, Action<bool> apply, string note)
        {
            cb.Checked += delegate { apply(true); SaveNow(note); };
            cb.Unchecked += delegate { apply(false); SaveNow(note); };
        }

        // ---------- unlock ----------

        FrameworkElement BuildUnlockPane()
        {
            StackPanel pane = new StackPanel();

            // 密码只存 PBKDF2-SHA256（12 万次迭代）的 Hash 和 Salt，配置文件里没有明文。
            Grid holder = new Grid();
            holder.Width = 260;
            holder.HorizontalAlignment = HorizontalAlignment.Left;

            pwd1 = new PasswordBox();
            Ui.StyleInput(pwd1);
            holder.Children.Add(pwd1);

            pwdReveal = new TextBox();
            Ui.StyleInput(pwdReveal);
            pwdReveal.Visibility = Visibility.Collapsed;
            holder.Children.Add(pwdReveal);

            Button eye = EyeButton();
            eye.HorizontalAlignment = HorizontalAlignment.Right;
            eye.VerticalAlignment = VerticalAlignment.Center;
            eye.Margin = new Thickness(0, 0, 4, 0);
            holder.Children.Add(eye);

            StackPanel pwdRow = new StackPanel();
            pwdRow.Orientation = Orientation.Horizontal;
            pwdRow.Children.Add(holder);

            Button apply = Ui.Button("设置", false);
            apply.Width = 72;
            apply.Margin = new Thickness(Design.Gap8, 0, 0, 0);
            apply.Click += delegate { ApplyPassword(); };
            pwdRow.Children.Add(apply);

            pane.Children.Add(Row("解锁密码", pwdRow));

            recStatus = Ui.Text(!string.IsNullOrEmpty(cfg.RecoveryHash) ? "已设置恢复码。" : "还没有恢复码。",
                Design.FontSmall, Ui.TextSecondary, FontWeights.Normal);

            recOut = new TextBox();
            recOut.IsReadOnly = true;
            recOut.Width = 200;
            recOut.FontFamily = new FontFamily("Consolas, Microsoft YaHei UI");
            Ui.StyleInput(recOut);
            recOut.Visibility = Visibility.Collapsed;
            recOut.Margin = new Thickness(0, Design.Gap8, 0, 0);

            Button genRec = Ui.Button("生成新的恢复码", false);
            genRec.Width = 150;
            genRec.Margin = new Thickness(0, Design.Gap8, 0, 0);
            genRec.Click += delegate { GenerateRecovery(); };

            StackPanel recStack = new StackPanel();
            recStack.Children.Add(recStatus);
            recStack.Children.Add(genRec);
            recStack.Children.Add(recOut);

            pane.Children.Add(Row("恢复码", recStack));
            return pane;
        }

        void ApplyPassword()
        {
            string p = CurrentPassword();
            if (p.Length < 4)
            {
                SetStatus("密码至少 4 位", true);
                return;
            }
            Auth.SetPassword(cfg, p);
            SaveNow("密码已更新");
            pwd1.Password = "";
            pwdReveal.Text = "";
        }

        void GenerateRecovery()
        {
            string code = Auth.NewRecoveryCode();
            Auth.SetRecovery(cfg, code);
            recOut.Text = code;
            recOut.Visibility = Visibility.Visible;
            recStatus.Text = "新恢复码已生成，也复制到剪贴板了。";
            recStatus.Foreground = Ui.Good;
            try { Clipboard.SetText(code); } catch { }
            SaveNow("恢复码已更新");
        }

        // ---------- look ----------

        FrameworkElement BuildLookPane()
        {
            StackPanel pane = new StackPanel();

            pane.Children.Add(BuildPreviewArea());

            themeWrap = new WrapPanel();
            themeWrap.Margin = new Thickness(0, -4, 0, 0);
            PaintThemes();

            pane.Children.Add(Block("锁屏主题", themeWrap));

            CheckBox clock = Toggle(cfg.ShowClock);
            Bind(clock, delegate(bool on) { cfg.ShowClock = on; RefreshPreview(); }, "已保存，下次锁屏生效");
            pane.Children.Add(Row("显示时钟和日期", clock));

            CheckBox snapshot = Toggle(cfg.BackgroundSnapshot);
            Bind(snapshot, delegate(bool on) { cfg.BackgroundSnapshot = on; RefreshPreview(); }, "已保存，下次锁屏生效");
            pane.Children.Add(Row("用屏幕截图做模糊背景", snapshot));

            subtitleBox = new TextBox();
            subtitleBox.Width = 190;
            subtitleBox.Text = cfg.Subtitle;
            subtitleBox.Margin = new Thickness(Design.Gap12, 0, 0, 0);
            Ui.StyleInput(subtitleBox);
            subtitleBox.IsEnabled = cfg.ShowSubtitle;
            subtitleBox.LostFocus += delegate { CommitSubtitle(); };
            subtitleBox.TextChanged += delegate { RefreshPreview(); };
            subtitleBox.KeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.Key == Key.Enter) { CommitSubtitle(); }
            };

            CheckBox showSubtitle = Toggle(cfg.ShowSubtitle);
            showSubtitle.HorizontalAlignment = HorizontalAlignment.Left;
            NameControl(showSubtitle, "显示提示文字");
            Bind(showSubtitle, delegate(bool on)
            {
                cfg.ShowSubtitle = on;
                subtitleBox.IsEnabled = on;
                RefreshPreview();
            }, "已保存，下次锁屏生效");

            StackPanel subtitleRow = new StackPanel();
            subtitleRow.Orientation = Orientation.Horizontal;
            subtitleRow.Children.Add(showSubtitle);
            subtitleRow.Children.Add(subtitleBox);
            pane.Children.Add(Row("提示文字", subtitleRow));

            return pane;
        }

        FrameworkElement BuildPreviewArea()
        {
            previewHost = new Squircle();
            previewHost.Height = 216;
            previewHost.CornerRadius = new CornerRadius(Design.RadiusCard);
            previewHost.BorderThickness = new Thickness(1);
            previewHost.BorderBrush = Ui.Line;
            previewHost.ClipToBounds = true;
            previewHost.Margin = new Thickness(0, Design.Gap4, 0, Design.Gap12);
            RefreshPreview();
            return previewHost;
        }

        // 预览按锁屏窗口的同一套字段和顺序拼出来，只改尺寸来源，保证所见接近锁屏本身。
        void RefreshPreview()
        {
            if (previewHost == null) return;
            LockTheme t = Themes.Find(cfg.ThemeId);

            Grid root = new Grid();
            root.Width = 960;
            root.Height = 408;
            root.ClipToBounds = true;
            root.Background = Ui.BrushOf(t.Background.Color, Brushes.Black);

            if (cfg.BackgroundSnapshot)
            {
                // 预览拿当前桌面壁纸顶替：真锁屏截的是锁那一刻的屏幕，壁纸就是最接近的东西。
                ImageSource bg = Wallpaper();
                if (bg != null)
                {
                    Image image = new Image();
                    image.Source = bg;
                    image.Stretch = Stretch.UniformToFill;
                    BlurEffect blur = new BlurEffect();
                    blur.Radius = Math.Max(0, t.Background.Blur * 0.5);
                    blur.KernelType = KernelType.Gaussian;
                    image.Effect = blur;
                    image.Margin = new Thickness(-blur.Radius * 2);
                    Border frame = new Border();
                    frame.ClipToBounds = true;
                    frame.Child = image;
                    root.Children.Add(frame);
                }
                else
                {
                    root.Background = new LinearGradientBrush(Color.FromRgb(44, 58, 84), Color.FromRgb(12, 15, 22), 40);
                }
                Color baseColor;
                if (Ui.TryColor(t.Background.Color, out baseColor))
                {
                    byte alpha = (byte)Math.Max(0, Math.Min(255, t.Background.Dim));
                    Rectangle dim = new Rectangle();
                    dim.Fill = new SolidColorBrush(Color.FromArgb(alpha, baseColor.R, baseColor.G, baseColor.B));
                    root.Children.Add(dim);
                }
            }

            StackPanel column = new StackPanel();
            column.HorizontalAlignment = HorizontalAlignment.Center;
            column.VerticalAlignment = VerticalAlignment.Center;

            bool showClock = cfg.ShowClock && t.Clock.Show;
            TextBlock clock = Ui.Text(DateTime.Now.ToString("HH:mm"), t.Clock.Size,
                Ui.BrushOf(t.Clock.Color, Brushes.White), FontWeight.FromOpenTypeWeight(t.Clock.Weight));
            clock.HorizontalAlignment = HorizontalAlignment.Center;
            clock.Visibility = showClock ? Visibility.Visible : Visibility.Collapsed;
            column.Children.Add(clock);

            TextBlock date = Ui.Text(DateTime.Now.ToString(t.DateFormat, new CultureInfo("zh-CN")), t.Date.Size,
                Ui.BrushOf(t.Date.Color, Ui.TextSecondary), FontWeights.Normal);
            date.HorizontalAlignment = HorizontalAlignment.Center;
            date.Margin = new Thickness(0, 2, 0, t.Date.Gap);
            date.Visibility = clock.Visibility;
            column.Children.Add(date);

            Border card = new Squircle();
            if (t.Card.Show)
            {
                card.Width = t.Card.Width;
                card.CornerRadius = new CornerRadius(t.Card.Radius);
                card.Padding = new Thickness(t.Card.PadH, t.Card.PadV, t.Card.PadH, t.Card.PadV);
                card.Background = Ui.BrushOf(t.Card.Bg, Brushes.Transparent);
                card.BorderThickness = new Thickness(1);
                card.BorderBrush = Ui.BrushOf(t.Card.Border, Brushes.Transparent);
            }
            else
            {
                card.Padding = new Thickness(0);
                card.Background = Brushes.Transparent;
                card.BorderThickness = new Thickness(0);
            }
            column.Children.Add(card);

            StackPanel body = new StackPanel();
            if (!t.Card.Show && t.Input.Width > 0) body.Width = t.Input.Width;
            card.Child = body;

            if (t.Brand.Show)
            {
                TextBlock brand = Ui.Text(t.Brand.Text, t.Brand.Size,
                    Ui.BrushOf(t.Brand.Color, Ui.TextSecondary), FontWeights.SemiBold);
                brand.Margin = new Thickness(0, 0, 0, t.Brand.Gap);
                body.Children.Add(brand);
            }

            string subtitle = subtitleBox == null ? cfg.Subtitle : subtitleBox.Text;
            if (subtitle == null) subtitle = "";
            subtitle = subtitle.Trim();
            if (subtitle.Length == 0) subtitle = "输入密码解锁";
            TextBlock sub = Ui.Text(subtitle, t.Subtitle.Size,
                Ui.BrushOf(t.Subtitle.Color, Ui.TextPrimary), FontWeights.Normal);
            sub.Margin = new Thickness(0, 0, 0, t.Subtitle.Gap);
            if (!cfg.ShowSubtitle || !t.Subtitle.Show) sub.Visibility = Visibility.Collapsed;
            body.Children.Add(sub);

            bool underline = string.Equals(t.Input.Kind, "underline", StringComparison.OrdinalIgnoreCase);
            Border input = new Squircle();
            input.Height = t.Input.Height;
            if (underline)
            {
                input.Background = Brushes.Transparent;
                input.BorderThickness = new Thickness(0, 0, 0, 1);
                input.BorderBrush = Ui.BrushOf(t.Input.Border, Ui.InputBorder);
            }
            else
            {
                input.Background = Ui.BrushOf(t.Input.Bg, Ui.InputBg);
                input.CornerRadius = new CornerRadius(t.Input.Radius);
                input.BorderThickness = new Thickness(1);
                input.BorderBrush = Ui.BrushOf(t.Input.Border, Ui.InputBorder);
            }
            TextBlock hint = Ui.Text("锁屏密码", Math.Max(10, t.Input.Size - 1),
                Ui.BrushOf(t.Input.Hint, Ui.TextSecondary), FontWeights.Normal);
            hint.VerticalAlignment = VerticalAlignment.Center;
            hint.Margin = new Thickness(underline ? 0 : 13, 0, 0, 0);
            input.Child = hint;

            bool outline = string.Equals(t.Button.Kind, "outline", StringComparison.OrdinalIgnoreCase);
            bool flat = string.Equals(t.Button.Kind, "text", StringComparison.OrdinalIgnoreCase);
            Brush btnBg = flat ? Brushes.Transparent : Ui.BrushOf(t.Button.Bg, Ui.AccentStrong);
            Brush btnFg = Ui.BrushOf(t.Button.Fg, Brushes.White);
            Brush btnHover = string.IsNullOrEmpty(t.Button.Hover)
                ? Ui.Shade(t.Button.Bg, Ui.AccentHover)
                : Ui.BrushOf(t.Button.Hover, Ui.AccentHover);
            Brush btnBorder = outline ? Ui.BrushOf(t.Button.Border, Ui.ChipBorder) : null;
            Button unlock = Ui.StyledButton("解锁", t.Button.Height, t.Button.Radius, btnBg, btnFg, btnHover, btnBorder, t.Button.Size);
            unlock.Focusable = false;
            unlock.IsHitTestVisible = false;

            if (t.Inline)
            {
                Grid row = new Grid();
                row.ColumnDefinitions.Add(new ColumnDefinition());
                ColumnDefinition auto = new ColumnDefinition();
                auto.Width = GridLength.Auto;
                row.ColumnDefinitions.Add(auto);
                row.Children.Add(input);
                unlock.Height = t.Input.Height;
                unlock.Width = t.Button.Width > 0 ? t.Button.Width : 96;
                unlock.Margin = new Thickness(8, 0, 0, 0);
                Grid.SetColumn(unlock, 1);
                row.Children.Add(unlock);
                body.Children.Add(row);
            }
            else
            {
                body.Children.Add(input);
                if (t.Button.Width > 0) unlock.Width = t.Button.Width;
                unlock.Margin = new Thickness(0, t.Button.Gap, 0, 0);
                body.Children.Add(unlock);
            }

            TextBlock forgot = Ui.Text("忘记密码？用恢复码解锁", t.Link.Size,
                Ui.BrushOf(t.Link.Color, Ui.Accent), FontWeights.Normal);
            forgot.HorizontalAlignment = HorizontalAlignment.Center;
            forgot.Margin = new Thickness(0, t.Link.Gap, 0, 0);
            body.Children.Add(forgot);

            root.Children.Add(column);

            Viewbox view = new Viewbox();
            view.Stretch = Stretch.Uniform;
            view.Child = root;
            previewHost.Child = view;
        }

        static ImageSource wallpaper;
        static bool wallpaperTried;

        // 取当前桌面壁纸：优先 Windows 转码后的那张（就是屏幕上显示的样子），没有再用注册表里的原图。
        static ImageSource Wallpaper()
        {
            if (wallpaperTried) return wallpaper;
            wallpaperTried = true;
            try
            {
                string path = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "Microsoft", "Windows", "Themes", "TranscodedWallpaper");
                if (!System.IO.File.Exists(path))
                    path = Registry.GetValue("HKEY_CURRENT_USER\\Control Panel\\Desktop", "WallPaper", null) as string;
                if (string.IsNullOrEmpty(path) || !System.IO.File.Exists(path)) return null;
                BitmapImage image = new BitmapImage();
                image.BeginInit();
                image.CacheOption = BitmapCacheOption.OnLoad;
                image.DecodePixelWidth = 960;
                image.UriSource = new Uri(path, UriKind.Absolute);
                image.EndInit();
                image.Freeze();
                wallpaper = image;
            }
            catch (Exception ex)
            {
                Log.Write("preview wallpaper failed: " + ex.Message);
            }
            return wallpaper;
        }

        void CommitSubtitle()
        {
            string text = subtitleBox.Text == null ? "" : subtitleBox.Text.Trim();
            if (text.Length == 0) text = "输入密码解锁";
            if (text == cfg.Subtitle) { subtitleBox.Text = text; return; }
            cfg.Subtitle = text;
            subtitleBox.Text = text;
            RefreshPreview();
            SaveNow("已保存，下次锁屏生效");
        }

        void PaintThemes()
        {
            if (themeWrap == null) return;
            themeWrap.Children.Clear();
            themeButtons.Clear();
            List<LockTheme> all = Themes.All();
            foreach (LockTheme t in all)
            {
                bool on = t.Id == cfg.ThemeId;
                Brush bg = on ? Ui.Accent : Ui.ChipSurface;
                Brush fg = on ? Brushes.White : Ui.TextPrimary;
                Button b = Ui.StyledButton(t.Name, Design.ChipHeight, Design.ChipHeight / 2, bg, fg, Ui.ChipBorder, null, 12.5);
                b.Width = 118;
                b.Margin = new Thickness(0, 4, 8, 0);
                string id = t.Id;
                b.Click += delegate { PickTheme(id); };
                themeButtons[id] = b;
                themeWrap.Children.Add(b);
            }

            Button folder = Ui.StyledButton("主题文件夹", Design.ChipHeight, Design.ChipHeight / 2, Ui.ChipSurface, Ui.TextPrimary, Ui.ChipBorder, null, 12.5);
            folder.Width = 118;
            folder.Margin = new Thickness(0, 4, 8, 0);
            folder.Click += delegate { OpenThemeFolder(); };
            themeWrap.Children.Add(folder);
        }

        void PickTheme(string id)
        {
            if (id == cfg.ThemeId) return;
            cfg.ThemeId = id;
            PaintThemes();
            RefreshPreview();
            SaveNow("已保存，下次锁屏生效");
        }

        void OpenThemeFolder()
        {
            try
            {
                Themes.EnsureSampleThemes();
                Process.Start("explorer.exe", Themes.Dir);
            }
            catch (Exception ex)
            {
                SetStatus("打不开主题文件夹：" + ex.Message, true);
            }
        }

        // ---------- start ----------

        FrameworkElement BuildStartPane()
        {
            StackPanel pane = new StackPanel();

            CheckBox winL = Toggle(cfg.InterceptWinL);
            Bind(winL, delegate(bool on) { cfg.InterceptWinL = on; }, "已保存");
            pane.Children.Add(Row("接管 Win+L", winL));

            CheckBox hotkey = Toggle(cfg.CustomHotkeyEnabled);
            Bind(hotkey, delegate(bool on) { cfg.CustomHotkeyEnabled = on; }, "已保存");
            pane.Children.Add(Row("Ctrl+Alt+L 热键", hotkey));

            CheckBox onStart = Toggle(cfg.LockOnStartup);
            Bind(onStart, delegate(bool on) { cfg.LockOnStartup = on; }, "已保存");
            pane.Children.Add(Row("程序启动后立即锁定", onStart));

            idleBox = new TextBox();
            idleBox.Width = 70;
            idleBox.Text = cfg.IdleLockMinutes.ToString();
            idleBox.TextAlignment = TextAlignment.Center;
            Ui.StyleInput(idleBox);
            idleBox.LostFocus += delegate { CommitIdle(); };
            idleBox.KeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.Key == Key.Enter) { CommitIdle(); }
            };
            StackPanel idleRow = new StackPanel();
            idleRow.Orientation = Orientation.Horizontal;
            idleRow.Children.Add(idleBox);
            TextBlock unit = Ui.Label("　分钟（0 = 不锁）");
            unit.VerticalAlignment = VerticalAlignment.Center;
            idleRow.Children.Add(unit);
            pane.Children.Add(Row("空闲自动锁定", idleRow));

            CheckBox autostart = Toggle(cfg.Autostart == "runkey" || cfg.Autostart == "task");
            autostart.Checked += delegate
            {
                cfg.Autostart = "runkey";
                SaveNow("已保存");
                ApplyAutostart(true);
            };
            autostart.Unchecked += delegate
            {
                cfg.Autostart = "none";
                SaveNow("已保存");
                ApplyAutostart(false);
            };
            pane.Children.Add(Row("登录时自动启动", autostart));

            CheckBox elevated = Toggle(cfg.RunElevated);
            Bind(elevated, delegate(bool on) { cfg.RunElevated = on; }, "已保存，下次启动生效");
            pane.Children.Add(Row("以管理员身份启动", elevated));

            return pane;
        }

        void CommitIdle()
        {
            string raw = idleBox.Text == null ? "" : idleBox.Text.Trim();
            int minutes;
            if (!int.TryParse(raw, out minutes) || minutes < 0 || minutes > 600)
            {
                SetStatus("空闲分钟数要填 0 到 600 之间的整数", true);
                idleBox.Text = cfg.IdleLockMinutes.ToString();
                return;
            }
            if (minutes == cfg.IdleLockMinutes) return;
            cfg.IdleLockMinutes = minutes;
            SaveNow("已保存");
        }

        void ApplyAutostart(bool on)
        {
            string err;
            bool ok = Autostart.Apply(on, out err);
            if (!ok) SetStatus("设置存下了，但启动项没写成功：" + (err == null ? "未知原因" : err), true);
        }

        // ---------- hard ----------

        FrameworkElement BuildHardPane()
        {
            StackPanel pane = new StackPanel();

            CheckBox block = Toggle(cfg.BlockSystemHotkeys);
            Bind(block, delegate(bool on) { cfg.BlockSystemHotkeys = on; }, "已保存");
            pane.Children.Add(Row("锁定时拦截系统热键", block));

            return pane;
        }

        // ---------- about ----------

        FrameworkElement BuildAboutPane()
        {
            StackPanel pane = new StackPanel();

            TextBlock ver = Ui.Text(Updater.VersionText(Updater.CurrentVersion()), Design.FontBody, Ui.TextPrimary, FontWeights.Normal);
            ver.VerticalAlignment = VerticalAlignment.Center;

            StackPanel verRow = new StackPanel();
            verRow.Orientation = Orientation.Horizontal;
            verRow.Children.Add(ver);

            Button check = Ui.Button("检查更新", false);
            check.Width = 100;
            check.Margin = new Thickness(Design.Gap12, 0, 0, 0);
            check.Click += delegate { if (onCheckUpdate != null) onCheckUpdate(); };
            verRow.Children.Add(check);

            pane.Children.Add(Row("版本", verRow));

            string files = "配置：" + ConfigStore.FilePath + "\n" +
                           "日志：" + Log.FilePath + "\n" +
                           "主题：" + Themes.Dir;
            TextBlock loc = Ui.Text(files, Design.FontTiny, Ui.TextSecondary, FontWeights.Normal);
            loc.TextWrapping = TextWrapping.Wrap;
            pane.Children.Add(Block("文件位置", loc));

            return pane;
        }

        // ---------- password eye ----------

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

        // ---------- save ----------

        void SaveNow(string note)
        {
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
            SetStatus(string.IsNullOrEmpty(note) ? "已保存" : note, false);
        }

        void SetStatus(string text, bool error)
        {
            if (statusText == null) return;
            statusText.Text = text;
            statusText.Foreground = error ? Ui.Error : Ui.Good;
        }

        void OnClosing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (cfg.HasPassword) return;
            e.Cancel = true;
            SwitchTo("unlock");
            SetStatus("先设一个解锁密码，不然锁上就打不开了", true);
        }
    }
}
