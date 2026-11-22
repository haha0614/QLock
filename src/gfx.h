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
float Measure(IDWriteTextFormat* fmt, const std::wstring& s);

// 字体名留空或系统里没有时退回微软雅黑 UI。
IDWriteTextFormat* Format(const std::wstring& family, float size, int weight, DWRITE_TEXT_ALIGNMENT align);

}  // namespace gfx
