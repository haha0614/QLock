using System;
using System.Diagnostics;
using System.Threading;
using System.Windows;
using System.Windows.Threading;

namespace QLock
{
    public static class Program
    {
        [STAThread]
        public static int Main(string[] args)
        {
            Log.Write("==== QLock start pid=" + Process.GetCurrentProcess().Id + " args=" + string.Join(" ", args));
            QLockApp app = new QLockApp(args);
            int code = app.Run();
            Log.Write("==== QLock exit code=" + code);
            return code;
        }
    }

    public class QLockApp : Application
    {
        const string MutexName = "Local\\QLock.Instance";
        const string LockEventName = "Local\\QLock.Cmd.Lock";
        const string SettingsEventName = "Local\\QLock.Cmd.Settings";
        const string RedisplayEventName = "Local\\QLock.Cmd.Redisplay";

        readonly string[] args;
        Mutex mutex;
        EventWaitHandle evLock;
        EventWaitHandle evSettings;
        EventWaitHandle evRedisplay;
        Thread listener;
        volatile bool stopping;
        DispatcherTimer idleTimer;
        TrayIcon tray;
        SettingsWindow settings;
        UpdateWindow updateWindow;

        public AppConfig Config;
        public LockController Controller;

        public QLockApp(string[] argv)
        {
            args = argv;
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            DispatcherUnhandledException += OnUnhandledException;
        }

        void OnUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            Log.Write("unhandled: " + e.Exception);
            e.Handled = true;
            if (Controller != null && Controller.IsLocked)
            {
                Log.Write("unlocking after unhandled exception");
                Controller.Unlock();
            }
        }

        bool HasArg(string name)
        {
            for (int i = 0; i < args.Length; i++)
                if (string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            if (HasArg("--demo")) { RunDemo(); return; }

            if (!AcquireInstance())
            {
                SignalExisting(CommandName());
                Shutdown(0);
                return;
            }

            Config = ConfigStore.Load();
            Updater.CleanupOld();
            try { Themes.EnsureSampleThemes(); } catch (Exception ex) { Log.Write("theme samples failed: " + ex.Message); }

            if (Config.RunElevated && !Native.IsElevated() && !HasArg("--elevated"))
            {
                ReleaseInstance();
                if (RelaunchElevated())
                {
                    Shutdown(0);
                    return;
                }
                if (!AcquireInstance())
                {
                    Shutdown(0);
                    return;
                }
            }

            CreateCommandEvents();

            Controller = new LockController(this);
            if (!Controller.InstallHook())
                Log.Write("keyboard hook install failed: " + Controller.HookError);
            Controller.StartHookWatchdog();
            try { Microsoft.Win32.SystemEvents.DisplaySettingsChanged += OnDisplayChanged; }
            catch (Exception ex) { Log.Write("display watch failed: " + ex.Message); }
            try { Microsoft.Win32.SystemEvents.PowerModeChanged += OnPowerModeChanged; }
            catch (Exception ex) { Log.Write("power watch failed: " + ex.Message); }

            if (Config.ShowTray)
            {
                tray = new TrayIcon(LockNow, ShowSettings, ExitApplication);
                UpdateTray();
            }

            SetUpIdleTimer();

            if (HasArg("--lock"))
            {
                LockNow();
                StartAutoUnlock();
            }
            else if (HasArg("--settings")) ShowSettings();
            else if (!Config.HasPassword) OpenSettings(true);
            else if (Config.LockOnStartup) LockNow();

            if (HasArg("--check-update")) CheckForUpdates(true);
            else ScheduleUpdateCheck();
        }

        void ScheduleUpdateCheck()
        {
            if (!Updater.ShouldAutoCheck(Config)) return;
            DispatcherTimer timer = new DispatcherTimer();
            timer.Interval = TimeSpan.FromSeconds(8);
            timer.Tick += delegate
            {
                if (Controller != null && Controller.IsLocked)
                {
                    timer.Interval = TimeSpan.FromSeconds(30);
                    return;
                }
                timer.Stop();
                CheckForUpdates(false);
            };
            timer.Start();
        }

        public void CheckForUpdates(bool manual)
        {
            if (Controller != null && Controller.IsLocked)
            {
                if (manual) Notify("锁屏中不检查更新", "解锁之后再试。");
                return;
            }
            Log.Write("update: checking (" + (manual ? "manual" : "auto") + ")");
            Thread worker = new Thread(delegate ()
            {
                ReleaseInfo info = null;
                string error = null;
                try { info = Updater.Check(); }
                catch (Exception ex) { error = Updater.DescribeWebError(ex); }
                Dispatcher.BeginInvoke(new Action(delegate { OnUpdateChecked(info, error, manual); }));
            });
            worker.IsBackground = true;
            worker.Start();
        }

        void OnUpdateChecked(ReleaseInfo info, string error, bool manual)
        {
            if (error != null)
            {
                Log.Write("update check failed: " + error);
                if (manual)
                    MessageBox.Show("检查更新失败：" + error, "QLock 更新", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            Version current = Updater.CurrentVersion();
            bool newer = Updater.IsNewer(info.Tag, current);
            Log.Write("update: latest=" + info.Tag + " current=" + Updater.VersionText(current) + " newer=" + newer);

            Updater.MarkChecked(Config);
            ConfigStore.Save(Config);

            if (!newer)
            {
                if (manual)
                    MessageBox.Show("已经是最新版本 " + Updater.VersionText(current) + "。", "QLock 更新", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }
            if (!manual && string.Equals(info.Tag, Config.SkippedVersion, StringComparison.OrdinalIgnoreCase)) return;
            OpenUpdateWindow(info);
        }

        void OpenUpdateWindow(ReleaseInfo info)
        {
            if (updateWindow != null)
            {
                updateWindow.Win.Activate();
                return;
            }
            UpdateWindow w = new UpdateWindow(this, info, SkipVersion);
            w.Win.Closed += delegate { updateWindow = null; };
            updateWindow = w;
            w.Win.Show();
            w.Win.Activate();
        }

        public void SkipVersion(string tag)
        {
            Config.SkippedVersion = tag == null ? "" : tag;
            ConfigStore.Save(Config);
            Log.Write("update: skipped " + tag);
        }

        public void RestartForUpdate()
        {
            if (stopping) return;
            stopping = true;
            Log.Write("update: handing over to the new version");

            if (idleTimer != null) idleTimer.Stop();
            if (Controller != null) Controller.Shutdown();
            if (tray != null)
            {
                tray.Dispose();
                tray = null;
            }

            ReleaseInstance();
            try { Updater.StartInstalled(); }
            catch (Exception ex)
            {
                Log.Write("update: restart failed: " + ex.Message);
                MessageBox.Show("新版本已经装好了，但启动失败：" + ex.Message + "\n请手动打开 QLock。", "QLock 更新", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
            Shutdown(0);
        }

        void StartAutoUnlock()
        {
            int seconds = IntArg("--autounlock", 0);
            if (seconds <= 0) return;
            Log.Write("autounlock armed: " + seconds + "s");
            DispatcherTimer timer = new DispatcherTimer();
            timer.Interval = TimeSpan.FromSeconds(seconds);
            timer.Tick += delegate
            {
                timer.Stop();
                Log.Write("autounlock firing");
                try { Controller.Unlock(); } catch { }
                if (HasArg("--exitafter")) ExitApplication();
            };
            timer.Start();
        }

        int IntArg(string name, int fallback)
        {
            for (int i = 0; i < args.Length - 1; i++)
            {
                if (!string.Equals(args[i], name, StringComparison.OrdinalIgnoreCase)) continue;
                int value;
                if (int.TryParse(args[i + 1], out value)) return value;
            }
            return fallback;
        }

        bool AcquireInstance()
        {
            bool created;
            mutex = new Mutex(true, MutexName, out created);
            if (created) return true;
            try { mutex.Dispose(); } catch { }
            mutex = null;

            if (!HasArg("--elevated")) return false;

            for (int i = 0; i < 40; i++)
            {
                Thread.Sleep(150);
                bool createdNow;
                mutex = new Mutex(true, MutexName, out createdNow);
                if (createdNow) return true;
                try { mutex.Dispose(); } catch { }
                mutex = null;
            }
            return false;
        }

        void ReleaseInstance()
        {
            if (mutex == null) return;
            try { mutex.ReleaseMutex(); } catch { }
            try { mutex.Dispose(); } catch { }
            mutex = null;
        }

        void OnDisplayChanged(object sender, EventArgs e)
        {
            try
            {
                Dispatcher.BeginInvoke(new Action(delegate
                {
                    if (Controller != null) Controller.OnDisplayChanged();
                }));
            }
            catch (Exception ex) { Log.Write("display change failed: " + ex.Message); }
        }

        string CommandName()
        {
            if (HasArg("--lock")) return LockEventName;
            if (HasArg("--redisplay")) return RedisplayEventName;
            return SettingsEventName;
        }

        void OnPowerModeChanged(object sender, Microsoft.Win32.PowerModeChangedEventArgs e)
        {
            if (e.Mode != Microsoft.Win32.PowerModes.Resume) return;
            try
            {
                Dispatcher.BeginInvoke(new Action(delegate
                {
                    if (Controller != null) Controller.OnPowerResumed();
                }));
            }
            catch (Exception ex) { Log.Write("power mode change failed: " + ex.Message); }
        }

        void DebugRedisplay()
        {
            Log.Write("diagnostic: simulated display change");
            if (Controller != null) Controller.OnDisplayChanged();
        }

        void SignalExisting(string name)
        {
            try
            {
                EventWaitHandle h = null;
                for (int i = 0; i < 12 && h == null; i++)
                {
                    try { h = EventWaitHandle.OpenExisting(name); }
                    catch (WaitHandleCannotBeOpenedException) { Thread.Sleep(150); }
                }
                if (h == null) throw new InvalidOperationException("command events never appeared: " + name);
                using (h) h.Set();
            }
            catch (Exception ex)
            {
                Log.Write("signal failed: " + ex.Message);
            }
        }

        void CreateCommandEvents()
        {
            evLock = new EventWaitHandle(false, EventResetMode.AutoReset, LockEventName);
            evSettings = new EventWaitHandle(false, EventResetMode.AutoReset, SettingsEventName);
            evRedisplay = new EventWaitHandle(false, EventResetMode.AutoReset, RedisplayEventName);

            listener = new Thread(delegate ()
            {
                try
                {
                    WaitHandle[] handles = new WaitHandle[] { evLock, evSettings, evRedisplay };
                    while (!stopping)
                    {
                        int index = WaitHandle.WaitAny(handles, 800);
                        if (stopping) break;
                        if (index == 0) Dispatcher.BeginInvoke(new Action(LockNow));
                        else if (index == 1) Dispatcher.BeginInvoke(new Action(ShowSettings));
                        else if (index == 2) Dispatcher.BeginInvoke(new Action(DebugRedisplay));
                    }
                }
                catch (Exception ex)
                {
                    Log.Write("command listener stopped: " + ex.Message);
                }
            });
            listener.IsBackground = true;
            listener.Start();
        }

        bool RelaunchElevated()
        {
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(Autostart.ExePath(), "--elevated --tray");
                psi.UseShellExecute = true;
                psi.Verb = "runas";
                Process.Start(psi);
                Log.Write("relaunched elevated");
                return true;
            }
            catch (Exception ex)
            {
                System.ComponentModel.Win32Exception w32 = ex as System.ComponentModel.Win32Exception;
                if (w32 != null && w32.NativeErrorCode == 1223)
                    Log.Write("elevation cancelled by user, continuing without admin rights");
                else
                    Log.Write("elevation refused: " + ex.Message);
                return false;
            }
        }

        void SetUpIdleTimer()
        {
            if (idleTimer != null)
            {
                idleTimer.Stop();
                idleTimer = null;
            }
            if (Config == null || Config.IdleLockMinutes <= 0) return;

            idleTimer = new DispatcherTimer();
            idleTimer.Interval = TimeSpan.FromSeconds(10);
            idleTimer.Tick += delegate { CheckIdle(); };
            idleTimer.Start();
        }

        void CheckIdle()
        {
            if (Config == null || Config.IdleLockMinutes <= 0) return;
            if (Controller == null || Controller.IsLocked) return;
            if (settings != null && settings.Win.IsVisible) return;
            if (Native.IdleTime().TotalMinutes >= Config.IdleLockMinutes)
            {
                Log.Write("idle lock triggered");
                LockNow();
            }
        }

        public void LockNow()
        {
            if (Controller == null) return;
            Controller.Lock();
            UpdateTray();
        }

        public void ShowSettings()
        {
            OpenSettings(false);
        }

        public void OpenSettings(bool firstRun)
        {
            if (settings != null)
            {
                settings.Win.Activate();
                return;
            }
            SettingsWindow w = new SettingsWindow(Config, firstRun, OnConfigSaved, delegate { CheckForUpdates(true); });
            w.Win.Closed += delegate { settings = null; };
            settings = w;
            w.Win.Show();
            w.Win.Activate();
        }

        void OnConfigSaved(AppConfig next)
        {
            Config = next;
            SetUpIdleTimer();
            UpdateTray();
            if (Config.RunElevated && !Native.IsElevated())
                Notify("需要重启生效", "已开启「以管理员身份运行」，下次启动时申请管理员权限。");
        }

        void UpdateTray()
        {
            if (tray == null) return;
            bool locked = Controller != null && Controller.IsLocked;
            tray.SetTooltip(locked ? "QLock：屏幕已锁定" : "QLock：运行中（Ctrl+Alt+L 锁定）");
        }

        public void Notify(string title, string text)
        {
            if (tray != null) tray.ShowBalloon(title, text);
            else Log.Write("notify: " + title + " / " + text);
        }

        void ExitApplication()
        {
            if (stopping) return;
            stopping = true;

            if (idleTimer != null) idleTimer.Stop();
            if (Controller != null) Controller.Shutdown();
            if (tray != null)
            {
                tray.Dispose();
                tray = null;
            }
            Shutdown(0);
        }

        protected override void OnExit(ExitEventArgs e)
        {
            stopping = true;
            try { if (Controller != null) Controller.Shutdown(); } catch { }
            try { if (tray != null) { tray.Dispose(); tray = null; } } catch { }
            ReleaseInstance();
            try { Microsoft.Win32.SystemEvents.DisplaySettingsChanged -= OnDisplayChanged; } catch { }
            try { Microsoft.Win32.SystemEvents.PowerModeChanged -= OnPowerModeChanged; } catch { }
            base.OnExit(e);
        }

        void RunDemo()
        {
            AppConfig cfg = ConfigStore.Load();
            System.Drawing.Rectangle bounds = System.Windows.Forms.Screen.PrimaryScreen.Bounds;
            System.Windows.Media.Imaging.BitmapSource bg = Snapshot.Capture(bounds, 0.25);
            LockWindow lw = new LockWindow(bounds, bg, cfg, true);
            lw.SubmitPassword += delegate (string p)
            {
                lw.SetMessage("演示模式：不校验密码（" + p.Length + " 字符）", false);
                lw.ClearInput();
            };
            lw.ShowLock();
            DispatcherTimer t = new DispatcherTimer();
            t.Interval = TimeSpan.FromSeconds(10);
            t.Tick += delegate
            {
                t.Stop();
                lw.Close();
                Shutdown(0);
            };
            t.Start();
        }
    }
}
