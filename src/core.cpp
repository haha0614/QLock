#pragma once
#include "qlock.h"
#include <shlobj.h>
#include <dpapi.h>
#include <bcrypt.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace app {
Config cfg;
HINSTANCE inst = nullptr;
bool locked = false;

static std::wstring Env(const wchar_t* name) {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD n = GetEnvironmentVariableW(name, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return std::wstring();
    return std::wstring(buf, n);
}

std::wstring DataDir() {
    std::wstring dir = Env(L"APPDATA");
    if (dir.empty()) return L".";
    return dir + L"\\QLock";
}

std::wstring LocalDir() {
    std::wstring dir = Env(L"LOCALAPPDATA");
    if (dir.empty()) return L".";
    return dir + L"\\QLock";
}

std::wstring ConfigPath() { return DataDir() + L"\\config.json"; }
std::wstring LogPath() { return LocalDir() + L"\\qlock.log"; }
std::wstring ThemeDir() { return DataDir() + L"\\themes"; }

static std::string Utf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], n, nullptr, nullptr);
    return out;
}

void Log(const wchar_t* fmt, ...) {
    wchar_t line[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(line, _countof(line), _TRUNCATE, fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t stamp[64] = {};
    _snwprintf_s(stamp, _countof(stamp), _TRUNCATE, L"%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    CreateDirectoryW(LocalDir().c_str(), nullptr);
    std::ofstream out(LogPath().c_str(), std::ios::binary | std::ios::app);
    if (!out) return;
    std::string text = Utf8(std::wstring(stamp) + line + L"\r\n");
    out.write(text.data(), (std::streamsize)text.size());
}

std::wstring UiFontName() {
    return cfg.FontFamily.empty() ? L"Microsoft YaHei UI" : cfg.FontFamily;
}

std::wstring ClockFontName() {
    if (cfg.ClockFontEnabled && !cfg.ClockFont.empty()) return cfg.ClockFont;
    return UiFontName();
}

static std::wstring ReadAll(const std::wstring& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::wstring();
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF)
        bytes.erase(0, 3);
    if (bytes.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), (int)bytes.size(), nullptr, 0);
    std::wstring out((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), (int)bytes.size(), &out[0], n);
    return out;
}

bool LoadConfig() {
    std::wstring text = ReadAll(ConfigPath());
    if (text.empty()) {
        Log(L"config: not found, using defaults");
        return false;
    }
    std::map<std::wstring, std::wstring> kv;
    if (!json::ParseObject(text, kv)) {
        Log(L"config: parse failed, using defaults");
        return false;
    }
    cfg.Version = json::GetInt(kv[L"Version"], 1);
    if (kv.count(L"Autostart")) cfg.Autostart = json::Unescape(kv[L"Autostart"]);
    if (kv.count(L"BackgroundImage")) cfg.BackgroundImage = json::Unescape(kv[L"BackgroundImage"]);
    cfg.BackgroundSnapshot = json::GetBool(kv[L"BackgroundSnapshot"], cfg.BackgroundSnapshot);
    if (kv.count(L"BlurRadius")) cfg.BlurRadius = json::GetInt(kv[L"BlurRadius"], cfg.BlurRadius);
    cfg.BlockSystemHotkeys = json::GetBool(kv[L"BlockSystemHotkeys"], cfg.BlockSystemHotkeys);
    if (kv.count(L"ClockFont")) cfg.ClockFont = json::Unescape(kv[L"ClockFont"]);
    cfg.ClockFontEnabled = json::GetBool(kv[L"ClockFontEnabled"], cfg.ClockFontEnabled);
    if (kv.count(L"CornerRadius")) cfg.CornerRadius = json::GetInt(kv[L"CornerRadius"], cfg.CornerRadius);
    cfg.CustomHotkeyEnabled = json::GetBool(kv[L"CustomHotkeyEnabled"], cfg.CustomHotkeyEnabled);
    if (kv.count(L"FontFamily")) cfg.FontFamily = json::Unescape(kv[L"FontFamily"]);
    cfg.IdleLockMinutes = json::GetInt(kv[L"IdleLockMinutes"], cfg.IdleLockMinutes);
    cfg.InterceptWinL = json::GetBool(kv[L"InterceptWinL"], cfg.InterceptWinL);
    cfg.LastUpdateCheckTicks = json::GetLongLong(kv[L"LastUpdateCheckTicks"], cfg.LastUpdateCheckTicks);
    cfg.LockOnStartup = json::GetBool(kv[L"LockOnStartup"], cfg.LockOnStartup);
    if (kv.count(L"MaskOpacity")) cfg.MaskOpacity = json::GetInt(kv[L"MaskOpacity"], cfg.MaskOpacity);
    if (kv.count(L"PasswordCipher")) cfg.PasswordCipher = json::Unescape(kv[L"PasswordCipher"]);
    if (kv.count(L"PasswordHash")) cfg.PasswordHash = json::Unescape(kv[L"PasswordHash"]);
    cfg.PasswordIterations = json::GetInt(kv[L"PasswordIterations"], cfg.PasswordIterations);
    if (kv.count(L"PasswordSalt")) cfg.PasswordSalt = json::Unescape(kv[L"PasswordSalt"]);
    if (kv.count(L"RecoveryHash")) cfg.RecoveryHash = json::Unescape(kv[L"RecoveryHash"]);
    if (kv.count(L"RecoverySalt")) cfg.RecoverySalt = json::Unescape(kv[L"RecoverySalt"]);
    cfg.RunElevated = json::GetBool(kv[L"RunElevated"], cfg.RunElevated);
    cfg.ShowClock = json::GetBool(kv[L"ShowClock"], cfg.ShowClock);
    cfg.ShowSubtitle = json::GetBool(kv[L"ShowSubtitle"], cfg.ShowSubtitle);
    cfg.ShowTray = json::GetBool(kv[L"ShowTray"], cfg.ShowTray);
    if (kv.count(L"SkippedVersion")) cfg.SkippedVersion = json::Unescape(kv[L"SkippedVersion"]);
    if (kv.count(L"Subtitle")) cfg.Subtitle = json::Unescape(kv[L"Subtitle"]);
    if (kv.count(L"ThemeId")) cfg.ThemeId = json::Unescape(kv[L"ThemeId"]);
    Log(L"config: loaded %s", ConfigPath().c_str());
    return true;
}

bool SaveConfig() {
    std::wstringstream out;
    out << L"{\"Autostart\":" << json::Quote(cfg.Autostart)
        << L",\"BackgroundImage\":" << json::Quote(cfg.BackgroundImage)
        << L",\"BackgroundSnapshot\":" << (cfg.BackgroundSnapshot ? L"true" : L"false")
        << L",\"BlurRadius\":" << cfg.BlurRadius
        << L",\"BlockSystemHotkeys\":" << (cfg.BlockSystemHotkeys ? L"true" : L"false")
        << L",\"ClockFont\":" << json::Quote(cfg.ClockFont)
        << L",\"ClockFontEnabled\":" << (cfg.ClockFontEnabled ? L"true" : L"false")
        << L",\"CornerRadius\":" << cfg.CornerRadius
        << L",\"CustomHotkeyEnabled\":" << (cfg.CustomHotkeyEnabled ? L"true" : L"false")
        << L",\"FontFamily\":" << json::Quote(cfg.FontFamily)
        << L",\"IdleLockMinutes\":" << cfg.IdleLockMinutes
        << L",\"InterceptWinL\":" << (cfg.InterceptWinL ? L"true" : L"false")
        << L",\"LastUpdateCheckTicks\":" << cfg.LastUpdateCheckTicks
        << L",\"LockOnStartup\":" << (cfg.LockOnStartup ? L"true" : L"false")
        << L",\"MaskOpacity\":" << cfg.MaskOpacity
        << L",\"PasswordCipher\":" << json::Quote(cfg.PasswordCipher)
        << L",\"PasswordHash\":" << json::Quote(cfg.PasswordHash)
        << L",\"PasswordIterations\":" << cfg.PasswordIterations
        << L",\"PasswordSalt\":" << json::Quote(cfg.PasswordSalt)
        << L",\"RecoveryHash\":" << json::Quote(cfg.RecoveryHash)
        << L",\"RecoverySalt\":" << json::Quote(cfg.RecoverySalt)
        << L",\"RunElevated\":" << (cfg.RunElevated ? L"true" : L"false")
        << L",\"ShowClock\":" << (cfg.ShowClock ? L"true" : L"false")
        << L",\"ShowSubtitle\":" << (cfg.ShowSubtitle ? L"true" : L"false")
        << L",\"ShowTray\":" << (cfg.ShowTray ? L"true" : L"false")
        << L",\"SkippedVersion\":" << json::Quote(cfg.SkippedVersion)
        << L",\"Subtitle\":" << json::Quote(cfg.Subtitle)
        << L",\"ThemeId\":" << json::Quote(cfg.ThemeId)
        << L",\"Version\":" << cfg.Version
        << L"}";

    std::wstring text = out.str();
    std::wstring dir = DataDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring temp = ConfigPath() + L".tmp";
    {
        std::ofstream file(temp.c_str(), std::ios::binary | std::ios::trunc);
        if (!file) return false;
        std::string bytes = Utf8(text);
        file.write(bytes.data(), (std::streamsize)bytes.size());
    }
    if (!MoveFileExW(temp.c_str(), ConfigPath().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Log(L"config: save failed err=%lu", GetLastError());
        return false;
    }
    return true;
}

bool HasPassword() {
    return !cfg.PasswordHash.empty() && !cfg.PasswordSalt.empty();
}

bool VerifyPassword(const std::wstring& password) {
    if (!HasPassword()) return false;
    std::vector<BYTE> salt;
    std::vector<BYTE> expect;
    if (!b64::Decode(cfg.PasswordSalt, salt) || !b64::Decode(cfg.PasswordHash, expect)) return false;

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;

    std::string bytes = Utf8(password);
    std::vector<BYTE> actual(expect.size());
    NTSTATUS st = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)bytes.data(), (ULONG)bytes.size(), salt.data(), (ULONG)salt.size(),
                                        (ULONGLONG)cfg.PasswordIterations, actual.data(), (ULONG)actual.size(), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st < 0) return false;
    if (actual.size() != expect.size()) return false;
    BYTE diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) diff |= (BYTE)(actual[i] ^ expect[i]);
    return diff == 0;
}

static std::vector<BYTE> Derive(const std::wstring& password, const std::vector<BYTE>& salt, int iterations, size_t bytes) {
    std::vector<BYTE> out(bytes);
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return out;
    std::string raw = Utf8(password);
    BCryptDeriveKeyPBKDF2(alg, (PUCHAR)raw.data(), (ULONG)raw.size(), (PUCHAR)salt.data(), (ULONG)salt.size(),
                          (ULONGLONG)iterations, out.data(), (ULONG)out.size(), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

// 另存一份 DPAPI 加密的明文，只给设置页回显用；校验只看上面的 Hash。
static std::wstring Protect(const std::wstring& text) {
    std::string bytes = Utf8(text);
    DATA_BLOB blobIn = { (DWORD)bytes.size(), (BYTE*)bytes.data() };
    DATA_BLOB blobOut = {};
    if (!CryptProtectData(&blobIn, L"QLock", nullptr, nullptr, nullptr, 0, &blobOut)) return L"";
    std::wstring result = b64::Encode(blobOut.pbData, blobOut.cbData);
    LocalFree(blobOut.pbData);
    return result;
}

void SetPassword(const std::wstring& password) {
    std::vector<BYTE> salt(16);
    if (BCryptGenRandom(nullptr, salt.data(), (ULONG)salt.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return;
    cfg.PasswordIterations = 120000;
    std::vector<BYTE> hash = Derive(password, salt, cfg.PasswordIterations, 32);
    cfg.PasswordSalt = b64::Encode(salt.data(), salt.size());
    cfg.PasswordHash = b64::Encode(hash.data(), hash.size());
    cfg.PasswordCipher = Protect(password);
    SaveConfig();
    Log(L"password updated");
}

std::wstring StoredPassword() {
    std::vector<BYTE> cipher;
    if (!b64::Decode(cfg.PasswordCipher, cipher) || cipher.empty()) return L"";
    DATA_BLOB blobIn = { (DWORD)cipher.size(), cipher.data() };
    DATA_BLOB blobOut = {};
    if (!CryptUnprotectData(&blobIn, nullptr, nullptr, nullptr, nullptr, 0, &blobOut)) return L"";
    const char* bytes = (const char*)blobOut.pbData;
    int len = (int)blobOut.cbData;
    int n = MultiByteToWideChar(CP_UTF8, 0, bytes, len, nullptr, 0);
    std::wstring result((size_t)(std::max)(0, n), 0);
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, bytes, len, &result[0], n);
    LocalFree(blobOut.pbData);
    return result;
}
static const wchar_t* kCodeAlphabet = L"0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// 去掉分隔符、转大写，校验和写盘都用这一份。
static std::wstring NormalizeCode(const std::wstring& code) {
    std::wstring out;
    out.reserve(code.size());
    for (size_t i = 0; i < code.size(); ++i) {
        wchar_t ch = (wchar_t)towupper(code[i]);
        if (ch == L'-' || ch == L' ') continue;
        out.push_back(ch);
    }
    return out;
}

std::wstring NewRecoveryCode() {
    BYTE raw[10] = {};
    if (BCryptGenRandom(nullptr, raw, (ULONG)sizeof(raw), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return L"";
    std::wstring code;
    for (int i = 0; i < 10; ++i) {
        if (i == 5) code.push_back(L'-');
        code.push_back(kCodeAlphabet[raw[i] % 32]);
    }
    return code;
}

void SetRecovery(const std::wstring& code) {
    std::vector<BYTE> salt(16);
    if (BCryptGenRandom(nullptr, salt.data(), (ULONG)salt.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return;
    std::vector<BYTE> hash = Derive(NormalizeCode(code), salt, 120000, 32);
    cfg.RecoverySalt = b64::Encode(salt.data(), salt.size());
    cfg.RecoveryHash = b64::Encode(hash.data(), hash.size());
    SaveConfig();
    Log(L"recovery code set");
}

void ClearRecovery() {
    cfg.RecoveryHash.clear();
    cfg.RecoverySalt.clear();
}

bool HasRecovery() {
    return !cfg.RecoveryHash.empty() && !cfg.RecoverySalt.empty();
}

bool VerifyRecovery(const std::wstring& code) {
    if (!HasRecovery()) return false;
    std::vector<BYTE> salt;
    std::vector<BYTE> expect;
    if (!b64::Decode(cfg.RecoverySalt, salt) || !b64::Decode(cfg.RecoveryHash, expect)) return false;
    std::vector<BYTE> actual = Derive(NormalizeCode(code), salt, 120000, expect.size());
    if (actual.size() != expect.size()) return false;
    BYTE diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) diff |= (BYTE)(actual[i] ^ expect[i]);
    return diff == 0;
}
} // namespace app

namespace json {
static void SkipWs(const std::wstring& s, size_t& i) {
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == L'\r' || s[i] == L'\n')) ++i;
}

static bool ParseString(const std::wstring& s, size_t& i, std::wstring& raw) {
    if (i >= s.size() || s[i] != L'"') return false;
    size_t start = i++;
    while (i < s.size()) {
        if (s[i] == L'\\') { i += 2; continue; }
        if (s[i] == L'"') { ++i; raw = s.substr(start, i - start); return true; }
        ++i;
    }
    return false;
}

static bool ParseValue(const std::wstring& s, size_t& i, std::wstring& raw) {
    SkipWs(s, i);
    if (i >= s.size()) return false;
    size_t start = i;
    if (s[i] == L'"') {
        if (!ParseString(s, i, raw)) return false;
        return true;
    }
    if (s[i] == L'{' || s[i] == L'[') {
        int depth = 0;
        while (i < s.size()) {
            if (s[i] == L'"') {
                std::wstring skipped;
                if (!ParseString(s, i, skipped)) return false;
                continue;
            }
            if (s[i] == L'{' || s[i] == L'[') ++depth;
            else if (s[i] == L'}' || s[i] == L']') {
                --depth;
                if (depth == 0) { ++i; raw = s.substr(start, i - start); return true; }
            }
            ++i;
        }
        return false;
    }
    while (i < s.size() && s[i] != L',' && s[i] != L'}' && s[i] != L']') ++i;
    raw = s.substr(start, i - start);
    while (!raw.empty() && (raw.back() == L' ' || raw.back() == L'\t' || raw.back() == L'\r' || raw.back() == L'\n')) raw.pop_back();
    while (!raw.empty() && (raw.front() == L' ' || raw.front() == L'\t' || raw.front() == L'\r' || raw.front() == L'\n')) raw.erase(raw.begin());
    return !raw.empty();
}

bool ParseObject(const std::wstring& text, std::map<std::wstring, std::wstring>& out) {
    size_t i = 0;
    SkipWs(text, i);
    if (i >= text.size() || text[i] != L'{') return false;
    ++i;
    while (true) {
        SkipWs(text, i);
        if (i >= text.size()) return false;
        if (text[i] == L'}') return true;
        std::wstring keyRaw;
        if (!ParseString(text, i, keyRaw)) return false;
        std::wstring key = Unescape(keyRaw);
        SkipWs(text, i);
        if (i >= text.size() || text[i] != L':') return false;
        ++i;
        std::wstring value;
        if (!ParseValue(text, i, value)) return false;
        out[key] = value;
        SkipWs(text, i);
        if (i < text.size() && text[i] == L',') { ++i; continue; }
        if (i < text.size() && text[i] == L'}') return true;
        return false;
    }
}

std::wstring Unescape(const std::wstring& raw) {
    if (raw.size() < 2 || raw.front() != L'"') return raw;
    std::wstring out;
    for (size_t i = 1; i + 1 < raw.size(); ++i) {
        wchar_t c = raw[i];
        if (c != L'\\') { out.push_back(c); continue; }
        if (++i + 1 >= raw.size()) break;
        wchar_t e = raw[i];
        switch (e) {
        case L'n': out.push_back(L'\n'); break;
        case L'r': out.push_back(L'\r'); break;
        case L't': out.push_back(L'\t'); break;
        case L'"': out.push_back(L'"'); break;
        case L'\\': out.push_back(L'\\'); break;
        case L'/': out.push_back(L'/'); break;
        case L'u': {
            if (i + 4 >= raw.size()) break;
            std::wstring hex = raw.substr(i + 1, 4);
            out.push_back((wchar_t)wcstoul(hex.c_str(), nullptr, 16));
            i += 4;
            break;
        }
        default: out.push_back(e); break;
        }
    }
    return out;
}

std::wstring Quote(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t c : value) {
        switch (c) {
        case L'"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default: out.push_back(c); break;
        }
    }
    out += L"\"";
    return out;
}

bool GetBool(const std::wstring& raw, bool fallback) {
    if (raw == L"true") return true;
    if (raw == L"false") return false;
    return fallback;
}

int GetInt(const std::wstring& raw, int fallback) {
    if (raw.empty()) return fallback;
    wchar_t* end = nullptr;
    long value = wcstol(raw.c_str(), &end, 10);
    if (end == raw.c_str()) return fallback;
    return (int)value;
}

long long GetLongLong(const std::wstring& raw, long long fallback) {
    if (raw.empty()) return fallback;
    wchar_t* end = nullptr;
    long long value = _wcstoi64(raw.c_str(), &end, 10);
    if (end == raw.c_str()) return fallback;
    return value;
}
} // namespace json

namespace b64 {
static const wchar_t* kTable = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::wstring Encode(const BYTE* data, size_t len) {
    std::wstring out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned v = data[i] << 16;
        if (i + 1 < len) v |= data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        out.push_back(kTable[(v >> 18) & 63]);
        out.push_back(kTable[(v >> 12) & 63]);
        out.push_back(i + 1 < len ? kTable[(v >> 6) & 63] : L'=');
        out.push_back(i + 2 < len ? kTable[v & 63] : L'=');
    }
    return out;
}

bool Decode(const std::wstring& text, std::vector<BYTE>& out) {
    out.clear();
    int bits = 0;
    unsigned buffer = 0;
    for (wchar_t c : text) {
        if (c == L'=' || c == L'\r' || c == L'\n' || c == L' ') continue;
        const wchar_t* hit = wcschr(kTable, c);
        if (!hit) return false;
        buffer = (buffer << 6) | (unsigned)(hit - kTable);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((BYTE)((buffer >> bits) & 0xFF));
        }
    }
    return true;
}
} // namespace b64
