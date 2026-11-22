#include "autostart.h"
#include "qlock.h"

#pragma comment(lib, "advapi32.lib")

namespace autostart {

namespace {
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kValueName = L"QLock";
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(app::inst, path, MAX_PATH);
    return std::wstring(path, n);
}

bool Apply(bool enabled, std::wstring* error) {
    HKEY key = nullptr;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE,
                             nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) {
        if (error) *error = L"打不开当前用户的启动项注册表";
        return false;
    }

    bool ok = true;
    if (enabled) {
        std::wstring command = L"\"" + ExePath() + L"\" --tray";
        rc = RegSetValueExW(key, kValueName, 0, REG_SZ, (const BYTE*)command.c_str(),
                            (DWORD)((command.size() + 1) * sizeof(wchar_t)));
        if (rc != ERROR_SUCCESS) {
            ok = false;
            if (error) *error = L"写入启动项失败（错误码 " + std::to_wstring(rc) + L"）";
        }
    } else {
        rc = RegDeleteValueW(key, kValueName);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
            ok = false;
            if (error) *error = L"删除启动项失败（错误码 " + std::to_wstring(rc) + L"）";
        }
    }
    RegCloseKey(key);
    return ok;
}

bool Enabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    bool found = false;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0 &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        found = true;
    }
    RegCloseKey(key);
    return found;
}

}  // namespace autostart
