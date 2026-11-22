#pragma once
#include "settings_window.h"
#include "qlock.h"
#include "theme.h"
#include "gfx.h"
#include "lock_view.h"
#include "font_list.h"
#include "autostart.h"
#include "updater.h"
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <objbase.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cwctype>
#include <thread>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

namespace {

// 后台线程查到的版本号，主线程收结果时取走。
std::wstring g_pendingTag;

using gfx::Color;

// 尺寸 token，跟 0.3.0 的设置窗一致。
const float PanePad = 28;
const float NavWidth = 192;
const float NavItemH = 34;
const float ControlH = 34;
const float ChipH = 30;
const float RowH = 58;
const float FooterH = 56;
const float PreviewH = 226;
const float WinW = 780;
const float WinH = 640;

const unsigned kAccent = 0xFF3A76E8;
const unsigned kAccentHover = 0xFF4E8AFA;
const unsigned kAccentSoft = 0x2E6096FA;
const unsigned kSurface = 0xFF181B23;
const unsigned kSurfaceNav = 0xFF12151C;
const unsigned kSurfaceFooter = 0xFF14171E;
const unsigned kLine = 0x1EFFFFFF;
const unsigned kRowLine = 0x18FFFFFF;
const unsigned kInputBg = 0x1EFFFFFF;
const unsigned kInputBorder = 0x50FFFFFF;
const unsigned kChipSurface = 0x23FFFFFF;
const unsigned kChipBorder = 0x5AFFFFFF;
const unsigned kSwitchTrack = 0x32FFFFFF;
const unsigned kText = 0xFFF2F4F8;
const unsigned kTextDim = 0xFFB4BAC8;
const unsigned kTextMuted = 0xFF8A90A0;
const unsigned kError = 0xFFFF8A8A;

// 子窗口 EDIT 不透明，这里取底色叠 12% 白的实色。

std::wstring LeafName(const std::wstring& path);

struct Rect {
    float l = 0, t = 0, r = 0, b = 0;
    float W() const { return r - l; }
    float H() const { return b - t; }
    float Mid() const { return (t + b) / 2; }
    bool Has(float x, float y) const { return x >= l && x <= r && y >= t && y <= b; }
};

enum Kind { K_NAV, K_PILL, K_TOGGLE, K_BTN, K_SLIDER, K_EDIT };

struct Widget {
    int id = 0;
    Kind kind = K_BTN;
    Rect rect;
    bool on = false;
    double value = 0, minv = 0, maxv = 0;
};

enum {
    ID_NAV_BASE = 100,
    ID_PILL_BASE = 200,
    ID_THEMES_FOLDER = 260,
    ID_BG_PICK = 300,
    ID_BG_CLEAR = 301,
    ID_SHOW_CLOCK = 310,
    ID_SNAPSHOT = 311,
    ID_SHOW_SUBTITLE = 312,
    ID_SLIDER_RADIUS = 320,
    ID_SLIDER_BLUR = 321,
    ID_SLIDER_MASK = 322,
    ID_APPLY_PASSWORD = 330,
    ID_RECOVERY = 332,
    ID_REVEAL = 331,
    ID_CLOSE = 400,
    ID_EDIT_SUBTITLE = 500,
    ID_EDIT_PASSWORD = 501,
    ID_EDIT_IDLE = 502,
    ID_INTERCEPT_WINL = 360,
    ID_HOTKEY = 361,
    ID_LOCK_ON_START = 362,
    ID_AUTOSTART = 363,
    ID_ELEVATED = 364,
    ID_BLOCK_HOTKEYS = 365,
    ID_CHECK_UPDATE = 366,
    ID_SKIP_UPDATE = 367,
    ID_FONT_UI = 340,
    ID_FONT_CLOCK = 341,
    ID_FONT_CLOCK_ON = 342,
    ID_FONT_FILTER = 350,
    ID_FONT_CANCEL = 351,
    ID_FONT_DEFAULT = 352,
    ID_FONT_USE = 353,
    ID_FONT_ROW = 1000,
};

const wchar_t* kClass = L"QLockSettings";
const wchar_t* kNavNames[5] = { L"解锁方式", L"外观", L"启动与锁定", L"加固", L"关于" };

class Settings {
public:
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1Bitmap1* target = nullptr;
    float scale = 1.0f;

    int pane = 1;
    float scroll = 0, scrollMax = 0;
    std::vector<Widget> widgets;
    int hot = -1, pressed = -1, dragSlider = 0;
    Rect contentArea;

    lockview::View preview;
    bool previewLoaded = false;

    // 字体选择器：窗口内的一层浮层，不另开窗口，省掉模态消息循环和 DPI 麻烦。
    bool fontOpen = false;
    bool fontForClock = false;
    std::wstring fontFilter;
    std::vector<std::wstring> fontRows;
    int fontSel = 0;
    int fontTop = 0;
    std::map<std::wstring, IDWriteTextFormat*> rowFormats;

    IDWriteTextFormat* fBrand = nullptr;
    IDWriteTextFormat* fTitle = nullptr;
    IDWriteTextFormat* fBody = nullptr;
    IDWriteTextFormat* fSmall = nullptr;
    IDWriteTextFormat* fControl = nullptr;

    std::wstring subtitleText;
    std::wstring passwordText;
    std::wstring idleText;

    // 关于页的更新流程：检查 → 发现新版 → 下载 → 校验 → 原地替换 → 重启。
    enum UpdateState { U_IDLE, U_CHECKING, U_NEWER, U_DOWNLOADING, U_INSTALLING, U_READY };
    UpdateState updateState = U_IDLE;
    std::wstring newTag;
    int updatePercent = 0;
    int focus = 0;
    bool reveal = false;
    bool caretOn = true;

    Rect subtitleRect, passwordRect;

    std::wstring status;
    bool statusError = false;
    std::wstring shownCode;   // 刚生成的恢复码，只在本次打开窗口时显示
    UINT_PTR saveTimer = 0;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool Create(HWND ownerWnd);
    void DestroyAll();

    float Sx(float logical) const { return logical * scale; }

    D2D1_ROUNDED_RECT Round(const Rect& r, float radius) {
        float rad = gfx::Radius(radius, r.H());
        return D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), rad, rad);
    }

    void MakeFormats() {
        ReleaseFmt(&fBrand);
        ReleaseFmt(&fTitle);
        ReleaseFmt(&fBody);
        ReleaseFmt(&fSmall);
        ReleaseFmt(&fControl);
        std::wstring ui = app::UiFontName();
        fBrand = gfx::Format(ui, 17.0f, 700, DWRITE_TEXT_ALIGNMENT_LEADING);
        fTitle = gfx::Format(ui, 19.0f, 700, DWRITE_TEXT_ALIGNMENT_LEADING);
        fBody = gfx::Format(ui, 13.5f, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        fSmall = gfx::Format(ui, 12.0f, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        fControl = gfx::Format(ui, 14.0f, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        for (auto& kv : rowFormats) if (kv.second) kv.second->Release();
        rowFormats.clear();
    }

    static void ReleaseFmt(IDWriteTextFormat** fmt) {
        if (*fmt) { (*fmt)->Release(); *fmt = nullptr; }
    }

    bool EnsureTarget() {
        if (!gfx::Init()) return false;
        if (!dc && FAILED(gfx::Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return false;
        dc->SetDpi(96.0f * scale, 96.0f * scale);
        if (!swap) {
            IDXGIDevice* dxgi = nullptr;
            if (FAILED(gfx::D3D()->QueryInterface(IID_PPV_ARGS(&dxgi)))) return false;
            IDXGIAdapter* adapter = nullptr;
            if (FAILED(dxgi->GetAdapter(&adapter))) { dxgi->Release(); return false; }
            IDXGIFactory2* factory = nullptr;
            HRESULT hr = adapter->GetParent(IID_PPV_ARGS(&factory));
            adapter->Release();
            dxgi->Release();
            if (FAILED(hr)) return false;
            RECT client;
            GetClientRect(hwnd, &client);
            DXGI_SWAP_CHAIN_DESC1 desc = {};
            desc.Width = (UINT)(std::max)(1L, client.right - client.left);
            desc.Height = (UINT)(std::max)(1L, client.bottom - client.top);
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.Scaling = DXGI_SCALING_NONE;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            hr = factory->CreateSwapChainForHwnd(gfx::D3D(), hwnd, &desc, nullptr, nullptr, &swap);
            factory->Release();
            if (FAILED(hr)) return false;
        }
        if (!target) {
            IDXGISurface* surface = nullptr;
            if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
            D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f * scale, 96.0f * scale);
            HRESULT hr = dc->CreateBitmapFromDxgiSurface(surface, &props, &target);
            surface->Release();
            if (FAILED(hr)) return false;
            dc->SetTarget(target);
        }
        return true;
    }

    void Resize() {
        if (!swap) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        dc->SetTarget(nullptr);
        if (target) { target->Release(); target = nullptr; }
        swap->ResizeBuffers(0, (UINT)(std::max)(1L, rc.right - rc.left), (UINT)(std::max)(1L, rc.bottom - rc.top),
                            DXGI_FORMAT_UNKNOWN, 0);
    }

    void Invalidate() { InvalidateRect(hwnd, nullptr, FALSE); }

    void QueueSave() {
        if (saveTimer) KillTimer(hwnd, saveTimer);
        saveTimer = SetTimer(hwnd, 3, 400, nullptr);
    }

    void FlushSave() {
        if (saveTimer) { KillTimer(hwnd, 3); saveTimer = 0; }
        if (app::SaveConfig()) {
            status = L"已保存，下次锁屏生效";
            statusError = false;
        } else {
            status = L"保存失败，看看日志";
            statusError = true;
        }
        Invalidate();
    }

    static void CopyText(const std::wstring& text) {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem) {
            void* dst = GlobalLock(mem);
            if (dst) {
                memcpy(dst, text.c_str(), bytes);
                GlobalUnlock(mem);
                SetClipboardData(CF_UNICODETEXT, mem);
            } else {
                GlobalFree(mem);
            }
        }
        CloseClipboard();
    }

    void SetStatus(const std::wstring& text, bool error) {
        status = text;
        statusError = error;
        Invalidate();
    }

    // ---- 绘制小件 ----
    void Toggle(const Rect& r, bool on) {
        Rect track{ r.r - 40, r.Mid() - 11, r.r, r.Mid() + 11 };
        float rad = track.H() / 2;
        gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(track.l, track.t, track.r, track.b), rad, rad),
                       Color(on ? kAccent : kSwitchTrack));
        float knob = track.H() - 4;
        float cx = on ? track.r - 2 - knob / 2 : track.l + 2 + knob / 2;
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(dc->CreateSolidColorBrush(on ? Color(0xFFFFFFFF) : Color(0xFFC8CDD8), &brush))) {
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, track.Mid()), knob / 2, knob / 2), brush);
            brush->Release();
        }
    }

    void Slider(const Rect& r, double value, double minv, double maxv) {
        float t = maxv > minv ? (float)((value - minv) / (maxv - minv)) : 0.0f;
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        float left = r.l + 7, right = r.r - 7;
        float mid = r.Mid();
        gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(left, mid - 2, right, mid + 2), 2, 2), Color(kSwitchTrack));
        float knobX = left + (right - left) * t;
        gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(left, mid - 2, knobX, mid + 2), 2, 2), Color(kAccent));
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(dc->CreateSolidColorBrush(Color(0xFFFFFFFF), &brush))) {
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, mid), 7, 7), brush);
            brush->Release();
        }
    }

    void Button(const Rect& r, const std::wstring& text, bool primary, bool hovered, bool enabled = true) {
        if (!enabled) {
            gfx::FillRound(dc, Round(r, 8), Color(0x0EFFFFFF));
            gfx::Text(dc, fControl, text, D2D1::RectF(r.l, r.t, r.r, r.b), Color(kTextMuted));
            return;
        }
        unsigned bg = primary ? (hovered ? kAccentHover : kAccent) : (hovered ? kChipSurface : 0x14FFFFFF);
        gfx::FillRound(dc, Round(r, 8), Color(bg));
        if (!primary) gfx::StrokeRound(dc, Round(r, 8), Color(kChipBorder), 1.0f);
        gfx::Text(dc, fControl, text, D2D1::RectF(r.l, r.t, r.r, r.b), Color(kText));
    }

    void Pill(const Rect& r, const std::wstring& text, bool on, bool hovered) {
        unsigned bg = on ? kAccent : (hovered ? kChipBorder : kChipSurface);
        gfx::FillRound(dc, Round(r, ChipH / 2), Color(bg));
        if (!on) gfx::StrokeRound(dc, Round(r, ChipH / 2), Color(hovered ? kChipBorder : kLine), 1.0f);
        gfx::Text(dc, fSmall, text, D2D1::RectF(r.l, r.t, r.r, r.b), Color(on ? kText : kTextDim));
    }

    void RowLine(float y, float left, float right) {
        gfx::FillRect(dc, D2D1::RectF(left, y, right, y + 1), Color(kRowLine));
    }

    // 优先用资源管理器缓存的当前壁纸，没有再读注册表里的路径。
#pragma comment(lib, "advapi32.lib")
    static std::wstring DesktopWallpaper() {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD n = (DWORD)_countof(buf);
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_READ, &key) == ERROR_SUCCESS) {
            DWORD type = 0, size = (DWORD)sizeof(buf);
            RegQueryValueExW(key, L"WallPaper", nullptr, &type, (BYTE*)buf, &size);
            RegCloseKey(key);
        }
        std::wstring path = buf;
        if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;

        wchar_t appData[MAX_PATH * 2] = {};
        if (GetEnvironmentVariableW(L"APPDATA", appData, _countof(appData))) {
            std::wstring cache = std::wstring(appData) + L"\\Microsoft\\Windows\\Themes\\TranscodedWallpaper";
            if (GetFileAttributesW(cache.c_str()) != INVALID_FILE_ATTRIBUTES) return cache;
        }
        return std::wstring();
    }

    int ThemeIndex() const {
        const std::vector<LockTheme>& all = themes::All();
        for (size_t i = 0; i < all.size(); ++i) if (all[i].Id == app::cfg.ThemeId) return (int)i;
        return 0;
    }

    void DrawPreview(const Rect& box) {
        // 没挑背景图又勾了截图背景时，预览拿当前桌面壁纸顶替——真锁屏截的是锁那一刻的屏幕。
        std::wstring override;
        if (app::cfg.BackgroundImage.empty() && app::cfg.BackgroundSnapshot) override = DesktopWallpaper();
        if (override != preview.imageOverride) {
            preview.imageOverride = override;
            preview.DropDeviceResources();
            previewLoaded = false;
        }
        if (!previewLoaded) {
            preview.theme = themes::Find(app::cfg.ThemeId);
            preview.Reload(dc);
            previewLoaded = true;
        }
        float s = (std::min)(box.W() / 960.0f, box.H() / 408.0f);
        float pw = 960 * s, ph = 408 * s;
        float ox = box.l + (box.W() - pw) / 2;
        float oy = box.t + (box.H() - ph) / 2;

        ID2D1RoundedRectangleGeometry* geom = nullptr;
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(box.l, box.t, box.r, box.b), 10, 10);
        gfx::D2D()->CreateRoundedRectangleGeometry(rr, &geom);
        if (geom) {
            D2D1_LAYER_PARAMETERS lp = D2D1::LayerParameters(D2D1::InfiniteRect(), geom);
            dc->PushLayer(lp, nullptr);
        }
        dc->SetTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(ox, oy));
        preview.Draw(dc, 960, 408);
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        if (geom) { dc->PopLayer(); geom->Release(); }
    }

    // ---- 布局 + 绘制 ----
    void Paint();
    void LayoutLook(float left, float right, float top, float bottom);
    void LayoutUnlock(float left, float right, float top, float bottom);
    void LayoutStart(float left, float right, float top, float bottom);
    void LayoutHard(float left, float right, float top, float bottom);
    void LayoutAbout(float left, float right, float top, float bottom);
    void CommitIdle();
    void CheckUpdate();
    void StartDownload();
    void PaintFooter(float w, float h, float left, float right);
    void PaintNav(float h);

    void DrawField(const Rect& r, const std::wstring& text, int id, bool password, bool enabled,
                   const std::wstring& hint);
    void DrawFontDialog(float w, float h);
    IDWriteTextFormat* RowFormat(const std::wstring& family);
    void RebuildFontRows();
    void OpenFontDialog(bool forClock);
    void CloseFontDialog();
    void CommitFont(const std::wstring& family);
    void Apply(int id);
    void ApplySlider(int id, float x);
    void ClickAt(float x, float y, bool down);
};

void Settings::PaintNav(float h) {
    gfx::FillRect(dc, D2D1::RectF(0, 0, NavWidth, h), Color(kSurfaceNav));
    gfx::FillRect(dc, D2D1::RectF(NavWidth - 1, 0, NavWidth, h), Color(kLine));
    gfx::Text(dc, fBrand, L"QLock", D2D1::RectF(28, 22, NavWidth - 16, 50), Color(kText));
    float y = 76;
    for (int i = 0; i < 5; ++i) {
        Rect r{ 16, y, NavWidth - 16, y + NavItemH };
        bool sel = pane == i;
        if (sel) {
            gfx::FillRound(dc, Round(r, 8), Color(kAccentSoft));
        } else if (hot == ID_NAV_BASE + i) {
            gfx::FillRound(dc, Round(r, 8), Color(kChipSurface));
        }
        gfx::Text(dc, fBody, kNavNames[i], D2D1::RectF(r.l + 12, r.t, r.r - 8, r.b), Color(sel ? kText : kTextDim));
        Widget w;
        w.id = ID_NAV_BASE + i;
        w.kind = K_NAV;
        w.rect = r;
        w.on = sel;
        widgets.push_back(w);
        y += NavItemH + 2;
    }
}

void Settings::PaintFooter(float w, float h, float left, float right) {
    gfx::FillRect(dc, D2D1::RectF(0, h - FooterH, w, h), Color(kSurfaceFooter));
    gfx::FillRect(dc, D2D1::RectF(0, h - FooterH, w, h - FooterH + 1), Color(kLine));
    if (!status.empty()) {
        gfx::Text(dc, fSmall, status, D2D1::RectF(left, h - FooterH, right - 110, h), Color(statusError ? kError : kTextMuted));
    }
    Rect close{ right - 96, h - FooterH + 11, right, h - FooterH + 11 + ControlH };
    Button(close, L"关闭", true, hot == ID_CLOSE);
    Widget wg;
    wg.id = ID_CLOSE;
    wg.kind = K_BTN;
    wg.rect = close;
    widgets.push_back(wg);
}

// ---- 字体选择器 ----
IDWriteTextFormat* Settings::RowFormat(const std::wstring& family) {
    auto it = rowFormats.find(family);
    if (it != rowFormats.end()) return it->second;
    IDWriteTextFormat* fmt = gfx::Format(family, 14.0f, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
    rowFormats[family] = fmt;
    return fmt;
}

void Settings::RebuildFontRows() {
    std::wstring needle = fontFilter;
    for (size_t i = 0; i < needle.size(); ++i) needle[i] = (wchar_t)towlower(needle[i]);
    fontRows.clear();
    for (const std::wstring& f : fonts::All()) {
        if (!needle.empty()) {
            std::wstring lower = f;
            for (size_t i = 0; i < lower.size(); ++i) lower[i] = (wchar_t)towlower(lower[i]);
            if (lower.find(needle) == std::wstring::npos) continue;
        }
        fontRows.push_back(f);
    }
    fontTop = 0;
    if (fontSel >= (int)fontRows.size()) fontSel = (int)fontRows.size() - 1;
    if (fontSel < 0) fontSel = 0;
}

void Settings::OpenFontDialog(bool forClock) {
    fontOpen = true;
    fontForClock = forClock;
    fontFilter.clear();
    fontSel = 0;
    RebuildFontRows();
    std::wstring current = forClock ? app::cfg.ClockFont : app::cfg.FontFamily;
    for (size_t i = 0; i < fontRows.size(); ++i) {
        if (fontRows[i] == current) { fontSel = (int)i; break; }
    }
    focus = ID_FONT_FILTER;
    caretOn = true;
    Invalidate();
}

void Settings::CloseFontDialog() {
    fontOpen = false;
    focus = 0;
    Invalidate();
}

void Settings::CommitFont(const std::wstring& family) {
    bool clock = fontForClock;
    if (clock) app::cfg.ClockFont = family;
    else app::cfg.FontFamily = family;
    MakeFormats();
    previewLoaded = false;
    QueueSave();
    CloseFontDialog();
    SetStatus(clock ? L"时钟字体已换，下次锁屏生效" : L"字体已换，界面和锁屏都用它", false);
}

void Settings::DrawFontDialog(float w, float h) {
    gfx::FillRect(dc, D2D1::RectF(0, 0, w, h), Color(0xB4000000));
    float cw = 440, ch = 500;
    Rect card{ (w - cw) / 2, (h - ch) / 2, (w + cw) / 2, (h + ch) / 2 };
    gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(card.l, card.t, card.r, card.b), 12, 12), Color(kSurface));
    gfx::StrokeRound(dc, D2D1::RoundedRect(D2D1::RectF(card.l, card.t, card.r, card.b), 12, 12), Color(kLine), 1.0f);
    gfx::Text(dc, fTitle, fontForClock ? L"选择时钟字体" : L"选择界面字体",
              D2D1::RectF(card.l + 20, card.t + 16, card.r - 20, card.t + 46), Color(kText));

    Rect filter{ card.l + 20, card.t + 52, card.r - 20, card.t + 52 + ControlH };
    DrawField(filter, fontFilter, ID_FONT_FILTER, false, true, L"输入名字筛选");

    float rowH = 32;
    Rect list{ card.l + 12, filter.b + 12, card.r - 12, card.b - 62 };
    int visible = (int)(list.H() / rowH);
    if (visible < 1) visible = 1;
    int total = (int)fontRows.size();
    if (fontSel < 0) fontSel = 0;
    if (fontSel >= total) fontSel = total - 1;
    if (fontSel < fontTop) fontTop = fontSel;
    if (fontSel >= fontTop + visible) fontTop = fontSel - visible + 1;
    if (fontTop > total - visible) fontTop = (std::max)(0, total - visible);

    dc->PushAxisAlignedClip(D2D1::RectF(list.l, list.t, list.r, list.b), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (int i = 0; i < visible; ++i) {
        int idx = fontTop + i;
        if (idx >= total) break;
        Rect row{ list.l, list.t + i * rowH, list.r, list.t + (i + 1) * rowH };
        if (idx == fontSel) {
            gfx::FillRound(dc, Round(row, 6), Color(kAccentSoft));
        } else if (hot == ID_FONT_ROW + idx) {
            gfx::FillRound(dc, Round(row, 6), Color(kChipSurface));
        }
        IDWriteTextFormat* fmt = RowFormat(fontRows[idx]);
        gfx::Text(dc, fmt ? fmt : fControl, fontRows[idx], D2D1::RectF(row.l + 12, row.t, row.r - 14, row.b),
                  Color(kText));
        Widget wg;
        wg.id = ID_FONT_ROW + idx;
        wg.kind = K_BTN;
        wg.rect = row;
        widgets.push_back(wg);
    }
    dc->PopAxisAlignedClip();

    if (total > visible) {
        float trackH = list.H();
        float thumbH = (std::max)(26.0f, trackH * visible / (float)total);
        float span = (float)(std::max)(1, total - visible);
        float pos = list.t + (trackH - thumbH) * (fontTop / (float)span);
        gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(list.r + 2, pos, list.r + 6, pos + thumbH), 2, 2),
                       Color(kChipBorder));
    }
    if (total == 0) {
        gfx::Text(dc, fSmall, L"没有匹配的字体", D2D1::RectF(list.l + 12, list.t + 8, list.r, list.t + 30),
                  Color(kTextMuted));
    }

    float by = card.b - 46;
    Rect use{ card.r - 20 - 88, by, card.r - 20, by + ControlH };
    Rect def{ use.l - 8 - 96, by, use.l - 8, by + ControlH };
    Rect cancel{ def.l - 8 - 76, by, def.l - 8, by + ControlH };
    Button(cancel, L"取消", false, hot == ID_FONT_CANCEL);
    Button(def, L"用默认", false, hot == ID_FONT_DEFAULT);
    Button(use, L"使用", true, hot == ID_FONT_USE);
    Widget a; a.id = ID_FONT_CANCEL; a.kind = K_BTN; a.rect = cancel; widgets.push_back(a);
    Widget b; b.id = ID_FONT_DEFAULT; b.kind = K_BTN; b.rect = def; widgets.push_back(b);
    Widget c; c.id = ID_FONT_USE; c.kind = K_BTN; c.rect = use; widgets.push_back(c);
}

void Settings::LayoutLook(float left, float right, float top, float bottom) {
    Rect box{ left, top, right, top + PreviewH };
    gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(box.l, box.t, box.r, box.b), 10, 10), Color(kSurfaceNav));
    DrawPreview(box);
    float y = box.b + 8 - scroll;

    dc->PushAxisAlignedClip(D2D1::RectF(left, box.b + 8, right, bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // 锁屏主题：标签一行，下面四颗药丸。
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"锁屏主题", D2D1::RectF(left, y + 10, right, y + 32), Color(kText));
    float px = left, py = y + 38;
    const std::vector<LockTheme>& all = themes::All();
    int current = ThemeIndex();
    for (size_t i = 0; i < all.size(); ++i) {
        Rect pill{ px, py, px + 118, py + ChipH };
        bool on = (int)i == current;
        Pill(pill, all[i].Name.empty() ? all[i].Id : all[i].Name, on, hot == ID_PILL_BASE + (int)i);
        Widget wg;
        wg.id = ID_PILL_BASE + (int)i;
        wg.kind = K_PILL;
        wg.rect = pill;
        wg.on = on;
        widgets.push_back(wg);
        px += 118 + 8;
    }
    {
        Rect pill{ px, py, px + 118, py + ChipH };
        Pill(pill, L"主题文件夹", false, hot == ID_THEMES_FOLDER);
        Widget wg;
        wg.id = ID_THEMES_FOLDER;
        wg.kind = K_PILL;
        wg.rect = pill;
        widgets.push_back(wg);
    }
    y += 38 + ChipH + 12;

    // 背景图片
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"背景图片", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    {
        Rect clear{ right - 88, y + 12, right, y + 12 + ControlH };
        Rect pick{ clear.l - 8 - 240, y + 12, clear.l - 8, y + 12 + ControlH };
        std::wstring name = app::cfg.BackgroundImage.empty() ? L"选一张本地图片" : LeafName(app::cfg.BackgroundImage);
        Button(pick, name, false, hot == ID_BG_PICK);
        Button(clear, L"恢复纯色", false, hot == ID_BG_CLEAR);
        Widget a; a.id = ID_BG_PICK; a.kind = K_BTN; a.rect = pick; widgets.push_back(a);
        Widget b; b.id = ID_BG_CLEAR; b.kind = K_BTN; b.rect = clear; widgets.push_back(b);
    }
    y += RowH;

    auto toggleRow = [&](float yy, const wchar_t* label, bool value, int id) {
        RowLine(yy, left, right);
        gfx::Text(dc, fBody, label, D2D1::RectF(left, yy, left + 320, yy + RowH), Color(kText));
        Rect r{ right - 40, yy + 12, right, yy + 12 + ControlH };
        Toggle(r, value);
        Widget wg; wg.id = id; wg.kind = K_TOGGLE; wg.rect = r; wg.on = value; widgets.push_back(wg);
    };
    toggleRow(y, L"显示时钟和日期", app::cfg.ShowClock, ID_SHOW_CLOCK);
    y += RowH;
    toggleRow(y, L"用屏幕截图做模糊背景", app::cfg.BackgroundSnapshot, ID_SNAPSHOT);
    y += RowH;

    const LockTheme& look = themes::Find(app::cfg.ThemeId);
    auto sliderRow = [&](float yy, const wchar_t* label, double value, double minv, double maxv,
                         const wchar_t* unit, int id) {
        RowLine(yy, left, right);
        gfx::Text(dc, fBody, label, D2D1::RectF(left, yy, left + 220, yy + RowH), Color(kText));
        Rect track{ right - 210 - 56, yy + 12, right - 56, yy + 12 + ControlH };
        Slider(track, value, minv, maxv);
        wchar_t text[32] = {};
        _snwprintf_s(text, _countof(text), _TRUNCATE, L"%.0f%s", value, unit);
        gfx::Text(dc, fSmall, text, D2D1::RectF(right - 50, yy, right, yy + RowH), Color(kTextDim));
        Widget wg; wg.id = id; wg.kind = K_SLIDER; wg.rect = track; wg.value = value; wg.minv = minv; wg.maxv = maxv;
        widgets.push_back(wg);
    };
    double radius = app::cfg.CornerRadius >= 0 ? app::cfg.CornerRadius : look.Card.Radius;
    double blur = app::cfg.BlurRadius >= 0 ? app::cfg.BlurRadius : look.Background.Blur;
    double mask = app::cfg.MaskOpacity >= 0 ? app::cfg.MaskOpacity : look.Background.Dim * 100.0 / 255.0;
    sliderRow(y, L"圆角", radius, 0, 32, L" px", ID_SLIDER_RADIUS); y += RowH;
    sliderRow(y, L"模糊度", blur, 0, 60, L" px", ID_SLIDER_BLUR); y += RowH;
    sliderRow(y, L"遮罩透明度", mask, 0, 100, L" %", ID_SLIDER_MASK); y += RowH;

    // 提示文字：开关 + 输入框
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"提示文字", D2D1::RectF(left, y, left + 160, y + RowH), Color(kText));
    {
        Rect toggleR{ right - 40, y + 12, right, y + 12 + ControlH };
        Toggle(toggleR, app::cfg.ShowSubtitle);
        Widget wg; wg.id = ID_SHOW_SUBTITLE; wg.kind = K_TOGGLE; wg.rect = toggleR; wg.on = app::cfg.ShowSubtitle;
        widgets.push_back(wg);
        subtitleRect = Rect{ toggleR.l - 8 - 190, y + 12, toggleR.l - 8, y + 12 + ControlH };
        DrawField(subtitleRect, subtitleText, ID_EDIT_SUBTITLE, false, app::cfg.ShowSubtitle, L"输入密码解锁");
    }
    y += RowH;

    // 界面字体
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"界面字体", D2D1::RectF(left, y, left + 220, y + RowH), Color(kText));
    {
        Rect pick{ right - 240, y + 12, right, y + 12 + ControlH };
        Button(pick, fonts::Label(app::cfg.FontFamily), false, hot == ID_FONT_UI);
        Widget wg; wg.id = ID_FONT_UI; wg.kind = K_BTN; wg.rect = pick; widgets.push_back(wg);
    }
    y += RowH;

    // 时钟字体：开关关掉时右边的按钮跟着变灰。
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"时钟字体", D2D1::RectF(left, y, left + 220, y + RowH), Color(kText));
    {
        bool on = app::cfg.ClockFontEnabled;
        Rect toggleR{ right - 40, y + 12, right, y + 12 + ControlH };
        Toggle(toggleR, on);
        Widget t; t.id = ID_FONT_CLOCK_ON; t.kind = K_TOGGLE; t.rect = toggleR; t.on = on; widgets.push_back(t);
        Rect pick{ toggleR.l - 8 - 190, y + 12, toggleR.l - 8, y + 12 + ControlH };
        Button(pick, fonts::Label(app::cfg.ClockFont), false, on && hot == ID_FONT_CLOCK, on);
        if (on) { Widget wg; wg.id = ID_FONT_CLOCK; wg.kind = K_BTN; wg.rect = pick; widgets.push_back(wg); }
    }
    y += RowH;

    float total = (y + scroll) - (box.b + 8);
    float visible = bottom - (box.b + 8);
    scrollMax = (std::max)(0.0f, total - visible);

    dc->PopAxisAlignedClip();
}

void Settings::LayoutUnlock(float left, float right, float top, float bottom) {
    float y = top - scroll;
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"锁屏密码", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    {
        Rect apply{ right - 76, y + 12, right, y + 12 + ControlH };
        Rect eye{ apply.l - 8 - 34, y + 12, apply.l - 8, y + 12 + ControlH };
        passwordRect = Rect{ eye.l - 8 - 260, y + 12, eye.l - 8, y + 12 + ControlH };
        DrawField(passwordRect, passwordText, ID_EDIT_PASSWORD, !reveal, true, L"输入新密码");
        gfx::FillRound(dc, Round(eye, 8), Color(hot == ID_REVEAL ? kChipSurface : 0x14FFFFFF));
        gfx::Text(dc, fSmall, reveal ? L"隐藏" : L"显示", D2D1::RectF(eye.l, eye.t, eye.r, eye.b), Color(kTextDim));
        Button(apply, L"设置", true, hot == ID_APPLY_PASSWORD);
        Widget a; a.id = ID_REVEAL; a.kind = K_BTN; a.rect = eye; widgets.push_back(a);
        Widget b; b.id = ID_APPLY_PASSWORD; b.kind = K_BTN; b.rect = apply; widgets.push_back(b);
    }
    y += RowH;
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"恢复码", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    {
        Rect make{ right - 132, y + 12, right, y + 12 + ControlH };
        std::wstring note = !shownCode.empty() ? (shownCode + L"（已复制，用一次就失效）")
                           : app::HasRecovery() ? L"已设置，用一次就失效"
                                                : L"还没设置";
        gfx::Text(dc, fSmall, note, D2D1::RectF(left + 200, y, make.l - 16, y + RowH),
                  Color(shownCode.empty() ? kTextMuted : kText));
        Button(make, L"生成新的恢复码", false, hot == ID_RECOVERY);
        Widget r; r.id = ID_RECOVERY; r.kind = K_BTN; r.rect = make; widgets.push_back(r);
    }
    y += RowH;
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"Windows 账户", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    gfx::Text(dc, fSmall, L"QLock 不碰系统账户，只盖一层自己的锁屏", D2D1::RectF(left + 200, y, right, y + RowH), Color(kTextMuted));
    y += RowH + 8;
    gfx::Text(dc, fSmall, L"改完点「设置」，密码按 PBKDF2-SHA256 存，另存一份 DPAPI 加密的明文只给这里回显；恢复码同样只存 hash，用过一次就清掉。",
              D2D1::RectF(left, y, right, y + 40), Color(kTextMuted));
}

void Settings::LayoutStart(float left, float right, float top, float bottom) {
    float y = top - scroll;
    auto toggleRow = [&](float yy, const wchar_t* label, bool value, int id) {
        RowLine(yy, left, right);
        gfx::Text(dc, fBody, label, D2D1::RectF(left, yy, left + 320, yy + RowH), Color(kText));
        Rect r{ right - 40, yy + 12, right, yy + 12 + ControlH };
        Toggle(r, value);
        Widget wg; wg.id = id; wg.kind = K_TOGGLE; wg.rect = r; wg.on = value; widgets.push_back(wg);
    };
    toggleRow(y, L"接管 Win+L", app::cfg.InterceptWinL, ID_INTERCEPT_WINL);
    y += RowH;
    toggleRow(y, L"Ctrl+Alt+L 热键", app::cfg.CustomHotkeyEnabled, ID_HOTKEY);
    y += RowH;
    toggleRow(y, L"程序启动后立即锁定", app::cfg.LockOnStartup, ID_LOCK_ON_START);
    y += RowH;

    // 空闲自动锁定：分钟数自己画一个输入框，0 表示不锁。
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"空闲自动锁定", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    {
        Rect field{ left + 200, y + 12, left + 270, y + 12 + ControlH };
        DrawField(field, idleText, ID_EDIT_IDLE, false, true, L"0");
        gfx::Text(dc, fSmall, L"分钟（0 = 不锁）", D2D1::RectF(field.r + 12, y, right - 40, y + RowH),
                  Color(kTextDim));
    }
    y += RowH;

    bool autoOn = app::cfg.Autostart == L"runkey" || app::cfg.Autostart == L"task";
    toggleRow(y, L"登录时自动启动", autoOn, ID_AUTOSTART);
    y += RowH;
    toggleRow(y, L"以管理员身份启动", app::cfg.RunElevated, ID_ELEVATED);
    y += RowH;

    scrollMax = (std::max)(0.0f, (y + scroll) - bottom);
}

void Settings::LayoutHard(float left, float right, float top, float bottom) {
    float y = top - scroll;
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"锁定时拦截系统热键", D2D1::RectF(left, y, left + 320, y + RowH), Color(kText));
    {
        Rect r{ right - 40, y + 12, right, y + 12 + ControlH };
        Toggle(r, app::cfg.BlockSystemHotkeys);
        Widget wg;
        wg.id = ID_BLOCK_HOTKEYS;
        wg.kind = K_TOGGLE;
        wg.rect = r;
        wg.on = app::cfg.BlockSystemHotkeys;
        widgets.push_back(wg);
    }
    y += RowH + 8;

    const wchar_t* notes[3] = {
        L"打开后，锁屏期间 Win、Win+Tab、Alt+Tab、Ctrl+Esc、Alt+F4 都不起作用。",
        L"关掉就交给系统：切窗口、调音量这些照样能用，但别人可能绕开锁屏。",
        L"接管 Win+L 和 Ctrl+Alt+L 在「启动与锁定」页。",
    };
    for (int i = 0; i < 3; ++i) {
        gfx::Text(dc, fSmall, notes[i], D2D1::RectF(left, y, right, y + 26), Color(kTextMuted));
        y += 26;
    }
    scrollMax = (std::max)(0.0f, (y + scroll) - bottom);
}

void Settings::LayoutAbout(float left, float right, float top, float bottom) {
    float y = top - scroll;
    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"版本", D2D1::RectF(left, y, left + 200, y + RowH), Color(kText));
    {
        bool busy = updateState == U_CHECKING || updateState == U_DOWNLOADING || updateState == U_INSTALLING ||
                    updateState == U_READY;
        bool skip = updateState == U_NEWER;
        float checkLeft = right - 100 - (skip ? 104 : 0);
        // 两个按钮并排时，标签栏留 200 就放不下版本号了，这一行改窄一点。
        float valueLeft = skip ? left + 100 : left + 200;
        float valueRight = (skip ? checkLeft - 104 : checkLeft) - 8;
        gfx::Text(dc, fBody, updater::CurrentVersion(),
                  D2D1::RectF(valueLeft, y, valueRight, y + RowH), Color(kTextDim));

        const wchar_t* label = L"检查更新";
        if (updateState == U_CHECKING) label = L"检查中…";
        else if (updateState == U_NEWER) label = L"下载并升级";
        else if (updateState == U_DOWNLOADING) label = L"下载中…";
        else if (updateState == U_INSTALLING || updateState == U_READY) label = L"正在安装…";

        Rect check{ checkLeft, y + 12, checkLeft + 100, y + 12 + ControlH };
        Button(check, label, updateState == U_NEWER, hot == ID_CHECK_UPDATE, !busy);
        if (!busy) {
            Widget wg;
            wg.id = ID_CHECK_UPDATE;
            wg.kind = K_BTN;
            wg.rect = check;
            widgets.push_back(wg);
        }
        if (skip) {
            Rect skipRect{ checkLeft - 104, y + 12, checkLeft - 8, y + 12 + ControlH };
            Button(skipRect, L"跳过这个版本", false, hot == ID_SKIP_UPDATE, true);
            Widget wg;
            wg.id = ID_SKIP_UPDATE;
            wg.kind = K_BTN;
            wg.rect = skipRect;
            widgets.push_back(wg);
        }
    }
    y += RowH - 8;
    gfx::Text(dc, fSmall, L"C++ 重写，Direct2D 直绘", D2D1::RectF(left, y, right, y + 22), Color(kTextMuted));
    y += 26;

    RowLine(y, left, right);
    gfx::Text(dc, fBody, L"文件位置", D2D1::RectF(left, y, left + 200, y + 30), Color(kText));
    y += 34;
    const std::wstring paths[3] = { L"配置：" + app::ConfigPath(), L"日志：" + app::LogPath(),
                                    L"主题：" + app::ThemeDir() };
    for (int i = 0; i < 3; ++i) {
        gfx::Text(dc, fSmall, paths[i], D2D1::RectF(left, y, right, y + 22), Color(kTextMuted));
        y += 22;
    }
    y += 12;
    gfx::Text(dc, fSmall, L"MIT 协议 · https://github.com/haha0614/QLock",
              D2D1::RectF(left, y, right, y + 22), Color(kTextDim));
    y += 22;
    gfx::Text(dc, fSmall, L"0.3.0 的 WPF 实现留在 wpf 分支和 v0.3.0 tag",
              D2D1::RectF(left, y, right, y + 22), Color(kTextMuted));
    y += 22;
    scrollMax = (std::max)(0.0f, (y + scroll) - bottom);
}

void Settings::CommitIdle() {
    int minutes = idleText.empty() ? -1 : _wtoi(idleText.c_str());
    if (minutes < 0 || minutes > 600) {
        SetStatus(L"空闲分钟数要填 0 到 600 之间的整数", true);
        idleText = std::to_wstring(app::cfg.IdleLockMinutes);
        Invalidate();
        return;
    }
    if (minutes == app::cfg.IdleLockMinutes) return;
    app::cfg.IdleLockMinutes = minutes;
    idleText = std::to_wstring(minutes);
    FlushSave();
}

// 检查更新走后台线程，主线程只收结果；发现新版就换成「下载并升级」。
void Settings::CheckUpdate() {
    if (updateState == U_CHECKING || updateState == U_DOWNLOADING) return;
    updateState = U_CHECKING;
    SetStatus(L"正在检查更新…", false);
    HWND target = hwnd;
    std::thread([target]() {
        std::wstring tag, page, error;
        std::wstring status;
        bool newer = false;
        if (!updater::Check(&tag, &page, &error)) {
            status = L"检查更新失败：" + error;
        } else if (updater::IsNewer(tag, updater::CurrentVersion())) {
            newer = true;
            status = tag == app::cfg.SkippedVersion ? L"新版本 " + tag + L" 你之前跳过过，可以在这里直接装"
                                                    : L"发现新版本 " + tag + L"，点「下载并升级」";
            g_pendingTag = tag;
        } else {
            status = L"已经是最新版本 " + updater::CurrentVersion();
        }
        std::wstring* text = new std::wstring(status);
        if (!PostMessageW(target, WM_APP + 5, (WPARAM)text, newer ? 1 : 0)) delete text;
    }).detach();
}

// 下载 → 校验 SHA256 → 原地替换，都在后台线程；进度用 WM_APP + 6 回报百分比。
void Settings::StartDownload() {
    if (updateState != U_NEWER) return;
    updateState = U_DOWNLOADING;
    updatePercent = 0;
    SetStatus(L"正在下载… 0%", false);
    Invalidate();
    HWND target = hwnd;
    std::thread([target]() {
        std::wstring error;
        std::wstring target_file = updater::DownloadPath();
        std::wstring text;
        const std::wstring exeUrl = updater::ExeUrl();
        const std::wstring sumsUrl = updater::SumsUrl();
        app::Log(L"update: downloading %s", exeUrl.c_str());
        int lastPercent = -1;
        auto progress = [target, &lastPercent](long long got, long long total) {
            if (total <= 0) return;
            int percent = (int)(got * 100 / total);
            if (percent == lastPercent) return;
            lastPercent = percent;
            PostMessageW(target, WM_APP + 6, (WPARAM)percent, 0);
        };
        bool installed = false;
        if (!updater::Download(exeUrl, target_file, progress, &error)) {
            text = L"下载失败：" + error;
        } else {
            PostMessageW(target, WM_APP + 6, (WPARAM)100, 0);
            std::wstring sumsError;
            std::wstring sums = updater::FetchText(sumsUrl, &sumsError);
            std::wstring expected = updater::ExpectedHash(sums);
            std::wstring actual = updater::Sha256File(target_file);
            if (expected.empty()) {
                text = L"下载完成，但没拿到校验值，已放弃这次升级";
            } else if (actual != expected) {
                text = L"下载的文件和发布页的校验值不一致，已放弃这次升级";
            } else if (!updater::Install(target_file, &error)) {
                text = L"升级失败：" + error;
            } else {
                installed = true;
                text = L"已经装好，正在重启 QLock…";
            }
        }
        std::wstring* message = new std::wstring(text);
        if (!PostMessageW(target, WM_APP + 7, (WPARAM)message, installed ? 1 : 0)) delete message;
    }).detach();
}

void Settings::Paint() {
    if (!EnsureTarget()) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    float w = (rc.right - rc.left) / scale;
    float h = (rc.bottom - rc.top) / scale;
    if (w <= 1 || h <= 1) return;

    widgets.clear();

    dc->BeginDraw();
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    gfx::FillRect(dc, D2D1::RectF(0, 0, w, h), Color(kSurface));

    PaintNav(h);

    float left = NavWidth + PanePad, right = w - PanePad;
    gfx::Text(dc, fTitle, kNavNames[pane], D2D1::RectF(left, 18, right, 50), Color(kText));
    float top = 62, bottom = h - FooterH;

    if (pane == 1) {
        LayoutLook(left, right, top, bottom);
    } else if (pane == 0) {
        LayoutUnlock(left, right, top, bottom);
    } else if (pane == 2) {
        LayoutStart(left, right, top, bottom);
    } else if (pane == 3) {
        LayoutHard(left, right, top, bottom);
    } else {
        LayoutAbout(left, right, top, bottom);
    }

    PaintFooter(w, h, left, right);

    if (fontOpen) {
        widgets.clear();
        DrawFontDialog(w, h);
    }

    HRESULT hr = dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        preview.DropDeviceResources();
        previewLoaded = false;
        if (target) { target->Release(); target = nullptr; }
        return;
    }
    swap->Present(1, 0);
}

// ---- 输入框：自己画，不用子窗口（flip 交换链上子窗口会被盖住）----
void Settings::DrawField(const Rect& r, const std::wstring& text, int id, bool password, bool enabled,
                         const std::wstring& hint) {
    bool focused = focus == id;
    gfx::FillRound(dc, Round(r, 8), Color(kInputBg));
    gfx::StrokeRound(dc, Round(r, 8), Color(focused ? kAccent : (enabled ? kInputBorder : kLine)), 1.0f);

    std::wstring shown = password ? std::wstring(text.size(), L'\u2022') : text;
    D2D1_RECT_F box = D2D1::RectF(r.l + 10, r.t, r.r - 12, r.b);
    if (shown.empty()) {
        gfx::Text(dc, fControl, hint, box, Color(kTextMuted));
    } else {
        gfx::Text(dc, fControl, shown, box, Color(enabled ? kText : kTextMuted));
    }
    if (focused && caretOn) {
        float cx = r.l + 10 + gfx::Measure(fControl, shown) + 1;
        gfx::FillRect(dc, D2D1::RectF(cx, r.t + 8, cx + 1.5f, r.b - 8), Color(kText));
    }

    Widget wg;
    wg.id = id;
    wg.kind = K_EDIT;
    wg.rect = r;
    wg.on = enabled;
    widgets.push_back(wg);
}

void Settings::ApplySlider(int id, float x) {
    for (const Widget& wg : widgets) {
        if (wg.id != id) continue;
        float t = (x - (wg.rect.l + 7)) / (std::max)(1.0f, wg.rect.W() - 14);
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        double value = wg.minv + t * (wg.maxv - wg.minv);
        int rounded = (int)(value + 0.5);
        if (id == ID_SLIDER_RADIUS) app::cfg.CornerRadius = rounded;
        else if (id == ID_SLIDER_BLUR) { app::cfg.BlurRadius = rounded; }
        else if (id == ID_SLIDER_MASK) app::cfg.MaskOpacity = rounded;
        QueueSave();
        Invalidate();
        return;
    }
}

void Settings::Apply(int id) {
    if (id == ID_FONT_UI) { OpenFontDialog(false); return; }
    if (id == ID_FONT_CLOCK) { OpenFontDialog(true); return; }
    if (id == ID_FONT_CLOCK_ON) {
        app::cfg.ClockFontEnabled = !app::cfg.ClockFontEnabled;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_FONT_CANCEL) { CloseFontDialog(); return; }
    if (id == ID_FONT_DEFAULT) { CommitFont(std::wstring()); return; }
    if (id == ID_FONT_USE) {
        if (fontSel >= 0 && fontSel < (int)fontRows.size()) CommitFont(fontRows[fontSel]);
        return;
    }
    if (id >= ID_FONT_ROW && id < ID_FONT_ROW + (int)fontRows.size()) {
        fontSel = id - ID_FONT_ROW;
        CommitFont(fontRows[fontSel]);
        return;
    }
    if (id >= ID_NAV_BASE && id < ID_NAV_BASE + 5) {
        pane = id - ID_NAV_BASE;
        scroll = 0;
        scrollMax = 0;
        Invalidate();
        return;
    }
    if (id >= ID_PILL_BASE && id < ID_PILL_BASE + (int)themes::All().size()) {
        const LockTheme& t = themes::All()[id - ID_PILL_BASE];
        app::cfg.ThemeId = t.Id;
        previewLoaded = false;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_THEMES_FOLDER) {
        ShellExecuteW(nullptr, L"open", themes::Dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (id == ID_BG_PICK) {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"图片\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0所有文件\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        ofn.lpstrTitle = L"选择锁屏背景图片";
        if (GetOpenFileNameW(&ofn)) {
            app::cfg.BackgroundImage = path;
            previewLoaded = false;
            QueueSave();
            Invalidate();
        }
        return;
    }
    if (id == ID_BG_CLEAR) {
        app::cfg.BackgroundImage.clear();
        previewLoaded = false;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_SHOW_CLOCK) { app::cfg.ShowClock = !app::cfg.ShowClock; QueueSave(); Invalidate(); return; }
    if (id == ID_SNAPSHOT) { app::cfg.BackgroundSnapshot = !app::cfg.BackgroundSnapshot; QueueSave(); Invalidate(); return; }
    if (id == ID_SHOW_SUBTITLE) {
        app::cfg.ShowSubtitle = !app::cfg.ShowSubtitle;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_REVEAL) {
        reveal = !reveal;
        Invalidate();
        return;
    }
    if (id == ID_RECOVERY) {
        std::wstring code = app::NewRecoveryCode();
        if (code.empty()) { SetStatus(L"生成恢复码失败", true); return; }
        app::SetRecovery(code);
        shownCode = code;
        CopyText(code);
        SetStatus(L"恢复码已生成并复制到剪贴板，记下来——用一次就换", false);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (id == ID_APPLY_PASSWORD) {
        if (passwordText.size() < 4) { SetStatus(L"密码至少 4 位", true); return; }
        if (passwordText == app::StoredPassword()) { SetStatus(L"密码没变", false); return; }
        app::SetPassword(passwordText);
        SetStatus(L"密码已更新", false);
        Invalidate();
        return;
    }
    if (id == ID_INTERCEPT_WINL) {
        app::cfg.InterceptWinL = !app::cfg.InterceptWinL;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_HOTKEY) {
        app::cfg.CustomHotkeyEnabled = !app::cfg.CustomHotkeyEnabled;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_LOCK_ON_START) {
        app::cfg.LockOnStartup = !app::cfg.LockOnStartup;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_AUTOSTART) {
        bool on = !(app::cfg.Autostart == L"runkey" || app::cfg.Autostart == L"task");
        std::wstring error;
        if (!autostart::Apply(on, &error)) {
            SetStatus(L"启动项没写成功：" + (error.empty() ? L"未知原因" : error), true);
            return;
        }
        app::cfg.Autostart = on ? L"runkey" : L"none";
        app::SaveConfig();
        SetStatus(on ? L"已加到当前用户的启动项，登录后自动运行" : L"已从启动项里删掉", false);
        Invalidate();
        return;
    }
    if (id == ID_ELEVATED) {
        app::cfg.RunElevated = !app::cfg.RunElevated;
        QueueSave();
        SetStatus(L"已保存，下次启动时申请管理员权限", false);
        Invalidate();
        return;
    }
    if (id == ID_BLOCK_HOTKEYS) {
        app::cfg.BlockSystemHotkeys = !app::cfg.BlockSystemHotkeys;
        QueueSave();
        Invalidate();
        return;
    }
    if (id == ID_CHECK_UPDATE) {
        if (updateState == U_NEWER) StartDownload();
        else CheckUpdate();
        return;
    }
    if (id == ID_SKIP_UPDATE) {
        if (!newTag.empty()) {
            app::cfg.SkippedVersion = newTag;
            app::SaveConfig();
            SetStatus(L"已跳过 " + newTag + L"，下次发新版本还会提醒", false);
        }
        updateState = U_IDLE;
        Invalidate();
        return;
    }
    if (id == ID_CLOSE) {
        DestroyWindow(hwnd);
        return;
    }
}

std::wstring LeafName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

void Settings::ClickAt(float x, float y, bool down) {
    for (const Widget& wg : widgets) {
        if (!wg.rect.Has(x, y)) continue;
        if (down) {
            if (wg.kind == K_SLIDER) {
                dragSlider = wg.id;
                SetCapture(hwnd);
                ApplySlider(wg.id, x);
            } else if (wg.kind == K_EDIT) {
                if (wg.on) { focus = wg.id; caretOn = true; Invalidate(); }
            } else {
                pressed = wg.id;
                SetCapture(hwnd);
                Invalidate();
            }
        }
        return;
    }
    if (down && focus) {
        if (focus == ID_EDIT_IDLE) CommitIdle();
        focus = 0;
        Invalidate();
    }
}

// ---- 窗口 ----
bool Settings::Create(HWND ownerWnd) {
    owner = ownerWnd;
    if (!gfx::Init()) {
        app::Log(L"settings: gfx init failed");
        return false;
    }
    scale = 1.0f;
    {
        typedef UINT(WINAPI* GetDpiForSystemFn)();
        GetDpiForSystemFn fn = (GetDpiForSystemFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
        if (fn) { UINT dpi = fn(); if (dpi) scale = dpi / 96.0f; }
    }

    if (!GetClassInfoW(app::inst, kClass, nullptr)) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = Settings::Proc;
        wc.hInstance = app::inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        wc.hbrBackground = nullptr;
        RegisterClassExW(&wc);
    }

    RECT frame = { 0, 0, (LONG)(WinW * scale), (LONG)(WinH * scale) };
    AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    int ww = frame.right - frame.left, wh = frame.bottom - frame.top;
    int sx = 0, sy = 0;
    if (ownerWnd && IsWindow(ownerWnd)) {
        RECT o;
        GetWindowRect(ownerWnd, &o);
        sx = o.left + ((o.right - o.left) - ww) / 2;
        sy = o.top + ((o.bottom - o.top) - wh) / 2;
    } else {
        RECT work;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        sx = work.left + ((work.right - work.left) - ww) / 2;
        sy = work.top + ((work.bottom - work.top) - wh) / 2;
    }

    hwnd = CreateWindowExW(0, kClass, L"QLock 设置", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                           sx, sy, ww, wh, nullptr, nullptr, app::inst, this);
    if (!hwnd) return false;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    MakeFormats();
    subtitleText = app::cfg.Subtitle;
    passwordText = app::StoredPassword();
    idleText = std::to_wstring(app::cfg.IdleLockMinutes);
    // 启动项以注册表为准：有人手动删过，这里的开关就跟着变。
    app::cfg.Autostart = autostart::Enabled() ? L"runkey" : L"none";

    if (!EnsureTarget()) {
        app::Log(L"settings: target failed");
        return false;
    }
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    SetTimer(hwnd, 4, 500, nullptr);
    return true;
}

void Settings::DestroyAll() {
    if (saveTimer) { KillTimer(hwnd, 3); saveTimer = 0; }
    app::SaveConfig();
    if (hwnd) KillTimer(hwnd, 4);
    preview.DropDeviceResources();
    ReleaseFmt(&fBrand);
    ReleaseFmt(&fTitle);
    ReleaseFmt(&fBody);
    ReleaseFmt(&fSmall);
    ReleaseFmt(&fControl);
    for (auto& kv : rowFormats) if (kv.second) kv.second->Release();
    rowFormats.clear();
    if (dc) dc->SetTarget(nullptr);
    if (target) { target->Release(); target = nullptr; }
    if (swap) { swap->Release(); swap = nullptr; }
    if (dc) { dc->Release(); dc = nullptr; }
}

Settings* g_settings = nullptr;

LRESULT CALLBACK Settings::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Settings* self = (Settings*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (Settings*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    // 后台线程的检查更新结果：wp=状态文字，lp=下载页（可能为空）。
    if (msg == WM_APP + 5) {
        std::wstring* text = (std::wstring*)wp;
        bool newer = lp != 0;
        if (text) {
            self->SetStatus(*text, !newer && text->find(L"失败") != std::wstring::npos);
            delete text;
        }
        self->updateState = newer ? U_NEWER : U_IDLE;
        if (newer) self->newTag = g_pendingTag;
        self->Invalidate();
        return 0;
    }

    // 后台线程报下载进度：wp 是百分比。
    if (msg == WM_APP + 6) {
        self->updatePercent = (int)wp;
        if (self->updateState == U_DOWNLOADING) {
            self->SetStatus(L"正在下载… " + std::to_wstring(self->updatePercent) + L"%", false);
        }
        return 0;
    }

    // 后台线程收尾：wp 是提示文字，lp 非零表示已经替换成功。
    if (msg == WM_APP + 7) {
        std::wstring* text = (std::wstring*)wp;
        bool installed = lp != 0;
        if (text) {
            self->SetStatus(*text, !installed);
            delete text;
        }
        if (installed) {
            self->updateState = U_READY;
            wchar_t self_path[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, self_path, MAX_PATH);
            std::wstring error;
            if (!updater::Restart(self_path, &error)) {
                self->SetStatus(L"已经装好，但" + error, true);
            } else {
                HWND main = FindWindowExW(HWND_MESSAGE, nullptr, L"QLockMain", nullptr);
                if (main) PostMessageW(main, WM_CLOSE, 0, 0);
            }
        } else {
            self->updateState = U_IDLE;
        }
        self->Invalidate();
        return 0;
    }

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        self->Paint();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) self->Resize();
        return 0;
    case WM_MOUSEMOVE: {
        float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
        if (self->dragSlider) {
            self->ApplySlider(self->dragSlider, x);
            return 0;
        }
        int hot = -1;
        for (const Widget& wg : self->widgets) {
            if (wg.rect.Has(x, y)) { hot = wg.id; break; }
        }
        if (hot != self->hot) {
            self->hot = hot;
            self->Invalidate();
        }
        TRACKMOUSEEVENT tme = { sizeof(tme) };
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (self->hot != -1) { self->hot = -1; self->Invalidate(); }
        return 0;
    case WM_LBUTTONDOWN: {
        float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
        SetFocus(hwnd);
        self->ClickAt(x, y, true);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (self->dragSlider) {
            self->dragSlider = 0;
            ReleaseCapture();
            self->FlushSave();
            return 0;
        }
        int pressed = self->pressed;
        self->pressed = -1;
        if (pressed >= 0) {
            ReleaseCapture();
            float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
            for (const Widget& wg : self->widgets) {
                if (wg.id == pressed && wg.rect.Has(x, y)) { self->Apply(pressed); break; }
            }
            self->Invalidate();
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (self->fontOpen) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            self->fontTop -= delta / 120 * 2;
            if (self->fontTop < 0) self->fontTop = 0;
            self->Invalidate();
            return 0;
        }
        if (self->pane != 1 || self->scrollMax <= 0) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        self->scroll -= delta / 120.0f * 60.0f;
        self->scroll = (std::max)(0.0f, (std::min)(self->scrollMax, self->scroll));
        self->Invalidate();
        return 0;
    }
    case WM_TIMER:
        if (wp == 3) self->FlushSave();
        if (wp == 4 && self->focus) { self->caretOn = !self->caretOn; self->Invalidate(); }
        return 0;
    case WM_CHAR: {
        wchar_t ch = (wchar_t)wp;
        if (self->fontOpen && self->focus == ID_FONT_FILTER) {
            if (ch == 8) {
                if (self->fontFilter.empty()) return 0;
                self->fontFilter.pop_back();
            } else if (ch >= 32 && self->fontFilter.size() < 24) {
                self->fontFilter.push_back(ch);
            } else {
                return 0;
            }
            self->fontSel = 0;
            self->RebuildFontRows();
            self->caretOn = true;
            self->Invalidate();
            return 0;
        }
        if (self->focus == ID_EDIT_SUBTITLE) {
            if (!app::cfg.ShowSubtitle) return 0;
            if (ch == 8) {
                if (self->subtitleText.empty()) return 0;
                self->subtitleText.pop_back();
            } else if (ch >= 32 && self->subtitleText.size() < 40) {
                self->subtitleText.push_back(ch);
            } else {
                return 0;
            }
            app::cfg.Subtitle = self->subtitleText;
            self->QueueSave();
        } else if (self->focus == ID_EDIT_IDLE) {
            if (ch == 8) {
                if (self->idleText.empty()) return 0;
                self->idleText.pop_back();
            } else if (ch >= L'0' && ch <= L'9' && self->idleText.size() < 3) {
                self->idleText.push_back(ch);
            } else {
                return 0;
            }
        } else if (self->focus == ID_EDIT_PASSWORD) {
            if (ch == 8) {
                if (self->passwordText.empty()) return 0;
                self->passwordText.pop_back();
            } else if (ch >= 32 && self->passwordText.size() < 64) {
                self->passwordText.push_back(ch);
            } else {
                return 0;
            }
        } else {
            return 0;
        }
        self->caretOn = true;
        self->Invalidate();
        return 0;
    }
    case WM_KEYDOWN:
        if (self->fontOpen) {
            if (wp == VK_ESCAPE) { self->CloseFontDialog(); return 0; }
            if (wp == VK_RETURN) { self->Apply(ID_FONT_USE); return 0; }
            if (wp == VK_UP && self->fontSel > 0) { self->fontSel--; self->Invalidate(); return 0; }
            if (wp == VK_DOWN && self->fontSel + 1 < (int)self->fontRows.size()) {
                self->fontSel++;
                self->Invalidate();
                return 0;
            }
            return 0;
        }
        if (self->focus == ID_EDIT_IDLE && wp == VK_RETURN) { self->CommitIdle(); return 0; }
        if (wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        self->DestroyAll();
        if (g_settings == self) g_settings = nullptr;
        delete self;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

void SettingsShow(HWND owner) {
    if (g_settings && g_settings->hwnd && IsWindow(g_settings->hwnd)) {
        ShowWindow(g_settings->hwnd, SW_SHOW);
        SetForegroundWindow(g_settings->hwnd);
        return;
    }
    Settings* s = new Settings();
    if (!s->Create(owner)) {
        delete s;
        app::Log(L"settings: window create failed");
        return;
    }
    g_settings = s;
    s->Invalidate();
}

bool SettingsOwnsWindow(HWND hwnd) {
    return g_settings && g_settings->hwnd == hwnd;
}
