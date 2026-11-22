#pragma once
#include <string>
#include <windows.h>

void SettingsShow(HWND owner);
void SettingsShowUpdate(HWND owner, const std::wstring& tag);  // 自动检查发现新版：直接开在关于页
bool SettingsOwnsWindow(HWND hwnd);
