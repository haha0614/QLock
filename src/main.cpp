#pragma once
#include "qlock.h"
#include "resource.h"
#include <shellapi.h>
#include <objbase.h>
#include <vector>
#include <string>
#include <cstdlib>

namespace {

const UINT WM_APP_TRAY = WM_APP + 1;
const UINT WM_APP_LOCK = WM_APP + 2;
const UINT WM_APP_UNLOCK = WM_APP + 3;
const UINT WM_APP_QUIT = WM_APP + 4;

HWND g_main = nullptr;
HHOOK g_hook = nullptr;
UINT_PTR g_idleTimer = 0;
UINT_PTR g_lifeTimer = 0;
int g_lifeSeconds = 0;
bool g_autoUnlock = false;
bool g_winDown = false;
DWORD g_lastReclaim = 0;
DWORD g_lastBlockLog = 0;
std::wstring g_args;

void TrayAdd() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = (HICON)LoadImageW(app::inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    lstrcpynW(nid.szTip, L"QLock", ARRAYSIZE(nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayRemove() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_LOCK, L"立即锁定");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, ID_MENU_SETTINGS, L"设置…（下一版）");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_MENU_QUIT, L"退出");
    SetForegroundWindow(g_main);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_main, nullptr);
    PostMessageW(g_main, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK HookProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION) return CallNextHookEx(g_hook, code, wp, lp);
    KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lp;
    DWORD vk = kb->vkCode;
    bool down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
    bool up = (wp == WM_KEYUP || wp == WM_SYSKEYUP);

    if (vk == VK_LWIN || vk == VK_RWIN) {
        if (down) g_winDown = true;
        if (up) g_winDown = false;
    }

    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool rightAlt = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

    // 覆盖层没起来就别吞键：宁可锁屏失败，也不能把人卡在没界面的键盘黑洞里。
    if (app::locked && LockScreensAlive()) {
        HWND fg = GetForegroundWindow();
        if (LockScreensContain(fg)) return CallNextHookEx(g_hook, code, wp, lp);

        DWORD now = GetTickCount();
        if (now - g_lastReclaim > 400) {
            g_lastReclaim = now;
            PostMessageW(g_main, WM_APP_LOCK + 100, 0, 0); // 让主线程去抢前台
        }
        if (now - g_lastBlockLog > 2000) {
            g_lastBlockLog = now;
            app::Log(L"input blocked, current=%p", (void*)fg);
        }
        return 1;
    }

    if (down && vk == L'L' && ctrl && alt && !rightAlt && app::cfg.CustomHotkeyEnabled) {
        if (app::HasPassword()) app::RequestLock();
        return 1;
    }
    if (down && vk == L'L' && g_winDown && app::cfg.InterceptWinL) {
        if (app::HasPassword()) app::RequestLock();
        return 1;
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

void TrimMemory() {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void DoLock() {
    if (app::locked) return;
    if (!app::HasPassword()) {
        app::Log(L"lock skipped: no password");
        return;
    }
    app::locked = true;
    LockScreensCreate();
    if (!LockScreensAlive()) {
        app::locked = false;
        app::Log(L"lock aborted: no overlay window");
    }
}

void DoUnlock() {
    if (!app::locked) return;
    app::locked = false;
    LockScreensClose();
    TrimMemory();
    app::Log(L"unlocked, screens closed");
}

void DoQuit() {
    DestroyWindow(g_main);
}

bool IdleReached(int minutes) {
    if (minutes <= 0) return false;
    LASTINPUTINFO li = { sizeof(li) };
    if (!GetLastInputInfo(&li)) return false;
    DWORD idleMs = GetTickCount() - li.dwTime;
    return idleMs >= (DWORD)minutes * 60000u;
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_APP_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) TrayMenu();
        else if (lp == WM_LBUTTONDBLCLK) DoLock();
        return 0;
    case WM_APP_LOCK:
        DoLock();
        return 0;
    case WM_APP_UNLOCK:
        DoUnlock();
        return 0;
    case WM_APP_QUIT:
        DoQuit();
        return 0;
    case WM_APP_LOCK + 100:
        LockScreensReclaim();
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            if (app::locked && !LockScreensAlive()) DoUnlock();
            if (app::cfg.IdleLockMinutes > 0 && IdleReached(app::cfg.IdleLockMinutes)) DoLock();
        } else if (wp == 2) {
            if (g_lifeSeconds > 0) {
                --g_lifeSeconds;
                if (g_lifeSeconds == 0) {
                    if (g_autoUnlock && app::locked) DoUnlock();
                    PostMessageW(hwnd, WM_APP_QUIT, 0, 0);
                }
            }
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_MENU_LOCK: DoLock(); return 0;
        case ID_MENU_SETTINGS:
            MessageBoxW(nullptr, L"设置界面在下一版里，先用托盘菜单锁定和退出。", L"QLock", MB_OK | MB_ICONINFORMATION);
            return 0;
        case ID_MENU_QUIT: DoQuit(); return 0;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool HasArg(const std::vector<std::wstring>& args, const wchar_t* name, std::wstring* value) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (_wcsicmp(args[i].c_str(), name) == 0) {
            if (value && i + 1 < args.size()) *value = args[i + 1];
            return true;
        }
    }
    return false;
}

void EnableDpiAwareness() {
    typedef BOOL(WINAPI* SetCtxFn)(DPI_AWARENESS_CONTEXT);
    static SetCtxFn fn = (SetCtxFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

} // namespace

namespace app {
void RequestLock() {
    if (g_main) PostMessageW(g_main, WM_APP_LOCK, 0, 0);
}

void Unlock() {
    if (g_main) PostMessageW(g_main, WM_APP_UNLOCK, 0, 0);
}
} // namespace app

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    app::inst = inst;
    EnableDpiAwareness();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
        if (i > 1) g_args += L" ";
        g_args += argv[i];
    }
    if (argv) LocalFree(argv);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\QLock.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        app::Log(L"another instance is running, exit");
        return 0;
    }

    app::LoadConfig();
    app::Log(L"QLock start pid=%lu args=%s", GetCurrentProcessId(), g_args.c_str());

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"QLockMain";
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, L"QLockMain", L"QLock", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, inst, nullptr);
    if (!g_main) {
        app::Log(L"main window failed err=%lu", GetLastError());
        return 1;
    }

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc, inst, 0);
    if (!g_hook) app::Log(L"hook failed err=%lu", GetLastError());

    if (app::cfg.ShowTray) TrayAdd();

    g_idleTimer = SetTimer(g_main, 1, 5000, nullptr);

    std::wstring value;
    if (HasArg(args, L"--lock", nullptr)) g_autoUnlock = true;
    if (HasArg(args, L"--autounlock", &value)) {
        g_autoUnlock = true;
        int seconds = _wtoi(value.c_str());
        if (seconds > 0) g_lifeSeconds = seconds;
    }
    if (HasArg(args, L"--exitafter", &value)) {
        int seconds = _wtoi(value.c_str());
        if (seconds > 0 && (g_lifeSeconds == 0 || seconds < g_lifeSeconds)) g_lifeSeconds = seconds;
    }
    if (g_lifeSeconds > 0) g_lifeTimer = SetTimer(g_main, 2, 1000, nullptr);

    if (HasArg(args, L"--lock", nullptr)) {
        DoLock();
    } else if (args.empty() && app::cfg.LockOnStartup) {
        DoLock();
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    LockScreensClose();
    TrayRemove();
    if (mutex) CloseHandle(mutex);
    CoUninitialize();
    app::Log(L"==== QLock exit");
    return 0;
}
