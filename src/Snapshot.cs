using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Media.Imaging;

namespace QLock
{
    public static class Snapshot
    {
        public static BitmapSource Capture(Rectangle bounds, double scale)
        {
            try
            {
                int w = Math.Max(1, bounds.Width);
                int h = Math.Max(1, bounds.Height);
                using (Bitmap full = new Bitmap(w, h, PixelFormat.Format32bppArgb))
                {
                    using (Graphics g = Graphics.FromImage(full))
                        g.CopyFromScreen(bounds.X, bounds.Y, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);

                    int sw = Math.Max(1, (int)(w * scale));
                    int sh = Math.Max(1, (int)(h * scale));
                    using (Bitmap small = new Bitmap(sw, sh, PixelFormat.Format32bppArgb))
                    {
                        using (Graphics g2 = Graphics.FromImage(small))
                        {
                            g2.InterpolationMode = InterpolationMode.HighQualityBilinear;
                            g2.PixelOffsetMode = PixelOffsetMode.HighQuality;
                            g2.DrawImage(full, new Rectangle(0, 0, sw, sh));
                        }
                        return ToSource(small);
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("snapshot failed: " + ex.Message);
                return null;
            }
        }

        static BitmapSource ToSource(Bitmap bmp)
        {
            IntPtr hbmp = bmp.GetHbitmap();
            try
            {
                BitmapSource src = System.Windows.Interop.Imaging.CreateBitmapSourceFromHBitmap(
                    hbmp, IntPtr.Zero, System.Windows.Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
                src.Freeze();
                return src;
            }
            finally
            {
                DeleteObject(hbmp);
            }
        }

        [System.Runtime.InteropServices.DllImport("gdi32.dll")]
        static extern bool DeleteObject(IntPtr hObject);
    }
}
