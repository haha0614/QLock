using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace QLock
{
    // Border 的圆角只能是正圆弧：直边曲率 0，拐角一下跳到 1/r，交界处看得见"起折"。
    // 这里把每个角换成一段超椭圆（|x|^n + |y|^n = r^n，n = 4），曲率从直边的 0
    // 连续长到拐角中点的最大值，也就是连续曲率圆角（squircle）。
    // 半径等于短边一半时（开关、药丸）退化成胶囊，和原来的正圆弧一致。
    public sealed class Squircle : Border
    {
        const int Steps = 20;          // 每个角的采样段数，r=14 时偏差约 0.01px
        const double Exponent = 4.0;   // 超椭圆指数，越大越方

        Geometry shape;
        Geometry clipShape;
        Size built;
        double builtRadius = -1;
        double builtInset = -1;

        protected override void OnRender(DrawingContext dc)
        {
            double radius = CornerRadius.TopLeft;
            if (!Uniform() || radius <= 0)
            {
                base.OnRender(dc);
                return;
            }
            if (RenderSize.Width <= 0 || RenderSize.Height <= 0) return;

            Build(RenderSize, radius);
            Brush fill = Background;
            if (fill != null) dc.DrawGeometry(fill, null, shape);
            Brush line = BorderBrush;
            double width = BorderWidth();
            if (width > 0 && line != null)
            {
                Pen pen = new Pen(line, width);
                pen.Freeze();
                dc.DrawGeometry(null, pen, shape);
            }
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            Size size = base.ArrangeOverride(finalSize);
            if (Uniform() && CornerRadius.TopLeft > 0 && size.Width > 0 && size.Height > 0)
            {
                Build(size, CornerRadius.TopLeft);
                Clip = clipShape;
            }
            else
            {
                Clip = null;
            }
            return size;
        }

        double BorderWidth()
        {
            Thickness t = BorderThickness;
            return t.Left;
        }

        bool Uniform()
        {
            Thickness t = BorderThickness;
            return t.Left == t.Top && t.Left == t.Right && t.Left == t.Bottom;
        }

        void Build(Size size, double radius)
        {
            double inset = BorderWidth() / 2;
            if (size == built && radius == builtRadius && inset == builtInset) return;
            built = size;
            builtRadius = radius;
            builtInset = inset;
            shape = Outline(size, inset, Math.Max(0.0, radius - inset));
            clipShape = Outline(size, 0, radius);
        }

        // 顺时针：上边 → 右上角 → 右边 → 右下角 → 下边 → 左下角 → 左边 → 左上角
        static Geometry Outline(Size size, double inset, double radius)
        {
            Rect box = new Rect(inset, inset,
                Math.Max(0.0, size.Width - inset * 2),
                Math.Max(0.0, size.Height - inset * 2));
            double r = Math.Min(radius, Math.Min(box.Width, box.Height) / 2);
            StreamGeometry g = new StreamGeometry();
            using (StreamGeometryContext ctx = g.Open())
            {
                if (r <= 0.25)
                {
                    ctx.BeginFigure(box.TopLeft, true, true);
                    ctx.LineTo(box.TopRight, true, false);
                    ctx.LineTo(box.BottomRight, true, false);
                    ctx.LineTo(box.BottomLeft, true, false);
                }
                else if (r >= Math.Min(box.Width, box.Height) / 2 - 0.01)
                {
                    Point start = new Point(box.Left + r, box.Top);
                    ctx.BeginFigure(start, true, true);
                    ctx.LineTo(new Point(box.Right - r, box.Top), true, false);
                    ctx.ArcTo(new Point(box.Right, box.Top + r), new Size(r, r), 0, false, SweepDirection.Clockwise, true, false);
                    ctx.LineTo(new Point(box.Right, box.Bottom - r), true, false);
                    ctx.ArcTo(new Point(box.Right - r, box.Bottom), new Size(r, r), 0, false, SweepDirection.Clockwise, true, false);
                    ctx.LineTo(new Point(box.Left + r, box.Bottom), true, false);
                    ctx.ArcTo(new Point(box.Left, box.Bottom - r), new Size(r, r), 0, false, SweepDirection.Clockwise, true, false);
                    ctx.LineTo(new Point(box.Left, box.Top + r), true, false);
                    ctx.ArcTo(start, new Size(r, r), 0, false, SweepDirection.Clockwise, true, false);
                }
                else
                {
                    ctx.BeginFigure(new Point(box.Left + r, box.Top), true, true);
                    ctx.LineTo(new Point(box.Right - r, box.Top), true, false);
                    Corner(ctx, box, r, 0);
                    ctx.LineTo(new Point(box.Right, box.Bottom - r), true, false);
                    Corner(ctx, box, r, 1);
                    ctx.LineTo(new Point(box.Left + r, box.Bottom), true, false);
                    Corner(ctx, box, r, 2);
                    ctx.LineTo(new Point(box.Left, box.Top + r), true, false);
                    Corner(ctx, box, r, 3);
                }
            }
            g.Freeze();
            return g;
        }

        // 0 右上、1 右下、2 左下、3 左上，每段从上一个直线端点接到下一个
        static void Corner(StreamGeometryContext ctx, Rect box, double r, int corner)
        {
            double e = 2.0 / Exponent;
            double cx = corner == 0 || corner == 1 ? box.Right - r : box.Left + r;
            double cy = corner == 0 || corner == 3 ? box.Top + r : box.Bottom - r;
            PointCollection points = new PointCollection(Steps);
            for (int i = 1; i <= Steps; i++)
            {
                double a = Math.PI / 2 * i / Steps;
                double u = r * Math.Pow(Math.Cos(a), e);
                double v = r * Math.Pow(Math.Sin(a), e);
                double x, y;
                if (corner == 0) { x = cx + v; y = cy - u; }
                else if (corner == 1) { x = cx + u; y = cy + v; }
                else if (corner == 2) { x = cx - v; y = cy + u; }
                else { x = cx - u; y = cy - v; }
                points.Add(new Point(x, y));
            }
            ctx.PolyLineTo(points, true, false);
        }
    }
}