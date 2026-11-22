#pragma once
#include "theme.h"
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>

// 锁屏内容的一次绘制。锁屏覆盖层和设置窗里的预览共用同一份代码，两边才不会画得不一样。
namespace lockview {

class View {
public:
    LockTheme theme;
    std::wstring password;
    // 忘了密码时切到恢复码面板，两个输入框各自留一份文字。
    std::wstring recovery;
    bool recovering = false;
    bool inputEnabled = true;
    std::wstring message;
    bool error = false;
    bool caretOn = true;

    ~View();

    // 主题、字体设置或者背景图变了之后调用；dc 为 null 时只重建字体。
    void Reload(ID2D1DeviceContext* dc);
    // 设备丢了（D2DERR_RECREATE_TARGET）时清掉跟设备绑定的位图。
    void DropDeviceResources();

    // 把整块锁屏画在 (0,0)-(w,h) 这个坐标里，调用方自己决定怎么变换和裁切。
    void Draw(ID2D1DeviceContext* dc, float w, float h);

    bool Ready() const { return clockFmt != nullptr; }

    // 上一次画出来的两处链接位置（屏幕坐标），给点击命中用。
    D2D1_RECT_F forgotRect = {};
    D2D1_RECT_F backRect = {};
    D2D1_RECT_F buttonRect = {};

    // 勾了「用屏幕截图做模糊背景」时，锁之前抓下来的那块屏；画完不负责释放。
    void SetSnapshot(HBITMAP bmp);
    // 设置页的预览用：没挑背景图时拿当前桌面壁纸顶替，真锁屏才有截图可抓。
    std::wstring imageOverride;

private:
    HBITMAP snapshot = nullptr;
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
    IDWriteTextFormat* hintFmt = nullptr;

    void ReleaseFormats();
    void CreateFormats();
    void DrawField(ID2D1DeviceContext* dc, const D2D1_RECT_F& box, const std::wstring& text, const wchar_t* hint,
                   bool enabled);
    void DrawButton(ID2D1DeviceContext* dc, const D2D1_RECT_F& box, const wchar_t* label);
    void LoadWallpaper(ID2D1DeviceContext* dc);
    float BlurRadius() const;
    void EnsureBlur(ID2D1DeviceContext* dc, float sigma);
};

}  // namespace lockview
