using System;
using System.IO;

namespace QLock
{
    public static class Log
    {
        static readonly object Gate = new object();

        public static string Dir
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "QLock"); }
        }

        public static string FilePath
        {
            get { return Path.Combine(Dir, "qlock.log"); }
        }

        public static void Write(string format, params object[] args)
        {
            try
            {
                lock (Gate)
                {
                    Directory.CreateDirectory(Dir);
                    FileInfo fi = new FileInfo(FilePath);
                    if (fi.Exists && fi.Length > 512 * 1024) fi.Delete();
                    string line = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff") + "  " +
                                  (args == null || args.Length == 0 ? format : string.Format(format, args));
                    File.AppendAllText(FilePath, line + Environment.NewLine);
                }
            }
            catch { }
        }
    }
}
