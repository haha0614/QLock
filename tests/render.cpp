#pragma once
#include "../src/gfx.h"
#include "../src/lock_view.h"
#include "../src/qlock.h"
#include "../src/theme.h"
#include <objbase.h>
#include <wincodec.h>
#include <cstdio>

// 离屏渲染一张锁屏，用来比设置窗里的预览和真锁屏是不是一回事。
// 用法：render <out.png> [宽 高] [recovery|snapshot] [dpi]
// dpi 用来验高 DPI：同样的 DIP 排版，像素数按 dpi/96 放大，画面应当一模一样。
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { wprintf(L"usage: render <out.png> [w h] [recovery|snapshot] [dpi]\n"); return 2; }
    UINT w = argc > 3 ? (UINT)_wtoi(argv[2]) : 1920;
    UINT h = argc > 3 ? (UINT)_wtoi(argv[3]) : 1080;
    const float dpi = argc > 5 ? (float)_wtoi(argv[5]) : 96.0f;
    if (dpi <= 0) { wprintf(L"bad dpi\n"); return 2; }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    app::inst = GetModuleHandleW(nullptr);
    app::LoadConfig();
    if (!gfx::Init()) { wprintf(L"gfx init failed\n"); return 1; }

    ID2D1DeviceContext* dc = nullptr;
    if (FAILED(gfx::Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) return 1;
    dc->SetDpi(dpi, dpi);

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi, dpi);
    ID2D1Bitmap1* target = nullptr;
    if (FAILED(dc->CreateBitmap(D2D1::SizeU(w, h), nullptr, 0, &props, &target))) { wprintf(L"target failed\n"); return 1; }
    dc->SetTarget(target);

    lockview::View view;
    view.theme = themes::Find(app::cfg.ThemeId);
    if (argc > 4 && _wcsicmp(argv[4], L"typed") == 0) {
        // 输了两位的状态，用来看圆点和光标的画法。
        view.password = L"12";
        view.caretOn = true;
    }
    if (argc > 4 && _wcsicmp(argv[4], L"recovery") == 0) {
        // 不锁屏也能看一眼恢复码面板长什么样。
        view.recovering = true;
        view.recovery = L"K7M2Q-9WXZ3";
    }
    if (argc > 4 && _wcsicmp(argv[4], L"snapshot") == 0) {
        // 走一遍「用屏幕截图做模糊背景」的路子：GDI 抓主屏 → View 直接拿 DIB 建位图。
        HDC screen = GetDC(nullptr);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = (LONG)w;
        bi.bmiHeader.biHeight = -(LONG)h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP shot = screen ? CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
        if (shot) {
            HDC mem = CreateCompatibleDC(screen);
            HGDIOBJ old = SelectObject(mem, shot);
            BitBlt(mem, 0, 0, (int)w, (int)h, screen, 0, 0, SRCCOPY | CAPTUREBLT);
            SelectObject(mem, old);
            DeleteDC(mem);
            view.SetSnapshot(shot);
            wprintf(L"snapshot attached\n");
        }
        if (screen) ReleaseDC(nullptr, screen);
    }
    view.Reload(dc);

    dc->BeginDraw();
    // 窗口那边传的是 DIP（像素数除以缩放），这里照着算。
    view.Draw(dc, (float)w * 96.0f / dpi, (float)h * 96.0f / dpi);
    dc->EndDraw();
    dc->SetTarget(nullptr);

    D2D1_BITMAP_PROPERTIES1 readProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
    ID2D1Bitmap1* cpu = nullptr;
    if (FAILED(dc->CreateBitmap(D2D1::SizeU(w, h), nullptr, 0, &readProps, &cpu))) { wprintf(L"cpu bitmap failed\n"); return 1; }
    if (FAILED(cpu->CopyFromBitmap(nullptr, target, nullptr))) { wprintf(L"copy failed\n"); return 1; }
    D2D1_MAPPED_RECT mapped = {};
    if (FAILED(cpu->Map(D2D1_MAP_OPTIONS_READ, &mapped))) { wprintf(L"map failed\n"); return 1; }

    IWICBitmapEncoder* enc = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* bag = nullptr;
    HRESULT hr = gfx::WIC()->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(argv[1], GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = gfx::WIC()->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    if (SUCCEEDED(hr)) hr = enc->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = enc->CreateNewFrame(&frame, &bag);
    if (SUCCEEDED(hr)) hr = frame->Initialize(bag);
    if (SUCCEEDED(hr)) hr = frame->SetSize(w, h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&fmt);
    if (SUCCEEDED(hr)) hr = frame->WritePixels(h, mapped.pitch, mapped.pitch * h, mapped.bits);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = enc->Commit();
    cpu->Unmap();
    if (FAILED(hr)) { wprintf(L"encode failed %08x\n", hr); return 1; }
    wprintf(L"saved %s %ux%u\n", argv[1], w, h);
    return 0;
}
