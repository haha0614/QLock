#pragma once

// 界面尺寸和颜色的唯一来源。设置窗、字体弹窗、锁屏的默认值都从这里取。
// 间距优先取 4 的倍数；标注了「栅格外」的是 0.3.0 就有的尺寸（控件高 34、导航项间距 2），
// 不要再被别处随手复制成新字面量。
namespace design {

// ---- 间距 ----
const float Gap4 = 4;
const float Gap8 = 8;
const float Gap12 = 12;
const float Gap16 = 16;
const float Gap20 = 20;

// ---- 设置窗骨架 ----
const float PanePad = 28;        // 标题栏、内容区、页脚共用的左右内边距
const float NavWidth = 192;
const float NavItemH = 34;       // 栅格外
const float NavItemGap = 2;      // 栅格外
const float ControlH = 34;       // 栅格外：输入框和按钮同高
const float ChipH = 30;          // 栅格外
const float RowH = 58;           // 栅格外：34 + 24
const float FooterH = 56;
const float PreviewH = 226;      // 532 宽 × 408/960，预览画布正好铺满内容区
const float WinW = 780;
const float WinH = 640;
const float TitleTop = 18;
const float TitleH = 32;
const float PaneTop = 62;        // 标题区下沿，各页内容的起点

// ---- 圆角 ----
const float RadiusControl = 8;   // 输入框、按钮、导航项
const float RadiusCard = 10;     // 卡片、预览框
const float RadiusDialog = 12;   // 字体弹窗
const float RadiusRow = 6;       // 字体列表行
const float RadiusTrack = 2;     // 开关轨道、滚动条

// ---- 字号 ----
const float FontSmall = 12;
const float FontBody = 13.5f;
const float FontControl = 14;
const float FontBrand = 17;
const float FontTitle = 19;

// ---- 输入框（设置窗和锁屏共用同一组几何，改动会同时生效）----
const float FieldInset = 13;     // 文字和圆点距框左，0.3.0 量出来的值
const float FieldCaretW = 1.8f;
const float FieldCaretH = 16;
const float FieldCaretLift = 2;  // 空框时笔尖比文字起点再靠左的距离
const float DotStep = 0.67f;     // 圆点步进，相对字号
const float DotRadius = 0.23f;   // 圆点半径，相对字号

// ---- 颜色 ----
const unsigned Accent = 0xFF3A76E8;
const unsigned AccentHover = 0xFF4E8AFA;
const unsigned AccentSoft = 0x2E6096FA;
const unsigned Surface = 0xFF181B23;
const unsigned SurfaceNav = 0xFF12151C;
const unsigned SurfaceFooter = 0xFF14171E;
const unsigned Line = 0x1EFFFFFF;
const unsigned RowLine = 0x18FFFFFF;
const unsigned InputBg = 0x1EFFFFFF;
const unsigned InputBorder = 0x50FFFFFF;
const unsigned ChipSurface = 0x23FFFFFF;
const unsigned ChipBorder = 0x5AFFFFFF;
const unsigned Hover = 0x14FFFFFF;
const unsigned RowHover = 0x0EFFFFFF;
const unsigned SwitchTrack = 0x32FFFFFF;
const unsigned SwitchKnob = 0xFFFFFFFF;
const unsigned SwitchTrackOff = 0xFFC8CDD8;
const unsigned Text = 0xFFF2F4F8;
const unsigned TextDim = 0xFFB4BAC8;
const unsigned TextMuted = 0xFF8A90A0;
const unsigned Error = 0xFFFF8A8A;
const unsigned Scrim = 0xB4000000;

}  // namespace design
