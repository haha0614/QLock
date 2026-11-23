#pragma once
#include <cmath>
#include "gfx.h"
#include "qlock.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace gfx {
namespace {

ID2D1Factory1* g_d2d = nullptr;
ID3D11Device* g_d3d = nullptr;
ID2D1Device* g_device = nullptr;
IDWriteFactory* g_dw = nullptr;
IWICImagingFactory* g_wic = nullptr;

}  // namespace

ID2D1Factory1* D2D() { return g_d2d; }
ID2D1Device* Device() { return g_device; }
ID3D11Device* D3D() { return g_d3d; }
IDWriteFactory* DW() { return g_dw; }
IWICImagingFactory* WIC() { return g_wic; }

bool Init() {
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

D2D1_COLOR_F Color(unsigned argb) {
    D2D1_COLOR_F c;
    c.a = ((argb >> 24) & 0xFF) / 255.0f;
    c.r = ((argb >> 16) & 0xFF) / 255.0f;
    c.g = ((argb >> 8) & 0xFF) / 255.0f;
    c.b = (argb & 0xFF) / 255.0f;
    return c;
}

float Radius(float wanted, float height) {
    float r = app::cfg.CornerRadius >= 0 ? (float)app::cfg.CornerRadius : wanted;
    if (height > 0 && r > height / 2) r = height / 2;
    return r < 0 ? 0 : r;
}

void FillRect(ID2D1DeviceContext* dc, const D2D1_RECT_F& r, D2D1_COLOR_F c) {
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->FillRectangle(r, brush);
        brush->Release();
    }
}

void FillRound(ID2D1DeviceContext* dc, const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F c) {
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->FillRoundedRectangle(r, brush);
        brush->Release();
    }
}

void StrokeRound(ID2D1DeviceContext* dc, const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F c, float width) {
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->DrawRoundedRectangle(r, brush, width);
        brush->Release();
    }
}

namespace {

// 一个角的连续曲率采样：t 从 0 到 1，(x,y) 从直边切点走到另一条边的切点。
// x=(1-t)^a、y=t^a 在两端的一阶、二阶导都退化成零，所以接上直边时曲率是连续的。
const int kCornerSteps = 24;
const double kCurveA = 2.5;

ID2D1PathGeometry* SmoothPath(const D2D1_RECT_F& box, float radius) {
    const float w = box.right - box.left, h = box.bottom - box.top;
    if (w <= 0 || h <= 0) return nullptr;
    float r = radius;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r < 0.5f) return nullptr;

    ID2D1PathGeometry* path = nullptr;
    if (FAILED(g_d2d->CreatePathGeometry(&path)) || !path) return nullptr;
    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(path->Open(&sink)) || !sink) {
        path->Release();
        return nullptr;
    }
    sink->BeginFigure(D2D1::Point2F(box.left + r, box.top), D2D1_FIGURE_BEGIN_FILLED);

    // (cx,cy) 是角点，sx/sy 指向角的两条边：+1 往右/下，-1 往左/上。
    // verticalFirst 表示这段曲线从竖边出发、落到横边（顺时针走的时候右下角和左上角是这样）。
    auto corner = [&](float cx, float cy, float sx, float sy, bool verticalFirst) {
        for (int i = 1; i <= kCornerSteps; ++i) {
            const double t = (double)i / kCornerSteps;
            const double lin = std::pow(t, kCurveA) * r;
            const double bend = std::pow(1.0 - t, kCurveA) * r;
            const float dx = (float)(verticalFirst ? lin : bend);
            const float dy = (float)(verticalFirst ? bend : lin);
            sink->AddLine(D2D1::Point2F(cx + sx * dx, cy + sy * dy));
        }
    };

    sink->AddLine(D2D1::Point2F(box.right - r, box.top));
    corner(box.right, box.top, -1.0f, 1.0f, false);
    sink->AddLine(D2D1::Point2F(box.right, box.bottom - r));
    corner(box.right, box.bottom, -1.0f, -1.0f, true);
    sink->AddLine(D2D1::Point2F(box.left + r, box.bottom));
    corner(box.left, box.bottom, 1.0f, -1.0f, false);
    sink->AddLine(D2D1::Point2F(box.left, box.top + r));
    corner(box.left, box.top, 1.0f, 1.0f, true);

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();
    return path;
}

}  // namespace

void FillSmooth(ID2D1DeviceContext* dc, const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c) {
    ID2D1PathGeometry* path = SmoothPath(r, radius);
    if (!path) {
        FillRound(dc, D2D1::RoundedRect(r, radius, radius), c);
        return;
    }
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->FillGeometry(path, brush);
        brush->Release();
    }
    path->Release();
}

void StrokeSmooth(ID2D1DeviceContext* dc, const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c, float width) {
    ID2D1PathGeometry* path = SmoothPath(r, radius);
    if (!path) {
        StrokeRound(dc, D2D1::RoundedRect(r, radius, radius), c, width);
        return;
    }
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->DrawGeometry(path, brush, width);
        brush->Release();
    }
    path->Release();
}

void TextWrapped(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, const std::wstring& s,
                 const D2D1_RECT_F& box, D2D1_COLOR_F c) {
    if (!fmt || s.empty()) return;
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    Text(dc, fmt, s, box, c);
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
}

void Text(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, const std::wstring& s,
          const D2D1_RECT_F& box, D2D1_COLOR_F c) {
    if (!fmt || s.empty()) return;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt, box.right - box.left,
                                      box.bottom - box.top, &layout))) {
        return;
    }
    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(dc->CreateSolidColorBrush(c, &brush))) {
        dc->DrawTextLayout(D2D1::Point2F(box.left, box.top), layout, brush);
        brush->Release();
    }
    layout->Release();
}

void TextCentered(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, const std::wstring& s,
                  const D2D1_RECT_F& box, D2D1_COLOR_F c) {
    if (!fmt || s.empty()) return;
    DWRITE_TEXT_ALIGNMENT prev = fmt->GetTextAlignment();
    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    Text(dc, fmt, s, box, c);
    fmt->SetTextAlignment(prev);
}

float Measure(IDWriteTextFormat* fmt, const std::wstring& s) {
    if (!fmt || s.empty()) return 0;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt, 4000.0f, 200.0f, &layout))) return 0;
    DWRITE_TEXT_METRICS m = {};
    layout->GetMetrics(&m);
    layout->Release();
    return m.widthIncludingTrailingWhitespace;
}

float LineHeight(IDWriteTextFormat* fmt) {
    if (!fmt) return 0;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(g_dw->CreateTextLayout(L"Ag中", 3, fmt, 4000.0f, 400.0f, &layout))) return 0;
    DWRITE_TEXT_METRICS m = {};
    layout->GetMetrics(&m);
    layout->Release();
    return m.height;
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

void Field(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, float fontSize, const D2D1_RECT_F& box,
           const std::wstring& text, const wchar_t* hint, const FieldStyle& st, bool enabled, bool caretOn) {
    if (st.underline) {
        FillRect(dc, D2D1::RectF(box.left, box.bottom - 1.0f, box.right, box.bottom), st.border);
    } else {
        const float fh = box.bottom - box.top;
        const float rad = st.globalRadius ? Radius(st.radius, fh)
                                          : (st.radius < fh * 0.5f ? st.radius : fh * 0.5f);
        if (st.smooth) {
            FillSmooth(dc, box, rad, st.bg);
            StrokeSmooth(dc, box, rad, st.border, 1.0f);
        } else {
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(box, rad, rad);
            FillRound(dc, r, st.bg);
            StrokeRound(dc, r, st.border, 1.0f);
        }
    }

    const float inset = st.underline ? 0.0f : st.inset;
    const float textLeft = box.left + inset;
    float textW = box.right - textLeft - inset - st.rightInset;
    if (textW < 12.0f) textW = 12.0f;
    if (text.empty() && hint) {
        Text(dc, fmt, hint, D2D1::RectF(textLeft, box.top, textLeft + textW, box.bottom), st.hint);
    }

    D2D1_COLOR_F fg = st.fg;
    if (!enabled) fg.a *= 0.45f;
    ID2D1SolidColorBrush* brush = nullptr;
    if (FAILED(dc->CreateSolidColorBrush(fg, &brush))) return;
    const float cy = (box.top + box.bottom) * 0.5f;
    if (st.dots) {
        const float step = fontSize * st.dotStep;
        const float r = fontSize * st.dotRadius;
        for (size_t i = 0; i < text.size(); ++i) {
            const float cx = textLeft + r + step * (float)i;
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush);
        }
    } else if (!text.empty()) {
        Text(dc, fmt, text, D2D1::RectF(textLeft, box.top, textLeft + textW, box.bottom), fg);
    }
    if (st.caret && caretOn && enabled) {
        float cx = textLeft - st.caretLift;
        if (!text.empty()) cx = st.dots ? textLeft + fontSize * st.dotStep * (float)text.size()
                                       : textLeft + Measure(fmt, text);
        dc->FillRectangle(D2D1::RectF(cx, cy - st.caretH * 0.5f, cx + st.caretW, cy + st.caretH * 0.5f), brush);
    }
    brush->Release();
}

}  // namespace gfx
