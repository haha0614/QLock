#include "updater.h"
#include "qlock.h"
#include <winhttp.h>
#include <winver.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")

namespace updater {

namespace {

const wchar_t* kDefaultApi = L"https://api.github.com/repos/haha0614/QLock/releases/latest";

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring out((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], n);
    return out;
}

bool HttpGet(const std::wstring& url, std::string* body, std::wstring* error) {
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        if (error) *error = L"下载地址解析失败";
        return false;
    }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET session = WinHttpOpen(L"QLock-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (error) *error = L"初始化网络失败";
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 8000);

    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    HINTERNET req = nullptr;
    bool ok = false;
    if (conn) {
        DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    }
    if (req) {
        WinHttpAddRequestHeaders(req, L"User-Agent: QLock-Updater\r\nAccept: application/vnd.github+json",
                                 (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, nullptr)) {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            if (status != 200) {
                if (error) *error = L"服务器返回 " + std::to_wstring(status);
            } else {
                ok = true;
                char buffer[8192];
                DWORD read = 0;
                while (WinHttpReadData(req, buffer, sizeof(buffer), &read) && read > 0) {
                    body->append(buffer, read);
                    if (body->size() > 1024 * 1024) break;
                }
            }
        } else if (error) {
            *error = L"连不上更新服务器";
        }
    } else if (error) {
        *error = L"建立请求失败";
    }

    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok;
}

std::wstring ApiUrl() {
    wchar_t buf[1024] = {};
    DWORD n = GetEnvironmentVariableW(L"QLOCK_UPDATE_URL", buf, 1024);
    if (n > 0 && n < 1024) return std::wstring(buf, n);
    return kDefaultApi;
}

void ParseVersion(const std::wstring& text, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    std::wstring s = text;
    if (!s.empty() && (s[0] == L'v' || s[0] == L'V')) s.erase(0, 1);
    int index = 0;
    size_t pos = 0;
    while (index < 3 && pos <= s.size()) {
        size_t dot = s.find(L'.', pos);
        std::wstring part = s.substr(pos, dot == std::wstring::npos ? std::wstring::npos : dot - pos);
        out[index++] = _wtoi(part.c_str());
        if (dot == std::wstring::npos) break;
        pos = dot + 1;
    }
}

}  // namespace

std::wstring CurrentVersion() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(app::inst, path, MAX_PATH);
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &dummy);
    if (size > 0) {
        std::vector<BYTE> data(size);
        if (GetFileVersionInfoW(path, 0, size, data.data())) {
            VS_FIXEDFILEINFO* info = nullptr;
            UINT len = 0;
            if (VerQueryValueW(data.data(), L"\\", (void**)&info, &len) && info) {
                return std::to_wstring(HIWORD(info->dwFileVersionMS)) + L"." +
                       std::to_wstring(LOWORD(info->dwFileVersionMS)) + L"." +
                       std::to_wstring(HIWORD(info->dwFileVersionLS));
            }
        }
    }
    return L"0.0.0";
}

bool IsNewer(const std::wstring& tag, const std::wstring& current) {
    int remote[3] = {}, local[3] = {};
    ParseVersion(tag, remote);
    ParseVersion(current, local);
    for (int i = 0; i < 3; ++i) {
        if (remote[i] != local[i]) return remote[i] > local[i];
    }
    return false;
}

bool Check(std::wstring* tag, std::wstring* url, std::wstring* error) {
    std::string body;
    if (!HttpGet(ApiUrl(), &body, error)) return false;
    std::map<std::wstring, std::wstring> fields;
    if (!json::ParseObject(Widen(body), fields)) {
        if (error) *error = L"返回的内容不是 JSON";
        return false;
    }
    std::map<std::wstring, std::wstring>::const_iterator it = fields.find(L"tag_name");
    if (it == fields.end()) {
        if (error) *error = L"返回里没有版本号";
        return false;
    }
    if (tag) *tag = json::Unescape(it->second);
    std::map<std::wstring, std::wstring>::const_iterator page = fields.find(L"html_url");
    if (url) {
        *url = page == fields.end() ? L"https://github.com/haha0614/QLock/releases"
                                    : json::Unescape(page->second);
    }
    return true;
}

}  // namespace updater
