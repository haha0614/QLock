using System;
using System.IO;

namespace QLock
{
    public static class TestMain
    {
        static int failures;

        static void Check(bool ok, string name)
        {
            Console.WriteLine((ok ? "PASS  " : "FAIL  ") + name);
            if (!ok) failures++;
        }

        [STAThread]
        public static int Main(string[] args)
        {
            Console.WriteLine("== QLock tests ==");

            byte[] salt = Auth.NewSalt();
            Check(salt.Length == 16, "salt length 16");
            string h = Auth.Hash64("hunter2", salt, 1000);
            Check(h.Length > 20, "hash produced");
            Check(Auth.Verify("hunter2", h, Convert.ToBase64String(salt), 1000), "verify correct password");
            Check(!Auth.Verify("hunter3", h, Convert.ToBase64String(salt), 1000), "reject wrong password");

            DateTime t0 = DateTime.UtcNow;
            byte[] s2 = Auth.NewSalt();
            Auth.Hash64("bench", s2, Auth.DefaultIterations);
            double ms = (DateTime.UtcNow - t0).TotalMilliseconds;
            Console.WriteLine("info  default iterations cost " + ms.ToString("0") + " ms");
            Check(Auth.Verify("bench", Auth.Hash64("bench", s2, Auth.DefaultIterations), Convert.ToBase64String(s2), Auth.DefaultIterations), "default iterations roundtrip");

            string code = Auth.NewRecoveryCode();
            Check(code.Length == 11 && code[5] == '-', "recovery code shape " + code);
            Check(Auth.NormalizeRecoveryCode(code.ToLowerInvariant().Replace("-", " ")) ==
                  Auth.NormalizeRecoveryCode(code), "recovery code normalization");

            AppConfig cfg = new AppConfig();
            Auth.SetPassword(cfg, "p@ss");
            Auth.SetRecovery(cfg, code);
            Check(Auth.VerifyPassword(cfg, "p@ss"), "cfg verify password");
            Check(Auth.VerifyRecovery(cfg, code), "cfg verify recovery");
            Check(cfg.HasPassword, "cfg has password");
            Check(Auth.StoredPassword(cfg) == "p@ss", "stored password reads back");

            string dir = Path.Combine(Path.GetTempPath(), "qlock-test-" + Guid.NewGuid().ToString("N"));
            ConfigStore.SetOverrideDir(dir);
            ConfigStore.Save(cfg);
            AppConfig loaded = ConfigStore.Load();
            Check(loaded.PasswordHash == cfg.PasswordHash, "config roundtrip hash");
            Check(Auth.VerifyPassword(loaded, "p@ss"), "loaded config verifies");
            Check(loaded.InterceptWinL && loaded.ShowTray && loaded.BlockSystemHotkeys, "config roundtrip keeps flags");

            AppConfig cfg2 = new AppConfig();
            Auth.SetPassword(cfg2, "pw");
            Auth.SetRecovery(cfg2, code);
            cfg2.PasswordIterations = 1000;
            Check(Auth.VerifyRecovery(cfg2, code), "recovery verify ignores password iterations");
            Auth.ClearRecovery(cfg2);
            Check(!Auth.VerifyRecovery(cfg2, code), "recovery code invalidated after use");
            Check(Auth.VerifyPassword(cfg2, "pw") == false, "password still enforced once recovery cleared");

            AppConfig partial = AppConfig.Deserialize("{\"Version\":1,\"PasswordHash\":\"x\",\"PasswordSalt\":\"y\"}");
            Check(partial.InterceptWinL && partial.BlockSystemHotkeys && partial.ShowTray, "partial config keeps hardening defaults");
            Check(!partial.BackgroundSnapshot && partial.ShowClock && partial.ShowSubtitle && partial.CustomHotkeyEnabled, "partial config keeps appearance defaults");
            Check(partial.PasswordIterations == Auth.DefaultIterations, "partial config keeps iteration count");
            Check(partial.PasswordCipher == "", "partial config keeps empty password cipher");
            Check(partial.Autostart == "none", "partial config keeps autostart default");
            Check(partial.LastUpdateCheckTicks == 0 && partial.SkippedVersion == "", "partial config keeps update defaults");
            Check(!partial.ClockFontEnabled && partial.FontFamily == "" && partial.ClockFont == "", "partial config keeps font defaults");
            Check(partial.ClockFontName == "", "clock font falls back to the unified font");
            Directory.Delete(dir, true);
            ConfigStore.SetOverrideDir(null);

            Version current = new Version(0, 1, 0, 0);
            Check(Updater.IsNewer("v0.2.0", current), "version: v0.2.0 is newer than 0.1.0");
            Check(Updater.IsNewer("0.1.1", current), "version: 0.1.1 is newer than 0.1.0");
            Check(!Updater.IsNewer("v0.1.0", current), "version: same version is not newer");
            Check(!Updater.IsNewer("v0.0.9", current), "version: older version is not newer");
            Check(!Updater.IsNewer("nightly", current), "version: unparsable tag is not newer");
            Version parsed;
            Check(Updater.TryParseTag("v1.2.3", out parsed) && parsed.Major == 1 && parsed.Minor == 2 && parsed.Build == 3, "version: tag parsing");

            Console.WriteLine(failures == 0 ? "== ALL PASS ==" : "== " + failures + " FAILED ==");
            return failures == 0 ? 0 : 1;
        }
    }
}
