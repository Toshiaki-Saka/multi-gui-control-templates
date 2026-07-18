using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace PidAvalonia.Views;

public sealed class PlotControl : Control
{
    private const double ML = 60, MR = 16, MT = 28, MB = 32;

    public static readonly StyledProperty<double[]?> SimTProperty =
        AvaloniaProperty.Register<PlotControl, double[]?>(nameof(SimT));

    public static readonly StyledProperty<double[]?> SimThetaProperty =
        AvaloniaProperty.Register<PlotControl, double[]?>(nameof(SimTheta));

    public static readonly StyledProperty<double> YMinProperty =
        AvaloniaProperty.Register<PlotControl, double>(nameof(YMin), 0.0);

    public static readonly StyledProperty<double> YMaxProperty =
        AvaloniaProperty.Register<PlotControl, double>(nameof(YMax), 1.0);

    public static readonly StyledProperty<double> XMaxProperty =
        AvaloniaProperty.Register<PlotControl, double>(nameof(XMax), 1.0);

    public static readonly StyledProperty<double> ThetaGoalProperty =
        AvaloniaProperty.Register<PlotControl, double>(nameof(ThetaGoal), 0.0);

    public static readonly StyledProperty<string> TitleProperty =
        AvaloniaProperty.Register<PlotControl, string>(nameof(Title), "");

    static PlotControl()
    {
        AffectsRender<PlotControl>(
            SimTProperty, SimThetaProperty,
            YMinProperty, YMaxProperty, XMaxProperty,
            ThetaGoalProperty, TitleProperty,
            BoundsProperty);
    }

    public double[]? SimT      { get => GetValue(SimTProperty);      set => SetValue(SimTProperty,      value); }
    public double[]? SimTheta  { get => GetValue(SimThetaProperty);  set => SetValue(SimThetaProperty,  value); }
    public double    YMin      { get => GetValue(YMinProperty);      set => SetValue(YMinProperty,      value); }
    public double    YMax      { get => GetValue(YMaxProperty);      set => SetValue(YMaxProperty,      value); }
    public double    XMax      { get => GetValue(XMaxProperty);      set => SetValue(XMaxProperty,      value); }
    public double    ThetaGoal { get => GetValue(ThetaGoalProperty); set => SetValue(ThetaGoalProperty, value); }
    public string    Title     { get => GetValue(TitleProperty);     set => SetValue(TitleProperty,     value); }

    // Returns a "nice" grid step for the given data range (20, 50, 100, …)
    private static double NiceStep(double range, int target = 6)
    {
        if (range <= 0) return 1;
        double rough = range / target;
        double mag   = Math.Pow(10, Math.Floor(Math.Log10(rough)));
        double norm  = rough / mag;
        double niceNorm = norm < 1.5 ? 1 : norm < 3.5 ? 2 : norm < 7.5 ? 5 : 10;
        return niceNorm * mag;
    }

    private static string FormatTick(double v)
    {
        double rounded = Math.Round(v, 10);
        return Math.Abs(rounded - Math.Round(rounded)) < 1e-9
            ? ((long)Math.Round(rounded)).ToString(CultureInfo.InvariantCulture)
            : rounded.ToString("G4", CultureInfo.InvariantCulture);
    }

    public override void Render(DrawingContext ctx)
    {
        double w = Bounds.Width;
        double h = Bounds.Height;
        if (w < 1 || h < 1) return;

        double plotW = w - ML - MR;
        double plotH = h - MT - MB;
        double yMin  = YMin;
        double yMax  = YMax;
        double xMax  = XMax;
        if (yMax - yMin < 1e-9) yMax = yMin + 1;
        if (xMax        < 1e-9) xMax = 1;

        Point ToPx(double t, double y) => new(
            ML + t / xMax * plotW,
            MT + (1.0 - (y - yMin) / (yMax - yMin)) * plotH);

        // Background
        ctx.DrawRectangle(Brushes.White, null, new Rect(0, 0, w, h));

        // Grid lines — nice intervals on both axes
        var gridPen = new Pen(new SolidColorBrush(Color.Parse("#E0E0E0")), 1);

        double yStep  = NiceStep(yMax - yMin, 6);
        double yStart = Math.Ceiling(yMin / yStep) * yStep;
        for (double yv = yStart; yv <= yMax + yStep * 1e-9; yv += yStep)
        {
            double py = MT + (1.0 - (yv - yMin) / (yMax - yMin)) * plotH;
            if (py < MT - 0.5 || py > MT + plotH + 0.5) continue;
            ctx.DrawLine(gridPen, new Point(ML, py), new Point(ML + plotW, py));
            DrawLabel(ctx, FormatTick(yv), new Point(4, py - 5));
        }

        double xStep = NiceStep(xMax, 6);
        for (double xv = 0; xv <= xMax + xStep * 1e-9; xv += xStep)
        {
            double px = ML + xv / xMax * plotW;
            if (px < ML - 0.5 || px > ML + plotW + 0.5) continue;
            ctx.DrawLine(gridPen, new Point(px, MT), new Point(px, MT + plotH));
            DrawLabel(ctx, FormatTick(xv), new Point(px - 10, MT + plotH + 4));
        }

        // Plot box border
        ctx.DrawRectangle(null, new Pen(Brushes.Black, 1), new Rect(ML, MT, plotW, plotH));

        // Target line (red dashed)
        double targetPy = ToPx(0, ThetaGoal).Y;
        var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
        ctx.DrawLine(
            new Pen(new SolidColorBrush(Color.Parse("#D62828")), 1.4, dashStyle),
            new Point(ML, targetPy), new Point(ML + plotW, targetPy));

        // Response polyline
        var t  = SimT;
        var th = SimTheta;
        if (t != null && th != null && t.Length > 1 && th.Length == t.Length)
        {
            var geo = new StreamGeometry();
            using (var gc = geo.Open())
            {
                gc.BeginFigure(ToPx(t[0], th[0]), false);
                for (int i = 1; i < t.Length; ++i)
                    gc.LineTo(ToPx(t[i], th[i]));
                gc.EndFigure(false);
            }
            ctx.DrawGeometry(null,
                new Pen(new SolidColorBrush(Color.Parse("#1F77B4")), 1.6), geo);
        }

        // Axis names
        DrawLabel(ctx, "t",     new Point(ML + plotW / 2 - 4, MT + plotH + 18));
        DrawLabel(ctx, "theta", new Point(2, MT + plotH / 2 - 20));

        // Title
        if (!string.IsNullOrEmpty(Title))
        {
            var ftBold = new FormattedText(Title, CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface("Segoe UI, Arial", FontStyle.Normal, FontWeight.Bold),
                11, Brushes.Black);
            ctx.DrawText(ftBold, new Point(ML + plotW / 2 - ftBold.Width / 2, 5));
        }

        // Legend
        DrawLegend(ctx, new Point(ML + plotW - 80, MT + 6));
    }

    private static void DrawLabel(DrawingContext ctx, string text, Point origin)
    {
        var ft = new FormattedText(text, CultureInfo.InvariantCulture,
            FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
        ctx.DrawText(ft, origin);
    }

    private static void DrawLegend(DrawingContext ctx, Point o)
    {
        var bg = new SolidColorBrush(Color.FromArgb(0xEE, 0xFF, 0xFF, 0xFF));
        ctx.DrawRectangle(bg,
            new Pen(new SolidColorBrush(Color.Parse("#BBB")), 1),
            new Rect(o.X - 4, o.Y - 4, 74, 38));

        var dash = new DashStyle(new[] { 3.0, 2.0 }, 0);
        ctx.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#D62828")), 2, dash),
            new Point(o.X, o.Y + 6), new Point(o.X + 16, o.Y + 6));
        DrawLabel(ctx, "Target", new Point(o.X + 20, o.Y + 0));

        ctx.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#1F77B4")), 2),
            new Point(o.X, o.Y + 22), new Point(o.X + 16, o.Y + 22));
        DrawLabel(ctx, "PID", new Point(o.X + 20, o.Y + 16));
    }
}
