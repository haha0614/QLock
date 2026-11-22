using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;

namespace QLock
{
    [DataContract]
    public class ThemeClock
    {
        [DataMember] public bool Show = true;
        [DataMember] public double Size = 74;
        [DataMember] public int Weight = 300;
        [DataMember] public string Color = "#FFFFFFFF";

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Show = true;
            Size = 74;
            Weight = 300;
            Color = "#FFFFFFFF";
        }
    }

    [DataContract]
    public class ThemeDate
    {
        [DataMember] public double Size = 16;
        [DataMember] public string Color = "#FFC9C9C9";
        [DataMember] public double Gap = 26;
        [DataMember] public string Format = "";

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Size = 16;
            Color = "#FFC9C9C9";
            Gap = 26;
            Format = "";
        }
    }

    [DataContract]
    public class ThemeCard
    {
        [DataMember] public bool Show = false;
        [DataMember] public double Width = 380;
        [DataMember] public double Radius = 14;
        [DataMember] public double PadH = 26;
        [DataMember] public double PadV = 20;
        [DataMember] public string Bg = "#D81C1C26";
        [DataMember] public string Border = "#37FFFFFF";

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Show = false;
            Width = 380;
            Radius = 14;
            PadH = 26;
            PadV = 20;
            Bg = "#D81C1C26";
            Border = "#37FFFFFF";
        }
    }

    [DataContract]
    public class ThemeBrand
    {
        [DataMember] public bool Show = false;
        [DataMember] public string Text = "Q L O C K";
        [DataMember] public double Size = 11;
        [DataMember] public string Color = "#FF78A0E6";
        [DataMember] public double Gap = 6;

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Show = false;
            Text = "Q L O C K";
            Size = 11;
            Color = "#FF78A0E6";
            Gap = 6;
        }
    }

    [DataContract]
    public class ThemeSubtitle
    {
        [DataMember] public double Size = 13;
        [DataMember] public string Color = "#FFE8EEF7";
        [DataMember] public double Gap = 14;

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Size = 13;
            Color = "#FFE8EEF7";
            Gap = 14;
        }
    }

    [DataContract]
    public class ThemeInput
    {
        [DataMember] public string Kind = "box";
        [DataMember] public double Height = 40;
        [DataMember] public double Radius = 7;
        [DataMember] public double Size = 15;
        [DataMember] public double Width = 340;
        [DataMember] public string Bg = "#1EFFFFFF";
        [DataMember] public string Border = "#50FFFFFF";
        [DataMember] public string Fg = "#FFFFFFFF";
        [DataMember] public string Hint = "#69FFFFFF";

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Kind = "box";
            Height = 40;
            Radius = 7;
            Size = 15;
            Width = 340;
            Bg = "#1EFFFFFF";
            Border = "#50FFFFFF";
            Fg = "#FFFFFFFF";
            Hint = "#69FFFFFF";
        }
    }

    [DataContract]
    public class ThemeButton
    {
        [DataMember] public string Kind = "filled";
        [DataMember] public double Height = 40;
        [DataMember] public double Radius = 8;
        [DataMember] public double Size = 15;
        [DataMember] public double Width = 0;
        [DataMember] public double Gap = 12;
        [DataMember] public string Bg = "#FF3A76E8";
        [DataMember] public string Fg = "#FFFFFFFF";
        [DataMember] public string Border = "#00000000";
        [DataMember] public string Hover = "";

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Kind = "filled";
            Height = 40;
            Radius = 8;
            Size = 15;
            Width = 0;
            Gap = 12;
            Bg = "#FF3A76E8";
            Fg = "#FFFFFFFF";
            Border = "#00000000";
            Hover = "";
        }
    }

    [DataContract]
    public class ThemeLink
    {
        [DataMember] public double Size = 12;
        [DataMember] public string Color = "#FF8CB4F0";
        [DataMember] public double Gap = 12;

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Size = 12;
            Color = "#FF8CB4F0";
            Gap = 12;
        }
    }

    [DataContract]
    public class ThemeBackground
    {
        [DataMember] public string Color = "#FF080A10";
        [DataMember] public int Dim = 150;
        [DataMember] public double Blur = 26;

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Color = "#FF080A10";
            Dim = 150;
            Blur = 26;
        }
    }

    [DataContract]
    public class LockTheme
    {
        [DataMember] public int Schema = 1;
        [DataMember] public string Id = "";
        [DataMember] public string Name = "";
        [DataMember] public string Layout = "stacked";
        [DataMember] public ThemeClock Clock = new ThemeClock();
        [DataMember] public ThemeDate Date = new ThemeDate();
        [DataMember] public ThemeCard Card = new ThemeCard();
        [DataMember] public ThemeBrand Brand = new ThemeBrand();
        [DataMember] public ThemeSubtitle Subtitle = new ThemeSubtitle();
        [DataMember] public ThemeInput Input = new ThemeInput();
        [DataMember] public ThemeButton Button = new ThemeButton();
        [DataMember] public ThemeLink Link = new ThemeLink();
        [DataMember] public ThemeBackground Background = new ThemeBackground();

        [OnDeserializing]
        void Defaults(StreamingContext ctx)
        {
            Schema = 1;
            Id = "";
            Name = "";
            Layout = "stacked";
            Clock = new ThemeClock();
            Date = new ThemeDate();
            Card = new ThemeCard();
            Brand = new ThemeBrand();
            Subtitle = new ThemeSubtitle();
            Input = new ThemeInput();
            Button = new ThemeButton();
            Link = new ThemeLink();
            Background = new ThemeBackground();
        }

        public bool Inline
        {
            get { return string.Equals(Layout, "inline", StringComparison.OrdinalIgnoreCase); }
        }

        public string DateFormat
        {
            get { return string.IsNullOrEmpty(Date.Format) ? "yyyy年M月d日 dddd" : Date.Format; }
        }
    }

    public static class Themes
    {
        public const string DefaultId = "fluent";
        const int MaxFiles = 40;
        const int MaxFileBytes = 64 * 1024;

        static List<LockTheme> cache;

        public static string Dir
        {
            get { return Path.Combine(ConfigStore.Dir, "themes"); }
        }

        public static List<LockTheme> All()
        {
            if (cache == null) cache = Load();
            return cache;
        }

        public static void Reset()
        {
            cache = null;
        }

        public static LockTheme Find(string id)
        {
            List<LockTheme> all = All();
            if (!string.IsNullOrEmpty(id))
            {
                for (int i = 0; i < all.Count; i++)
                {
                    if (string.Equals(all[i].Id, id, StringComparison.OrdinalIgnoreCase)) return all[i];
                }
            }
            for (int i = 0; i < all.Count; i++)
            {
                if (string.Equals(all[i].Id, DefaultId, StringComparison.OrdinalIgnoreCase)) return all[i];
            }
            return BuiltinFluent();
        }

        static List<LockTheme> Load()
        {
            List<LockTheme> list = new List<LockTheme>();
            List<LockTheme> external = LoadExternal();
            for (int i = 0; i < external.Count; i++) list.Add(external[i]);
            List<LockTheme> builtin = new List<LockTheme>();
            builtin.Add(BuiltinFluent());
            builtin.Add(BuiltinCard());
            builtin.Add(BuiltinMinimal());
            for (int i = 0; i < builtin.Count; i++)
            {
                bool shadowed = false;
                for (int j = 0; j < external.Count; j++)
                {
                    if (string.Equals(external[j].Id, builtin[i].Id, StringComparison.OrdinalIgnoreCase)) { shadowed = true; break; }
                }
                if (!shadowed) list.Add(builtin[i]);
            }
            return list;
        }

        static List<LockTheme> LoadExternal()
        {
            List<LockTheme> list = new List<LockTheme>();
            try
            {
                if (!Directory.Exists(Dir)) return list;
                string[] files = Directory.GetFiles(Dir, "*.json");
                Array.Sort(files, StringComparer.OrdinalIgnoreCase);
                int used = 0;
                for (int i = 0; i < files.Length && used < MaxFiles; i++)
                {
                    try
                    {
                        FileInfo fi = new FileInfo(files[i]);
                        if (fi.Length > MaxFileBytes)
                        {
                            Log.Write("theme skipped (too large): " + fi.Name);
                            continue;
                        }
                        LockTheme t = Parse(File.ReadAllText(files[i], Encoding.UTF8));
                        if (t == null)
                        {
                            Log.Write("theme skipped (unreadable): " + fi.Name);
                            continue;
                        }
                        if (string.IsNullOrEmpty(t.Id)) t.Id = Path.GetFileNameWithoutExtension(fi.Name);
                        if (string.IsNullOrEmpty(t.Name)) t.Name = t.Id;
                        if (t.Schema > 1) Log.Write("theme " + t.Id + " uses newer schema " + t.Schema + ", loading anyway");
                        list.Add(t);
                        used++;
                    }
                    catch (Exception ex)
                    {
                        Log.Write("theme skipped (" + Path.GetFileName(files[i]) + "): " + ex.Message);
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("theme folder unreadable: " + ex.Message);
            }
            return list;
        }

        public static LockTheme Parse(string json)
        {
            DataContractJsonSerializer s = new DataContractJsonSerializer(typeof(LockTheme));
            using (MemoryStream ms = new MemoryStream(Encoding.UTF8.GetBytes(json)))
            {
                LockTheme t = (LockTheme)s.ReadObject(ms);
                if (t != null) Normalize(t);
                return t;
            }
        }

        static void Normalize(LockTheme t)
        {
            if (t.Clock == null) t.Clock = new ThemeClock();
            if (t.Date == null) t.Date = new ThemeDate();
            if (t.Card == null) t.Card = new ThemeCard();
            if (t.Brand == null) t.Brand = new ThemeBrand();
            if (t.Subtitle == null) t.Subtitle = new ThemeSubtitle();
            if (t.Input == null) t.Input = new ThemeInput();
            if (t.Button == null) t.Button = new ThemeButton();
            if (t.Link == null) t.Link = new ThemeLink();
            if (t.Background == null) t.Background = new ThemeBackground();
            if (t.Clock.Size < 8) t.Clock.Size = 8;
            if (t.Date.Size < 6) t.Date.Size = 6;
            if (t.Input.Height < 20) t.Input.Height = 20;
            if (t.Button.Height < 20) t.Button.Height = 20;
            if (t.Card.Width < 120) t.Card.Width = 120;
            t.Clock.Weight = ClampWeight(t.Clock.Weight);
        }

        static int ClampWeight(int w)
        {
            if (w < 100) return 100;
            if (w > 900) return 900;
            return w;
        }

        public static string Serialize(LockTheme t)
        {
            DataContractJsonSerializer s = new DataContractJsonSerializer(typeof(LockTheme));
            using (MemoryStream ms = new MemoryStream())
            {
                s.WriteObject(ms, t);
                return Encoding.UTF8.GetString(ms.ToArray());
            }
        }

        public static void EnsureSampleThemes()
        {
            try
            {
                if (Directory.Exists(Dir)) return;
                Directory.CreateDirectory(Dir);
                WriteSample(BuiltinFluent());
                WriteSample(BuiltinCard());
                WriteSample(BuiltinMinimal());
                Log.Write("theme folder created: " + Dir);
            }
            catch (Exception ex)
            {
                Log.Write("theme folder setup failed: " + ex.Message);
            }
        }

        static void WriteSample(LockTheme t)
        {
            try
            {
                string path = Path.Combine(Dir, t.Id + ".json");
                File.WriteAllText(path, Pretty(Serialize(t)), new UTF8Encoding(false));
            }
            catch (Exception ex)
            {
                Log.Write("theme sample write failed: " + ex.Message);
            }
        }

        static string Pretty(string json)
        {
            StringBuilder sb = new StringBuilder();
            bool inString = false;
            int indent = 0;
            for (int i = 0; i < json.Length; i++)
            {
                char ch = json[i];
                if (inString)
                {
                    sb.Append(ch);
                    if (ch == '\\' && i + 1 < json.Length)
                    {
                        sb.Append(json[i + 1]);
                        i++;
                    }
                    else if (ch == '"')
                    {
                        inString = false;
                    }
                    continue;
                }
                if (ch == '"')
                {
                    inString = true;
                    sb.Append(ch);
                }
                else if (ch == '{' || ch == '[')
                {
                    indent++;
                    sb.Append(ch).Append('\n').Append(Spaces(indent));
                }
                else if (ch == '}' || ch == ']')
                {
                    indent--;
                    sb.Append('\n').Append(Spaces(indent)).Append(ch);
                }
                else if (ch == ',')
                {
                    sb.Append(ch).Append('\n').Append(Spaces(indent));
                }
                else if (ch == ':')
                {
                    sb.Append(": ");
                }
                else
                {
                    sb.Append(ch);
                }
            }
            return sb.ToString();
        }

        static string Spaces(int n)
        {
            return new string(' ', n * 2);
        }

        public static LockTheme BuiltinFluent()
        {
            LockTheme t = new LockTheme();
            t.Id = "fluent";
            t.Name = "Windows 11 Fluent";
            t.Layout = "stacked";
            t.Clock.Size = 74;
            t.Clock.Weight = 200;
            t.Clock.Color = "#FFFFFFFF";
            t.Date.Size = 15;
            t.Date.Color = "#FFC9C9C9";
            t.Date.Gap = 26;
            t.Card.Show = false;
            t.Brand.Show = false;
            t.Subtitle.Size = 13;
            t.Subtitle.Color = "#FFC9C9C9";
            t.Subtitle.Gap = 14;
            t.Input.Kind = "box";
            t.Input.Height = 40;
            t.Input.Radius = 4;
            t.Input.Bg = "#FF2B2B2B";
            t.Input.Border = "#FF3D3D3D";
            t.Input.Hint = "#FF9A9A9A";
            t.Button.Kind = "filled";
            t.Button.Height = 40;
            t.Button.Radius = 4;
            t.Button.Bg = "#FF0078D4";
            t.Button.Hover = "#FF1A86DC";
            t.Button.Gap = 12;
            t.Link.Size = 12;
            t.Link.Color = "#FF60CDFF";
            t.Link.Gap = 12;
            t.Background.Color = "#FF080A10";
            t.Background.Dim = 150;
            return t;
        }

        public static LockTheme BuiltinCard()
        {
            LockTheme t = new LockTheme();
            t.Id = "card";
            t.Name = "卡片";
            t.Layout = "stacked";
            t.Clock.Size = 74;
            t.Clock.Weight = 300;
            t.Clock.Color = "#FFFFFFFF";
            t.Date.Size = 17;
            t.Date.Color = "#FFBEC8D7";
            t.Date.Gap = 26;
            t.Card.Show = true;
            t.Card.Width = 380;
            t.Card.Radius = 14;
            t.Card.PadH = 26;
            t.Card.PadV = 20;
            t.Card.Bg = "#D81C1C26";
            t.Card.Border = "#37FFFFFF";
            t.Brand.Show = true;
            t.Brand.Text = "Q L O C K";
            t.Brand.Size = 11;
            t.Brand.Color = "#FF78A0E6";
            t.Brand.Gap = 6;
            t.Subtitle.Size = 14.5;
            t.Subtitle.Color = "#FFE8EEF7";
            t.Subtitle.Gap = 14;
            t.Input.Height = 40;
            t.Input.Radius = 7;
            t.Input.Bg = "#1EFFFFFF";
            t.Input.Border = "#50FFFFFF";
            t.Input.Hint = "#69FFFFFF";
            t.Button.Height = 40;
            t.Button.Radius = 8;
            t.Button.Bg = "#FF3A76E8";
            t.Button.Hover = "#FF4E8AFA";
            t.Button.Gap = 12;
            t.Link.Size = 12;
            t.Link.Color = "#FF8CB4F0";
            t.Link.Gap = 12;
            t.Background.Color = "#FF080A10";
            t.Background.Dim = 150;
            return t;
        }

        public static LockTheme BuiltinMinimal()
        {
            LockTheme t = new LockTheme();
            t.Id = "minimal";
            t.Name = "极简";
            t.Layout = "stacked";
            t.Clock.Size = 76;
            t.Clock.Weight = 200;
            t.Clock.Color = "#FFFFFFFF";
            t.Date.Size = 13;
            t.Date.Color = "#FF6F7683";
            t.Date.Gap = 28;
            t.Card.Show = false;
            t.Brand.Show = false;
            t.Subtitle.Size = 12;
            t.Subtitle.Color = "#FF4B515C";
            t.Subtitle.Gap = 16;
            t.Input.Kind = "underline";
            t.Input.Height = 34;
            t.Input.Radius = 0;
            t.Input.Size = 14;
            t.Input.Bg = "#00000000";
            t.Input.Border = "#FF2A2F38";
            t.Input.Fg = "#FFE8EEF7";
            t.Input.Hint = "#FF5C636E";
            t.Button.Kind = "text";
            t.Button.Height = 32;
            t.Button.Radius = 6;
            t.Button.Bg = "#00000000";
            t.Button.Fg = "#FFCFD5DF";
            t.Button.Hover = "#FF1A1D23";
            t.Button.Gap = 10;
            t.Link.Size = 11;
            t.Link.Color = "#FF5C636E";
            t.Link.Gap = 10;
            t.Background.Color = "#FF000000";
            t.Background.Dim = 210;
            return t;
        }
    }
}
