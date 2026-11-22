#pragma once
#include "qlock.h"
#include <d2d1.h>

// 主题字段与 WPF 版（wpf 分支的 src\LockTheme.cs）一一对应，外部主题 JSON 两边通用。
struct ThemeClock {
    bool Show = true;
    double Size = 74;
    int Weight = 300;
    std::wstring Color = L"#FFFFFFFF";
};

struct ThemeDate {
    double Size = 16;
    std::wstring Color = L"#FFC9C9C9";
    double Gap = 26;
    std::wstring Format;
};

struct ThemeCard {
    bool Show = false;
    double Width = 380;
    double Radius = 14;
    double PadH = 26;
    double PadV = 20;
    std::wstring Bg = L"#D81C1C26";
    std::wstring Border = L"#37FFFFFF";
};

struct ThemeBrand {
    bool Show = false;
    std::wstring Text = L"Q L O C K";
    double Size = 11;
    std::wstring Color = L"#FF78A0E6";
    double Gap = 6;
};

struct ThemeSubtitle {
    bool Show = true;
    double Size = 13;
    std::wstring Color = L"#FFE8EEF7";
    double Gap = 14;
};

struct ThemeInput {
    std::wstring Kind = L"box";  // box | underline
    double Height = 40;
    double Radius = 7;
    double Size = 15;
    double Width = 340;
    std::wstring Bg = L"#1EFFFFFF";
    std::wstring Border = L"#50FFFFFF";
    std::wstring Fg = L"#FFFFFFFF";
    std::wstring Hint = L"#69FFFFFF";
};

struct ThemeButton {
    std::wstring Kind = L"filled";  // filled | outline | text
    double Height = 40;
    double Radius = 8;
    double Size = 15;
    double Width = 0;  // 0 = 跟输入框同宽
    double Gap = 12;
    std::wstring Bg = L"#FF3A76E8";
    std::wstring Fg = L"#FFFFFFFF";
    std::wstring Border = L"#00000000";
    std::wstring Hover;
};

struct ThemeLink {
    double Size = 12;
    std::wstring Color = L"#FF8CB4F0";
    double Gap = 12;
};

struct ThemeBackground {
    std::wstring Color = L"#FF080A10";
    int Dim = 150;
    double Blur = 26;
};

struct LockTheme {
    int Schema = 1;
    std::wstring Id;
    std::wstring Name;
    std::wstring Layout = L"stacked";  // stacked | inline
    ThemeClock Clock;
    ThemeDate Date;
    ThemeCard Card;
    ThemeBrand Brand;
    ThemeSubtitle Subtitle;
    ThemeInput Input;
    ThemeButton Button;
    ThemeLink Link;
    ThemeBackground Background;

    bool Inline() const;
    std::wstring DateFormat() const;  // Date.Format 为空时回落 "yyyy年M月d日 dddd"
};

namespace themes {

extern const wchar_t* const kDefaultId;  // L"fluent"

std::wstring Dir();  // app::ThemeDir()
const std::vector<LockTheme>& All();
void Reset();
const LockTheme& Find(const std::wstring& id);
bool Parse(const std::wstring& json, LockTheme& out);
LockTheme BuiltinFluent();
LockTheme BuiltinCard();
LockTheme BuiltinMinimal();
void EnsureSampleThemes();  // 主题目录不存在时写三份样例
std::wstring Serialize(const LockTheme& t);

// "#AARRGGBB" 或 "#RRGGBB"；解析失败返回 fallback。
D2D1_COLOR_F Color(const std::wstring& text, D2D1_COLOR_F fallback);

}  // namespace themes
