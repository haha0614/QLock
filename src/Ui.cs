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
            c.FontFamily = Font;
            c.FontSize = 14;
            c.Foreground = Brushes.White;
            c.BorderThickness = new Thickness(1);
            c.BorderBrush = Fg(80, 255, 255, 255);
            c.Background = Fg(30, 255, 255, 255);

            ControlTemplate template = new ControlTemplate(c.GetType());
            FrameworkElementFactory border = new FrameworkElementFactory(typeof(Border));
            border.Name = "bg";
            border.SetValue(Border.CornerRadiusProperty, new CornerRadius(7));
            border.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            border.SetValue(Border.BorderBrushProperty, new TemplateBindingExtension(Control.BorderBrushProperty));
            border.SetValue(Border.BorderThicknessProperty, new TemplateBindingExtension(Control.BorderThicknessProperty));
            FrameworkElementFactory host = new FrameworkElementFactory(typeof(ScrollViewer));
            host.Name = "PART_ContentHost";
            host.SetValue(ScrollViewer.VerticalAlignmentProperty, VerticalAlignment.Center);
            host.SetValue(ScrollViewer.HorizontalScrollBarVisibilityProperty, ScrollBarVisibility.Hidden);
            host.SetValue(ScrollViewer.VerticalScrollBarVisibilityProperty, ScrollBarVisibility.Hidden);
            host.SetValue(FrameworkElement.MarginProperty, new Thickness(10, 0, 10, 0));
            host.SetValue(UIElement.FocusableProperty, false);
            border.AppendChild(host);
            template.VisualTree = border;
            c.Template = template;
            c.Height = 34;
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
