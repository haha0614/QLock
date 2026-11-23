#pragma once
#include <windows.h>
#include <string>

// 发现新版本时弹的小窗：版本号、更新说明、跳过/稍后/下载。
// 下载、校验、原地替换都在这个窗口里跑，装好后重启进程。
void UpdateShow(HWND owner, const std::wstring& tag, const std::wstring& notes);
bool UpdateOwns(HWND hwnd);
