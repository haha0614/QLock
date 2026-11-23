#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>

// 全局的 D2D/DWrite/WIC 工厂和一批画图小工具。锁屏覆盖层和设置窗都从这里拿设备。
namespace gfx {

bool Init();
ID2D1Factory1* D2D();
ID2D1Device* Device();
ID3D11Device* D3D();
IDWriteFactory* DW();
IWICImagingFactory* WIC();

// 0xAARRGGBB。
D2D1_COLOR_F Color(unsigned argb);

// 圆角超过控件半个高时截住，否则两边的弧会互相吃进去。
float Radius(float wanted, float height);

void FillRect(ID2D1DeviceContext* dc, const D2D1_RECT_F& r, D2D1_COLOR_F c);
void FillRound(ID2D1DeviceContext* dc, const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F c);
void StrokeRound(ID2D1DeviceContext* dc, const D2D1_ROUNDED_RECT& r, D2D1_COLOR_F c, float width);

// 单行文字，按 rect 的左上角开始排，垂直居中。
void Text(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, const std::wstring& s,
          const D2D1_RECT_F& box, D2D1_COLOR_F c);
// 允许换行的版本：窄窗口或高 DPI 下长句子要能折行，不然右端会被裁掉。
void TextWrapped(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, const std::wstring& s,
                 const D2D1_RECT_F& box, D2D1_COLOR_F c);
float Measure(IDWriteTextFormat* fmt, const std::wstring& s);

// 字体名留空或系统里没有时退回微软雅黑 UI。
IDWriteTextFormat* Format(const std::wstring& family, float size, int weight, DWRITE_TEXT_ALIGNMENT align);

// 输入框的画法：底、边、提示、密码圆点、光标都按同一套几何算。
// 设置窗和锁屏都用它，改一处两边同时变。
struct FieldStyle {
    float radius = 8.0f;
    float inset = 13.0f;      // 文字/圆点距框左；下划线样式时内部按 0 处理
    float caretW = 1.8f;
    float caretH = 16.0f;
    float caretLift = 2.0f;   // 空框时笔尖比文字起点再靠左的距离
    float dotStep = 0.67f;    // 圆点步进，相对字号
    float dotRadius = 0.23f;  // 圆点半径，相对字号
    bool underline = false;
    bool dots = false;        // 密码框：不显示字符，画圆点
    bool caret = true;
    D2D1_COLOR_F bg = {};
    D2D1_COLOR_F border = {};
    D2D1_COLOR_F fg = {};
    D2D1_COLOR_F hint = {};
};

void Field(ID2D1DeviceContext* dc, IDWriteTextFormat* fmt, float fontSize, const D2D1_RECT_F& box,
           const std::wstring& text, const wchar_t* hint, const FieldStyle& st, bool enabled, bool caretOn);

}  // namespace gfx
