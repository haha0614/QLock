#pragma once
#include "qlock.h"
#include "theme.h"
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

ID2D1Factory1* g_d2d = nullptr;
ID3D11Device* g_d3d = nullptr;
ID2D1Device* g_device = nullptr;
IDWriteFactory* g_dw = nullptr;
IWICImagingFactory* g_wic = nullptr;
std::vector<class Overlay*> g_windows;

void SafeRelease(IUnknown** p) {
    if (*p) { (*p)->Release(); *p = nullptr; }
}

D2D1_COLOR_F Rgb(unsigned argb) {
    D2D1_COLOR_F c;
    c.a = ((argb >> 24) & 0xFF) / 255.0f;
    c.r = ((argb >> 16) & 0xFF) / 255.0f;
    c.g = ((argb >> 8) & 0xFF) / 255.0f;
    c.b = (argb & 0xFF) / 255.0f;
    return c;
}

bool InitFactories() {
    if (!g_d2d) {
        D2D1_FACTORY_OPTIONS options = {};
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options,
                                     reinterpret_cast<void**>(&g_d2d)))) {
            return false;
        }
    }
    if (!g_d3d) {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                     D3D11_SDK_VERSION, &g_d3d, nullptr, nullptr))) {
            return false;
        }
        IDXGIDevice* dxgi = nullptr;
        if (FAILED(g_d3d->QueryInterface(IID_PPV_ARGS(&dxgi)))) return false;
        HRESULT hr = g_d2d->CreateDevice(dxgi, &g_device);
        dxgi->Release();
        if (FAILED(hr)) return false;
    }
    if (!g_dw && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                            reinterpret_cast<IUnknown**>(&g_dw)))) {
        return false;
    }
    if (!g_wic && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&g_wic)))) {
        return false;
    }
    return true;
}

// 圆角超过控件半个高时截住，否则两边的弧会互相吃进去。
float Radius(float wanted, float height) {
    float r = app::cfg.CornerRadius >= 0 ? (float)app::cfg.CornerRadius : wanted;
    if (height > 0 && r > height / 2) r = height / 2;
    return r < 0 ? 0 : r;
}

class Overlay {
public:
    HWND hwnd = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1Bitmap1* target = nullptr;
    ID2D1Bitmap1* wallpaper = nullptr;
    ID2D1Effect* blur = nullptr;

    IDWriteTextFormat* clockFmt = nullptr;
    IDWriteTextFormat* dateFmt = nullptr;
    IDWriteTextFormat* brandFmt = nullptr;
    IDWriteTextFormat* subFmt = nullptr;
    IDWriteTextFormat* editFmt = nullptr;
    IDWriteTextFormat* btnFmt = nullptr;
    IDWriteTextFormat* linkFmt = nullptr;
    IDWriteTextFormat* msgFmt = nullptr;

    LockTheme theme;
    std::wstring password;
    std::wstring message;
    bool error = false;
    bool caretOn = true;
    float scale = 1.0f;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool Create(const RECT& rect, const std::wstring& klass) {
        theme = themes::Find(app::cfg.ThemeId);
        hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, klass.c_str(), L"QLock", WS_POPUP,
                               rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, app::inst, this);
        if (!hwnd) return false;
        UpdateScale();
        if (!EnsureTarget()) return false;
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
        if (dpi == 0) dpi = 96;
        scale = dpi / 96.0f;
    }

    bool EnsureDeviceContext() {
        if (dc) return true;
        if (!InitFactories() || !g_device) return false;
        if (FAILED(g_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return false;
        dc->SetDpi(96.0f * scale, 96.0f * scale);
        return true;
    }

    bool CreateSwapChain() {
        IDXGIDevice* dxgi = nullptr;
        if (FAILED(g_d3d->QueryInterface(IID_PPV_ARGS(&dxgi)))) return false;
        IDXGIAdapter* adapter = nullptr;
        if (FAILED(dxgi->GetAdapter(&adapter))) { dxgi->Release(); return false; }
        IDXGIFactory2* factory = nullptr;
        HRESULT hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        adapter->Release();
        dxgi->Release();
        if (FAILED(hr)) return false;

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = 0;
        desc.Height = 0;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        hr = factory->CreateSwapChainForHwnd(g_d3d, hwnd, &desc, nullptr, nullptr, &swap);
        factory->Release();
        return SUCCEEDED(hr);
    }

    bool BindTarget() {
        SafeRelease((IUnknown**)&target);
        dc->SetTarget(nullptr);
        IDXGISurface* surface = nullptr;
        if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
        D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f * scale, 96.0f * scale);
        HRESULT hr = dc->CreateBitmapFromDxgiSurface(surface, &props, &target);
        surface->Release();
        if (FAILED(hr)) return false;
        dc->SetTarget(target);
        return true;
    }

    bool EnsureTarget() {
        if (!EnsureDeviceContext()) return false;
        if (!swap && !CreateSwapChain()) {
            app::Log(L"lock: swap chain failed");
            return false;
        }
        if (!target && !BindTarget()) {
            app::Log(L"lock: bind target failed");
            return false;
        }
        return true;
    }

    void Resize() {
        if (!swap) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        UINT w = (UINT)(std::max)(1L, rc.right - rc.left);
        UINT h = (UINT)(std::max)(1L, rc.bottom - rc.top);
        dc->SetTarget(nullptr);
        SafeRelease((IUnknown**)&target);
        if (SUCCEEDED(swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) BindTarget();
    }

    void ReleaseFormats() {
        SafeRelease((IUnknown**)&clockFmt);
        SafeRelease((IUnknown**)&dateFmt);
        SafeRelease((IUnknown**)&brandFmt);
        SafeRelease((IUnknown**)&subFmt);
        SafeRelease((IUnknown**)&editFmt);
        SafeRelease((IUnknown**)&btnFmt);
        SafeRelease((IUnknown**)&linkFmt);
        SafeRelease((IUnknown**)&msgFmt);
    }

    IDWriteTextFormat* Format(const std::wstring& family, float size, int weight, DWRITE_TEXT_ALIGNMENT align) {
        IDWriteTextFormat* fmt = nullptr;
        std::wstring face = family.empty() ? L"Microsoft YaHei UI" : family;
        HRESULT hr = g_dw->CreateTextFormat(face.c_str(), nullptr, (DWRITE_FONT_WEIGHT)weight,
                                            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                            size, L"zh-cn", &fmt);
        if (FAILED(hr) && face != L"Microsoft YaHei UI") {
            hr = g_dw->CreateTextFormat(L"Microsoft YaHei UI", nullptr, (DWRITE_FONT_WEIGHT)weight,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        size, L"zh-cn", &fmt);
        }
        if (FAILED(hr)) return nullptr;
        fmt->SetTextAlignment(align);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        return fmt;
    }

    void CreateFormats() {
        ReleaseFormats();
        std::wstring ui = app::UiFontName();
        std::wstring clock = app::ClockFontName();
        clockFmt = Format(clock, (float)theme.Clock.Size, theme.Clock.Weight, DWRITE_TEXT_ALIGNMENT_CENTER);
        dateFmt = Format(ui, (float)theme.Date.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
        brandFmt = Format(ui, (float)theme.Brand.Size, 600, DWRITE_TEXT_ALIGNMENT_CENTER);
        subFmt = Format(ui, (float)theme.Subtitle.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
        editFmt = Format(ui, (float)theme.Input.Size, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        btnFmt = Format(ui, (float)theme.Button.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
        linkFmt = Format(ui, (float)theme.Link.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
        msgFmt = Format(ui, 12.0f, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    void LoadWallpaper() {
        if (wallpaper || app::cfg.BackgroundImage.empty() || !dc) return;
        IWICBitmapDecoder* decoder = nullptr;
        if (FAILED(g_wic->CreateDecoderFromFilename(app::cfg.BackgroundImage.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder))) {
            app::Log(L"wallpaper: cannot open %s", app::cfg.BackgroundImage.c_str());
            return;
        }
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* conv = nullptr;
        if (SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(g_wic->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                       WICBitmapPaletteTypeMedianCut))) {
            if (FAILED(dc->CreateBitmapFromWicBitmap(conv, nullptr, &wallpaper))) {
                app::Log(L"wallpaper: create bitmap failed");
            }
        }
        if (conv) conv->Release();
        if (frame) frame->Release();
        decoder->Release();
    }

    float BlurRadius() const {
        return app::cfg.BlurRadius >= 0 ? (float)app::cfg.BlurRadius : (float)theme.Background.Blur;
    }

    void EnsureBlur(float sigma) {
        if (!wallpaper || sigma <= 0.4f) return;
        if (!blur && FAILED(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blur))) {
            app::Log(L"blur: create effect failed");
            return;
        }
        blur->SetInput(0, wallpaper);
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma);
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    }

    void DrawText(IDWriteTextFormat* fmt, const std::wstring& text, const D2D1_RECT_F& box, D2D1_COLOR_F color) {
        if (!fmt || text.empty()) return;
        IDWriteTextLayout* layout = nullptr;
        if (FAILED(g_dw->CreateTextLayout(text.c_str(), (UINT32)text.size(), fmt, box.right - box.left,
                                          box.bottom - box.top, &layout))) {
            return;
        }
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(dc->CreateSolidColorBrush(color, &brush))) {
            dc->DrawTextLayout(D2D1::Point2F(box.left, box.top), layout, brush);
            brush->Release();
        }
        layout->Release();
    }

    void FillRoundRect(const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F color) {
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(dc->CreateSolidColorBrush(color, &brush))) {
            dc->FillRoundedRectangle(r, brush);
            brush->Release();
        }
    }

    void StrokeRoundRect(const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F color, float width) {
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(dc->CreateSolidColorBrush(color, &brush))) {
            dc->DrawRoundedRectangle(r, brush, width);
            brush->Release();
        }
    }

    void Paint() {
        if (!EnsureTarget()) return;
        LoadWallpaper();

        RECT rc;
        GetClientRect(hwnd, &rc);
        float w = (float)(rc.right - rc.left) / scale;
        float h = (float)(rc.bottom - rc.top) / scale;
        if (w <= 1 || h <= 1) return;

        const bool card = theme.Card.Show;
        const bool showClock = app::cfg.ShowClock && theme.Clock.Show;
        const bool showSub = app::cfg.ShowSubtitle && theme.Subtitle.Show;
        const bool inlineRow = theme.Inline();
        const bool underline = _wcsicmp(theme.Input.Kind.c_str(), L"underline") == 0;
        const bool outlineBtn = _wcsicmp(theme.Button.Kind.c_str(), L"outline") == 0;
        const bool flatBtn = _wcsicmp(theme.Button.Kind.c_str(), L"text") == 0;

        float colW = card ? (float)theme.Card.Width
                          : (theme.Input.Width > 0 ? (float)theme.Input.Width : 340.0f);
        float innerW = card ? colW - (float)theme.Card.PadH * 2 : colW;
        if (innerW < 120) innerW = 120;

        float clockH = showClock ? (float)theme.Clock.Size * 1.32f : 0.0f;
        float dateH = showClock ? (float)theme.Date.Size * 1.5f + 2.0f : 0.0f;
        float dateGap = showClock ? (float)theme.Date.Gap : 0.0f;
        float brandH = theme.Brand.Show ? (float)theme.Brand.Size * 1.7f + (float)theme.Brand.Gap : 0.0f;
        float subH = showSub ? (float)theme.Subtitle.Size * 1.7f + (float)theme.Subtitle.Gap : 0.0f;
        float inputH = (float)theme.Input.Height;
        float btnH = (float)theme.Button.Height;
        float btnGap = (float)theme.Button.Gap;
        float linkH = (float)theme.Link.Size * 1.7f;
        float linkGap = (float)theme.Link.Gap;
        float msgH = message.empty() ? 0.0f : 28.0f;

        float btnW = inlineRow ? (theme.Button.Width > 0 ? (float)theme.Button.Width : 96.0f) : innerW;
        float rowH = inlineRow ? inputH : 0.0f;

        float bodyH = brandH + subH + (inlineRow ? rowH : inputH + btnGap + btnH) + linkGap + linkH + msgH;
        float cardH = card ? bodyH + (float)theme.Card.PadV * 2 : bodyH;
        float total = clockH + dateH + dateGap + cardH;
        float left = (w - colW) / 2;
        float top = (h - total) / 2;

        dc->BeginDraw();
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        D2D1_COLOR_F bgColor = themes::Color(theme.Background.Color, Rgb(0xFF080A10));
        dc->Clear(bgColor);

        if (wallpaper) {
            D2D1_SIZE_F img = wallpaper->GetSize();
            float s = (std::max)(w / img.width, h / img.height);
            float dw = img.width * s, dh = img.height * s;
            D2D1_RECT_F dest = D2D1::RectF((w - dw) / 2, (h - dh) / 2, (w + dw) / 2, (h + dh) / 2);
            dc->PushAxisAlignedClip(D2D1::RectF(0, 0, w, h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            float radius = BlurRadius();
            // WPF 的 Radius 是屏幕上的像素，模糊作用在原图坐标里，先换算回原图尺度。
            EnsureBlur(radius > 0 ? radius * 0.5f / (s > 0 ? s : 1.0f) : 0.0f);
            if (blur && radius > 0) {
                // 效果图的坐标就是位图的 DIP 尺寸，按 cover 比例缩放到 dest 再平移到左上角。
                float sx = (dest.right - dest.left) / img.width;
                float sy = (dest.bottom - dest.top) / img.height;
                dc->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy) *
                                 D2D1::Matrix3x2F::Translation(dest.left, dest.top));
                dc->DrawImage(blur, D2D1::Point2F(0, 0), D2D1::RectF(0, 0, img.width, img.height));
                dc->SetTransform(D2D1::Matrix3x2F::Identity());
            } else {
                dc->DrawBitmap(wallpaper, dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            dc->PopAxisAlignedClip();
        }

        int dimAlpha = app::cfg.MaskOpacity >= 0 ? (int)(app::cfg.MaskOpacity * 255.0 / 100.0 + 0.5)
                                                 : theme.Background.Dim;
        if (dimAlpha > 0) {
            D2D1_COLOR_F dim = bgColor;
            dim.a = (dimAlpha > 255 ? 255 : dimAlpha) / 255.0f;
            ID2D1SolidColorBrush* brush = nullptr;
            if (SUCCEEDED(dc->CreateSolidColorBrush(dim, &brush))) {
                dc->FillRectangle(D2D1::RectF(0, 0, w, h), brush);
                brush->Release();
            }
        }

        SYSTEMTIME st;
        GetLocalTime(&st);

        if (showClock) {
            wchar_t timeText[32] = {};
            _snwprintf_s(timeText, _countof(timeText), _TRUNCATE, L"%02d:%02d", st.wHour, st.wMinute);
            DrawText(clockFmt, timeText, D2D1::RectF(left, top, left + colW, top + clockH),
                     themes::Color(theme.Clock.Color, Rgb(0xFFFFFFFF)));
            top += clockH;

            static const wchar_t* week[7] = { L"星期日", L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六" };
            int idx = st.wDayOfWeek;
            if (idx < 0 || idx > 6) idx = 0;
            wchar_t dateText[96] = {};
            _snwprintf_s(dateText, _countof(dateText), _TRUNCATE, L"%d年%d月%d日 %s", st.wYear, st.wMonth, st.wDay, week[idx]);
            DrawText(dateFmt, dateText, D2D1::RectF(left, top + 2.0f, left + colW, top + dateH),
                     themes::Color(theme.Date.Color, Rgb(0xFFC9C9C9)));
            top += dateH + dateGap;
        }

        if (card) {
            D2D1_ROUNDED_RECT box = D2D1::RoundedRect(D2D1::RectF(left, top, left + colW, top + cardH),
                                                      Radius((float)theme.Card.Radius, 0),
                                                      Radius((float)theme.Card.Radius, 0));
            FillRoundRect(box, themes::Color(theme.Card.Bg, Rgb(0xD81C1C26)));
            StrokeRoundRect(box, themes::Color(theme.Card.Border, Rgb(0x37FFFFFF)), 1.0f);
            top += (float)theme.Card.PadV;
            left += (float)theme.Card.PadH;
        }

        if (theme.Brand.Show) {
            DrawText(brandFmt, theme.Brand.Text, D2D1::RectF(left, top, left + innerW, top + brandH),
                     themes::Color(theme.Brand.Color, Rgb(0xFF78A0E6)));
            top += brandH;
        }
        if (showSub) {
            DrawText(subFmt, app::cfg.Subtitle, D2D1::RectF(left, top, left + innerW, top + subH),
                     themes::Color(theme.Subtitle.Color, Rgb(0xFFE8EEF7)));
            top += subH;
        }

        float inputW = inlineRow ? innerW - btnW - 8.0f : innerW;
        if (inputW < 80) inputW = 80;
        D2D1_RECT_F inputBox = D2D1::RectF(left, top, left + inputW, top + inputH);
        float inputRadius = Radius((float)theme.Input.Radius, inputH);
        if (underline) {
            D2D1_COLOR_F line = themes::Color(theme.Input.Border, Rgb(0xFF2A2F38));
            ID2D1SolidColorBrush* brush = nullptr;
            if (SUCCEEDED(dc->CreateSolidColorBrush(line, &brush))) {
                dc->FillRectangle(D2D1::RectF(inputBox.left, inputBox.bottom - 1.0f, inputBox.right, inputBox.bottom), brush);
                brush->Release();
            }
        } else {
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(inputBox, inputRadius, inputRadius);
            FillRoundRect(r, themes::Color(theme.Input.Bg, Rgb(0xFF1E2129)));
            StrokeRoundRect(r, themes::Color(theme.Input.Border, Rgb(0xFF4A4F5A)), 1.0f);
        }

        float inset = underline ? 0.0f : 13.0f;
        float textLeft = inputBox.left + inset;
        float textW = inputBox.right - textLeft - inset;
        if (password.empty()) {
            DrawText(editFmt, L"锁屏密码", D2D1::RectF(textLeft, inputBox.top, textLeft + textW, inputBox.bottom),
                     themes::Color(theme.Input.Hint, Rgb(0xFF7B8391)));
        } else {
            ID2D1SolidColorBrush* dot = nullptr;
            if (SUCCEEDED(dc->CreateSolidColorBrush(themes::Color(theme.Input.Fg, Rgb(0xFFFFFFFF)), &dot))) {
                float cy = (inputBox.top + inputBox.bottom) / 2;
                float step = (float)theme.Input.Size * 0.85f;
                float r = (std::max)(3.0f, (float)theme.Input.Size * 0.22f);
                for (size_t i = 0; i < password.size(); ++i) {
                    float cx = textLeft + r + step * (float)i;
                    dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), dot);
                }
                if (caretOn) {
                    float cx = textLeft + step * (float)password.size();
                    dc->FillRectangle(D2D1::RectF(cx, cy - 9.0f, cx + 1.5f, cy + 9.0f), dot);
                }
                dot->Release();
            }
        }

        if (inlineRow) {
            float bx = left + inputW + 8.0f;
            float by = top + (inputH - btnH) / 2;
            D2D1_RECT_F box = D2D1::RectF(bx, by, bx + btnW, by + btnH);
            float r = Radius((float)theme.Button.Radius, btnH);
            if (!flatBtn) {
                FillRoundRect(D2D1::RoundedRect(box, r, r), themes::Color(theme.Button.Bg, Rgb(0xFF3A76E8)));
            }
            if (outlineBtn) {
                StrokeRoundRect(D2D1::RoundedRect(box, r, r), themes::Color(theme.Button.Border, Rgb(0x50FFFFFF)), 1.0f);
            }
            DrawText(btnFmt, L"解锁", box, themes::Color(theme.Button.Fg, Rgb(0xFFFFFFFF)));
            top += rowH;
        } else {
            top += inputH + btnGap;
            D2D1_RECT_F box = D2D1::RectF(left, top, left + btnW, top + btnH);
            float r = Radius((float)theme.Button.Radius, btnH);
            if (!flatBtn) {
                FillRoundRect(D2D1::RoundedRect(box, r, r), themes::Color(theme.Button.Bg, Rgb(0xFF3A76E8)));
            }
            if (outlineBtn) {
                StrokeRoundRect(D2D1::RoundedRect(box, r, r), themes::Color(theme.Button.Border, Rgb(0x50FFFFFF)), 1.0f);
            }
            DrawText(btnFmt, L"解锁", box, themes::Color(theme.Button.Fg, Rgb(0xFFFFFFFF)));
            top += btnH;
        }

        top += linkGap;
        DrawText(linkFmt, L"忘记密码？用恢复码解锁", D2D1::RectF(left, top, left + innerW, top + linkH),
                 themes::Color(theme.Link.Color, Rgb(0xFF8CB4F0)));
        top += linkH;

        if (!message.empty()) {
            DrawText(msgFmt, message, D2D1::RectF(left, top + 10.0f, left + innerW, top + 10.0f + 18.0f),
                     error ? Rgb(0xFFFF7D7D) : Rgb(0xFF82D28C));
        }

        HRESULT hr = dc->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            SafeRelease((IUnknown**)&target);
            SafeRelease((IUnknown**)&wallpaper);
            SafeRelease((IUnknown**)&blur);
            return;
        }
        swap->Present(1, 0);
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
        SafeRelease((IUnknown**)&blur);
        SafeRelease((IUnknown**)&wallpaper);
        if (dc) dc->SetTarget(nullptr);
        SafeRelease((IUnknown**)&target);
        SafeRelease((IUnknown**)&swap);
        SafeRelease((IUnknown**)&dc);
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
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) self->Resize();
        return 0;
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
            self->caretOn = true;
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
        if (self->dc) self->dc->SetDpi(96.0f * self->scale, 96.0f * self->scale);
        self->Resize();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

const wchar_t* kClass = L"QLockOverlay";

}  // namespace

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
        w->theme = themes::Find(app::cfg.ThemeId);
        w->UpdateScale();
        w->CreateFormats();
        SafeRelease((IUnknown**)&w->wallpaper);
        SafeRelease((IUnknown**)&w->blur);
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
}
