#pragma once
#include "lock_view.h"
#include "gfx.h"
#include "design.h"
#include "qlock.h"
#include <d2d1effects.h>
#include <algorithm>

namespace lockview {
namespace {

template <class T>
void Release(T** p) {
    if (*p) { (*p)->Release(); *p = nullptr; }
}

unsigned CfgColor(unsigned fallback) { return fallback; }

}  // namespace

View::~View() {
    Release(&blur);
    Release(&wallpaper);
    ReleaseFormats();
    if (snapshot) { DeleteObject(snapshot); snapshot = nullptr; }
}

void View::SetSnapshot(HBITMAP bmp) {
    if (snapshot) DeleteObject(snapshot);
    snapshot = bmp;
    Release(&wallpaper);
}

void View::ReleaseFormats() {
    Release(&clockFmt);
    Release(&dateFmt);
    Release(&brandFmt);
    Release(&subFmt);
    Release(&editFmt);
    Release(&btnFmt);
    Release(&linkFmt);
    Release(&hintFmt);
    Release(&msgFmt);
}

void View::CreateFormats() {
    ReleaseFormats();
    std::wstring ui = app::UiFontName();
    std::wstring clock = app::ClockFontName();
    clockFmt = gfx::Format(clock, (float)theme.Clock.Size, theme.Clock.Weight, DWRITE_TEXT_ALIGNMENT_CENTER);
    dateFmt = gfx::Format(ui, (float)theme.Date.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    brandFmt = gfx::Format(ui, (float)theme.Brand.Size, 600, DWRITE_TEXT_ALIGNMENT_CENTER);
    subFmt = gfx::Format(ui, (float)theme.Subtitle.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    editFmt = gfx::Format(ui, (float)theme.Input.Size, 400, DWRITE_TEXT_ALIGNMENT_LEADING);
    btnFmt = gfx::Format(ui, (float)theme.Button.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    linkFmt = gfx::Format(ui, (float)theme.Link.Size, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    msgFmt = gfx::Format(ui, 12.0f, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
    float hintSize = (float)(std::max)(10, (int)theme.Input.Size - 3);
    hintFmt = gfx::Format(ui, hintSize, 400, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void View::DropDeviceResources() {
    Release(&blur);
    Release(&wallpaper);
}

void View::Reload(ID2D1DeviceContext* dc) {
    CreateFormats();
    DropDeviceResources();
    if (dc) LoadWallpaper(dc);
}

void View::LoadWallpaper(ID2D1DeviceContext* dc) {
    if (wallpaper || !dc) return;
    const std::wstring& path = imageOverride.empty() ? app::cfg.BackgroundImage : imageOverride;
    if (path.empty()) {
        BITMAP bm = {};
        if (snapshot && GetObjectW(snapshot, sizeof(bm), &bm) && bm.bmBits && bm.bmWidth > 0 && bm.bmHeight > 0) {
            D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
            if (FAILED(dc->CreateBitmap(D2D1::SizeU(bm.bmWidth, bm.bmHeight), bm.bmBits, (UINT32)bm.bmWidthBytes,
                                       props, &wallpaper))) {
                app::Log(L"snapshot: create bitmap failed");
            }
        }
        return;
    }
    if (!gfx::WIC()) return;
    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(gfx::WIC()->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                     WICDecodeMetadataCacheOnLoad, &decoder))) {
        app::Log(L"wallpaper: cannot open %s", path.c_str());
        return;
    }
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    if (SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(gfx::WIC()->CreateFormatConverter(&conv)) &&
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

float View::BlurRadius() const {
    return app::cfg.BlurRadius >= 0 ? (float)app::cfg.BlurRadius : (float)theme.Background.Blur;
}

void View::EnsureBlur(ID2D1DeviceContext* dc, float sigma) {
    if (!wallpaper || sigma <= 0.4f) return;
    if (!blur && FAILED(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blur))) {
        app::Log(L"blur: create effect failed");
        return;
    }
    blur->SetInput(0, wallpaper);
    blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sigma);
    blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
}

void View::Draw(ID2D1DeviceContext* dc, float w, float h) {
    if (!dc || w <= 1 || h <= 1) return;
    LoadWallpaper(dc);

    // 调用方可能已经设了缩放（设置页的预览就是），画完要还原回去。
    D2D1_MATRIX_3X2_F saved;
    dc->GetTransform(&saved);

    const bool card = theme.Card.Show;
    const bool showClock = app::cfg.ShowClock && theme.Clock.Show;
    const bool showSub = app::cfg.ShowSubtitle && theme.Subtitle.Show;
    const bool inlineRow = theme.Inline();

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
    float hintH = recovering ? (float)(std::max)(10, (int)theme.Input.Size - 3) * 1.5f + 16.0f : 0.0f;

    float btnW = inlineRow ? (theme.Button.Width > 0 ? (float)theme.Button.Width : 96.0f) : innerW;
    float rowH = inlineRow ? inputH : 0.0f;

    float fieldH = recovering ? hintH + inputH + btnGap + btnH : (inlineRow ? rowH : inputH + btnGap + btnH);
    float bodyH = brandH + subH + fieldH + linkGap + linkH + msgH;
    float cardH = card ? bodyH + (float)theme.Card.PadV * 2 : bodyH;
    float total = clockH + dateH + dateGap + cardH;
    float left = (w - colW) / 2;
    float top = (h - total) / 2;

    // 纯色背景：设置里填了颜色就用它，没填沿用主题自带底色。
    D2D1_COLOR_F bgColor = app::cfg.BackgroundColor.empty()
                               ? themes::Color(theme.Background.Color, gfx::Color(0xFF080A10))
                               : themes::Color(app::cfg.BackgroundColor, gfx::Color(0xFF080A10));
    gfx::FillRect(dc, D2D1::RectF(0, 0, w, h), bgColor);

    if (wallpaper) {
        D2D1_SIZE_F img = wallpaper->GetSize();
        float s = (std::max)(w / img.width, h / img.height);
        float dw = img.width * s, dh = img.height * s;
        D2D1_RECT_F dest = D2D1::RectF((w - dw) / 2, (h - dh) / 2, (w + dw) / 2, (h + dh) / 2);
        dc->PushAxisAlignedClip(D2D1::RectF(0, 0, w, h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        float radius = BlurRadius();
        // 模糊半径是屏幕上的像素，效果作用在原图坐标里，先换算回原图尺度。
        EnsureBlur(dc, radius > 0 ? radius * 0.5f / (s > 0 ? s : 1.0f) : 0.0f);
        if (blur && radius > 0) {
            float sx = (dest.right - dest.left) / img.width;
            float sy = (dest.bottom - dest.top) / img.height;
            dc->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy) *
                             D2D1::Matrix3x2F::Translation(dest.left, dest.top));
            dc->DrawImage(blur, D2D1::Point2F(0, 0), D2D1::RectF(0, 0, img.width, img.height));
            dc->SetTransform(&saved);
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
        gfx::FillRect(dc, D2D1::RectF(0, 0, w, h), dim);
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    if (showClock) {
        wchar_t timeText[32] = {};
        _snwprintf_s(timeText, _countof(timeText), _TRUNCATE, L"%02d:%02d", st.wHour, st.wMinute);
        gfx::Text(dc, clockFmt, timeText, D2D1::RectF(left, top, left + colW, top + clockH),
                  themes::Color(theme.Clock.Color, gfx::Color(0xFFFFFFFF)));
        top += clockH;

        static const wchar_t* week[7] = { L"星期日", L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六" };
        int idx = st.wDayOfWeek;
        if (idx < 0 || idx > 6) idx = 0;
        wchar_t dateText[96] = {};
        _snwprintf_s(dateText, _countof(dateText), _TRUNCATE, L"%d年%d月%d日 %s", st.wYear, st.wMonth, st.wDay, week[idx]);
        gfx::Text(dc, dateFmt, dateText, D2D1::RectF(left, top + 2.0f, left + colW, top + dateH),
                  themes::Color(theme.Date.Color, gfx::Color(0xFFC9C9C9)));
        top += dateH + dateGap;
    }

    if (card) {
        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(D2D1::RectF(left, top, left + colW, top + cardH),
                                                  gfx::Radius((float)theme.Card.Radius, 0),
                                                  gfx::Radius((float)theme.Card.Radius, 0));
        gfx::FillRound(dc, box, themes::Color(theme.Card.Bg, gfx::Color(0xD81C1C26)));
        gfx::StrokeRound(dc, box, themes::Color(theme.Card.Border, gfx::Color(0x37FFFFFF)), 1.0f);
        top += (float)theme.Card.PadV;
        left += (float)theme.Card.PadH;
    }

    if (theme.Brand.Show) {
        // 卡片里没有提示文字时，字标在「卡片上边缘 → 输入框」这段里垂直居中，
        // 否则它会贴着卡片顶、离输入框太近。
        const bool spread = card && !showSub;
        float brandTop = spread ? top - (float)theme.Card.PadV : top;
        float brandBox = spread ? brandH + (float)theme.Card.PadV : brandH;
        gfx::Text(dc, brandFmt, theme.Brand.Text, D2D1::RectF(left, brandTop, left + innerW, brandTop + brandBox),
                  themes::Color(theme.Brand.Color, gfx::Color(0xFF78A0E6)));
        top += brandH;
    }
    if (showSub) {
        gfx::Text(dc, subFmt, app::cfg.Subtitle, D2D1::RectF(left, top, left + innerW, top + subH),
                  themes::Color(theme.Subtitle.Color, gfx::Color(0xFFE8EEF7)));
        top += subH;
    }

    float inputW = inlineRow ? innerW - btnW - 8.0f : innerW;
    if (inputW < 80) inputW = 80;
    forgotRect = D2D1::RectF(0, 0, 0, 0);
    backRect = D2D1::RectF(0, 0, 0, 0);
    buttonRect = D2D1::RectF(0, 0, 0, 0);
    D2D1_RECT_F btnBox = D2D1::RectF(0, 0, 0, 0);

    if (recovering) {
        gfx::Text(dc, hintFmt, L"输入 10 位恢复码（形如 XXXXX-XXXXX）",
                  D2D1::RectF(left, top, left + innerW, top + hintH - 8.0f),
                  themes::Color(theme.Input.Hint, gfx::Color(0xFF7B8391)));
        top += hintH;
        DrawField(dc, D2D1::RectF(left, top, left + inputW, top + inputH), recovery, L"恢复码", inputEnabled);
        top += inputH + btnGap;
        btnBox = D2D1::RectF(left, top, left + btnW, top + btnH);
        top += btnH;
        DrawButton(dc, btnBox, L"用恢复码解锁");
        buttonRect = btnBox;
        top += linkGap;
        backRect = D2D1::RectF(left, top, left + innerW, top + linkH);
        gfx::Text(dc, linkFmt, L"返回密码输入", backRect, themes::Color(theme.Link.Color, gfx::Color(0xFF8CB4F0)));
        top += linkH;
    } else {
        D2D1_RECT_F inputBox = D2D1::RectF(left, top, left + inputW, top + inputH);
        DrawField(dc, inputBox, password, L"锁屏密码", inputEnabled);
        if (inlineRow) {
            float bx = left + inputW + 8.0f;
            float by = top + (inputH - btnH) / 2;
            btnBox = D2D1::RectF(bx, by, bx + btnW, by + btnH);
            top += rowH;
        } else {
            top += inputH + btnGap;
            btnBox = D2D1::RectF(left, top, left + btnW, top + btnH);
            top += btnH;
        }
        DrawButton(dc, btnBox, L"解锁");
        buttonRect = btnBox;
        top += linkGap;
        forgotRect = D2D1::RectF(left, top, left + innerW, top + linkH);
        gfx::Text(dc, linkFmt, L"忘记密码？用恢复码解锁", forgotRect,
                  themes::Color(theme.Link.Color, gfx::Color(0xFF8CB4F0)));
        top += linkH;
    }

    if (!message.empty()) {
        gfx::Text(dc, msgFmt, message, D2D1::RectF(left, top + 10.0f, left + innerW, top + 10.0f + 18.0f),
                  error ? gfx::Color(0xFFFF7D7D) : gfx::Color(0xFF82D28C));
    }

    dc->SetTransform(&saved);
}

void View::DrawField(ID2D1DeviceContext* dc, const D2D1_RECT_F& box, const std::wstring& text, const wchar_t* hint,
                     bool enabled) {
    // 几何全在 design.h，这里只把主题的颜色和半径填进同一个输入框画法。
    gfx::FieldStyle st;
    st.underline = _wcsicmp(theme.Input.Kind.c_str(), L"underline") == 0;
    st.radius = (float)theme.Input.Radius;
    st.inset = design::FieldInset;
    st.caretW = design::FieldCaretW;
    st.caretH = design::FieldCaretH;
    st.caretLift = design::FieldCaretLift;
    st.dotStep = design::DotStep;
    st.dotRadius = design::DotRadius;
    st.dots = true;
    st.bg = themes::Color(theme.Input.Bg, gfx::Color(0xFF1E2129));
    st.border = themes::Color(theme.Input.Border, gfx::Color(st.underline ? 0xFF2A2F38 : 0xFF4A4F5A));
    st.fg = themes::Color(theme.Input.Fg, gfx::Color(0xFFFFFFFF));
    st.hint = themes::Color(theme.Input.Hint, gfx::Color(0xFF7B8391));
    gfx::Field(dc, editFmt, (float)theme.Input.Size, box, text, hint, st, enabled, caretOn);
}

void View::DrawButton(ID2D1DeviceContext* dc, const D2D1_RECT_F& box, const wchar_t* label) {
    const bool outline = _wcsicmp(theme.Button.Kind.c_str(), L"outline") == 0;
    const bool flat = _wcsicmp(theme.Button.Kind.c_str(), L"text") == 0;
    float radius = gfx::Radius((float)theme.Button.Radius, box.bottom - box.top);
    if (!flat) {
        gfx::FillRound(dc, D2D1::RoundedRect(box, radius, radius),
                       themes::Color(theme.Button.Bg, gfx::Color(0xFF3A76E8)));
    }
    if (outline) {
        gfx::StrokeRound(dc, D2D1::RoundedRect(box, radius, radius),
                         themes::Color(theme.Button.Border, gfx::Color(0x50FFFFFF)), 1.0f);
    }
    gfx::Text(dc, btnFmt, label, box, themes::Color(theme.Button.Fg, gfx::Color(0xFFFFFFFF)));
}

}  // namespace lockview
