#pragma once
#include <string>

// 检查更新：只读 GitHub 的 latest release，不下载、不替换，有新版就把下载页交出去。
namespace updater {
std::wstring CurrentVersion();                      // 取 exe 的版本资源，形如 "0.4.0"
bool IsNewer(const std::wstring& tag,
             const std::wstring& current);          // "v0.4.1" 比 "0.4.0" 新
bool Check(std::wstring* tag, std::wstring* url, std::wstring* error);
}
