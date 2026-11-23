#include "updater.h"
#include "qlock.h"
#include <bcrypt.h>
#include <map>
#include <winhttp.h>
#include <winver.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "bcrypt.lib")

namespace updater {

namespace {

const wchar_t* kDefaultApi = L"https://api.github.com/repos/haha0614/QLock/releases/latest";
const wchar_t* kDownloadBase = L"https://github.com/haha0614/QLock/releases/latest/download/";
const wchar_t* kExeName = L"QLock.exe";
const wchar_t* kSumsName = L"SHA256SUMS.txt";

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

std::wstring Env(const wchar_t* name) {
    wchar_t buf[1024] = {};
    DWORD n = GetEnvironmentVariableW(name, buf, 1024);
    if (n > 0 && n < 1024) return std::wstring(buf, n);
    return std::wstring();
}

std::wstring ApiUrl() {
    std::wstring custom = Env(L"QLOCK_UPDATE_URL");
    return custom.empty() ? std::wstring(kDefaultApi) : custom;
}

void CreateDirs(const std::wstring& dir) {
    std::wstring current;
    for (size_t i = 0; i < dir.size(); ++i) {
        current.push_back(dir[i]);
        if (dir[i] == L'\\' || dir[i] == L'/') {
            if (current.size() > 3) CreateDirectoryW(current.c_str(), nullptr);
        }
    }
    CreateDirectoryW(dir.c_str(), nullptr);
}

// body 和 destFile 二选一：要么把回包读进内存，要么边读边写文件。
bool HttpFetch(const std::wstring& url, std::string* body, const std::wstring& destFile,
               const Progress& progress, std::wstring* error) {
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
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 60000);

    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    HINTERNET req = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;
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
                long long total = 0;
                wchar_t lengthText[64] = {};
                DWORD lengthSize = sizeof(lengthText);
                if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                                        lengthText, &lengthSize, WINHTTP_NO_HEADER_INDEX)) {
                    total = _wcstoi64(lengthText, nullptr, 10);
                }
                if (!destFile.empty()) {
                    file = CreateFileW(destFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (file == INVALID_HANDLE_VALUE) {
                        if (error) *error = L"写不进 " + destFile;
                    }
                }
                if (destFile.empty() || file != INVALID_HANDLE_VALUE) {
                    ok = true;
                    long long got = 0;
                    if (progress) progress(0, total);
                    for (;;) {
                        DWORD avail = 0;
                        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                        std::vector<char> chunk(avail);
                        DWORD read = 0;
                        if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0) break;
                        if (destFile.empty()) {
                            body->append(chunk.data(), read);
                            if (body->size() > 1024 * 1024) break;
                        } else {
                            DWORD written = 0;
                            if (!WriteFile(file, chunk.data(), read, &written, nullptr) || written != read) {
                                if (error) *error = L"写入下载文件失败";
                                ok = false;
                                break;
                            }
                        }
                        got += read;
                        if (progress) progress(got, total);
                    }
                }
            }
        } else if (error) {
            *error = L"连不上更新服务器";
        }
    } else if (error) {
        *error = L"建立请求失败";
    }

    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    if (!ok && !destFile.empty()) DeleteFileW(destFile.c_str());
    return ok;
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

std::wstring ExePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
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

bool Check(std::wstring* tag, std::wstring* page, std::wstring* error, std::wstring* notes) {
    std::string body;
    if (!HttpFetch(ApiUrl(), &body, std::wstring(), nullptr, error)) return false;
    std::map<std::wstring, std::wstring> fields;
    if (!json::ParseObject(Widen(body), fields)) {
        if (error) *error = L"看不懂服务器返回的内容";
        return false;
    }
    if (tag) *tag = json::Unescape(fields[L"tag_name"]);
    if (page) *page = json::Unescape(fields[L"html_url"]);
    if (notes) *notes = json::Unescape(fields[L"body"]);
    if (tag && tag->empty()) {
        if (error) *error = L"服务器没有返回版本号";
        return false;
    }
    return true;
}

std::wstring ExeUrl() {
    std::wstring custom = Env(L"QLOCK_UPDATE_EXE_URL");
    return custom.empty() ? std::wstring(kDownloadBase) + kExeName : custom;
}

std::wstring SumsUrl() {
    std::wstring custom = Env(L"QLOCK_UPDATE_SUMS_URL");
    return custom.empty() ? std::wstring(kDownloadBase) + kSumsName : custom;
}

std::wstring DownloadPath() {
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    std::wstring dir = std::wstring(temp) + L"QLock-update";
    CreateDirs(dir);
    return dir + L"\\" + kExeName;
}

bool Download(const std::wstring& url, const std::wstring& dest, const Progress& progress,
              std::wstring* error) {
    return HttpFetch(url, nullptr, dest, progress, error);
}

std::wstring FetchText(const std::wstring& url, std::wstring* error) {
    std::string body;
    if (!HttpFetch(url, &body, std::wstring(), nullptr, error)) return std::wstring();
    return Widen(body);
}

std::wstring ExpectedHash(const std::wstring& sums) {
    size_t pos = 0;
    while (pos < sums.size()) {
        size_t end = sums.find_first_of(L"\r\n", pos);
        std::wstring line = sums.substr(pos, end == std::wstring::npos ? std::wstring::npos : end - pos);
        pos = end == std::wstring::npos ? sums.size() : end + 1;
        while (!line.empty() && (line.back() == L' ' || line.back() == L'\t')) line.pop_back();
        if (line.size() < 64) continue;
        std::wstring lower;
        for (size_t i = 0; i < line.size(); ++i) lower.push_back((wchar_t)towlower(line[i]));
        if (lower.find(L"qlock.exe") == std::wstring::npos) continue;
        std::wstring head = line.substr(0, 64);
        bool hex = true;
        for (size_t i = 0; i < head.size(); ++i) {
            wchar_t c = (wchar_t)towlower(head[i]);
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) {
                hex = false;
                break;
            }
        }
        if (hex) {
            for (size_t i = 0; i < head.size(); ++i) head[i] = (wchar_t)towlower(head[i]);
            return head;
        }
    }
    return std::wstring();
}

std::wstring Sha256File(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::wstring();
    std::wstring result;
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
            std::vector<BYTE> buffer(65536);
            DWORD read = 0;
            bool ok = true;
            while (ReadFile(file, buffer.data(), (DWORD)buffer.size(), &read, nullptr) && read > 0) {
                if (BCryptHashData(hash, buffer.data(), read, 0) != 0) {
                    ok = false;
                    break;
                }
            }
            BYTE digest[32] = {};
            if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
                static const wchar_t* hex = L"0123456789abcdef";
                for (int i = 0; i < 32; ++i) {
                    result.push_back(hex[digest[i] >> 4]);
                    result.push_back(hex[digest[i] & 0xF]);
                }
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    CloseHandle(file);
    return result;
}

bool Install(const std::wstring& downloaded, std::wstring* error) {
    std::wstring exe = ExePath();
    std::wstring backup = exe + L".old";
    DeleteFileW(backup.c_str());
    if (!MoveFileW(exe.c_str(), backup.c_str())) {
        if (error) *error = L"旧版本改名失败（" + std::to_wstring(GetLastError()) + L"）";
        return false;
    }
    if (!MoveFileW(downloaded.c_str(), exe.c_str())) {
        MoveFileW(backup.c_str(), exe.c_str());
        if (error) *error = L"新版本写不进去（" + std::to_wstring(GetLastError()) + L"）";
        return false;
    }
    app::Log(L"update: installed, old kept as %s", backup.c_str());
    return true;
}

bool Restart(const std::wstring& exe, std::wstring* error) {
    wchar_t comspec[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH) == 0) {
        if (error) *error = L"找不到 cmd.exe";
        return false;
    }
    // 新进程要等本进程放掉单实例锁，所以延迟两秒再启动。
    std::wstring command = L"\"";
    command += comspec;
    command += L"\" /c ping -n 3 127.0.0.1 >nul & start \"\" \"";
    command += exe;
    command += L"\" --tray";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi)) {
        if (error) *error = L"拉不起新版本（" + std::to_wstring(GetLastError()) + L"）";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void CleanupOld() {
    std::wstring backup = ExePath() + L".old";
    if (DeleteFileW(backup.c_str())) app::Log(L"update: removed leftover %s", backup.c_str());

    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    std::wstring dir = std::wstring(temp) + L"QLock-update";
    WIN32_FIND_DATAW fd = {};
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            DeleteFileW((dir + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    RemoveDirectoryW(dir.c_str());
}

}  // namespace updater
