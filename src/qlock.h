#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <map>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

// 配置字段名与 0.3.0（WPF 版）的 config.json 完全一致，换实现不丢用户设置。
struct Config {
    int         Version = 1;
    std::wstring Autostart = L"runkey";
    std::wstring BackgroundImage;
    bool        BackgroundSnapshot = false;
    int         BlurRadius = -1;
    bool        BlockSystemHotkeys = true;
    std::wstring ClockFont;
    bool        ClockFontEnabled = false;
    int         CornerRadius = -1;
    bool        CustomHotkeyEnabled = true;
    std::wstring FontFamily;
    int         IdleLockMinutes = 0;
    bool        InterceptWinL = true;
    long long   LastUpdateCheckTicks = 0;
    bool        LockOnStartup = true;
    int         MaskOpacity = -1;
    std::wstring PasswordCipher;
    std::wstring PasswordHash;
    int         PasswordIterations = 120000;
    std::wstring PasswordSalt;
    std::wstring RecoveryHash;
    std::wstring RecoverySalt;
    bool        RunElevated = false;
    bool        ShowClock = true;
    bool        ShowSubtitle = true;
    bool        ShowTray = true;
    std::wstring SkippedVersion;
    std::wstring Subtitle = L"输入密码解锁";
    std::wstring ThemeId = L"fluent";
};

namespace app {
extern Config cfg;
extern HINSTANCE inst;
extern bool locked;

std::wstring DataDir();          // %APPDATA%QLock
std::wstring LocalDir();         // %LOCALAPPDATA%QLock
std::wstring ConfigPath();
std::wstring LogPath();
std::wstring ThemeDir();
void Log(const wchar_t* fmt, ...);

bool LoadConfig();
bool SaveConfig();
bool HasPassword();
bool VerifyPassword(const std::wstring& password);
void SetPassword(const std::wstring& password);   // 写 Hash/Salt/Cipher 并立即保存
std::wstring StoredPassword();                    // 解出 DPAPI 那份明文，只给设置页回显
std::wstring ClockFontName();    // 时钟字体没开或没挑过就回落界面字体
std::wstring UiFontName();

void RequestLock();
void Unlock();
} // namespace app

namespace json {
bool ParseObject(const std::wstring& text, std::map<std::wstring, std::wstring>& out);
std::wstring Unescape(const std::wstring& raw);
std::wstring Quote(const std::wstring& value);
bool GetBool(const std::wstring& raw, bool fallback);
int GetInt(const std::wstring& raw, int fallback);
long long GetLongLong(const std::wstring& raw, long long fallback);
}

namespace b64 {
std::wstring Encode(const BYTE* data, size_t len);
bool Decode(const std::wstring& text, std::vector<BYTE>& out);
}

// 锁屏窗口
void LockScreensCreate();
void LockScreensClose();
void LockScreensGuard();
void LockScreensReclaim();
bool LockScreensContain(HWND hwnd);
bool LockScreensAlive();
void LockScreensSetError(const std::wstring& text);
void LockScreensLayoutChanged();
