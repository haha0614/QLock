using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace QLock
{
    public class LockController
    {
        const int VK_TAB = 0x09;
        const int VK_ESCAPE = 0x1B;
        const int VK_CONTROL = 0x11;
        const int VK_MENU = 0x12;
        const int VK_LCONTROL = 0xA2;
        const int VK_RCONTROL = 0xA3;
        const int VK_LMENU = 0xA4;
        const int VK_RMENU = 0xA5;
        const int VK_LWIN = 0x5B;
        const int VK_RWIN = 0x5C;
        const int VK_F4 = 0x73;
        const int VK_L = 0x4C;

        readonly QLockApp app;
        readonly LowLevelKeyboardHook hook = new LowLevelKeyboardHook();
        readonly List<LockWindow> windows = new List<LockWindow>();

        bool locked;
        bool ctrlDown;
        bool altDown;
        bool winDown;
        bool suppressWinUp;
        bool suppressLUp;
        int failures;
        int lastReclaim;
        int lastBlockedLog;
        bool rightAlt;
        bool lockPending;
        DispatcherTimer punishTimer;
        DispatcherTimer hookTimer;
        DispatcherTimer rebuildTimer;

        public bool IsLocked { get { return locked; } }
        public string HookError { get { return hook.LastError; } }

        public LockController(QLockApp owner)
        {
            app = owner;
            hook.Filter = Filter;
        }

        public bool InstallHook()
        {
            return hook.Install();
        }

        public void StartHookWatchdog()
        {
            hookTimer = new DispatcherTimer();
            hookTimer.Interval = TimeSpan.FromSeconds(45);
            hookTimer.Tick += delegate
            {
                try
                {
                    hook.Rearm();
                    if (!hook.IsInstalled) Log.Write("hook rearm failed: " + hook.LastError);
                }
                catch (Exception ex) { Log.Write("hook rearm error: " + ex.Message); }
            };
            hookTimer.Start();
        }

        public void OnDisplayChanged()
        {
            if (!locked) return;
            if (LayoutUnchanged())
            {
                Log.Write("display changed while locked, layout unchanged, re-asserting overlay");
                ForceGuard();
                return;
            }
            ScheduleRebuild("display layout changed while locked");
        }

        public void OnPowerResumed()
        {
            if (!locked) return;
            Log.Write("resumed from sleep while locked");
            ForceGuard();
            if (!LayoutUnchanged()) ScheduleRebuild("display layout changed after resume");
        }

        void ForceGuard()
        {
            for (int i = 0; i < windows.Count; i++)
            {
                try { windows[i].ForceGuard(); }
                catch (Exception ex) { Log.Write("overlay guard failed: " + ex.Message); }
            }
        }

        bool LayoutUnchanged()
        {
            Screen[] screens = Screen.AllScreens;
            if (screens.Length != windows.Count) return false;
            for (int i = 0; i < screens.Length; i++)
            {
                bool found = false;
                for (int j = 0; j < windows.Count; j++)
                {
                    if (windows[j].ScreenBounds == screens[i].Bounds) { found = true; break; }
                }
                if (!found) return false;
            }
            return true;
        }

        void ScheduleRebuild(string reason)
        {
            Log.Write(reason + ", rebuilding overlay");
            if (rebuildTimer == null)
            {
                rebuildTimer = new DispatcherTimer();
                rebuildTimer.Interval = TimeSpan.FromMilliseconds(800);
                rebuildTimer.Tick += delegate
                {
                    rebuildTimer.Stop();
                    Rebuild();
                };
            }
            rebuildTimer.Stop();
            rebuildTimer.Start();
        }

        void Rebuild()
        {
            if (!locked) return;

            List<LockWindow> created;
            try
            {
                created = CreateWindows(app.Config);
            }
            catch (Exception ex)
            {
                Log.Write("overlay rebuild failed, keeping current overlay: " + ex.Message);
                return;
            }

            List<LockWindow> old = new List<LockWindow>(windows);
            for (int i = 0; i < old.Count; i++) old[i].Retire();

            windows.Clear();
            windows.AddRange(created);
            try
            {
                for (int i = 0; i < windows.Count; i++) windows[i].ShowLock();
            }
            catch (Exception ex)
            {
                Log.Write("overlay rebuild show failed: " + ex.Message);
                for (int i = 0; i < created.Count; i++)
                {
                    try { created[i].Close(); } catch { }
                }
                windows.Clear();
                windows.AddRange(old);
                for (int i = 0; i < old.Count; i++)
                {
                    try { old[i].ShowLock(); } catch { }
                }
                return;
            }

            for (int i = 0; i < old.Count; i++)
            {
                try { old[i].Close(); } catch (Exception ex) { Log.Write("close failed: " + ex.Message); }
            }
            Log.Write("overlay rebuilt, screens=" + windows.Count);
        }

        public void Shutdown()
        {
            if (hookTimer != null) hookTimer.Stop();
            if (rebuildTimer != null) rebuildTimer.Stop();
            if (trimTimer != null) trimTimer.Stop();
            Unlock();
            hook.Dispose();
        }

        public void Lock()
        {
            if (locked)
            {
                Log.Write("lock ignored: already locked");
                return;
            }

            AppConfig cfg = app.Config;
            if (!cfg.HasPassword)
            {
                Log.Write("lock refused: no password configured");
                app.Notify("还没有设置锁屏密码，无法锁定。", "请在设置中先设置密码。");
                return;
            }

            try
            {
                Auth.VerifyPassword(cfg, "");
            }
            catch (Exception ex)
            {
                Log.Write("lock refused: stored credential unusable: " + ex.Message);
                app.Notify("锁屏密码数据已损坏，为避免把你锁在门外，已取消这次锁定。", "请在设置中重新设置密码。");
                return;
            }

            List<LockWindow> created;
            try
            {
                created = CreateWindows(cfg);
            }
            catch (Exception ex)
            {
                Log.Write("lock failed: " + ex);
                app.Notify("锁定失败", ex.Message);
                return;
            }

            windows.Clear();
            windows.AddRange(created);
            locked = true;
            failures = 0;
            ResetKeys();
            try
            {
                for (int i = 0; i < windows.Count; i++) windows[i].ShowLock();
            }
            catch (Exception ex)
            {
                Log.Write("lock show failed: " + ex);
                Unlock();
                app.Notify("锁定失败", ex.Message);
                return;
            }
            Log.Write("locked, screens=" + windows.Count);
        }

        List<LockWindow> CreateWindows(AppConfig cfg)
        {
            List<LockWindow> created = new List<LockWindow>();
            try
            {
                Screen[] screens = Screen.AllScreens;
                for (int i = 0; i < screens.Length; i++)
                {
                    Screen screen = screens[i];
                    // 挑了背景图就不用再抓屏；图丢了就当没挑过，截图照旧能兜住。
                    bool custom = !string.IsNullOrEmpty(cfg.BackgroundImage) && System.IO.File.Exists(cfg.BackgroundImage);
                    BitmapSource bg = (cfg.BackgroundSnapshot && !custom) ? Snapshot.Capture(screen.Bounds, 0.25) : null;
                    bool interactive = screen.Primary || screens.Length == 1;
                    LockWindow lw = new LockWindow(screen.Bounds, bg, cfg, interactive);
                    if (interactive)
                    {
                        lw.SubmitPassword += OnPasswordSubmitted;
                        lw.SubmitRecovery += OnRecoverySubmitted;
                    }
                    created.Add(lw);
                }
                return created;
            }
            catch
            {
                for (int i = 0; i < created.Count; i++)
                {
                    try { created[i].Close(); } catch { }
                }
                throw;
            }
        }

        public void Unlock()
        {
            if (!locked) return;
            locked = false;
            foreach (LockWindow w in windows)
            {
                try { w.Close(); } catch (Exception ex) { Log.Write("close failed: " + ex.Message); }
            }
            windows.Clear();
            failures = 0;
            ResetKeys();
            if (punishTimer != null) punishTimer.Stop();
            Log.Write("unlocked");
            ScheduleMemoryTrim();
        }

        DispatcherTimer trimTimer;

        void ScheduleMemoryTrim()
        {
            if (trimTimer == null)
            {
                trimTimer = new DispatcherTimer();
                trimTimer.Interval = TimeSpan.FromMilliseconds(1200);
                trimTimer.Tick += delegate
                {
                    trimTimer.Stop();
                    Native.TrimWorkingSet();
                };
            }
            trimTimer.Stop();
            trimTimer.Start();
        }

        void ResetKeys()
        {
            ctrlDown = false;
            altDown = false;
            rightAlt = false;
            winDown = false;
            suppressWinUp = false;
            suppressLUp = false;
        }

        void OnPasswordSubmitted(string password)
        {
            LockWindow target = InteractiveWindow();
            if (target == null) return;

            if (password == null || password.Length == 0)
            {
                target.SetMessage("请输入密码", true);
                return;
            }

            bool ok;
            try
            {
                ok = Auth.VerifyPassword(app.Config, password);
            }
            catch (Exception ex)
            {
                Log.Write("verify failed: " + ex);
                target.SetMessage("校验出错，已自动解锁以免将你锁在门外", true);
                Unlock();
                return;
            }

            if (ok)
            {
                Unlock();
                return;
            }

            failures++;
            Log.Write("bad password, failures=" + failures);
            target.ClearInput();
            target.SetMessage("密码不正确", true);
            if (failures >= 3) Punish(target, failures);
        }

        void Punish(LockWindow target, int count)
        {
            int seconds = Math.Min(10, count - 2);
            target.SetInputEnabled(false);
            target.SetMessage("过于频繁，请等待 " + seconds + " 秒", true);
            if (punishTimer != null) punishTimer.Stop();
            punishTimer = new DispatcherTimer();
            punishTimer.Interval = TimeSpan.FromSeconds(seconds);
            punishTimer.Tick += delegate
            {
                punishTimer.Stop();
                target.SetInputEnabled(true);
                target.ClearInput();
                target.SetMessage("", false);
            };
            punishTimer.Start();
        }

        void OnRecoverySubmitted(string code)
        {
            LockWindow target = InteractiveWindow();
            if (target == null) return;

            bool ok;
            try
            {
                ok = Auth.VerifyRecovery(app.Config, code);
            }
            catch (Exception ex)
            {
                Log.Write("recovery verify failed: " + ex);
                target.SetMessage("校验出错，已自动解锁以免将你锁在门外", true);
                Unlock();
                return;
            }

            if (!ok)
            {
                failures++;
                Log.Write("bad recovery code, failures=" + failures);
                target.ClearInput();
                target.SetMessage("恢复码不正确", true);
                if (failures >= 3) Punish(target, failures);
                return;
            }

            Log.Write("unlocked with recovery code");
            Auth.ClearRecovery(app.Config);
            try { ConfigStore.Save(app.Config); } catch (Exception ex) { Log.Write("recovery invalidate save failed: " + ex.Message); }
            Unlock();
            app.OpenSettings(true);
        }

        bool HoldsForeground()
        {
            LockWindow target = InteractiveWindow();
            return target != null && target.HoldsForeground();
        }

        void ReclaimForeground()
        {
            if (unchecked((uint)(Environment.TickCount - lastReclaim)) < 400u) return;
            lastReclaim = Environment.TickCount;
            LockWindow target = InteractiveWindow();
            if (target == null) return;
            Log.Write("reclaim foreground");
            try { target.Reclaim(); } catch (Exception ex) { Log.Write("reclaim failed: " + ex.Message); }
        }

        LockWindow InteractiveWindow()
        {
            for (int i = 0; i < windows.Count; i++)
                if (windows[i].Interactive) return windows[i];
            return null;
        }

        void RequestLock()
        {
            if (locked || lockPending) return;
            lockPending = true;
            app.Dispatcher.BeginInvoke((Action)delegate
            {
                try { Lock(); }
                catch (Exception ex) { Log.Write("lock via hook failed: " + ex); }
                finally { lockPending = false; }
            });
        }

        public bool Filter(int vk, bool down)
        {
            AppConfig cfg = app.Config;

            if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)
            {
                ctrlDown = down;
                return down && locked && !HoldsForeground();
            }
            if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)
            {
                altDown = down;
                if (vk == VK_RMENU) rightAlt = down;
                return down && locked && !HoldsForeground();
            }

            bool isWin = vk == VK_LWIN || vk == VK_RWIN;

            if (locked && !HoldsForeground())
            {
                if (unchecked((uint)(Environment.TickCount - lastBlockedLog)) >= 2000u)
                {
                    lastBlockedLog = Environment.TickCount;
                    try { Log.Write("input blocked, overlay without foreground, current=" + Native.DescribeWindow(Native.GetForegroundWindow())); }
                    catch { }
                }
                ReclaimForeground();
                return true;
            }

            if (locked && cfg.BlockSystemHotkeys)
            {
                if (isWin)
                {
                    winDown = false;
                    return down;
                }
                if (winDown) return true;
                if (vk == VK_TAB && altDown) return true;
                if (vk == VK_ESCAPE && (ctrlDown || altDown)) return true;
                if (vk == VK_F4 && altDown) return true;
            }

            if (cfg.InterceptWinL && isWin)
            {
                if (down)
                {
                    winDown = true;
                }
                else
                {
                    winDown = false;
                    if (suppressWinUp)
                    {
                        suppressWinUp = false;
                        return true;
                    }
                }
                return false;
            }

            if (cfg.InterceptWinL && vk == VK_L && winDown)
            {
                if (down)
                {
                    suppressWinUp = true;
                    suppressLUp = true;
                    RequestLock();
                    return true;
                }
                if (suppressLUp)
                {
                    suppressLUp = false;
                    return true;
                }
                return true;
            }

            if (cfg.CustomHotkeyEnabled && vk == VK_L && ctrlDown && altDown && !rightAlt)
            {
                if (down) RequestLock();
                return true;
            }

            return false;
        }
    }
}
