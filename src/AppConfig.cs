using System;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace QLock
{
    [DataContract]
    public class AppConfig
    {
        [DataMember] public int Version = 1;

        [DataMember] public string PasswordHash = "";
        [DataMember] public string PasswordSalt = "";
        [DataMember] public int PasswordIterations = Auth.DefaultIterations;
        // 设置里要能回显当前密码，所以另存一份 DPAPI 加密的拷贝；校验只看上面的 Hash。
        [DataMember] public string PasswordCipher = "";
        [DataMember] public string RecoveryHash = "";
        [DataMember] public string RecoverySalt = "";

        [DataMember] public bool InterceptWinL = true;
        [DataMember] public bool CustomHotkeyEnabled = true;
        [DataMember] public bool ShowTray = true;
        [DataMember] public int IdleLockMinutes = 0;

        [DataMember] public bool BlockSystemHotkeys = true;

        [DataMember] public bool RunElevated = false;
        [DataMember] public string Autostart = "none";
        [DataMember] public bool LockOnStartup = false;

        [DataMember] public bool BackgroundSnapshot = false;
        [DataMember] public bool ShowClock = true;
        [DataMember] public bool ShowSubtitle = true;
        [DataMember] public string Subtitle = "输入密码解锁";
        [DataMember] public string ThemeId = "fluent";
        [DataMember] public string FontFamily = "";
        [DataMember] public bool ClockFontEnabled = false;
        [DataMember] public string ClockFont = "";

        [DataMember] public long LastUpdateCheckTicks = 0;
        [DataMember] public string SkippedVersion = "";

        [OnDeserializing]
        void ApplyDefaults(StreamingContext ctx)
        {
            Version = 1;
            PasswordHash = "";
            PasswordSalt = "";
            PasswordIterations = Auth.DefaultIterations;
            PasswordCipher = "";
            RecoveryHash = "";
            RecoverySalt = "";
            InterceptWinL = true;
            CustomHotkeyEnabled = true;
            ShowTray = true;
            IdleLockMinutes = 0;
            BlockSystemHotkeys = true;

            RunElevated = false;
            Autostart = "none";
            LockOnStartup = false;
            BackgroundSnapshot = false;
            ShowClock = true;
            ShowSubtitle = true;
            Subtitle = "输入密码解锁";
            ThemeId = "fluent";
            FontFamily = "";
            ClockFontEnabled = false;
            ClockFont = "";
            LastUpdateCheckTicks = 0;
            SkippedVersion = "";
        }

        public bool HasPassword { get { return !string.IsNullOrEmpty(PasswordHash) && !string.IsNullOrEmpty(PasswordSalt); } }

        // 时钟单独字体没开、或者没挑过，就用界面字体。
        public string ClockFontName
        {
            get { return ClockFontEnabled && !string.IsNullOrEmpty(ClockFont) ? ClockFont : FontFamily; }
        }

        public AppConfig Clone()
        {
            return (AppConfig)MemberwiseClone();
        }

        public static string Serialize(AppConfig c)
        {
            DataContractJsonSerializer s = new DataContractJsonSerializer(typeof(AppConfig));
            using (MemoryStream ms = new MemoryStream())
            {
                s.WriteObject(ms, c);
                return System.Text.Encoding.UTF8.GetString(ms.ToArray());
            }
        }

        public static AppConfig Deserialize(string json)
        {
            DataContractJsonSerializer s = new DataContractJsonSerializer(typeof(AppConfig));
            using (MemoryStream ms = new MemoryStream(System.Text.Encoding.UTF8.GetBytes(json)))
            {
                return (AppConfig)s.ReadObject(ms);
            }
        }
    }

    public static class ConfigStore
    {
        static string overrideDir;

        public static string Dir
        {
            get
            {
                if (!string.IsNullOrEmpty(overrideDir)) return overrideDir;
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "QLock");
            }
        }

        public static string FilePath { get { return Path.Combine(Dir, "config.json"); } }

        public static void SetOverrideDir(string dir) { overrideDir = dir; }

        public static AppConfig Load()
        {
            try
            {
                if (!File.Exists(FilePath)) return new AppConfig();
                AppConfig c = AppConfig.Deserialize(File.ReadAllText(FilePath, System.Text.Encoding.UTF8));
                if (c == null) return new AppConfig();
                if (c.PasswordIterations <= 0) c.PasswordIterations = Auth.DefaultIterations;
                if (string.IsNullOrEmpty(c.ThemeId)) c.ThemeId = "fluent";
                return c;
            }
            catch (Exception ex)
            {
                Log.Write("config load failed: " + ex.Message);
                AppConfig fallback = new AppConfig();
                try
                {
                    if (File.Exists(FilePath))
                        File.Copy(FilePath, FilePath + ".broken-" + DateTime.Now.ToString("yyyyMMddHHmmss"), true);
                }
                catch { }
                return fallback;
            }
        }

        public static void Save(AppConfig c)
        {
            Directory.CreateDirectory(Dir);
            string tmp = FilePath + ".tmp";
            byte[] bytes = System.Text.Encoding.UTF8.GetBytes(AppConfig.Serialize(c));
            using (FileStream fs = new FileStream(tmp, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                fs.Write(bytes, 0, bytes.Length);
                fs.Flush(true);
            }
            if (!File.Exists(FilePath))
            {
                File.Move(tmp, FilePath);
                return;
            }
            string backup = FilePath + ".bak";
            try
            {
                File.Replace(tmp, FilePath, backup);
            }
            catch
            {
                File.Delete(FilePath);
                File.Move(tmp, FilePath);
            }
            try { if (File.Exists(backup)) File.Delete(backup); } catch { }
        }
    }
}
