#include "theme.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <vector>

// 主题模型与加载：逐字段对应 WPF 版 src\LockTheme.cs，外部主题文件两边通用。
// 接口由 theme.h 冻结，这里只做实现。

namespace {

typedef std::map<std::wstring, std::wstring> Kv;

const int kMaxFiles = 40;
const long long kMaxFileBytes = 64 * 1024;

bool Same(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::wstring Trim(const std::wstring& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && (text[begin] == L' ' || text[begin] == L'\t' || text[begin] == L'\r' || text[begin] == L'\n')) ++begin;
    while (end > begin && (text[end - 1] == L' ' || text[end - 1] == L'\t' || text[end - 1] == L'\r' || text[end - 1] == L'\n')) --end;
    return text.substr(begin, end - begin);
}

std::wstring Raw(const Kv& kv, const wchar_t* key) {
    Kv::const_iterator it = kv.find(key);
    return it == kv.end() ? std::wstring() : it->second;
}

// 字符串字段：值不是字符串时保留默认值，坏文件不至于把类型错误带进界面。
std::wstring Str(const Kv& kv, const wchar_t* key, const std::wstring& fallback) {
    const std::wstring raw = Raw(kv, key);
    if (raw.size() < 2 || raw.front() != L'"') return fallback;
    return json::Unescape(raw);
}

// qlock.h 的 json 没有 GetDouble，按需在这里补一个。
double Num(const Kv& kv, const wchar_t* key, double fallback) {
    const std::wstring raw = Raw(kv, key);
    if (raw.empty() || raw.front() == L'"') return fallback;
    wchar_t* end = nullptr;
    const double value = wcstod(raw.c_str(), &end);
    if (end == raw.c_str()) return fallback;
    return value;
}

bool Flag(const Kv& kv, const wchar_t* key, bool fallback) {
    Kv::const_iterator it = kv.find(key);
    if (it == kv.end()) return fallback;
    return json::GetBool(it->second, fallback);
}

int Int(const Kv& kv, const wchar_t* key, int fallback) {
    Kv::const_iterator it = kv.find(key);
    if (it == kv.end()) return fallback;
    return json::GetInt(it->second, fallback);
}

// 嵌套对象：外层只给出原始文本，这里再解析一次。
bool Sub(const Kv& kv, const wchar_t* key, Kv& out) {
    const std::wstring raw = Raw(kv, key);
    if (raw.empty() || raw.front() != L'{') return false;
    Kv parsed;
    if (!json::ParseObject(raw, parsed)) return false;
    out.swap(parsed);
    return true;
}

void ReadClock(const Kv& root, ThemeClock& v) {
    Kv kv;
    if (!Sub(root, L"Clock", kv)) return;
    v.Show = Flag(kv, L"Show", v.Show);
    v.Size = Num(kv, L"Size", v.Size);
    v.Weight = Int(kv, L"Weight", v.Weight);
    v.Color = Str(kv, L"Color", v.Color);
}

void ReadDate(const Kv& root, ThemeDate& v) {
    Kv kv;
    if (!Sub(root, L"Date", kv)) return;
    v.Size = Num(kv, L"Size", v.Size);
    v.Color = Str(kv, L"Color", v.Color);
    v.Gap = Num(kv, L"Gap", v.Gap);
    v.Format = Str(kv, L"Format", v.Format);
}

void ReadCard(const Kv& root, ThemeCard& v) {
    Kv kv;
    if (!Sub(root, L"Card", kv)) return;
    v.Show = Flag(kv, L"Show", v.Show);
    v.Width = Num(kv, L"Width", v.Width);
    v.Radius = Num(kv, L"Radius", v.Radius);
    v.PadH = Num(kv, L"PadH", v.PadH);
    v.PadV = Num(kv, L"PadV", v.PadV);
    v.Bg = Str(kv, L"Bg", v.Bg);
    v.Border = Str(kv, L"Border", v.Border);
}

void ReadBrand(const Kv& root, ThemeBrand& v) {
    Kv kv;
    if (!Sub(root, L"Brand", kv)) return;
    v.Show = Flag(kv, L"Show", v.Show);
    v.Text = Str(kv, L"Text", v.Text);
    v.Size = Num(kv, L"Size", v.Size);
    v.Color = Str(kv, L"Color", v.Color);
    v.Gap = Num(kv, L"Gap", v.Gap);
}

void ReadSubtitle(const Kv& root, ThemeSubtitle& v) {
    Kv kv;
    if (!Sub(root, L"Subtitle", kv)) return;
    v.Show = Flag(kv, L"Show", v.Show);
    v.Size = Num(kv, L"Size", v.Size);
    v.Color = Str(kv, L"Color", v.Color);
    v.Gap = Num(kv, L"Gap", v.Gap);
}

void ReadInput(const Kv& root, ThemeInput& v) {
    Kv kv;
    if (!Sub(root, L"Input", kv)) return;
    v.Kind = Str(kv, L"Kind", v.Kind);
    v.Height = Num(kv, L"Height", v.Height);
    v.Radius = Num(kv, L"Radius", v.Radius);
    v.Size = Num(kv, L"Size", v.Size);
    v.Width = Num(kv, L"Width", v.Width);
    v.Bg = Str(kv, L"Bg", v.Bg);
    v.Border = Str(kv, L"Border", v.Border);
    v.Fg = Str(kv, L"Fg", v.Fg);
    v.Hint = Str(kv, L"Hint", v.Hint);
}

void ReadButton(const Kv& root, ThemeButton& v) {
    Kv kv;
    if (!Sub(root, L"Button", kv)) return;
    v.Kind = Str(kv, L"Kind", v.Kind);
    v.Height = Num(kv, L"Height", v.Height);
    v.Radius = Num(kv, L"Radius", v.Radius);
    v.Size = Num(kv, L"Size", v.Size);
    v.Width = Num(kv, L"Width", v.Width);
    v.Gap = Num(kv, L"Gap", v.Gap);
    v.Bg = Str(kv, L"Bg", v.Bg);
    v.Fg = Str(kv, L"Fg", v.Fg);
    v.Border = Str(kv, L"Border", v.Border);
    v.Hover = Str(kv, L"Hover", v.Hover);
}

void ReadLink(const Kv& root, ThemeLink& v) {
    Kv kv;
    if (!Sub(root, L"Link", kv)) return;
    v.Size = Num(kv, L"Size", v.Size);
    v.Color = Str(kv, L"Color", v.Color);
    v.Gap = Num(kv, L"Gap", v.Gap);
}

void ReadBackground(const Kv& root, ThemeBackground& v) {
    Kv kv;
    if (!Sub(root, L"Background", kv)) return;
    v.Color = Str(kv, L"Color", v.Color);
    v.Dim = Int(kv, L"Dim", v.Dim);
    v.Blur = Num(kv, L"Blur", v.Blur);
}

int ClampWeight(int weight) {
    if (weight < 100) return 100;
    if (weight > 900) return 900;
    return weight;
}

// 归一化规则照 LockTheme.cs:361-385。
void Normalize(LockTheme& t) {
    if (t.Clock.Size < 8) t.Clock.Size = 8;
    if (t.Date.Size < 6) t.Date.Size = 6;
    if (t.Input.Height < 20) t.Input.Height = 20;
    if (t.Button.Height < 20) t.Button.Height = 20;
    if (t.Card.Width < 120) t.Card.Width = 120;
    t.Clock.Weight = ClampWeight(t.Clock.Weight);
}

bool DirExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// 和 Directory.CreateDirectory 一样，父目录不存在时一起建出来。
bool EnsureDir(const std::wstring& path) {
    if (DirExists(path)) return true;
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\' && path[i] != L'/') continue;
        const std::wstring part = path.substr(0, i);
        if (part.size() == 2 && part[1] == L':') continue;  // 盘符本身已存在
        CreateDirectoryW(part.c_str(), nullptr);
    }
    return DirExists(path);
}

bool FileExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

long long FileBytes(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return -1;
    return ((long long)data.nFileSizeHigh << 32) | (long long)data.nFileSizeLow;
}

std::wstring ReadText(const std::wstring& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::wstring();
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF) bytes.erase(0, 3);
    if (bytes.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), &out[0], n);
    return out;
}

// 样例主题写成 UTF-8 无 BOM，和 WPF 版 File.WriteAllText(UTF8Encoding(false)) 一致。
bool WriteText(const std::wstring& path, const std::wstring& text) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (n < 0) return false;
    std::string bytes((size_t)n, 0);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &bytes[0], n, nullptr, nullptr);
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(bytes.data(), (std::streamsize)bytes.size());
    return out.good();
}

std::vector<std::wstring> ListJson(const std::wstring& dir) {
    std::vector<std::wstring> list;
    WIN32_FIND_DATAW fd = {};
    HANDLE find = FindFirstFileW((dir + L"\\*.json").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return list;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        list.push_back(fd.cFileName);
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    std::sort(list.begin(), list.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    return list;
}

// 去掉 ".json" 后缀，对应 Path.GetFileNameWithoutExtension。
std::wstring Stem(const std::wstring& name) {
    if (name.size() > 5 && Same(name.substr(name.size() - 5), L".json")) return name.substr(0, name.size() - 5);
    return name;
}

// 最短的能原样读回的十进制写法：74 -> "74"，14.5 -> "14.5"。
std::wstring NumText(double value) {
    wchar_t buf[64] = {};
    for (int digits = 15; digits <= 17; ++digits) {
        swprintf(buf, 64, L"%.*g", digits, value);
        if (wcstod(buf, nullptr) == value) break;
    }
    return buf;
}

std::wstring Brace(std::initializer_list<std::wstring> items) {
    std::wstring out = L"{";
    bool first = true;
    for (const std::wstring& item : items) {
        if (!first) out += L",";
        out += item;
        first = false;
    }
    out += L"}";
    return out;
}

std::wstring Key(const wchar_t* key) { return json::Quote(key); }
std::wstring SText(const wchar_t* key, const std::wstring& value) { return Key(key) + L":" + json::Quote(value); }
std::wstring NText(const wchar_t* key, double value) { return Key(key) + L":" + NumText(value); }
std::wstring IText(const wchar_t* key, int value) { return Key(key) + L":" + std::to_wstring(value); }
std::wstring BText(const wchar_t* key, bool value) { return Key(key) + L":" + (value ? L"true" : L"false"); }
std::wstring Group(const wchar_t* key, std::initializer_list<std::wstring> items) { return Key(key) + L":" + Brace(items); }

std::wstring Spaces(int n) { return std::wstring((size_t)n * 2, L' '); }

// 排版照 LockTheme.cs:427-478 的 Pretty，只是把一行 JSON 拆成缩进的多行。
std::wstring Pretty(const std::wstring& json) {
    std::wstring out;
    out.reserve(json.size() * 2);
    bool inString = false;
    int indent = 0;
    for (size_t i = 0; i < json.size(); ++i) {
        const wchar_t ch = json[i];
        if (inString) {
            out.push_back(ch);
            if (ch == L'\\' && i + 1 < json.size()) {
                out.push_back(json[i + 1]);
                ++i;
            } else if (ch == L'"') {
                inString = false;
            }
            continue;
        }
        if (ch == L'"') {
            inString = true;
            out.push_back(ch);
        } else if (ch == L'{' || ch == L'[') {
            ++indent;
            out.push_back(ch);
            out.push_back(L'\n');
            out += Spaces(indent);
        } else if (ch == L'}' || ch == L']') {
            --indent;
            out.push_back(L'\n');
            out += Spaces(indent);
            out.push_back(ch);
        } else if (ch == L',') {
            out.push_back(ch);
            out.push_back(L'\n');
            out += Spaces(indent);
        } else if (ch == L':') {
            out += L": ";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace

bool LockTheme::Inline() const {
    return Same(Layout, L"inline");
}

std::wstring LockTheme::DateFormat() const {
    return Date.Format.empty() ? std::wstring(L"yyyy年M月d日 dddd") : Date.Format;
}

namespace themes {

const wchar_t* const kDefaultId = L"fluent";

namespace {

std::vector<LockTheme> g_cache;
bool g_loaded = false;

std::vector<LockTheme> LoadExternal() {
    std::vector<LockTheme> list;
    const std::wstring dir = app::ThemeDir();
    if (!DirExists(dir)) return list;
    const std::vector<std::wstring> files = ListJson(dir);
    int used = 0;
    for (size_t i = 0; i < files.size() && used < kMaxFiles; ++i) {
        const std::wstring& name = files[i];
        const std::wstring path = dir + L"\\" + name;
        const long long bytes = FileBytes(path);
        if (bytes < 0) {
            app::Log(L"theme skipped (unreadable): %s", name.c_str());
            continue;
        }
        if (bytes > kMaxFileBytes) {
            app::Log(L"theme skipped (too large): %s", name.c_str());
            continue;
        }
        const std::wstring text = ReadText(path);
        LockTheme t;
        if (text.empty() || !Parse(text, t)) {
            app::Log(L"theme skipped (unreadable): %s", name.c_str());
            continue;
        }
        if (t.Id.empty()) t.Id = Stem(name);
        if (t.Name.empty()) t.Name = t.Id;
        if (t.Schema > 1) app::Log(L"theme %s uses newer schema %d, loading anyway", t.Id.c_str(), t.Schema);
        list.push_back(t);
        ++used;
    }
    return list;
}

std::vector<LockTheme> Load() {
    std::vector<LockTheme> list = LoadExternal();
    std::vector<LockTheme> builtin;
    builtin.push_back(BuiltinFluent());
    builtin.push_back(BuiltinCard());
    builtin.push_back(BuiltinMinimal());
    for (const LockTheme& b : builtin) {
        bool shadowed = false;
        for (const LockTheme& e : list) {
            if (Same(e.Id, b.Id)) {
                shadowed = true;
                break;
            }
        }
        if (!shadowed) list.push_back(b);
    }
    return list;
}

void WriteSample(const LockTheme& t) {
    const std::wstring path = app::ThemeDir() + L"\\" + t.Id + L".json";
    if (FileExists(path)) {
        app::Log(L"theme sample kept (exists): %s", path.c_str());
        return;
    }
    if (!WriteText(path, Pretty(Serialize(t)))) app::Log(L"theme sample write failed: %s", path.c_str());
}

}  // namespace

std::wstring Dir() {
    return app::ThemeDir();
}

bool Parse(const std::wstring& json, LockTheme& out) {
    Kv kv;
    if (!json::ParseObject(json, kv)) {
        app::Log(L"theme: JSON parse failed");
        return false;
    }
    LockTheme t;
    t.Schema = Int(kv, L"Schema", t.Schema);
    t.Id = Str(kv, L"Id", t.Id);
    t.Name = Str(kv, L"Name", t.Name);
    t.Layout = Str(kv, L"Layout", t.Layout);
    ReadClock(kv, t.Clock);
    ReadDate(kv, t.Date);
    ReadCard(kv, t.Card);
    ReadBrand(kv, t.Brand);
    ReadSubtitle(kv, t.Subtitle);
    ReadInput(kv, t.Input);
    ReadButton(kv, t.Button);
    ReadLink(kv, t.Link);
    ReadBackground(kv, t.Background);
    Normalize(t);
    out = t;
    return true;
}

const std::vector<LockTheme>& All() {
    if (!g_loaded) {
        g_cache = Load();
        g_loaded = true;
    }
    return g_cache;
}

void Reset() {
    g_cache.clear();
    g_loaded = false;
}

const LockTheme& Find(const std::wstring& id) {
    const std::vector<LockTheme>& all = All();
    if (!id.empty()) {
        for (const LockTheme& t : all) {
            if (Same(t.Id, id)) return t;
        }
    }
    for (const LockTheme& t : all) {
        if (Same(t.Id, kDefaultId)) return t;
    }
    static const LockTheme fallback = BuiltinFluent();
    return fallback;
}

LockTheme BuiltinFluent() {
    LockTheme t;
    t.Id = L"fluent";
    t.Name = L"Windows 11 Fluent";
    t.Layout = L"stacked";
    t.Clock.Size = 74;
    t.Clock.Weight = 200;
    t.Clock.Color = L"#FFFFFFFF";
    t.Date.Size = 15;
    t.Date.Color = L"#FFC9C9C9";
    t.Date.Gap = 26;
    t.Card.Show = false;
    t.Brand.Show = false;
    t.Subtitle.Size = 13;
    t.Subtitle.Color = L"#FFC9C9C9";
    t.Subtitle.Gap = 14;
    t.Input.Kind = L"box";
    t.Input.Height = 40;
    t.Input.Radius = 4;
    t.Input.Bg = L"#FF2B2B2B";
    t.Input.Border = L"#FF3D3D3D";
    t.Input.Hint = L"#FF9A9A9A";
    t.Button.Kind = L"filled";
    t.Button.Height = 40;
    t.Button.Radius = 4;
    t.Button.Bg = L"#FF0078D4";
    t.Button.Hover = L"#FF1A86DC";
    t.Button.Gap = 12;
    t.Link.Size = 12;
    t.Link.Color = L"#FF60CDFF";
    t.Link.Gap = 12;
    t.Background.Color = L"#FF080A10";
    t.Background.Dim = 150;
    return t;
}

LockTheme BuiltinCard() {
    LockTheme t;
    t.Id = L"card";
    t.Name = L"卡片";
    t.Layout = L"stacked";
    t.Clock.Size = 74;
    t.Clock.Weight = 300;
    t.Clock.Color = L"#FFFFFFFF";
    t.Date.Size = 17;
    t.Date.Color = L"#FFBEC8D7";
    t.Date.Gap = 26;
    t.Card.Show = true;
    t.Card.Width = 380;
    t.Card.Radius = 14;
    t.Card.PadH = 26;
    t.Card.PadV = 20;
    t.Card.Bg = L"#D81C1C26";
    t.Card.Border = L"#37FFFFFF";
    t.Brand.Show = true;
    t.Brand.Text = L"Q L O C K";
    t.Brand.Size = 11;
    t.Brand.Color = L"#FF78A0E6";
    t.Brand.Gap = 6;
    t.Subtitle.Size = 14.5;
    t.Subtitle.Color = L"#FFE8EEF7";
    t.Subtitle.Gap = 14;
    t.Input.Height = 40;
    t.Input.Radius = 7;
    t.Input.Bg = L"#1EFFFFFF";
    t.Input.Border = L"#50FFFFFF";
    t.Input.Hint = L"#69FFFFFF";
    t.Button.Height = 40;
    t.Button.Radius = 8;
    t.Button.Bg = L"#FF3A76E8";
    t.Button.Hover = L"#FF4E8AFA";
    t.Button.Gap = 12;
    t.Link.Size = 12;
    t.Link.Color = L"#FF8CB4F0";
    t.Link.Gap = 12;
    t.Background.Color = L"#FF080A10";
    t.Background.Dim = 150;
    return t;
}

LockTheme BuiltinMinimal() {
    LockTheme t;
    t.Id = L"minimal";
    t.Name = L"极简";
    t.Layout = L"stacked";
    t.Clock.Size = 76;
    t.Clock.Weight = 200;
    t.Clock.Color = L"#FFFFFFFF";
    t.Date.Size = 13;
    t.Date.Color = L"#FF6F7683";
    t.Date.Gap = 28;
    t.Card.Show = false;
    t.Brand.Show = false;
    t.Subtitle.Size = 12;
    t.Subtitle.Color = L"#FF4B515C";
    t.Subtitle.Gap = 16;
    t.Input.Kind = L"underline";
    t.Input.Height = 34;
    t.Input.Radius = 0;
    t.Input.Size = 14;
    t.Input.Bg = L"#00000000";
    t.Input.Border = L"#FF2A2F38";
    t.Input.Fg = L"#FFE8EEF7";
    t.Input.Hint = L"#FF5C636E";
    t.Button.Kind = L"text";
    t.Button.Height = 32;
    t.Button.Radius = 6;
    t.Button.Bg = L"#00000000";
    t.Button.Fg = L"#FFCFD5DF";
    t.Button.Hover = L"#FF1A1D23";
    t.Button.Gap = 10;
    t.Link.Size = 11;
    t.Link.Color = L"#FF5C636E";
    t.Link.Gap = 10;
    t.Background.Color = L"#FF080A10";
    t.Background.Dim = 150;
    return t;
}

std::wstring Serialize(const LockTheme& t) {
    std::vector<std::wstring> top;
    top.push_back(Group(L"Background", {NText(L"Blur", t.Background.Blur), SText(L"Color", t.Background.Color), IText(L"Dim", t.Background.Dim)}));
    top.push_back(Group(L"Brand", {SText(L"Color", t.Brand.Color), NText(L"Gap", t.Brand.Gap), BText(L"Show", t.Brand.Show), NText(L"Size", t.Brand.Size), SText(L"Text", t.Brand.Text)}));
    top.push_back(Group(L"Button", {SText(L"Bg", t.Button.Bg), SText(L"Border", t.Button.Border), SText(L"Fg", t.Button.Fg), NText(L"Gap", t.Button.Gap),
                                    NText(L"Height", t.Button.Height), SText(L"Hover", t.Button.Hover), SText(L"Kind", t.Button.Kind), NText(L"Radius", t.Button.Radius),
                                    NText(L"Size", t.Button.Size), NText(L"Width", t.Button.Width)}));
    top.push_back(Group(L"Card", {SText(L"Bg", t.Card.Bg), SText(L"Border", t.Card.Border), NText(L"PadH", t.Card.PadH), NText(L"PadV", t.Card.PadV),
                                  NText(L"Radius", t.Card.Radius), BText(L"Show", t.Card.Show), NText(L"Width", t.Card.Width)}));
    top.push_back(Group(L"Clock", {SText(L"Color", t.Clock.Color), BText(L"Show", t.Clock.Show), NText(L"Size", t.Clock.Size), IText(L"Weight", t.Clock.Weight)}));
    top.push_back(Group(L"Date", {SText(L"Color", t.Date.Color), SText(L"Format", t.Date.Format), NText(L"Gap", t.Date.Gap), NText(L"Size", t.Date.Size)}));
    top.push_back(SText(L"Id", t.Id));
    top.push_back(Group(L"Input", {SText(L"Bg", t.Input.Bg), SText(L"Border", t.Input.Border), SText(L"Fg", t.Input.Fg), NText(L"Height", t.Input.Height),
                                   SText(L"Hint", t.Input.Hint), SText(L"Kind", t.Input.Kind), NText(L"Radius", t.Input.Radius), NText(L"Size", t.Input.Size),
                                   NText(L"Width", t.Input.Width)}));
    top.push_back(SText(L"Layout", t.Layout));
    top.push_back(Group(L"Link", {SText(L"Color", t.Link.Color), NText(L"Gap", t.Link.Gap), NText(L"Size", t.Link.Size)}));
    top.push_back(SText(L"Name", t.Name));
    top.push_back(IText(L"Schema", t.Schema));
    top.push_back(Group(L"Subtitle", {SText(L"Color", t.Subtitle.Color), NText(L"Gap", t.Subtitle.Gap), BText(L"Show", t.Subtitle.Show), NText(L"Size", t.Subtitle.Size)}));

    std::wstring out = L"{";
    for (size_t i = 0; i < top.size(); ++i) {
        if (i > 0) out += L",";
        out += top[i];
    }
    out += L"}";
    return out;
}

void EnsureSampleThemes() {
    const std::wstring dir = app::ThemeDir();
    if (DirExists(dir)) return;
    if (!EnsureDir(dir)) {
        app::Log(L"theme folder setup failed: %s", dir.c_str());
        return;
    }
    WriteSample(BuiltinFluent());
    WriteSample(BuiltinCard());
    WriteSample(BuiltinMinimal());
    app::Log(L"theme folder created: %s", dir.c_str());
}

D2D1_COLOR_F Color(const std::wstring& text, D2D1_COLOR_F fallback) {
    std::wstring s = Trim(text);
    if (!s.empty() && s.front() == L'#') s.erase(0, 1);
    if (s.size() != 6 && s.size() != 8) return fallback;
    for (wchar_t c : s) {
        const bool hex = (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
        if (!hex) return fallback;
    }
    const unsigned long v = wcstoul(s.c_str(), nullptr, 16);
    D2D1_COLOR_F c = {};
    if (s.size() == 8) {
        c.a = (float)((v >> 24) & 0xFF) / 255.0f;
        c.r = (float)((v >> 16) & 0xFF) / 255.0f;
        c.g = (float)((v >> 8) & 0xFF) / 255.0f;
        c.b = (float)(v & 0xFF) / 255.0f;
    } else {
        c.a = 1.0f;
        c.r = (float)((v >> 16) & 0xFF) / 255.0f;
        c.g = (float)((v >> 8) & 0xFF) / 255.0f;
        c.b = (float)(v & 0xFF) / 255.0f;
    }
    return c;
}

}  // namespace themes
