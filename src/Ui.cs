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

        public static TextBlock Text(string text, double size, Brush brush, FontWeight weight)
        {
            TextBlock tb = new TextBlock();
            tb.Text = text;
            tb.FontSize = size;
            tb.Foreground = brush;
            tb.FontWeight = weight;
            tb.FontFamily = Font;
            return tb;
        }

        public static TextBlock Label(string text)
        {
            return Text(text, 13, TextSecondary, FontWeights.Normal);
        }

        public static void StyleInput(Control c)
        {
            StyleInput(c, false, 34, 7, Fg(30, 255, 255, 255), Fg(80, 255, 255, 255), Brushes.White, 14);
        }

        public static void StyleInput(Control c, bool multiline)
        {
            StyleInput(c, multiline, 34, 7, Fg(30, 255, 255, 255), Fg(80, 255, 255, 255), Brushes.White, 14);
        }

        public static void StyleInput(Control c, bool multiline, double height, double radius, Brush background, Brush borderBrush, Brush foreground, double fontSize)
        {
            c.FontFamily = Font;
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
            c.Template = template;
            if (!multiline) c.Height = height;
        }

        public static Button Button(string text, bool primary)
        {
            Button b = new Button();
            b.Content = text;
            b.Height = 38;
            b.FontSize = 14;
            b.FontFamily = Font;
            b.Cursor = Cursors.Hand;
            b.Foreground = Brushes.White;
            b.BorderThickness = new Thickness(0);
            b.Background = primary ? Fg(255, 58, 118, 232) : Fg(45, 255, 255, 255);

            ControlTemplate template = new ControlTemplate(typeof(Button));
            FrameworkElementFactory border = new FrameworkElementFactory(typeof(Border));
            border.Name = "bg";
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(8));
            border.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            FrameworkElementFactory presenter = new FrameworkElementFactory(typeof(ContentPresenter));
            presenter.SetValue(FrameworkElement.HorizontalAlignmentProperty, HorizontalAlignment.Center);
            presenter.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            border.AppendChild(presenter);
            template.VisualTree = border;

            Trigger hover = new Trigger();
            hover.Property = UIElement.IsMouseOverProperty;
            hover.Value = true;
            hover.Setters.Add(new Setter(Border.BackgroundProperty, Fg(255, 78, 138, 250), "bg"));
            template.Triggers.Add(hover);

            Trigger pressed = new Trigger();
            pressed.Property = ButtonBase.IsPressedProperty;
            pressed.Value = true;
            pressed.Setters.Add(new Setter(Border.BackgroundProperty, Fg(255, 42, 96, 200), "bg"));
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
            b.FontFamily = Font;
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

        public static CheckBox Check(bool value, string text, double topMargin)
        {
            CheckBox cb = new CheckBox();
            cb.Content = text;
            cb.IsChecked = value;
            cb.FontFamily = Font;
            cb.FontSize = 13.5;
            cb.Foreground = TextPrimary;
            cb.Margin = new Thickness(0, topMargin, 0, 0);
            cb.Cursor = Cursors.Hand;
            cb.VerticalContentAlignment = VerticalAlignment.Center;
            return cb;
        }

        public static Border Card(UIElement content, double topMargin)
        {
            Border card = new Border();
            card.CornerRadius = new CornerRadius(10);
            card.Padding = new Thickness(18, 14, 18, 16);
            card.Background = Fg(210, 26, 30, 40);
            card.BorderThickness = new Thickness(1);
            card.BorderBrush = Fg(40, 255, 255, 255);
            card.Margin = new Thickness(0, topMargin, 0, 0);
            card.Child = content;
            return card;
        }

        public static StackPanel Section(string title)
        {
            StackPanel sp = new StackPanel();
            TextBlock head = Text(title, 13, Fg(255, 120, 165, 235), FontWeights.SemiBold);
            head.Margin = new Thickness(0, 0, 0, 10);
            sp.Children.Add(head);
            return sp;
        }
    }
}
