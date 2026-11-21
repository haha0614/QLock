using System;
using System.Drawing;
using System.Windows.Forms;

namespace QLock
{
    public class TrayIcon : IDisposable
    {
        NotifyIcon icon;
        Icon appIcon;
        IntPtr iconHandle;
        readonly Action onLock;
        readonly Action onSettings;
        readonly Action onExit;

        public TrayIcon(Action onLock, Action onSettings, Action onExit)
        {
            this.onLock = onLock;
            this.onSettings = onSettings;
            this.onExit = onExit;

            ContextMenuStrip menu = new ContextMenuStrip();
            menu.Items.Add("立即锁定", null, delegate { if (this.onLock != null) this.onLock(); });
            menu.Items.Add("设置...", null, delegate { if (this.onSettings != null) this.onSettings(); });
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("退出 QLock", null, delegate { if (this.onExit != null) this.onExit(); });

            icon = new NotifyIcon();
            icon.Icon = BuildIcon();
            icon.Text = "QLock 锁屏";
            icon.ContextMenuStrip = menu;
            icon.Visible = true;
            icon.DoubleClick += delegate { if (this.onLock != null) this.onLock(); };
        }

        Icon BuildIcon()
        {
            // 优先用 exe 的应用图标（与资源管理器/任务栏一致），失败才退回自绘的锁
            try
            {
                appIcon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
                if (appIcon != null) return appIcon;
            }
            catch (Exception ex)
            {
                Log.Write("tray icon from exe failed: " + ex.Message);
            }

            using (Bitmap bmp = new Bitmap(32, 32))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                    g.Clear(Color.Transparent);
                    using (SolidBrush body = new SolidBrush(Color.FromArgb(40, 44, 52)))
                        g.FillEllipse(body, 1, 1, 30, 30);
                    using (Pen pen = new Pen(Color.FromArgb(235, 238, 245), 2.4f))
                    {
                        g.DrawArc(pen, 10, 8, 12, 14, 180, 180);
                        g.DrawLine(pen, 10, 15, 10, 19);
                        g.DrawLine(pen, 22, 15, 22, 19);
                    }
                    using (SolidBrush body = new SolidBrush(Color.FromArgb(235, 238, 245)))
                        g.FillRectangle(body, 8, 18, 16, 11);
                    using (SolidBrush key = new SolidBrush(Color.FromArgb(40, 44, 52)))
                    {
                        g.FillEllipse(key, 14, 21, 4, 4);
                        g.FillRectangle(key, 15, 24, 2, 3);
                    }
                }
                iconHandle = bmp.GetHicon();
                return Icon.FromHandle(iconHandle);
            }
        }

        public void ShowBalloon(string title, string text)
        {
            try
            {
                icon.BalloonTipTitle = title;
                icon.BalloonTipText = text;
                icon.ShowBalloonTip(6000);
            }
            catch { }
        }

        public void SetTooltip(string text)
        {
            try { icon.Text = text.Length > 63 ? text.Substring(0, 63) : text; } catch { }
        }

        public void Dispose()
        {
            if (icon != null)
            {
                icon.Visible = false;
                icon.Dispose();
                icon = null;
            }
            if (appIcon != null)
            {
                appIcon.Dispose();
                appIcon = null;
            }
            if (iconHandle != IntPtr.Zero)
            {
                Native.DestroyIcon(iconHandle);
                iconHandle = IntPtr.Zero;
            }
        }
    }
}
