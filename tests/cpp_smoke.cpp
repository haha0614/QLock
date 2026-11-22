#include "../src/qlock.h"
#include <cstdio>
#include <string>

static int g_fail = 0;

static void Out(const std::wstring& text) {
    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, nullptr, nullptr);
    fputs(utf8.c_str(), stdout);
    fputs("\n", stdout);
}

static void Check(bool ok, const wchar_t* what) {
    Out(std::wstring(ok ? L"PASS  " : L"FAIL  ") + what);
    if (!ok) g_fail++;
}

int main(int argc, char** argv) {
    bool doSave = false;
    for (int i = 1; i < argc; i++) if (std::string(argv[i]) == "--save") doSave = true;

    Check(app::LoadConfig(), L"load config");

    Out(L"--- config ---");
    Out(L"Version=" + std::to_wstring(app::cfg.Version) + L" ThemeId=" + app::cfg.ThemeId +
        L" Autostart=" + app::cfg.Autostart);
    Out(L"Subtitle=" + app::cfg.Subtitle);
    Out(L"ShowClock=" + std::to_wstring(app::cfg.ShowClock) + L" ShowSubtitle=" + std::to_wstring(app::cfg.ShowSubtitle) +
        L" ShowTray=" + std::to_wstring(app::cfg.ShowTray));
    Out(L"CornerRadius=" + std::to_wstring(app::cfg.CornerRadius) + L" BlurRadius=" + std::to_wstring(app::cfg.BlurRadius) +
        L" MaskOpacity=" + std::to_wstring(app::cfg.MaskOpacity));
    Out(L"BackgroundImage=" + app::cfg.BackgroundImage);
    Out(L"PasswordHash len=" + std::to_wstring(app::cfg.PasswordHash.size()) + L" Iterations=" +
        std::to_wstring(app::cfg.PasswordIterations) + L" Salt len=" + std::to_wstring(app::cfg.PasswordSalt.size()));
    Out(L"ClockFontName=[" + app::ClockFontName() + L"] UiFontName=[" + app::UiFontName() + L"]");

    Check(app::cfg.ThemeId == L"card", L"themeId parsed");
    Check(app::cfg.Subtitle == L"密码解锁", L"chinese subtitle parsed");
    Check(app::cfg.BackgroundImage == L"E:\\111\\视频\\国旗4096.png", L"background path with chinese");
    Check(app::cfg.CornerRadius == 32 && app::cfg.BlurRadius == 60 && app::cfg.MaskOpacity == 60, L"sliders parsed");
    Check(app::cfg.LockOnStartup && app::cfg.InterceptWinL && app::cfg.BlockSystemHotkeys, L"bool flags parsed");
    Check(app::cfg.ShowSubtitle == false && app::cfg.ShowClock == true, L"show flags parsed");
    Check(app::HasPassword(), L"has password");
    Check(!app::VerifyPassword(L"wrong-password-xyz"), L"wrong password rejected");
    Check(app::cfg.LastUpdateCheckTicks > 0, L"ticks parsed");

    std::map<std::wstring, std::wstring> obj;
    Check(json::ParseObject(L"{\"A\":\"x\\/y\",\"B\":\"中\",\"C\":false}", obj), L"parse sample object");
    Check(json::Unescape(obj[L"A"]) == L"x/y", L"slash escape unescaped");
    Check(json::Unescape(obj[L"B"]) == L"中", L"unicode escape unescaped");
    Check(json::GetBool(obj[L"C"], true) == false, L"bool getter");
    Check(json::Quote(L"a\"b") == L"\"a\\\"b\"", L"quote roundtrip");

    const BYTE raw[] = {1, 2, 250, 3};
    std::wstring enc = b64::Encode(raw, sizeof(raw));
    std::vector<BYTE> back;
    Check(b64::Decode(enc, back) && back.size() == 4 && back[2] == 250, L"base64 roundtrip");

    std::wstring hash = app::cfg.PasswordHash;
    if (doSave) {
        Check(app::SaveConfig(), L"save config");
        Check(app::LoadConfig(), L"reload after save");
        Check(app::cfg.PasswordHash == hash, L"password hash survived save");
        Check(app::cfg.Subtitle == L"密码解锁" && app::cfg.BackgroundImage.find(L"国旗4096") != std::wstring::npos,
              L"text fields survived save");
    }

    Out(g_fail == 0 ? L"== ALL PASS ==" : L"== FAILED ==");
    return g_fail == 0 ? 0 : 1;
}
