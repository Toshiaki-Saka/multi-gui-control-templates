// ViewModels/MainWindowViewModel.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using Avalonia;
using Avalonia.Collections;
using Avalonia.Media;

using TdofAvalonia.Native;

namespace TdofAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    // Plot canvas dimensions (kept in lock-step with MainWindow.axaml).
    private const double PlotWidthPx  = 460.0;
    private const double PlotHeightPx = 250.0;
    private const double MarginL      = 50.0;
    private const double MarginR      = 14.0;
    private const double MarginT      = 16.0;
    private const double MarginB      = 28.0;

    private static readonly IBrush BlueBrush   = new SolidColorBrush(Color.FromRgb(31, 119, 180));
    private static readonly IBrush OrangeBrush = new SolidColorBrush(Color.FromRgb(255, 127, 14));

    public MainWindowViewModel()
    {
        RefOriginalPoints = new AvaloniaList<Point>();
        RefFilteredPoints = new AvaloniaList<Point>();
        OutPidPoints      = new AvaloniaList<Point>();
        Out2DofPoints     = new AvaloniaList<Point>();

        ResetParametersToDefaults();
        Run();
    }

    // ===== Parameters =====
    private double _m;  public double M  { get => _m;  set => SetField(ref _m,  value); }
    private double _c;  public double C  { get => _c;  set => SetField(ref _c,  value); }
    private double _k;  public double K  { get => _k;  set => SetField(ref _k,  value); }
    private double _kp; public double Kp { get => _kp; set => SetField(ref _kp, value); }
    private double _ki; public double Ki { get => _ki; set => SetField(ref _ki, value); }
    private double _kd; public double Kd { get => _kd; set => SetField(ref _kd, value); }
    private double _ref;   public double Ref   { get => _ref;   set => SetField(ref _ref,   value); }
    private double _tEnd;  public double TEnd  { get => _tEnd;  set => SetField(ref _tEnd,  value); }
    private double _dt;    public double Dt    { get => _dt;    set => SetField(ref _dt,    value); }

    // ===== Render state =====
    public AvaloniaList<Point> RefOriginalPoints { get; }
    public AvaloniaList<Point> RefFilteredPoints { get; }
    public AvaloniaList<Point> OutPidPoints      { get; }
    public AvaloniaList<Point> Out2DofPoints     { get; }

    // Axis-tick label strings (set after each run).
    private string _refYTop = "", _refY25 = "", _refY50 = "", _refY75 = "", _refYBot = "";
    private string _refXEnd = "", _refX25 = "", _refX50 = "", _refX75 = "";
    public string RefYTop { get => _refYTop; set => SetField(ref _refYTop, value); }
    public string RefY25  { get => _refY25;  set => SetField(ref _refY25,  value); }
    public string RefY50  { get => _refY50;  set => SetField(ref _refY50,  value); }
    public string RefY75  { get => _refY75;  set => SetField(ref _refY75,  value); }
    public string RefYBot { get => _refYBot; set => SetField(ref _refYBot, value); }
    public string RefXEnd { get => _refXEnd; set => SetField(ref _refXEnd, value); }
    public string RefX25  { get => _refX25;  set => SetField(ref _refX25,  value); }
    public string RefX50  { get => _refX50;  set => SetField(ref _refX50,  value); }
    public string RefX75  { get => _refX75;  set => SetField(ref _refX75,  value); }

    private string _outYTop = "", _outY25 = "", _outY50 = "", _outY75 = "", _outYBot = "";
    private string _outXEnd = "", _outX25 = "", _outX50 = "", _outX75 = "";
    public string OutYTop { get => _outYTop; set => SetField(ref _outYTop, value); }
    public string OutY25  { get => _outY25;  set => SetField(ref _outY25,  value); }
    public string OutY50  { get => _outY50;  set => SetField(ref _outY50,  value); }
    public string OutY75  { get => _outY75;  set => SetField(ref _outY75,  value); }
    public string OutYBot { get => _outYBot; set => SetField(ref _outYBot, value); }
    public string OutXEnd { get => _outXEnd; set => SetField(ref _outXEnd, value); }
    public string OutX25  { get => _outX25;  set => SetField(ref _outX25,  value); }
    public string OutX50  { get => _outX50;  set => SetField(ref _outX50,  value); }
    public string OutX75  { get => _outX75;  set => SetField(ref _outX75,  value); }

    private string _tfText = ""; public string TfText { get => _tfText; set => SetField(ref _tfText, value); }
    private string _logText = ""; public string LogText { get => _logText; set => SetField(ref _logText, value); }
    private string _statusMessage = "Ready"; public string StatusMessage { get => _statusMessage; set => SetField(ref _statusMessage, value); }

    // ===== Commands =====
    public void Run()
    {
        var cfg = new TdofConfig
        {
            M = M, C = C, K = K, Kp = Kp, Ki = Ki, Kd = Kd,
            Ref = Ref, TEnd = TEnd, Dt = Dt,
        };

        TdofSimulationResult sim;
        try { sim = TdofSolver.Simulate(cfg); }
        catch (Exception ex) { StatusMessage = $"Simulation failed: {ex.Message}"; return; }

        BuildPlot(sim.T, sim.R, sim.Z,
                  RefOriginalPoints, RefFilteredPoints,
                  out var rYLo, out var rYHi, out var rXLo, out var rXHi);
        RefYTop = rYHi.ToString("G4", CultureInfo.InvariantCulture);
        RefY25  = (rYHi * 0.75 + rYLo * 0.25).ToString("G4", CultureInfo.InvariantCulture);
        RefY50  = ((rYHi + rYLo) * 0.5).ToString("G4", CultureInfo.InvariantCulture);
        RefY75  = (rYHi * 0.25 + rYLo * 0.75).ToString("G4", CultureInfo.InvariantCulture);
        RefYBot = rYLo.ToString("G4", CultureInfo.InvariantCulture);
        RefXEnd = rXHi.ToString("G3", CultureInfo.InvariantCulture);
        RefX25  = (rXLo + 0.25 * (rXHi - rXLo)).ToString("G3", CultureInfo.InvariantCulture);
        RefX50  = (rXLo + 0.50 * (rXHi - rXLo)).ToString("G3", CultureInfo.InvariantCulture);
        RefX75  = (rXLo + 0.75 * (rXHi - rXLo)).ToString("G3", CultureInfo.InvariantCulture);

        BuildPlot(sim.T, sim.YPid, sim.Y2Dof,
                  OutPidPoints, Out2DofPoints,
                  out var oYLo, out var oYHi, out var oXLo, out var oXHi);
        OutYTop = oYHi.ToString("G4", CultureInfo.InvariantCulture);
        OutY25  = (oYHi * 0.75 + oYLo * 0.25).ToString("G4", CultureInfo.InvariantCulture);
        OutY50  = ((oYHi + oYLo) * 0.5).ToString("G4", CultureInfo.InvariantCulture);
        OutY75  = (oYHi * 0.25 + oYLo * 0.75).ToString("G4", CultureInfo.InvariantCulture);
        OutYBot = oYLo.ToString("G4", CultureInfo.InvariantCulture);
        OutXEnd = oXHi.ToString("G3", CultureInfo.InvariantCulture);
        OutX25  = (oXLo + 0.25 * (oXHi - oXLo)).ToString("G3", CultureInfo.InvariantCulture);
        OutX50  = (oXLo + 0.50 * (oXHi - oXLo)).ToString("G3", CultureInfo.InvariantCulture);
        OutX75  = (oXLo + 0.75 * (oXHi - oXLo)).ToString("G3", CultureInfo.InvariantCulture);

        // TF readout.
        TfText =
            FmtTf("Plant P",    TdofSolver.GetTf(cfg, TdofSystem.Plant))
          + FmtTf("PID K1",     TdofSolver.GetTf(cfg, TdofSystem.Pid))
          + FmtTf("Filter K2",  TdofSolver.GetTf(cfg, TdofSystem.Filter))
          + FmtTf("Closed Gyz", TdofSolver.GetTf(cfg, TdofSystem.ClosedLoop));

        // Metrics.
        double pidPeak = Max(sim.YPid), twoPeak = Max(sim.Y2Dof);
        double r = cfg.Ref;
        LogText =
            $"samples         : {sim.T.Length}\n"
          + $"y_pid  peak     : {pidPeak:F4}\n"
          + $"y_pid  final    : {sim.YPid[^1]:F4}\n"
          + $"y_pid  overshoot: {100.0 * (pidPeak / r - 1.0):F1}%\n"
          + $"y_2dof peak     : {twoPeak:F4}\n"
          + $"y_2dof final    : {sim.Y2Dof[^1]:F4}\n"
          + $"y_2dof overshoot: {100.0 * (twoPeak / r - 1.0):F1}%\n"
          + $"z final         : {sim.Z[^1]:F4}";

        StatusMessage = $"OK ({sim.T.Length} samples)";
    }

    public void ResetAndRun()
    {
        ResetParametersToDefaults();
        Run();
    }

    // ===== Internals =====
    private void ResetParametersToDefaults()
    {
        var c = TdofConfig.Default();
        M = c.M; C = c.C; K = c.K;
        Kp = c.Kp; Ki = c.Ki; Kd = c.Kd;
        Ref = c.Ref; TEnd = c.TEnd; Dt = c.Dt;
    }

    private static string FmtTf(string name, TransferFunction tf)
    {
        string Poly(double[] cs)
        {
            var parts = new string[cs.Length];
            for (int i = 0; i < cs.Length; ++i)
                parts[i] = cs[i].ToString("G4", CultureInfo.InvariantCulture);
            return string.Join("  ", parts);
        }
        return $"{name}:\n  num [{Poly(tf.Num)}]\n  den [{Poly(tf.Den)}]\n";
    }

    private static double Max(double[] v)
    {
        double m = double.NegativeInfinity;
        foreach (var x in v) if (x > m) m = x;
        return m;
    }

    // Build two pixel-space polylines that share one auto-scaled axis.
    private static void BuildPlot(
        double[] t, double[] yA, double[] yB,
        AvaloniaList<Point> outA, AvaloniaList<Point> outB,
        out double yLo, out double yHi, out double xLo, out double xHi)
    {
        double tMin = t.Length > 0 ? t[0] : 0.0;
        double tMax = t.Length > 0 ? t[^1] : 1.0;
        if (tMax - tMin < 1e-12) tMax = tMin + 1.0;

        yLo = double.PositiveInfinity; yHi = double.NegativeInfinity;
        foreach (var v in yA) { if (v < yLo) yLo = v; if (v > yHi) yHi = v; }
        foreach (var v in yB) { if (v < yLo) yLo = v; if (v > yHi) yHi = v; }
        if (double.IsInfinity(yLo)) { yLo = 0; yHi = 1; }
        if (yHi - yLo < 1e-9) { yLo -= 0.5; yHi += 0.5; }
        var pad = 0.08 * (yHi - yLo);
        yLo -= pad; yHi += pad;

        var plotW = PlotWidthPx - MarginL - MarginR;
        var plotH = PlotHeightPx - MarginT - MarginB;
        double yLoLocal = yLo, yHiLocal = yHi, tMinLocal = tMin, tMaxLocal = tMax;

        Point Px(double tt, double yy)
        {
            var fx = (tt - tMinLocal) / (tMaxLocal - tMinLocal);
            var fy = (yy - yLoLocal) / (yHiLocal - yLoLocal);
            return new Point(MarginL + fx * plotW,
                             MarginT + (1.0 - fy) * plotH);
        }

        outA.Clear(); outB.Clear();
        for (int i = 0; i < t.Length; ++i) outA.Add(Px(t[i], yA[i]));
        for (int i = 0; i < t.Length; ++i) outB.Add(Px(t[i], yB[i]));
        xLo = tMin;
        xHi = tMax;
    }

    // ===== INotifyPropertyChanged =====
    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        return true;
    }
}
