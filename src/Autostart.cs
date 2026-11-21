using System;
using System.Reflection;
using Microsoft.Win32;

namespace QLock
{
    public static class Autostart
    {
        public const string RunKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        public const string RunValueName = "QLock";

        public static string ExePath()
        {
            return Assembly.GetExecutingAssembly().Location;
        }

        public static bool Apply(bool enabled, out string error)
        {
            error = null;
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RunKey, true))
                {
                    if (key == null)
                    {
                        error = "无法打开当前用户的启动项注册表";
                        return false;
                    }
                    if (enabled) key.SetValue(RunValueName, "\"" + ExePath() + "\" --tray");
                    else if (key.GetValue(RunValueName) != null) key.DeleteValue(RunValueName, false);
                }
                return true;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }

        public static string DetectCurrent()
        {
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RunKey, false))
                {
                    if (key != null && key.GetValue(RunValueName) != null) return "runkey";
                }
            }
            catch { }
            return "none";
        }
    }
}
