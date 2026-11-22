#pragma once
#include "qlock.h"
#include "theme.h"
#include "gfx.h"
#include "lock_view.h"
#include <vector>
#include <string>
#include <algorithm>
#include <windowsx.h>

namespace {

std::vector<class Overlay*> g_windows;

bool Inside(const D2D1_RECT_F& r, POINT p) {
    return r.right > r.left && p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}
const wchar_t* kClass = L"QLockOverlay";

template <class T>
void Release(T** p) {
    if (*p) { (*p)->Release(); *p = nullptr; }
}

class Overlay {
public:
    HWND hwnd = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1Bitmap1* target = nullptr;
    lockview::View view;
    float scale = 1.0f;
    int failures = 0;   // 连续输错的次数，解锁后清零

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool Create(const RECT& rect, const std::wstring& klass) {
        view.theme = themes::Find(app::cfg.ThemeId);
        hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, klass.c_str(), L"QLock", WS_POPUP,
                               rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, app::inst, this);
        if (!hwnd) return false;
        UpdateScale();
        if (!EnsureTarget()) return false;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        SetTimer(hwnd, 1, 500, nullptr);
        SetTimer(hwnd, 2, 350, nullptr);
        return true;
    }

    // 显示器换了分辨率或插拔屏时把窗口挪到新的整块屏上。
    void SetBounds(const RECT& rect) {
        MoveWindow(hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
        UpdateScale();
        if (dc) dc->SetDpi(96.0f * scale, 96.0f * scale);
        Resize();
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void UpdateScale() {
        UINT dpi = 96;
        typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
        static GetDpiForWindowFn fn = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        if (fn) dpi = fn(hwnd);
        if (dpi == 0) dpi = 96;
        scale = dpi / 96.0f;
    }

    bool EnsureTarget() {
        if (!gfx::Init()) {
            app::Log(L"lock: factories failed");
            return false;
        }
        if (!dc && FAILED(gfx::Device()->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc))) {
            return false;
        }
        dc->SetDpi(96.0f * scale, 96.0f * scale);
        if (!swap && !CreateSwapChain()) {
            app::Log(L"lock: swap chain failed");
            return false;
        }
        if (!target && !BindTarget()) {
            app::Log(L"lock: bind target failed");
            return false;
        }
        if (!view.Ready()) view.Reload(dc);
        return true;
    }

    bool CreateSwapChain() {
        IDXGIDevice* dxgi = nullptr;
        if (FAILED(gfx::D3D()->QueryInterface(IID_PPV_ARGS(&dxgi)))) return false;
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
        hr = factory->CreateSwapChainForHwnd(gfx::D3D(), hwnd, &desc, nullptr, nullptr, &swap);
        factory->Release();
        return SUCCEEDED(hr);
    }

    bool BindTarget() {
        Release(&target);
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

    void Resize() {
        if (!swap) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        UINT w = (UINT)(std::max)(1L, rc.right - rc.left);
        UINT h = (UINT)(std::max)(1L, rc.bottom - rc.top);
        dc->SetTarget(nullptr);
        Release(&target);
        if (SUCCEEDED(swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) BindTarget();
    }

    void Paint() {
        if (!EnsureTarget()) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        float w = (float)(rc.right - rc.left) / scale;
        float h = (float)(rc.bottom - rc.top) / scale;
        if (w <= 1 || h <= 1) return;

        dc->BeginDraw();
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        view.Draw(dc, w, h);
        HRESULT hr = dc->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            view.DropDeviceResources();
            Release(&target);
            return;
        }
        swap->Present(1, 0);
    }

    void TryUnlock() {
        if (!view.inputEnabled) return;
        if (view.password.empty()) {
            view.message = L"请输入密码";
            view.error = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (app::VerifyPassword(view.password)) {
            app::Log(L"unlocked");
            app::Unlock();
            return;
        }
        app::Log(L"bad password, failures=%d", failures);
        view.password.clear();
        view.message = L"密码不正确";
        view.error = true;
        if (++failures >= 3) Punish(failures);
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void TryRecovery() {
        if (!view.inputEnabled || view.recovery.empty()) return;
        if (app::VerifyRecovery(view.recovery)) {
            app::Log(L"unlocked with recovery code");
            app::ClearRecovery();   // 恢复码用一次就作废
            app::SaveConfig();
            app::Unlock();
            HWND main = FindWindowExW(HWND_MESSAGE, nullptr, L"QLockMain", nullptr);
            static const UINT kShowSettings = RegisterWindowMessageW(L"QLock.ShowSettings");
            if (main) PostMessageW(main, kShowSettings, 0, 0);
            return;
        }
        app::Log(L"bad recovery code, failures=%d", failures);
        view.recovery.clear();
        view.message = L"恢复码不正确";
        view.error = true;
        if (++failures >= 3) Punish(failures);
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    // 连续输错就锁住输入几秒，越错越久，最多 10 秒。
    void Punish(int count) {
        int seconds = count - 2;
        if (seconds > 10) seconds = 10;
        if (seconds < 1) seconds = 1;
        std::wstring text = L"过于频繁，请等待 " + std::to_wstring(seconds) + L" 秒";
        for (size_t i = 0; i < g_windows.size(); ++i) {
            Overlay* w = g_windows[i];
            w->view.inputEnabled = false;
            w->view.password.clear();
            w->view.recovery.clear();
            w->view.message = text;
            w->view.error = true;
            InvalidateRect(w->hwnd, nullptr, FALSE);
        }
        SetTimer(hwnd, 3, (UINT)seconds * 1000, nullptr);
    }

    void EndPunish() {
        KillTimer(hwnd, 3);
        for (size_t i = 0; i < g_windows.size(); ++i) {
            Overlay* w = g_windows[i];
            w->view.inputEnabled = true;
            w->view.message.clear();
            InvalidateRect(w->hwnd, nullptr, FALSE);
        }
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
        view.DropDeviceResources();
        if (dc) dc->SetTarget(nullptr);
        Release(&target);
        Release(&swap);
        Release(&dc);
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
            self->view.caretOn = !self->view.caretOn;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == 2) {
            self->Guard();
        } else if (wp == 3) {
            self->EndPunish();
        }
        return 0;
    case WM_CHAR: {
        wchar_t c = (wchar_t)wp;
        if (c >= 32 && c != 127 && self->view.inputEnabled) {
            std::wstring& field = self->view.recovering ? self->view.recovery : self->view.password;
            size_t limit = self->view.recovering ? 12 : 64;
            if (field.size() < limit) field.push_back(c);
            self->view.message.clear();
            self->view.error = false;
            self->view.caretOn = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_BACK) {
            std::wstring& field = self->view.recovering ? self->view.recovery : self->view.password;
            if (!field.empty()) field.pop_back();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_RETURN) {
            if (self->view.recovering) self->TryRecovery();
            else self->TryUnlock();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(hwnd);
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (Inside(self->view.buttonRect, pt)) {
            if (self->view.recovering) self->TryRecovery();
            else self->TryUnlock();
        } else if (!self->view.recovering && Inside(self->view.forgotRect, pt)) {
            self->view.recovering = true;
            self->view.password.clear();
            self->view.message.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (Inside(self->view.backRect, pt)) {
            self->view.recovering = false;
            self->view.recovery.clear();
            self->view.message.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_DISPLAYCHANGE:
        // 每块屏都会收到一次，函数内部做了节流。
        LockScreensLayoutChanged();
        return 0;
    case WM_DPICHANGED:
        self->UpdateScale();
        self->view.Reload(self->dc);
        if (self->dc) self->dc->SetDpi(96.0f * self->scale, 96.0f * self->scale);
        self->Resize();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool LockScreensContain(HWND hwnd) {
    for (Overlay* w : g_windows) if (w->hwnd == hwnd) return true;
    return false;
}

bool LockScreensAlive() {
    return !g_windows.empty();
}

// GDI 抓一块屏，给「用屏幕截图做模糊背景」用；返回的位图交给 Overlay 释放。
static HBITMAP CaptureScreen(const RECT& rc) {
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return nullptr;
    HDC screen = GetDC(nullptr);
    if (!screen) return nullptr;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp) {
        HDC mem = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, w, h, screen, rc.left, rc.top, SRCCOPY | CAPTUREBLT);
        SelectObject(mem, old);
        DeleteDC(mem);
    } else {
        app::Log(L"snapshot: capture failed");
    }
    ReleaseDC(nullptr, screen);
    return bmp;
}

// 每块屏的整块矩形，按系统给的顺序（主屏在前）。
static std::vector<RECT> MonitorRects() {
    std::vector<RECT> rects;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR mon, HDC, LPRECT, LPARAM data) -> BOOL {
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfoW(mon, &mi)) ((std::vector<RECT>*)data)->push_back(mi.rcMonitor);
        return TRUE;
    }, (LPARAM)&rects);
    return rects;
}

void LockScreensCreate() {
    if (!g_windows.empty()) return;
    if (!gfx::Init()) {
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

    std::vector<RECT> rects = MonitorRects();

    // 先抓屏再建窗口，否则后面几块屏会把自己刚盖上去的黑窗抓进去。
    const bool snapshot = app::cfg.BackgroundSnapshot && app::cfg.BackgroundImage.empty();
    std::vector<HBITMAP> shots;
    if (snapshot) {
        for (const RECT& rc : rects) shots.push_back(CaptureScreen(rc));
    }

    for (size_t i = 0; i < rects.size(); ++i) {
        Overlay* w = new Overlay();
        if (w->Create(rects[i], kClass)) {
            if (snapshot && i < shots.size() && shots[i]) {
                w->view.SetSnapshot(shots[i]);
                shots[i] = nullptr;
                app::Log(L"snapshot: attached %dx%d", (int)(rects[i].right - rects[i].left),
                         (int)(rects[i].bottom - rects[i].top));
            }
            g_windows.push_back(w);
        } else {
            delete w;
        }
    }
    for (HBITMAP b : shots) if (b) DeleteObject(b);
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
        w->view.message = text;
        w->view.error = true;
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
}

// 分辨率变了、插拔了屏：把窗口重新铺到每块屏上，多出来的屏补窗口、少了的收掉。
void LockScreensLayoutChanged() {
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (now - last < 500) return;   // 每块屏都会收到 WM_DISPLAYCHANGE
    last = now;

    const std::vector<RECT> rects = MonitorRects();
    if (rects.empty()) return;
    while (g_windows.size() > rects.size()) {
        Overlay* w = g_windows.back();
        w->Destroy();
        delete w;
        g_windows.pop_back();
    }
    for (size_t i = 0; i < g_windows.size(); ++i) g_windows[i]->SetBounds(rects[i]);
    for (size_t i = g_windows.size(); i < rects.size(); ++i) {
        Overlay* w = new Overlay();
        if (w->Create(rects[i], kClass)) g_windows.push_back(w); else delete w;
    }
    for (Overlay* w : g_windows) {
        w->view.theme = themes::Find(app::cfg.ThemeId);
        w->view.Reload(w->dc);
        InvalidateRect(w->hwnd, nullptr, FALSE);
    }
    app::Log(L"layout changed, screens=%d", (int)g_windows.size());
}
