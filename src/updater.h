#pragma once
#include <functional>
#include <string>

// 检查、下载、校验、原地替换。这里不碰 UI，进度和结果都通过回调和返回值交出去。
namespace updater {
std::wstring CurrentVersion();                       // 取 exe 的版本资源，形如 "0.1.0"
bool IsNewer(const std::wstring& tag,
             const std::wstring& current);           // "v0.1.1" 比 "0.1.0" 新
// notes 是发布说明正文，GitHub 的 markdown 原文，交给界面自己排。
bool Check(std::wstring* tag, std::wstring* page, std::wstring* error, std::wstring* notes = nullptr);

std::wstring ExeUrl();                               // 新版 exe 的下载地址
std::wstring SumsUrl();                              // 同一发布页的 SHA256SUMS.txt
std::wstring DownloadPath();                         // %TEMP%\QLock-update\QLock.exe

typedef std::function<void(long long got, long long total)> Progress;
bool Download(const std::wstring& url, const std::wstring& dest, const Progress& progress,
              std::wstring* error);
std::wstring FetchText(const std::wstring& url, std::wstring* error);
std::wstring ExpectedHash(const std::wstring& sums);  // 从 SHA256SUMS.txt 里挑出 exe 那一行
std::wstring Sha256File(const std::wstring& path);    // 小写十六进制，失败返回空

bool Install(const std::wstring& downloaded, std::wstring* error);  // 旧 exe 改名留底，新 exe 就位
bool Restart(const std::wstring& exe, std::wstring* error);        // 等本进程退出后再拉起新版
void CleanupOld();                                                 // 启动时收尾：删 .old 和临时目录
}  // namespace updater
