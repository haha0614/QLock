#include "font_list.h"
#include <windows.h>
#include <algorithm>
#include <set>
#include <string>

namespace fonts {

namespace {

int CALLBACK Collect(const LOGFONTW* lf, const TEXTMETRICW*, DWORD type, LPARAM param) {
    if (!(type & TRUETYPE_FONTTYPE)) return 1;
    if (lf->lfFaceName[0] == L'@') return 1;   // @ 开头是竖排变体，界面上不用
    reinterpret_cast<std::set<std::wstring>*>(param)->insert(lf->lfFaceName);
    return 1;
}

bool LessByLocale(const std::wstring& a, const std::wstring& b) {
    return CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, a.c_str(), -1, b.c_str(), -1, nullptr, nullptr, 0) ==
           CSTR_LESS_THAN;
}

}  // namespace

const std::vector<std::wstring>& All() {
    static std::vector<std::wstring> cached;
    if (!cached.empty()) return cached;
    std::set<std::wstring> names;
    HDC dc = GetDC(nullptr);
    if (dc) {
        LOGFONTW lf = {};
        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc, &lf, Collect, (LPARAM)&names, 0);
        ReleaseDC(nullptr, dc);
    }
    cached.assign(names.begin(), names.end());
    std::sort(cached.begin(), cached.end(), LessByLocale);
    return cached;
}

std::wstring Label(const std::wstring& family) {
    return family.empty() ? std::wstring(L"微软雅黑 UI（默认）") : family;
}

}  // namespace fonts
