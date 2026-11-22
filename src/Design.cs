namespace QLock
{
    // 界面尺寸的唯一来源：间距只取 4 的倍数，控件高度、圆角、字号都在这里，窗口代码不再写字面量。
    // 颜色放在 Ui（TextPrimary、Accent、Surface 这些），两边一起改才算一套设计 token。
    public static class Design
    {
        public const double Gap4 = 4;
        public const double Gap8 = 8;
        public const double Gap12 = 12;
        public const double Gap16 = 16;
        public const double Gap20 = 20;

        // 标题栏、滚动区、页脚共用这个左右内边距，右缘才对得齐
        public const double PanePad = 28;

        public const double ControlHeight = 34;   // 输入框和按钮同高
        public const double ChipHeight = 30;      // 药丸按钮
        public const double NavWidth = 192;
        public const double NavItemHeight = 34;

        public const double RadiusControl = 8;    // 输入框、按钮、导航项
        public const double RadiusCard = 10;      // 卡片、预览框

        public const double FontTiny = 11.5;
        public const double FontSmall = 12;
        public const double FontBody = 13.5;
        public const double FontControl = 14;
        public const double FontBrand = 17;
        public const double FontTitle = 19;
    }
}
