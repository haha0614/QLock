#pragma once
#include <string>
#include <vector>

namespace fonts {
// 系统里能用的字体族名，按用户语言排序；空字符串表示用内置默认字体。
const std::vector<std::wstring>& All();
std::wstring Label(const std::wstring& family);
}  // namespace fonts
