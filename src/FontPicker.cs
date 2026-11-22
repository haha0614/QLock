using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Markup;
using System.Windows.Media;

namespace QLock
{
    // 挑字体用自己的小窗口：列出系统装了的字体，每行用它自己的字体画，上面顶个筛选框。
    static class FontPicker
    {
        public const string DefaultLabel = "微软雅黑 UI（默认）";

        static List<Entry> cached;

        public sealed class Entry
        {
            public string Name { get; set; }
            public string Source { get; set; }
            public FontFamily Family { get; set; }
        }

        static List<Entry> All()
        {
            if (cached != null) return cached;
            List<Entry> list = new List<Entry>();
            foreach (FontFamily family in Fonts.SystemFontFamilies)
            {
                Entry e = new Entry();
                e.Source = family.Source;
                e.Name = Display(family);
                e.Family = family;
                list.Add(e);
            }
            list.Sort(delegate(Entry a, Entry b) { return string.Compare(a.Name, b.Name, StringComparison.CurrentCulture); });
            cached = list;
            return cached;
        }

        static string Display(FontFamily family)
        {
            string name;
            if (family.FamilyNames.TryGetValue(XmlLanguage.GetLanguage("zh-cn"), out name) && !string.IsNullOrEmpty(name)) return name;
            if (family.FamilyNames.TryGetValue(XmlLanguage.GetLanguage("en-us"), out name) && !string.IsNullOrEmpty(name)) return name;
            return family.Source;
        }

        // 配置里存的是 FontFamily.Source 那串，界面上显示系统里的中文名。
        public static string Label(string source)
        {
            if (string.IsNullOrEmpty(source)) return DefaultLabel;
            foreach (Entry e in All())
            {
                if (string.Equals(e.Source, source, StringComparison.OrdinalIgnoreCase)) return e.Name;
                if (string.Equals(e.Name, source, StringComparison.OrdinalIgnoreCase)) return e.Name;
            }
            return source;
        }

        // 返回选中的字体名；空字符串表示用默认字体；null 表示取消。
        public static string Pick(Window owner, string current, FontFamily uiFont)
        {
            Window win = new Window();
            win.Title = "选择字体";
            win.Width = 460;
            win.Height = 600;
            win.ResizeMode = ResizeMode.NoResize;
            win.WindowStartupLocation = WindowStartupLocation.CenterOwner;
            win.Owner = owner;
            win.Background = Ui.Surface;
            win.Foreground = Ui.TextPrimary;
            win.Resources[typeof(ScrollBar)] = Ui.SlimScrollBar();
            win.FontFamily = uiFont;

            Grid root = new Grid();
            root.Margin = new Thickness(Design.Gap16);
            root.RowDefinitions.Add(new RowDefinition());
            root.RowDefinitions.Add(new RowDefinition());
            root.RowDefinitions.Add(new RowDefinition());
            root.RowDefinitions[0].Height = GridLength.Auto;
            root.RowDefinitions[2].Height = GridLength.Auto;
            win.Content = root;

            TextBox filter = new TextBox();
            Ui.StyleInput(filter);
            filter.Margin = new Thickness(0, 0, 0, Design.Gap12);
            Grid.SetRow(filter, 0);
            root.Children.Add(filter);

            ListBox list = new ListBox();
            list.Background = Brushes.Transparent;
            list.BorderThickness = new Thickness(0);
            list.Foreground = Ui.TextPrimary;
            list.HorizontalContentAlignment = HorizontalAlignment.Stretch;
            list.ItemContainerStyle = ItemStyle();

            DataTemplate row = new DataTemplate(typeof(Entry));
            FrameworkElementFactory nameText = new FrameworkElementFactory(typeof(TextBlock));
            nameText.SetBinding(TextBlock.TextProperty, new Binding("Name"));
            nameText.SetBinding(TextBlock.FontFamilyProperty, new Binding("Family"));
            nameText.SetValue(TextBlock.FontSizeProperty, Design.FontControl);
            nameText.SetValue(TextBlock.TextTrimmingProperty, TextTrimming.CharacterEllipsis);
            row.VisualTree = nameText;
            list.ItemTemplate = row;
            Grid.SetRow(list, 1);
            root.Children.Add(list);

            StackPanel footer = new StackPanel();
            footer.Orientation = Orientation.Horizontal;
            footer.HorizontalAlignment = HorizontalAlignment.Right;
            footer.Margin = new Thickness(0, Design.Gap12, 0, 0);
            Grid.SetRow(footer, 2);
            root.Children.Add(footer);

            Button cancel = Ui.Button("取消", false);
            cancel.Width = 76;
            Button plain = Ui.Button("用默认", false);
            plain.Width = 88;
            plain.Margin = new Thickness(Design.Gap8, 0, 0, 0);
            Button ok = Ui.Button("使用", true);
            ok.Width = 76;
            ok.Margin = new Thickness(Design.Gap8, 0, 0, 0);
            footer.Children.Add(cancel);
            footer.Children.Add(plain);
            footer.Children.Add(ok);

            Action<string> fill = null;
            fill = delegate(string query)
            {
                string needle = query == null ? "" : query.Trim();
                list.Items.Clear();
                Entry restore = null;
                foreach (Entry e in All())
                {
                    if (needle.Length > 0
                        && e.Name.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0
                        && e.Source.IndexOf(needle, StringComparison.OrdinalIgnoreCase) < 0) continue;
                    list.Items.Add(e);
                    if (restore == null && !string.IsNullOrEmpty(current) && string.Equals(e.Source, current, StringComparison.OrdinalIgnoreCase)) restore = e;
                }
                if (restore != null)
                {
                    list.SelectedItem = restore;
                    list.ScrollIntoView(restore);
                }
            };
            fill("");
            filter.TextChanged += delegate { fill(filter.Text); };

            bool done = false;
            string picked = null;
            Action accept = delegate
            {
                Entry e = list.SelectedItem as Entry;
                picked = e == null ? current : e.Source;
                done = true;
                win.Close();
            };
            ok.Click += delegate { accept(); };
            list.MouseDoubleClick += delegate { if (list.SelectedItem != null) accept(); };
            cancel.Click += delegate { win.Close(); };
            plain.Click += delegate
            {
                picked = "";
                done = true;
                win.Close();
            };
            win.PreviewKeyDown += delegate(object s, System.Windows.Input.KeyEventArgs e)
            {
                if (e.Key == System.Windows.Input.Key.Escape) win.Close();
            };

            win.ShowDialog();
            return done ? picked : null;
        }

        static Style ItemStyle()
        {
            Style style = new Style(typeof(ListBoxItem));
            style.Setters.Add(new Setter(FrameworkElement.HeightProperty, Design.ControlHeight));
            style.Setters.Add(new Setter(FrameworkElement.MarginProperty, new Thickness(0, 1, 0, 1)));
            style.Setters.Add(new Setter(Control.BackgroundProperty, Brushes.Transparent));
            style.Setters.Add(new Setter(Control.ForegroundProperty, Ui.TextPrimary));
            style.Setters.Add(new Setter(Control.PaddingProperty, new Thickness(Design.Gap12, 0, Design.Gap12, 0)));
            style.Setters.Add(new Setter(System.Windows.Automation.AutomationProperties.NameProperty, new Binding("Name")));
            style.Setters.Add(new Setter(Control.HorizontalContentAlignmentProperty, HorizontalAlignment.Left));
            style.Setters.Add(new Setter(Control.TemplateProperty, RowTemplate()));

            Trigger hover = new Trigger();
            hover.Property = UIElement.IsMouseOverProperty;
            hover.Value = true;
            hover.Setters.Add(new Setter(Control.BackgroundProperty, Ui.ChipSurface));
            style.Triggers.Add(hover);

            Trigger selected = new Trigger();
            selected.Property = ListBoxItem.IsSelectedProperty;
            selected.Value = true;
            selected.Setters.Add(new Setter(Control.BackgroundProperty, Ui.AccentSoft));
            style.Triggers.Add(selected);
            return style;
        }

        static ControlTemplate RowTemplate()
        {
            ControlTemplate template = new ControlTemplate(typeof(ListBoxItem));
            FrameworkElementFactory bg = new FrameworkElementFactory(typeof(Squircle));
            bg.Name = "bg";
            bg.SetValue(Border.BackgroundProperty, new TemplateBindingExtension(Control.BackgroundProperty));
            bg.SetValue(Border.CornerRadiusProperty, new CornerRadius(Design.RadiusControl));
            bg.SetValue(Border.PaddingProperty, new TemplateBindingExtension(Control.PaddingProperty));
            FrameworkElementFactory content = new FrameworkElementFactory(typeof(ContentPresenter));
            content.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Center);
            bg.AppendChild(content);
            template.VisualTree = bg;
            return template;
        }
    }
}
