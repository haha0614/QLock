#include "../src/qlock.h"
#include "../src/theme.h"
#include <bcrypt.h>
#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

static void Out(const std::wstring& text) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, nullptr, nullptr);
    fputs(utf8.c_str(), stdout);
    fputs("\n", stdout);
}

static void Check(bool ok, const std::wstring& what) {
    Out(std::wstring(ok ? L"PASS  " : L"FAIL  ") + what);
    if (ok) ++g_pass; else ++g_fail;
}

// 测试全程在临时目录里跑，不碰用户真实的 %APPDATA%\QLock。
static std::wstring TempRoot() {
    wchar_t base[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, base);
    wchar_t leaf[64] = {};
    _snwprintf_s(leaf, _countof(leaf), _TRUNCATE, L"qlock_tests_%lu", GetCurrentProcessId());
    return std::wstring(base) + leaf;
}

static void RemoveTree(const std::wstring& path) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((path + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring child = path + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                RemoveTree(child);
            } else {
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(path.c_str());
}

static std::wstring Pbkdf2(const std::wstring& password, const std::vector<BYTE>& salt, int iterations) {
    int len = WideCharToMultiByte(CP_UTF8, 0, password.c_str(), (int)password.size(), nullptr, 0, nullptr, nullptr);
    std::string bytes((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, password.c_str(), (int)password.size(), &bytes[0], len, nullptr, nullptr);

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return L"";
    BYTE raw[32] = {};
    NTSTATUS st = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)bytes.data(), (ULONG)bytes.size(), (PUCHAR)salt.data(),
                                        (ULONG)salt.size(), (ULONGLONG)iterations, raw, sizeof(raw), 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st < 0) return L"";
    return b64::Encode(raw, sizeof(raw));
}

static bool Near(float a, float b) { return (a > b ? a - b : b - a) < 0.004f; }

static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

int main(int, char**) {
    std::wstring root = TempRoot();
    RemoveTree(root);
    CreateDirectoryW(root.c_str(), nullptr);
    SetEnvironmentVariableW(L"APPDATA", root.c_str());
    SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str());

    Out(L"== 1. 没有配置文件时用默认值 ==");
    Check(!app::LoadConfig(), L"missing config reports not found");
    Check(app::cfg.ThemeId == L"fluent", L"default theme is fluent");
    Check(app::cfg.Subtitle == L"输入密码解锁", L"default subtitle");
    Check(app::cfg.CornerRadius == -1 && app::cfg.BlurRadius == -1 && app::cfg.MaskOpacity == -1,
          L"sliders default to -1 (follow theme)");
    Check(app::cfg.LockOnStartup && app::cfg.ShowClock && app::cfg.ShowTray, L"default bool flags");
    Check(!app::HasPassword(), L"no password by default");
    Check(!app::VerifyPassword(L"x"), L"verify without hash is false");

    Out(L"== 2. 保存再读回（含中文、引号、反斜杠） ==");
    app::cfg.ThemeId = L"card";
    app::cfg.Subtitle = L"密码 \"解锁\" \\ 试试";
    app::cfg.BackgroundImage = L"C:\\图片 目录\\壁纸.png";
    app::cfg.CornerRadius = 18;
    app::cfg.BlurRadius = 12;
    app::cfg.MaskOpacity = 40;
    app::cfg.ShowSubtitle = false;
    app::cfg.FontFamily = L"Consolas";
    app::cfg.ClockFont = L"Courier New";
    app::cfg.ClockFontEnabled = true;
    app::cfg.LastUpdateCheckTicks = 639309207032530893LL;
    app::cfg.SkippedVersion = L"0.2.0";
    std::vector<BYTE> salt = {1, 2, 3, 250, 200, 9, 9, 9};
    app::cfg.PasswordIterations = 12000;
    app::cfg.PasswordSalt = b64::Encode(salt.data(), salt.size());
    app::cfg.PasswordHash = Pbkdf2(L"p@ss 密码", salt, app::cfg.PasswordIterations);
    Check(!app::cfg.PasswordHash.empty(), L"test hash computed");
    app::SetPassword(L"p@ss 密码");
    Check(app::StoredPassword() == L"p@ss 密码", L"stored password decrypts as utf-8");
    Check(app::VerifyPassword(L"p@ss 密码"), L"password written by SetPassword verifies");

    Check(!app::HasRecovery(), L"no recovery code by default");
    std::wstring code = app::NewRecoveryCode();
    Check(code.size() == 11 && code[5] == L'-', L"recovery code looks like XXXXX-XXXXX");
    app::SetRecovery(code);
    Check(app::HasRecovery(), L"recovery code saved");
    Check(app::VerifyRecovery(code), L"recovery code verifies");
    Check(app::VerifyRecovery(L" " + code + L" "), L"recovery code ignores surrounding spaces");
    std::wstring lower = code;
    for (size_t i = 0; i < lower.size(); ++i) lower[i] = (wchar_t)towlower(lower[i]);
    Check(app::VerifyRecovery(lower), L"recovery code ignores case and dashes");
    Check(!app::VerifyRecovery(L"AAAAA-AAAAA"), L"wrong recovery code rejected");
    app::ClearRecovery();
    Check(!app::HasRecovery() && !app::VerifyRecovery(code), L"cleared recovery code stops working");

    Check(app::SaveConfig(), L"save config");
    Check(Exists(app::ConfigPath()), L"config file exists");

    app::cfg = Config();  // 清空内存里的值，确认真的是从文件读回来的
    Check(app::LoadConfig(), L"reload config");
    Check(app::cfg.ThemeId == L"card", L"theme survived");
    Check(app::cfg.Subtitle == L"密码 \"解锁\" \\ 试试", L"quotes and backslash survived");
    Check(app::cfg.BackgroundImage == L"C:\\图片 目录\\壁纸.png", L"path with chinese and space survived");
    Check(app::cfg.CornerRadius == 18 && app::cfg.BlurRadius == 12 && app::cfg.MaskOpacity == 40, L"sliders survived");
    Check(app::cfg.ShowSubtitle == false, L"false flag survived");
    Check(app::cfg.FontFamily == L"Consolas" && app::cfg.ClockFont == L"Courier New" && app::cfg.ClockFontEnabled,
          L"fonts survived");
    Check(app::ClockFontName() == L"Courier New", L"clock font name uses clock font when enabled");
    Check(app::cfg.LastUpdateCheckTicks == 639309207032530893LL, L"ticks survived");
    Check(app::cfg.SkippedVersion == L"0.2.0", L"skipped version survived");

    Out(L"== 3. 密码校验 ==");
    Check(app::HasPassword(), L"has password");
    Check(app::VerifyPassword(L"p@ss 密码"), L"correct password accepted");
    Check(!app::VerifyPassword(L"p@ss 密码 "), L"trailing space rejected");
    Check(!app::VerifyPassword(L"P@ss 密码"), L"wrong case rejected");
    app::cfg.ClockFontEnabled = false;
    Check(app::ClockFontName() == L"Consolas", L"clock font falls back to the ui font");

    Out(L"== 4. JSON ==");
    std::map<std::wstring, std::wstring> obj;
    Check(json::ParseObject(L"{\"A\":\"x\\/y\",\"B\":\"中\\u6587\",\"C\":false,\"D\":42,\"E\":{\"a\":1}}", obj),
          L"parse object with escapes and nesting");
    Check(json::Unescape(obj[L"A"]) == L"x/y", L"slash escape unescaped");
    Check(json::Unescape(obj[L"B"]) == L"中文", L"unicode escape unescaped");
    Check(json::GetBool(obj[L"C"], true) == false, L"bool getter");
    Check(json::GetInt(obj[L"D"], 0) == 42, L"int getter");
    Check(json::GetInt(obj[L"missing"], 7) == 7, L"int getter falls back");
    Check(json::GetLongLong(L"639309207032530893", 0) == 639309207032530893LL, L"long long getter");
    Check(json::Quote(L"a\"b") == L"\"a\\\"b\"", L"quote escapes quotes");
    Check(json::Quote(L"中文") == L"\"中文\"", L"quote keeps chinese");
    Check(!json::ParseObject(L"not json", obj), L"garbage rejected");
    Check(!json::ParseObject(L"[1,2]", obj), L"array rejected at top level");

    Out(L"== 5. base64 ==");
    const BYTE raw[] = {0, 1, 2, 250, 255, 128};
    std::wstring enc = b64::Encode(raw, sizeof(raw));
    std::vector<BYTE> back;
    Check(b64::Decode(enc, back) && back.size() == sizeof(raw), L"base64 roundtrip size");
    bool same = back.size() == sizeof(raw);
    for (size_t i = 0; same && i < sizeof(raw); ++i) same = back[i] == raw[i];
    Check(same, L"base64 roundtrip bytes");
    std::vector<BYTE> none;
    Check(!b64::Decode(L"!!!not base64!!!", none), L"invalid base64 rejected");

    Out(L"== 6. 主题解析与回落 ==");
    LockTheme parsed;
    Check(themes::Parse(L"{\"Schema\":1,\"Id\":\"t\",\"Name\":\"测试\",\"Card\":{\"Show\":true,\"Width\":300},"
                        L"\"Input\":{\"Kind\":\"underline\"},\"Layout\":\"inline\"}", parsed),
          L"parse partial theme");
    Check(parsed.Id == L"t" && parsed.Name == L"测试", L"theme id and name");
    Check(parsed.Card.Show && parsed.Card.Width == 300, L"given card fields");
    Check(parsed.Card.Radius == 14, L"missing card radius falls back");
    Check(parsed.Input.Kind == L"underline" && parsed.Input.Width == 340, L"input kind given, width default");
    Check(parsed.Inline(), L"inline layout detected");
    Check(parsed.Clock.Size == 74 && parsed.Clock.Weight == 300, L"clock defaults");
    LockTheme bad;
    Check(!themes::Parse(L"not a theme", bad), L"garbage theme rejected");
    Check(!themes::Parse(L"[1,2,3]", bad), L"array theme rejected");
    Check(themes::Find(L"does-not-exist").Id == L"fluent", L"find falls back to fluent");
    Check(themes::Find(L"card").Card.Show, L"find card theme");
    Check(themes::All().size() >= 3, L"all() has builtins");

    Out(L"== 7. 主题序列化回读 ==");
    std::wstring text = themes::Serialize(themes::BuiltinCard());
    LockTheme again;
    Check(themes::Parse(text, again), L"serialized theme parses back");
    Check(again.Id == L"card" && again.Card.Show && again.Card.Width == 380, L"roundtrip keeps card fields");
    Check(themes::Serialize(again) == text, L"serialize is idempotent");

    Out(L"== 8. 颜色解析 ==");
    D2D1_COLOR_F fallback = {0.5f, 0.25f, 0.125f, 0.75f};
    D2D1_COLOR_F six = themes::Color(L"#3A76E8", fallback);
    Check(Near(six.a, 1.0f) && Near(six.r, 58 / 255.0f) && Near(six.b, 232 / 255.0f), L"six digit color");
    D2D1_COLOR_F eight = themes::Color(L"#804E8AFA", fallback);
    Check(Near(eight.a, 128 / 255.0f) && Near(eight.g, 138 / 255.0f), L"eight digit color with alpha");
    D2D1_COLOR_F broken = themes::Color(L"#GGGGGG", fallback);
    Check(broken.r == fallback.r && broken.a == fallback.a, L"broken color returns fallback");

    Out(L"== 9. 样例主题写入 ==");
    themes::EnsureSampleThemes();
    for (const wchar_t* id : {L"fluent", L"card", L"minimal"}) {
        Check(Exists(themes::Dir() + L"\\" + id + L".json"), std::wstring(L"sample written: ") + id);
    }

    RemoveTree(root);
    Out(L"");
    Out(L"== " + std::to_wstring(g_pass) + L" PASS / " + std::to_wstring(g_fail) + L" FAIL ==");
    Out(g_fail == 0 ? L"== ALL PASS ==" : L"== FAILED ==");
    return g_fail == 0 ? 0 : 1;
}
