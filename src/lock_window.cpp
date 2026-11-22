#pragma once
#include "qlock.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {

ID2D1Factory* g_d2d = nullptr;
IDWriteFactory* g_dw = nullptr;
IWICImagingFactory* g_wic = nullptr;
std::vector<class Overlay*> g_windows;
HHOOK g_hook = nullptr;

const float kColumnWidth = 340.0f;
const float kClockSize = 74.0f;
const float kDateSize = 16.0f;
const float kSubtitleSize = 13.0f;
const float kInputHeight = 40.0f;
const float kButtonHeight = 40.0f;
const float kInputRadius = 8.0f;
const float kButtonRadius = 8.0f;
const float kLinkSize = 12.0f;
const float kPad = 16.0f;

D2D1_COLOR_F Color(unsigned argb) {
    D2D1_COLOR_F c;
    c.a = ((argb >> 24) & 0xFF) / 255.0f;
    c.r = ((argb >> 16) & 0xFF) / 255.0f;
    c.g = ((argb >> 8) & 0xFF) / 255.0f;
    c.b = (argb & 0xFF) / 255.0f;
    return c;
}

bool InitFactories() {
    if (!g_d2d && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d))) return false;
    if (!g_dw && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_dw)))) return false;
    if (!g_wic && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wic)))) return false;
    return true;
}

void SafeRelease(IUnknown** p) {
    if (*p) { (*p)->Release(); *p = nullptr; }
}

class Overlay {
public:
    HWND hwnd = nullptr;
    ID2D1HwndRenderTarget* rt = nullptr;
    IDWriteTextFormat* clockFmt = nullptr;
    IDWriteTextFormat* dateFmt = nullptr;
    IDWriteTextFormat* subFmt = nullptr;
    IDWriteTextFormat* editFmt = nullptr;
    IDWriteTextFormat* btnFmt = nullptr;
    IDWriteTextFormat* linkFmt = nullptr;
    ID2D1Bitmap* wallpaper = nullptr;
    std::wstring password;
    std::wstring message;
    bool error = false;
    bool caretOn = true;
    float scale = 1.0f;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool Create(const RECT& rect, const std::wstring& klass) {
        hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, klass.c_str(), L"QLock", WS_POPUP,
                               rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, app::inst, this);
        if (!hwnd) return false;
        UpdateScale();
        CreateFormats();
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        SetTimer(hwnd, 1, 500, nullptr);
        SetTimer(hwnd, 2, 350, nullptr);
        return true;
    }

    void UpdateScale() {
        UINT dpi = 96;
        typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
        static GetDpiForWindowFn fn = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (fn) dpi = fn(hwnd);
        scale = dpi / 96.0f;
    }

    void ReleaseFormats() {
        SafeRelease((IUnknown**)&clockFmt);
        SafeRelease((IUnknown**)&dateFmt);
        SafeRelease((IUnknown**)&subFmt);
        SafeRelease((IUnknown**)&editFmt);
        SafeRelease((IUnknown**)&btnFmt);
        SafeRelease((IUnknown**)&linkFmt);
    }

    IDWriteTextFormat* Format(const std::wstring& family, float size, DWRITE_FONT_WEIGHT weight) {
        IDWriteTextFormat* fmt = nullptr;
        if (FAILED(g_dw->CreateTextFormat(family.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                          size, L"zh-cn", &fmt))) {
            return nullptr;
        }
        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        return fmt;
    }

    void CreateFormats() {
        ReleaseFormats();
        std::wstring ui = app::UiFontName();
        std::wstring clock = app::ClockFontName();
        clockFmt = Format(clock, kClockSize * scale, DWRITE_FONT_WEIGHT_LIGHT);
        dateFmt = Format(ui, kDateSize * scale, DWRITE_FONT_WEIGHT_NORMAL);
        subFmt = Format(ui, kSubtitleSize * scale, DWRITE_FONT_WEIGHT_NORMAL);
        editFmt = Format(ui, 15.0f * scale, DWRITE_FONT_WEIGHT_NORMAL);
        btnFmt = Format(ui, 15.0f * scale, DWRITE_FONT_WEIGHT_NORMAL);
        linkFmt = Format(ui, kLinkSize * scale, DWRITE_FONT_WEIGHT_NORMAL);
    }

    bool EnsureTarget() {
        if (rt) return true;
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        if (FAILED(g_d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size), &rt))) {
            return false;
        }
        rt->SetDpi(96.0f * scale, 96.0f * scale);
        return true;
    }

    void LoadWallpaper() {
        if (wallpaper || app::cfg.BackgroundImage.empty() || !rt) return;
        IWICBitmapDecoder* decoder = nullptr;
        if (FAILED(g_wic->CreateDecoderFromFilename(app::cfg.BackgroundImage.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder))) {
            app::Log(L"wallpaper: cannot open %s", app::cfg.BackgroundImage.c_str());
            return;
        }
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* conv = nullptr;
        if (SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(g_wic->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut))) {
            if (FAILED(rt->CreateBitmapFromWicBitmap(conv, nullptr, &wallpaper))) {
                app::Log(L"wallpaper: create bitmap failed");
            }
        }
        if (conv) conv->Release();
        if (frame) frame->Release();
        decoder->Release();
    }

    static void DrawTextBlock(ID2D1RenderTarget* rt, IDWriteTextFormat* fmt, const std::wstring& text,
                              float x, float y, float width, float height, unsigned color,
                              IDWriteFactory* factory) {
        if (!fmt) return;
        IDWriteTextLayout* layout = nullptr;
        if (FAILED(factory->CreateTextLayout(text.c_str(), (UINT32)text.size(), fmt, width, height, &layout))) return;
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(Color(color), &brush))) {
            rt->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
            brush->Release();
        }
        layout->Release();
    }

    void Paint() {
        if (!EnsureTarget()) return;
        LoadWallpaper();

        RECT rc;
        GetClientRect(hwnd, &rc);
        float w = (float)(rc.right - rc.left) / scale;
        float h = (float)(rc.bottom - rc.top) / scale;

        rt->BeginDraw();
        rt->SetTransform(D2D1::Matrix3x2F::Identity());
        rt->Clear(Color(0xFF080A10));

        if (wallpaper) {
            D2D1_SIZE_F img = wallpaper->GetSize();
            float sx = w / img.width, sy = h / img.height;
            float s = (std::max)(sx, sy);
            float dw = img.width * s, dh = img.height * s;
            rt->DrawBitmap(wallpaper, D2D1::RectF((w - dw) / 2, (h - dh) / 2, (w + dw) / 2, (h + dh) / 2), 1.0f,
                           D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }

        int mask = app::cfg.MaskOpacity >= 0 ? app::cfg.MaskOpacity : 59;
        int alpha = (int)(mask * 255.0 / 100.0 + 0.5);
        ID2D1SolidColorBrush* dim = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, alpha / 255.0f), &dim))) {
            rt->FillRectangle(D2D1::RectF(0, 0, w, h), dim);
            dim->Release();
        }

        float showClock = app::cfg.ShowClock ? 1.0f : 0.0f;
        float showSub = app::cfg.ShowSubtitle ? 1.0f : 0.0f;
        float clockH = showClock > 0 ? kClockSize * scale * 1.2f : 0.0f;
        float dateH = showClock > 0 ? kDateSize * scale * 2.0f + 26.0f * scale : 0.0f;
        float subH = showSub > 0 ? kSubtitleSize * scale * 2.0f + 14.0f * scale : 0.0f;
        float inputH = kInputHeight * scale;
        float gapButton = 12.0f * scale;
        float buttonH = kButtonHeight * scale;
        float gapLink = 12.0f * scale;
        float linkH = 20.0f * scale;
        float msgH = message.empty() ? 0.0f : 18.0f * scale + 10.0f * scale;
        float total = clockH + dateH + subH + inputH + gapButton + buttonH + gapLink + linkH + msgH;

        float colW = kColumnWidth * scale;
        float x = (w - colW) / 2;
        float y = (h - total) / 2;

        SYSTEMTIME st;
        GetLocalTime(&st);
        if (showClock > 0) {
            wchar_t timeText[32] = {};
            _snwprintf_s(timeText, _countof(timeText), _TRUNCATE, L"%02d:%02d", st.wHour, st.wMinute);
            DrawTextBlock(rt, clockFmt, timeText, x, y, colW, clockH, 0xFFFFFFFF, g_dw);
            y += clockH;

            static const wchar_t* week[7] = { L"星期日", L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六" };
            int idx = st.wDayOfWeek;
            if (idx < 0 || idx > 6) idx = 0;
            wchar_t dateText[64] = {};
            _snwprintf_s(dateText, _countof(dateText), _TRUNCATE, L"%d年%d月%d日 %s", st.wYear, st.wMonth, st.wDay, week[idx]);
            DrawTextBlock(rt, dateFmt, dateText, x, y, colW, kDateSize * scale * 2.0f, 0xFFC9C9C9, g_dw);
            y += dateH;
        }

        if (showSub > 0) {
            DrawTextBlock(rt, subFmt, app::cfg.Subtitle, x, y, colW, kSubtitleSize * scale * 2.0f, 0xFFE8EEF7, g_dw);
            y += subH;
        }

        D2D1_ROUNDED_RECT input = D2D1::RoundedRect(D2D1::RectF(x, y, x + colW, y + inputH),
                                                    kInputRadius * scale, kInputRadius * scale);
        ID2D1SolidColorBrush* fill = nullptr;
        ID2D1SolidColorBrush* line = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(Color(0xFF1E2129), &fill)) &&
            SUCCEEDED(rt->CreateSolidColorBrush(Color(0xFF4A4F5A), &line))) {
            rt->FillRoundedRectangle(input, fill);
            rt->DrawRoundedRectangle(input, line, 1.0f);
            line->Release();
        }

        float textLeft = x + kPad * scale;
        if (password.empty()) {
            DrawTextBlock(rt, editFmt, L"锁屏密码", textLeft, y, colW - kPad * scale * 2, inputH, 0xFF7B8391, g_dw);
        } else {
            ID2D1SolidColorBrush* dot = nullptr;
            if (SUCCEEDED(rt->CreateSolidColorBrush(Color(0xFFFFFFFF), &dot))) {
                float r = 3.5f * scale;
                float step = 12.0f * scale;
                float cy = y + inputH / 2;
                for (size_t i = 0; i < password.size(); ++i) {
                    float cx = textLeft + r + step * (float)i;
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), dot);
                }
                dot->Release();
                if (caretOn) {
                    ID2D1SolidColorBrush* caret = nullptr;
                    if (SUCCEEDED(rt->CreateSolidColorBrush(Color(0xFFFFFFFF), &caret))) {
                        float cx = textLeft + step * (float)password.size();
                        rt->FillRectangle(D2D1::RectF(cx, cy - 9 * scale, cx + 1.5f * scale, cy + 9 * scale), caret);
                        caret->Release();
                    }
                }
            }
        }
        if (fill) fill->Release();
        y += inputH + gapButton;

        D2D1_ROUNDED_RECT button = D2D1::RoundedRect(D2D1::RectF(x, y, x + colW, y + buttonH),
                                                     kButtonRadius * scale, kButtonRadius * scale);
        if (SUCCEEDED(rt->CreateSolidColorBrush(Color(0xFF3A76E8), &fill))) {
            rt->FillRoundedRectangle(button, fill);
            fill->Release();
            DrawTextBlock(rt, btnFmt, L"解锁", x, y, colW, buttonH, 0xFFFFFFFF, g_dw);
        }
        y += buttonH + gapLink;

        DrawTextBlock(rt, linkFmt, L"忘记密码？用恢复码解锁", x, y, colW, linkH, 0xFF8CB4F0, g_dw);
        y += linkH;

        if (!message.empty()) {
            DrawTextBlock(rt, linkFmt, message, x, y + 10.0f * scale, colW, 18.0f * scale,
                          error ? 0xFFFF7D7D : 0xFF82D28C, g_dw);
        }

        HRESULT hr = rt->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            SafeRelease((IUnknown**)&rt);
            SafeRelease((IUnknown**)&wallpaper);
        }
    }

    void TryUnlock() {
        if (password.empty()) return;
        if (app::VerifyPassword(password)) {
            app::Log(L"unlocked");
            app::Unlock();
            return;
        }
        app::Log(L"unlock failed");
        password.clear();
        message = L"密码不正确";
        error = true;
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void Guard() {
        if (!IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_SHOW);
        if (GetForegroundWindow() != hwnd) {
            SetForegroundWindow(hwnd);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    void Destroy() {
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        SafeRelease((IUnknown**)&wallpaper);
        SafeRelease((IUnknown**)&rt);
        ReleaseFormats();
        if (hwnd) DestroyWindow(hwnd);
        hwnd = nullptr;
    }
};

LRESULT CALLBACK Overlay::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Overlay* self = (Overlay*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (Overlay*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

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
    case WM_TIMER:
        if (wp == 1) {
            self->caretOn = !self->caretOn;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == 2) {
            self->Guard();
        }
        return 0;
    case WM_CHAR: {
        wchar_t c = (wchar_t)wp;
        if (c >= 32 && c != 127) {
            self->password.push_back(c);
            self->message.clear();
            self->error = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_BACK) {
            if (!self->password.empty()) self->password.pop_back();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_RETURN) {
            self->TryUnlock();
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        return 0;
    case WM_DPICHANGED:
        self->UpdateScale();
        self->CreateFormats();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

const wchar_t* kClass = L"QLockOverlay";

} // namespace

bool LockScreensContain(HWND hwnd) {
    for (Overlay* w : g_windows) if (w->hwnd == hwnd) return true;
    return false;
}

bool LockScreensAlive() {
    return !g_windows.empty();
}

void LockScreensCreate() {
    if (!g_windows.empty()) return;
    if (!InitFactories()) {
        app::Log(L"lock: factories failed");
        return;
    }
    if (!GetClassInfoW(app::inst, kClass, nullptr)) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = Overlay::Proc;
        wc.hInstance = app::inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);
    }

    std::vector<RECT> rects;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR mon, HDC, LPRECT, LPARAM data) -> BOOL {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(mon, &mi)) ((std::vector<RECT>*)data)->push_back(mi.rcMonitor);
        return TRUE;
    }, (LPARAM)&rects);

    for (const RECT& rc : rects) {
        Overlay* w = new Overlay();
        if (w->Create(rc, kClass)) {
            g_windows.push_back(w);
        } else {
            delete w;
        }
    }
    app::Log(L"locked, screens=%d", (int)g_windows.size());
}

void LockScreensClose() {
    for (Overlay* w : g_windows) {
        w->Destroy();
        delete w;
    }
    g_windows.clear();
}

void LockScreensGuard() {
    for (Overlay* w : g_windows) w->Guard();
}

void LockScreensReclaim() {
    LockScreensGuard();
    if (!g_windows.empty() && g_windows[0]->hwnd) SetForegroundWindow(g_windows[0]->hwnd);
}

void LockScreensSetError(const std::wstring& text) {
    for (Overlay* w : g_windows) {
        w->message = text;
        w->error = true;
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
}

void LockScreensLayoutChanged() {
    for (Overlay* w : g_windows) {
        w->UpdateScale();
        w->CreateFormats();
        SafeRelease((IUnknown**)&w->wallpaper);
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
}
