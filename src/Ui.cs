using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;

namespace QLock
{
    public static class Ui
    {
        public static readonly FontFamily Font = new FontFamily("Microsoft YaHei UI, Segoe UI");

        // 配置里存的是字体名；空或者系统里没有这个名字就回落默认，不让一个坏值把界面搞崩。
        public static FontFamily FontOf(string name)
        {
            if (name == null || name.Trim().Length == 0) return Font;
            try
            {
                return new FontFamily(name);
            }
            catch (Exception)
            {
                return Font;
            }
        }

        // 深色滚动条：默认那个浅灰带箭头的样子放在深色窗口里太扎眼。
        public static Style SlimScrollBar()
        {
            const string xaml =
                "<Style xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'"
                + " xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' TargetType='ScrollBar'>"
                + "<Setter Property='Width' Value='10'/>"
                + "<Setter Property='Background' Value='Transparent'/>"
                + "<Setter Property='Template'><Setter.Value>"
                + "<ControlTemplate TargetType='ScrollBar'><Grid Background='Transparent'>"
                + "<Track x:Name='PART_Track' IsDirectionReversed='true'>"
                + "<Track.DecreaseRepeatButton><RepeatButton Command='ScrollBar.PageUpCommand' Opacity='0' Focusable='False' IsTabStop='False'/></Track.DecreaseRepeatButton>"
                + "<Track.Thumb><Thumb><Thumb.Template><ControlTemplate TargetType='Thumb'>"
                + "<Border x:Name='bd' Background='#44FFFFFF' CornerRadius='4' Margin='2,0,2,0'/>"
                + "<ControlTemplate.Triggers><Trigger Property='IsMouseOver' Value='True'><Setter TargetName='bd' Property='Background' Value='#88FFFFFF'/></Trigger></ControlTemplate.Triggers>"
                + "</ControlTemplate></Thumb.Template></Thumb></Track.Thumb>"
                + "<Track.IncreaseRepeatButton><RepeatButton Command='ScrollBar.PageDownCommand' Opacity='0' Focusable='False' IsTabStop='False'/></Track.IncreaseRepeatButton>"
                + "</Track></Grid></ControlTemplate>"
                + "</Setter.Value></Setter></Style>";
            return (Style)System.Windows.Markup.XamlReader.Parse(xaml);
        }

        // 深色滑块：4px 轨道，走过的部分用主色，拇指是白圆点。
        public static Slider Slider(double min, double max, double value, Action<double> onChanged)
        {
            const string xaml =
                "<ControlTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'"
                + " xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' TargetType='Slider'>"
                + "<Grid Height='18' Background='Transparent'>"
                + "<Border Height='4' CornerRadius='2' Background='#1EFFFFFF' VerticalAlignment='Center'/>"
                + "<Track x:Name='PART_Track' VerticalAlignment='Center'>"
                + "<Track.DecreaseRepeatButton><RepeatButton Command='Slider.DecreaseLarge' Focusable='False' IsTabStop='False'>"
                + "<RepeatButton.Template><ControlTemplate TargetType='RepeatButton'><Border Height='4' CornerRadius='2' Background='#3A76E8'/></ControlTemplate></RepeatButton.Template>"
                + "</RepeatButton></Track.DecreaseRepeatButton>"
                + "<Track.Thumb><Thumb Width='14' Height='14'>"
                + "<Thumb.Template><ControlTemplate TargetType='Thumb'><Ellipse Width='14' Height='14' Fill='#FFFFFF'/></ControlTemplate></Thumb.Template>"
                + "</Thumb></Track.Thumb>"
                + "<Track.IncreaseRepeatButton><RepeatButton Command='Slider.IncreaseLarge' Focusable='False' IsTabStop='False'>"
                + "<RepeatButton.Template><ControlTemplate TargetType='RepeatButton'><Border Background='Transparent'/></ControlTemplate></RepeatButton.Template>"
                + "</RepeatButton></Track.IncreaseRepeatButton>"
                + "</Track></Grid></ControlTemplate>";

            Slider s = new Slider();
            s.Minimum = min;
            s.Maximum = max;
            s.Value = value;
            s.Width = 210;
            s.Height = 18;
            s.IsMoveToPointEnabled = true;
            s.Template = (ControlTemplate)System.Windows.Markup.XamlReader.Parse(xaml);
            if (onChanged != null)
            {
                s.ValueChanged += delegate(object sender, RoutedPropertyChangedEventArgs<double> e) { onChanged(e.NewValue); };
            }
            return s;
        }

        public static SolidColorBrush Fg(byte a, byte r, byte g, byte b)
        {
            SolidColorBrush br = new SolidColorBrush(Color.FromArgb(a, r, g, b));
            br.Freeze();
            return br;
        }

        public static readonly Brush TextPrimary = Fg(255, 232, 238, 247);
        public static readonly Brush TextSecondary = Fg(160, 180, 195, 215);
        public static readonly Brush Accent = Fg(255, 96, 150, 250);
        public static readonly Brush Error = Fg(255, 255, 125, 125);
        public static readonly Brush Good = Fg(255, 130, 210, 140);

        // 强调色的几个档位：实心按钮、悬停、按下、导航选中底色
        public static readonly Brush AccentStrong = Fg(255, 58, 118, 232);
        public static readonly Brush AccentHover = Fg(255, 78, 138, 250);
        public static readonly Brush AccentPressed = Fg(255, 42, 96, 200);
        public static readonly Brush AccentSoft = Fg(46, 96, 150, 250);

        public static readonly Brush Surface = Fg(255, 24, 27, 35);
        public static readonly Brush SurfaceNav = Fg(255, 18, 21, 28);
        public static readonly Brush SurfaceFooter = Fg(255, 20, 23, 30);
        public static readonly Brush Line = Fg(30, 255, 255, 255);
        public static readonly Brush RowLine = Fg(24, 255, 255, 255);
        public static readonly Brush InputBg = Fg(30, 255, 255, 255);
        public static readonly Brush InputBorder = Fg(80, 255, 255, 255);
        public static readonly Brush ChipSurface = Fg(35, 255, 255, 255);
        public static readonly Brush ChipBorder = Fg(90, 255, 255, 255);
        public static readonly Brush SwitchTrack = Fg(50, 255, 255, 255);

        public static TextBlock Text(string text, double size, Brush brush, FontWeight weight)
        {
            TextBlock tb = new TextBlock();
            tb.Text = text;
            tb.FontSize = size;
            tb.Foreground = brush;
            tb.FontWeight = weight;
            return tb;
        }

        public static TextBlock Label(string text)
        {
            return Text(text, Design.FontBody, TextSecondary, FontWeights.Normal);
        }

        public static void StyleInput(Control c)
        {
            StyleInput(c, false, Design.ControlHeight, Design.RadiusControl, InputBg, InputBorder, Brushes.White, Design.FontControl);
        }

        public static void StyleInput(Control c, bool multiline)
        {
            StyleInput(c, multiline, 34, 7, Fg(30, 255, 255, 255), Fg(80, 255, 255, 255), Brushes.White, 14);
        }

        public static void StyleInput(Control c, bool multiline, double height, double radius, Brush background, Brush borderBrush, Brush foreground, double fontSize)
        {
            c.FontSize = fontSize;
            c.Foreground = foreground;
            c.BorderThickness = new Thickness(1);
            c.BorderBrush = borderBrush;
            c.Background = background;

            ControlTemplate template = new ControlTemplate(c.GetType());
            FrameworkElementFactory border = new FrameworkElementFactory(typeof(Border));
            border.Name = "bg";
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(radius));
            border.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            border.SetValue(Border.BorderBrushProperty, new TemplateBindingExtension(Control.BorderBrushProperty));
            border.SetValue(Border.BorderThicknessProperty, new TemplateBindingExtension(Control.BorderThicknessProperty));
            FrameworkElementFactory host = new FrameworkElementFactory(typeof(ScrollViewer));
            host.Name = "PART_ContentHost";
            host.SetValue(ScrollViewer.VerticalAlignmentProperty, multiline ? VerticalAlignment.Stretch : VerticalAlignment.Center);
            host.SetValue(ScrollViewer.HorizontalScrollBarVisibilityProperty, ScrollBarVisibility.Hidden);
            host.SetValue(ScrollViewer.VerticalScrollBarVisibilityProperty, multiline ? ScrollBarVisibility.Auto : ScrollBarVisibility.Hidden);
            host.SetValue(FrameworkElement.MarginProperty, multiline ? new Thickness(8, 6, 8, 6) : new Thickness(10, 0, 10, 0));
            host.SetValue(UIElement.FocusableProperty, false);
            border.AppendChild(host);
            template.VisualTree = border;

            // 禁用时整块压暗，不然自定义模板看不出动静。
            Trigger off = new Trigger();
            off.Property = UIElement.IsEnabledProperty;
            off.Value = false;
            off.Setters.Add(new Setter(UIElement.OpacityProperty, 0.4, "bg"));
            template.Triggers.Add(off);

            c.Template = template;
            if (!multiline) c.Height = height;
        }

        public static Button Button(string text, bool primary)
        {
            Button b = new Button();
            b.Content = text;
            b.Height = Design.ControlHeight;
            b.FontSize = Design.FontControl;
            b.Cursor = Cursors.Hand;
            b.Foreground = Brushes.White;
            b.BorderThickness = new Thickness(0);
            b.Background = primary ? AccentStrong : ChipSurface;

            ControlTemplate template = new ControlTemplate(typeof(Button));
            FrameworkElementFactory border = new FrameworkElementFactory(typeof(Border));
            border.Name = "bg";
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(Design.RadiusControl));
            border.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            FrameworkElementFactory presenter = new FrameworkElementFactory(typeof(ContentPresenter));
            presenter.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            presenter.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            border.AppendChild(presenter);
            template.VisualTree = border;

            Trigger hover = new Trigger();
            hover.Property = UIElement.IsMouseOverProperty;
            hover.Value = true;
            hover.Setters.Add(new Setter(Border.BackgroundProperty, AccentHover, "bg"));
            template.Triggers.Add(hover);

            Trigger pressed = new Trigger();
            pressed.Property = ButtonBase.IsPressedProperty;
            pressed.Value = true;
            pressed.Setters.Add(new Setter(Border.BackgroundProperty, AccentPressed, "bg"));
            template.Triggers.Add(pressed);

            b.Template = template;
            return b;
        }

        public static Button StyledButton(string text, double height, double radius, Brush background, Brush foreground, Brush hover, Brush border, double fontSize)
        {
            Button b = new Button();
            b.Content = text;
            b.Height = height;
            b.FontSize = fontSize;
            b.Cursor = Cursors.Hand;
            b.Foreground = foreground;
            b.BorderThickness = new Thickness(border == null ? 0 : 1);
            b.BorderBrush = border == null ? Brushes.Transparent : border;
            b.Background = background;

            ControlTemplate template = new ControlTemplate(typeof(Button));
            FrameworkElementFactory frame = new FrameworkElementFactory(typeof(Border));
            frame.Name = "bg";
            frame.SetValue(Border.CornerRadiusProperty, new CornerRadius(radius));
            frame.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            frame.SetValue(Border.BorderBrushProperty, new TemplateBindingExtension(Control.BorderBrushProperty));
            frame.SetValue(Border.BorderThicknessProperty, new TemplateBindingExtension(Control.BorderThicknessProperty));
            FrameworkElementFactory presenter = new FrameworkElementFactory(typeof(ContentPresenter));
            presenter.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            presenter.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            frame.AppendChild(presenter);
            template.VisualTree = frame;

            if (hover != null)
            {
                Trigger over = new Trigger();
                over.Property = UIElement.IsMouseOverProperty;
                over.Value = true;
                over.Setters.Add(new Setter(Border.BackgroundProperty, hover, "bg"));
                template.Triggers.Add(over);
            }

            Trigger off = new Trigger();
            off.Property = UIElement.IsEnabledProperty;
            off.Value = false;
            off.Setters.Add(new Setter(UIElement.OpacityProperty, 0.4, "bg"));
            template.Triggers.Add(off);

            b.Template = template;
            return b;
        }

        public static bool TryColor(string hex, out Color color)
        {
            color = Colors.Black;
            if (string.IsNullOrEmpty(hex)) return false;
            string s = hex.Trim();
            if (s.Length > 0 && s[0] == '#') s = s.Substring(1);
            if (s.Length != 6 && s.Length != 8) return false;
            try
            {
                if (s.Length == 8)
                {
                    color = Color.FromArgb(
                        Convert.ToByte(s.Substring(0, 2), 16),
                        Convert.ToByte(s.Substring(2, 2), 16),
                        Convert.ToByte(s.Substring(4, 2), 16),
                        Convert.ToByte(s.Substring(6, 2), 16));
                }
                else
                {
                    color = Color.FromArgb(255,
                        Convert.ToByte(s.Substring(0, 2), 16),
                        Convert.ToByte(s.Substring(2, 2), 16),
                        Convert.ToByte(s.Substring(4, 2), 16));
                }
                return true;
            }
            catch
            {
                return false;
            }
        }

        public static Brush BrushOf(string hex, Brush fallback)
        {
            Color c;
            if (!TryColor(hex, out c)) return fallback;
            SolidColorBrush b = new SolidColorBrush(c);
            b.Freeze();
            return b;
        }

        public static Brush Shade(string hex, Brush fallback)
        {
            Color c;
            if (!TryColor(hex, out c)) return fallback;
            byte a = c.A;
            int r = c.R, g = c.G, b = c.B;
            if (a < 255)
            {
                a = (byte)Math.Min(255, a + 40);
                r = Math.Min(255, r + 30);
                g = Math.Min(255, g + 30);
                b = Math.Min(255, b + 30);
            }
            else
            {
                r = Math.Min(255, r + 22);
                g = Math.Min(255, g + 22);
                b = Math.Min(255, b + 22);
            }
            SolidColorBrush br = new SolidColorBrush(Color.FromArgb(a, (byte)r, (byte)g, (byte)b));
            br.Freeze();
            return br;
        }

        // 开关滑块：保留 CheckBox 的语义（UIA 里仍是勾选控件、空格能切换），只换外观。
        public static CheckBox Switch(bool value)
        {
            CheckBox cb = new CheckBox();
            cb.IsChecked = value;
            cb.Cursor = Cursors.Hand;
            cb.Foreground = Brushes.White;
            cb.Background = SwitchTrack;
            cb.BorderBrush = Ui.Fg(40, 255, 255, 255);
            cb.BorderThickness = new Thickness(1);
            cb.VerticalAlignment = VerticalAlignment.Center;

            ControlTemplate template = new ControlTemplate(typeof(CheckBox));
            FrameworkElementFactory track = new FrameworkElementFactory(typeof(Border));
            track.Name = "track";
            track.SetValue(FrameworkElement.WidthProperty, 40.0);
            track.SetValue(FrameworkElement.HeightProperty, 22.0);
            track.SetValue(Border.CornerRadiusProperty, new CornerRadius(11));
            track.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            track.SetValue(Border.BorderBrushProperty, new TemplateBindingExtension(Control.BorderBrushProperty));
            track.SetValue(Border.BorderThicknessProperty, new TemplateBindingExtension(Control.BorderThicknessProperty));

            FrameworkElementFactory knob = new FrameworkElementFactory(typeof(System.Windows.Shapes.Ellipse));
            knob.Name = "knob";
            knob.SetValue(FrameworkElement.WidthProperty, 14.0);
            knob.SetValue(FrameworkElement.HeightProperty, 14.0);
            knob.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Left);
            knob.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            knob.SetValue(FrameworkElement.MarginProperty, new Thickness(4, 0, 0, 0));
            knob.SetValue(System.Windows.Shapes.Shape.FillProperty, new TemplateBindingExtension(Control.ForegroundProperty));
            track.AppendChild(knob);
            template.VisualTree = track;

            Trigger hover = new Trigger();
            hover.Property = UIElement.IsMouseOverProperty;
            hover.Value = true;
            hover.Setters.Add(new Setter(Border.BorderBrushProperty, Fg(120, 255, 255, 255), "track"));
            template.Triggers.Add(hover);

            Trigger on = new Trigger();
            on.Property = CheckBox.IsCheckedProperty;
            on.Value = true;
            on.Setters.Add(new Setter(Border.BackgroundProperty, AccentStrong, "track"));
            on.Setters.Add(new Setter(Border.BorderBrushProperty, AccentStrong, "track"));
            on.Setters.Add(new Setter(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Right, "knob"));
            on.Setters.Add(new Setter(FrameworkElement.MarginProperty, new Thickness(0, 0, 4, 0), "knob"));
            template.Triggers.Add(on);

            cb.Template = template;
            return cb;
        }

    }
}
