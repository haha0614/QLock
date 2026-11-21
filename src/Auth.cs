using System;
using System.Security.Cryptography;
using System.Text;

namespace QLock
{
    public static class Auth
    {
        public const int DefaultIterations = 120000;
        public const int SaltBytes = 16;
        public const int KeyBytes = 32;

        const string CodeAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

        public static byte[] NewSalt()
        {
            byte[] salt = new byte[SaltBytes];
            using (RNGCryptoServiceProvider rng = new RNGCryptoServiceProvider()) rng.GetBytes(salt);
            return salt;
        }

        public static byte[] Derive(string password, byte[] salt, int iterations, int length)
        {
            using (Rfc2898DeriveBytes k = new Rfc2898DeriveBytes(password, salt, iterations, HashAlgorithmName.SHA256))
                return k.GetBytes(length);
        }

        public static string Hash64(string password, byte[] salt, int iterations)
        {
            return Convert.ToBase64String(Derive(password, salt, iterations, KeyBytes));
        }

        public static bool Verify(string password, string hashBase64, string saltBase64, int iterations)
        {
            if (string.IsNullOrEmpty(hashBase64) || string.IsNullOrEmpty(saltBase64)) return false;
            byte[] expected;
            byte[] salt;
            try
            {
                expected = Convert.FromBase64String(hashBase64);
                salt = Convert.FromBase64String(saltBase64);
            }
            catch { return false; }
            byte[] actual = Derive(password ?? "", salt, iterations, expected.Length);
            return FixedTimeEquals(expected, actual);
        }

        static bool FixedTimeEquals(byte[] a, byte[] b)
        {
            if (a == null || b == null || a.Length != b.Length) return false;
            int diff = 0;
            for (int i = 0; i < a.Length; i++) diff |= a[i] ^ b[i];
            return diff == 0;
        }

        public static string NewRecoveryCode()
        {
            byte[] raw = new byte[10];
            using (RNGCryptoServiceProvider rng = new RNGCryptoServiceProvider()) rng.GetBytes(raw);
            StringBuilder sb = new StringBuilder(12);
            for (int i = 0; i < raw.Length; i++)
            {
                if (i == 5) sb.Append('-');
                sb.Append(CodeAlphabet[raw[i] % CodeAlphabet.Length]);
            }
            return sb.ToString();
        }

        public static string NormalizeRecoveryCode(string code)
        {
            if (code == null) return "";
            StringBuilder sb = new StringBuilder(code.Length);
            foreach (char ch in code.ToUpperInvariant())
            {
                if (ch == '-' || ch == ' ') continue;
                if (ch == 'O') sb.Append('0');
                else if (ch == 'I' || ch == 'L') sb.Append('1');
                else sb.Append(ch);
            }
            return sb.ToString();
        }

        public static void SetPassword(AppConfig cfg, string password)
        {
            byte[] salt = NewSalt();
            cfg.PasswordIterations = DefaultIterations;
            cfg.PasswordSalt = Convert.ToBase64String(salt);
            cfg.PasswordHash = Hash64(password, salt, DefaultIterations);
        }

        public static void SetRecovery(AppConfig cfg, string code)
        {
            byte[] salt = NewSalt();
            cfg.RecoverySalt = Convert.ToBase64String(salt);
            cfg.RecoveryHash = Hash64(NormalizeRecoveryCode(code), salt, DefaultIterations);
        }

        public static void ClearRecovery(AppConfig cfg)
        {
            cfg.RecoveryHash = "";
            cfg.RecoverySalt = "";
        }

        public static bool VerifyPassword(AppConfig cfg, string password)
        {
            return Verify(password, cfg.PasswordHash, cfg.PasswordSalt, cfg.PasswordIterations);
        }

        public static bool VerifyRecovery(AppConfig cfg, string code)
        {
            return Verify(NormalizeRecoveryCode(code), cfg.RecoveryHash, cfg.RecoverySalt, DefaultIterations);
        }
    }
}
