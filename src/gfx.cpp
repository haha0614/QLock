#pragma once
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

float Measure(IDWriteTextFormat* fmt, const std::wstring& s) {
    if (!fmt || s.empty()) return 0;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), fmt, 4000.0f, 200.0f, &layout))) return 0;
    DWRITE_TEXT_METRICS m = {};
    layout->GetMetrics(&m);
    layout->Release();
    return m.widthIncludingTrailingWhitespace;
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

}  // namespace gfx
