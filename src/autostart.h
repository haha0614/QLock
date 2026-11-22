#pragma once
#include <string>

// 登录时自动启动：只写当前用户的 Run 键，不需要管理员权限。
namespace autostart {
std::wstring ExePath();
bool Apply(bool enabled, std::wstring* error);
bool Enabled();
}
