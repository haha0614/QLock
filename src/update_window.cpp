#include "update_window.h"

#include <d2d1_1.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <thread>

#include "design.h"
#include "gfx.h"
#include "qlock.h"
#include "updater.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

const wchar_t* kClass = L"QLockUpdate";
const float WinW = 560.0f;
const float WinH = 440.0f;
const float Pad = 24.0f;
const float BtnW = 112.0f;
const float BtnSmallW = 96.0f;
const float SkipW = 120.0f;

const UINT WM_APP_PROGRESS = WM_APP + 1;
const UINT WM_APP_DONE = WM_APP + 2;

const int ID_SKIP = 1;
const int ID_LATER = 2;
const int ID_DOWNLOAD = 3;

float ScaleOf(HMONITOR mon) {
    static HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI * GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
        GetDpiForMonitorFn fn = (GetDpiForMonitorFn)GetProcAddress(shcore, "GetDpiForMonitor");
        UINT dx = 96, dy = 96;
        if (fn && SUCCEEDED(fn(mon, 0 /*MDT_EFFECTIVE_DPI*/, &dx, &dy)) && dx >= 96) return dx / 96.0f;
    }
    HDC screen = GetDC(nullptr);
    UINT dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) ReleaseDC(nullptr, screen);
    return dpi >= 96 ? dpi / 96.0f : 1.0f;
}

// 发布说明是 GitHub 上的 markdown：去掉标题的 # 和列表的 -，直接当纯文本排，
// 比把符号原样显示出来干净。
std::wstring CleanLine(std::wstring line) {
    size_t i = 0;
    while (i < line.size() && line[i] == L'#') ++i;
    if (i > 0) {
        while (i < line.size() && line[i] == L' ') ++i;
        line = line.substr(i);
    }
    if (line.rfind(L"- ", 0) == 0) line = L"• " + line.substr(2);
    // 行内的反引号包住的命令名，直接去掉引号，读起来更像一句话。
    line.erase(std::remove(line.begin(), line.end(), L'`'), line.end());
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\t')) line.pop_back();
    return line;
}

std::wstring Normalize(const std::wstring& text) {
    std::wstring flat;
    flat.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') continue;
            flat.push_back(L'\n');
        } else {
            flat.push_back(text[i]);
        }
    }
    std::wstring out;
    size_t start = 0;
    while (start <= flat.size()) {
        size_t end = flat.find(L'\n', start);
        if (end == std::wstring::npos) end = flat.size();
        out += CleanLine(flat.substr(start, end - start));
        out.push_back(L'\n');
        start = end + 1;
        if (end == flat.size()) break;
    }
    while (!out.empty() && (out.back() == L'\n' || out.back() == L' ')) out.pop_back();
    return out;
}

class UpdateDlg {
public:
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1Bitmap1* target = nullptr;
    float scale = 1.0f;
    float winH = WinH;

    std::wstring tag;
    std::wstring notes;
    std::wstring status;
    bool statusError = false;
    bool downloading = false;
    bool installed = false;
    int percent = 0;
    float scroll = 0, scrollMax = 0;
    int hot = -1, pressed = -1;

    D2D1_RECT_F skipRect = {}, laterRect = {}, dlRect = {};

    IDWriteTextFormat* fTitle = nullptr;
    IDWriteTextFormat* fBody = nullptr;
    IDWriteTextFormat* fSmall = nullptr;
    IDWriteTextFormat* fControl = nullptr;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    static void ReleaseFmt(IDWriteTextFormat** fmt) {
        if (*fmt) { (*fmt)->Release(); *fmt = nullptr; }
    }

    void MakeFormats() {
        ReleaseFmt(&fTitle);
        ReleaseFmt(&fBody);
        ReleaseFmt(&fSmall);
        ReleaseFmt(&fControl);
        std::wstring ui = app::UiFontName();
        fTitle = gfx::Format(ui, design::FontTitle, 700, DWRITE_TEXT_ALIGNMENT_LEADING);
        fBody = gfx::Format(ui, design::FontBody, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        fSmall = gfx::Format(ui, design::FontSmall, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
        fControl = gfx::Format(ui, design::FontControl, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
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
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f * scale,
                96.0f * scale);
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

    void ApplyScale(float s) {
        if (s <= 0) s = 1.0f;
        scale = s;
        if (dc) dc->SetDpi(96.0f * s, 96.0f * s);
        RECT frame = { 0, 0, (LONG)(WinW * s), (LONG)(winH * s) };
        AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
        RECT cur = {};
        GetWindowRect(hwnd, &cur);
        const int ww = frame.right - frame.left, wh = frame.bottom - frame.top;
        SetWindowPos(hwnd, nullptr, cur.left, cur.top, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
        Invalidate();
    }

    void Button(const D2D1_RECT_F& r, const std::wstring& text, bool primary, bool hovered, bool enabled) {
        float rad = gfx::Radius(design::RadiusControl, r.bottom - r.top);
        if (!enabled) {
            gfx::FillRound(dc, D2D1::RoundedRect(r, rad, rad), gfx::Color(design::RowHover));
            gfx::TextCentered(dc, fControl, text, r, gfx::Color(design::TextMuted));
            return;
        }
        unsigned bg = primary ? (hovered ? design::AccentHover : design::Accent)
                              : (hovered ? design::ChipSurface : design::Hover);
        gfx::FillRound(dc, D2D1::RoundedRect(r, rad, rad), gfx::Color(bg));
        if (!primary) gfx::StrokeRound(dc, D2D1::RoundedRect(r, rad, rad), gfx::Color(design::ChipBorder), 1.0f);
        gfx::TextCentered(dc, fControl, text, r, gfx::Color(design::Text));
    }

    static bool Inside(const D2D1_RECT_F& r, float x, float y) {
        return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
    }

    void Paint() {
        if (!EnsureTarget()) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        float w = (rc.right - rc.left) / scale;
        float h = (rc.bottom - rc.top) / scale;
        if (w <= 1 || h <= 1) return;

        dc->BeginDraw();
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        gfx::FillRect(dc, D2D1::RectF(0, 0, w, h), gfx::Color(design::Surface));

        const float left = Pad, right = w - Pad;
        gfx::Text(dc, fTitle, L"有新版本可用", D2D1::RectF(left, Pad, right, Pad + 30), gfx::Color(design::Text));
        const std::wstring verLine = L"最新版本 " + tag + L"，当前版本 " + updater::CurrentVersion();
        gfx::Text(dc, fSmall, verLine, D2D1::RectF(left, Pad + 34, right, Pad + 54), gfx::Color(design::TextDim));

        const float buttonsTop = h - Pad - design::ControlH;
        const float cardTop = Pad + 66, cardBottom = buttonsTop - 34;
        float cardRad = gfx::Radius(design::RadiusCard, cardBottom - cardTop);
        gfx::FillRound(dc, D2D1::RoundedRect(D2D1::RectF(left, cardTop, right, cardBottom), cardRad, cardRad),
                       gfx::Color(design::SurfaceNav));
        gfx::StrokeRound(dc, D2D1::RoundedRect(D2D1::RectF(left, cardTop, right, cardBottom), cardRad, cardRad),
                         gfx::Color(design::Line), 1.0f);

        D2D1_RECT_F box = D2D1::RectF(left + 14, cardTop + 14, right - 14, cardBottom - 14);
        const std::wstring text = notes.empty() ? L"这个版本没有写更新说明。" : notes;
        const float textW = box.right - box.left;
        const float contentH = gfx::MeasureWrapped(fBody, text, textW) + 2;
        scrollMax = (std::max)(0.0f, contentH - (box.bottom - box.top));
        if (scroll > scrollMax) scroll = scrollMax;
        if (scroll < 0) scroll = 0;
        dc->PushAxisAlignedClip(box, D2D1_ANTIALIAS_MODE_ALIASED);
        gfx::TextWrapped(dc, fBody, text,
                         D2D1::RectF(box.left, box.top - scroll, box.right, box.top - scroll + contentH),
                         gfx::Color(design::TextDim));
        dc->PopAxisAlignedClip();
        if (scrollMax > 0) {
            const float trackH = box.bottom - box.top;
            const float thumbH = (std::max)(24.0f, trackH * trackH / (trackH + scrollMax));
            const float thumbTop = box.top + (trackH - thumbH) * (scroll / scrollMax);
            gfx::FillRect(dc, D2D1::RectF(box.right + 6, box.top, box.right + 8, box.bottom),
                          gfx::Color(design::Line));
            gfx::FillRect(dc, D2D1::RectF(box.right + 6, thumbTop, box.right + 8, thumbTop + thumbH),
                          gfx::Color(design::ChipBorder));
        }

        gfx::Text(dc, fSmall, status, D2D1::RectF(left, cardBottom + 8, right, cardBottom + 28),
                  gfx::Color(statusError ? design::Error : design::TextMuted));

        skipRect = D2D1::RectF(left, buttonsTop, left + SkipW, buttonsTop + design::ControlH);
        dlRect = D2D1::RectF(right - BtnW, buttonsTop, right, buttonsTop + design::ControlH);
        laterRect = D2D1::RectF(dlRect.left - design::Gap8 - BtnSmallW, buttonsTop,
                                dlRect.left - design::Gap8, buttonsTop + design::ControlH);
        Button(skipRect, L"跳过这个版本", false, hot == ID_SKIP, !downloading && !installed);
        Button(laterRect, L"稍后提醒我", false, hot == ID_LATER, true);
        Button(dlRect, installed ? L"正在安装…" : (downloading ? L"下载中…" : L"下载更新"), true,
               hot == ID_DOWNLOAD, !downloading && !installed);

        HRESULT hr = dc->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            if (target) { target->Release(); target = nullptr; }
            return;
        }
        swap->Present(1, 0);
    }

    int HitTest(float x, float y) {
        if (Inside(dlRect, x, y)) return ID_DOWNLOAD;
        if (Inside(laterRect, x, y)) return ID_LATER;
        if (Inside(skipRect, x, y)) return ID_SKIP;
        return 0;
    }

    void StartDownload() {
        if (downloading || installed) return;
        downloading = true;
        percent = 0;
        status = L"正在下载…";
        statusError = false;
        Invalidate();
        HWND target = hwnd;
        std::thread([target]() {
            std::wstring error;
            const std::wstring file = updater::DownloadPath();
            const std::wstring exeUrl = updater::ExeUrl();
            const std::wstring sumsUrl = updater::SumsUrl();
            std::wstring text;
            app::Log(L"update: dialog downloading %s", exeUrl.c_str());
            int lastPercent = -1;
            auto progress = [target, &lastPercent](long long got, long long total) {
                if (total <= 0) return;
                int percent = (int)(got * 100 / total);
                if (percent == lastPercent) return;
                lastPercent = percent;
                PostMessageW(target, WM_APP_PROGRESS, (WPARAM)percent, 0);
            };
            bool ok = false;
            if (!updater::Download(exeUrl, file, progress, &error)) {
                text = L"下载失败：" + error;
            } else {
                PostMessageW(target, WM_APP_PROGRESS, (WPARAM)100, 0);
                std::wstring sumsError;
                std::wstring sums = updater::FetchText(sumsUrl, &sumsError);
                std::wstring expected = updater::ExpectedHash(sums);
                std::wstring actual = updater::Sha256File(file);
                if (expected.empty()) {
                    text = L"下载完成，但没拿到校验值，已放弃这次升级";
                } else if (actual != expected) {
                    text = L"下载的文件和发布页的校验值不一致，已放弃这次升级";
                } else if (!updater::Install(file, &error)) {
                    text = L"升级失败：" + error;
                } else {
                    ok = true;
                    text = L"已经装好，正在重启 QLock…";
                }
            }
            std::wstring* message = new std::wstring(text);
            if (!PostMessageW(target, WM_APP_DONE, (WPARAM)message, ok ? 1 : 0)) delete message;
        }).detach();
    }

    // 说明短就别留一大片空卡片；长到上限后固定住，让说明区自己滚。
    void FitHeight() {
        const std::wstring text = notes.empty() ? L"这个版本没有写更新说明。" : notes;
        const float contentW = WinW - 2 * Pad - 28;
        float cardH = gfx::MeasureWrapped(fBody, text, contentW) + 34;
        if (cardH < 120) cardH = 120;
        if (cardH > 300) cardH = 300;
        winH = 182 + cardH;
    }

    bool Create(HWND ownerWnd, const std::wstring& t, const std::wstring& n) {
        owner = ownerWnd;
        if (!gfx::Init()) return false;
        HMONITOR mon = MonitorFromWindow(ownerWnd && IsWindow(ownerWnd) ? ownerWnd : GetDesktopWindow(),
                                         MONITOR_DEFAULTTONEAREST);
        scale = ScaleOf(mon);
        tag = t;
        notes = Normalize(n);
        MakeFormats();
        FitHeight();

        WNDCLASSEXW existing = { sizeof(existing) };
        if (!GetClassInfoExW(app::inst, kClass, &existing)) {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = UpdateDlg::Proc;
            wc.hInstance = app::inst;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kClass;
            RegisterClassExW(&wc);
        }

        RECT frame = { 0, 0, (LONG)(WinW * scale), (LONG)(winH * scale) };
        AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
        const int ww = frame.right - frame.left, wh = frame.bottom - frame.top;

        MONITORINFO mi = { sizeof(mi) };
        RECT work = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        if (GetMonitorInfoW(mon, &mi)) work = mi.rcWork;
        int sx = work.left + (work.right - work.left - ww) / 2;
        int sy = work.top + (work.bottom - work.top - wh) / 2;
        // 有设置窗就贴着它居中，看起来像它弹出来的。
        if (ownerWnd && IsWindow(ownerWnd) && IsWindowVisible(ownerWnd)) {
            RECT o = {};
            GetWindowRect(ownerWnd, &o);
            sx = o.left + (o.right - o.left - ww) / 2;
            sy = o.top + (o.bottom - o.top - wh) / 2;
        }
        sx = (std::max)((int)work.left, (std::min)(sx, (int)work.right - ww));
        sy = (std::max)((int)work.top, (std::min)(sy, (int)work.bottom - wh));

        hwnd = CreateWindowExW(0, kClass, L"QLock 更新", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, sx, sy, ww, wh,
                               nullptr, nullptr, app::inst, this);
        if (!hwnd) return false;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
        if (!EnsureTarget()) return false;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        app::Log(L"update dialog: tag=%s notes=%zu chars", tag.c_str(), notes.size());
        return true;
    }

    void DestroyAll() {
        for (IDWriteTextFormat** fmt : { &fTitle, &fBody, &fSmall, &fControl }) ReleaseFmt(fmt);
        if (dc) dc->SetTarget(nullptr);
        if (target) { target->Release(); target = nullptr; }
        if (swap) { swap->Release(); swap = nullptr; }
        if (dc) { dc->Release(); dc = nullptr; }
    }
};

UpdateDlg* g_update = nullptr;

LRESULT CALLBACK UpdateDlg::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UpdateDlg* self = (UpdateDlg*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (UpdateDlg*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    if (msg == WM_APP_PROGRESS) {
        self->percent = (int)wp;
        self->status = L"正在下载… " + std::to_wstring(self->percent) + L"%";
        self->Invalidate();
        return 0;
    }
    if (msg == WM_APP_DONE) {
        std::wstring* text = (std::wstring*)wp;
        const bool ok = lp != 0;
        self->downloading = false;
        if (text) {
            self->status = *text;
            self->statusError = !ok;
            delete text;
        }
        if (ok) {
            self->installed = true;
            wchar_t selfPath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
            std::wstring error;
            if (!updater::Restart(selfPath, &error)) {
                self->status = L"已经装好，但" + error;
                self->statusError = true;
            } else {
                HWND main = FindWindowExW(HWND_MESSAGE, nullptr, L"QLockMain", nullptr);
                if (main) PostMessageW(main, WM_CLOSE, 0, 0);
            }
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
    case WM_DPICHANGED:
        self->ApplyScale((float)HIWORD(wp) / 96.0f);
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) self->Resize();
        return 0;
    case WM_MOUSEMOVE: {
        float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
        int id = self->HitTest(x, y);
        if (id != self->hot) {
            self->hot = id;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            self->Invalidate();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (self->hot) { self->hot = -1; self->Invalidate(); }
        return 0;
    case WM_LBUTTONDOWN: {
        float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
        self->pressed = self->HitTest(x, y);
        if (self->pressed) SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        float x = GET_X_LPARAM(lp) / self->scale, y = GET_Y_LPARAM(lp) / self->scale;
        int id = self->HitTest(x, y);
        int pressed = self->pressed;
        self->pressed = 0;
        if (GetCapture() == hwnd) ReleaseCapture();
        if (!id || id != pressed) return 0;
        if (id == ID_SKIP) {
            app::cfg.SkippedVersion = self->tag;
            app::SaveConfig();
            app::Log(L"update: skipped %s", self->tag.c_str());
            DestroyWindow(hwnd);
        } else if (id == ID_LATER) {
            DestroyWindow(hwnd);
        } else if (id == ID_DOWNLOAD) {
            self->StartDownload();
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        self->scroll -= delta / 120.0f * 48.0f;
        if (self->scroll < 0) self->scroll = 0;
        if (self->scroll > self->scrollMax) self->scroll = self->scrollMax;
        self->Invalidate();
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_update == self) g_update = nullptr;
        return 0;
    case WM_NCDESTROY:
        self->DestroyAll();
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        if (g_update == self) g_update = nullptr;
        delete self;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

void UpdateShow(HWND owner, const std::wstring& tag, const std::wstring& notes) {
    if (g_update && g_update->hwnd && IsWindow(g_update->hwnd)) {
        g_update->tag = tag;
        g_update->notes = Normalize(notes);
        g_update->scroll = 0;
        g_update->status.clear();
        g_update->statusError = false;
        g_update->downloading = false;
        g_update->installed = false;
        g_update->FitHeight();
        ShowWindow(g_update->hwnd, SW_SHOW);
        SetForegroundWindow(g_update->hwnd);
        g_update->Invalidate();
        return;
    }
    UpdateDlg* w = new UpdateDlg();
    if (!w->Create(owner, tag, notes)) {
        delete w;
        return;
    }
    g_update = w;
}

bool UpdateOwns(HWND hwnd) { return g_update && g_update->hwnd == hwnd; }