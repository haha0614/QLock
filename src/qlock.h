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
    std::wstring BackgroundColor;     // 纯色背景；空表示用主题自带的底色
    std::wstring BackgroundImage;
    bool        BackgroundSnapshot = false;
    std::wstring BackgroundMode;      // "solid" | "snapshot" | "image"；空表示按上面两个旧字段推断
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
// 背景样式：0 纯色、1 屏幕截图、2 自定义图片。旧的配置文件没有 BackgroundMode，就按老字段推断。
int BackgroundKind();
// 恢复码：10 位（形如 XXXXX-XXXXX），和密码一样走 PBKDF2-SHA256；用一次就作废。
std::wstring NewRecoveryCode();
void SetRecovery(const std::wstring& code);     // 写 Hash/Salt 并立即保存
void ClearRecovery();
bool HasRecovery();
bool VerifyRecovery(const std::wstring& code);

// 自启项写的是 "<exe>" --tray，手动双击是空参数；提权重开带 --elevated，这三种都算正常启动。
bool TrayLaunch(const std::vector<std::wstring>& args);
// 正常启动且开了「启动后立即锁定」才锁；升级完自己重启带 --updated，不重复锁。
bool ShouldLockOnStart(const std::vector<std::wstring>& args, bool lockOnStartup);
// 锁屏时要吞掉的系统组合键：Win 键、按住 Win 的任何键、Alt+Tab/Esc/F4、Ctrl/Shift+Esc、菜单键、PrintScreen。
bool IsSystemCombo(UINT vk, bool winDown, bool ctrl, bool alt, bool shift);

std::wstring ClockFontName();    // 时钟字体没开或没挑过就回落界面字体
long long UtcTicksNow();         // 和 .NET DateTime.UtcNow.Ticks 同一把尺子，配置里存的是它
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
HWND LockScreensPrimary();
void LockScreensSetError(const std::wstring& text);
void LockScreensLayoutChanged();
