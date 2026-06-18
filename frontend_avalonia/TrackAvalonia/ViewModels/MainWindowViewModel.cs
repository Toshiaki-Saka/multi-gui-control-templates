// ViewModels/MainWindowViewModel.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using Avalonia;
using Avalonia.Collections;

using TrackAvalonia.Models;
using TrackAvalonia.Native;

namespace TrackAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    // ---- XY canvas geometry (matches MainWindow.axaml) ----
    private const double XyWidthPx  = 540.0;
    private const double XyHeightPx = 380.0;
    private const double XyMargin   = 32.0;

    // ---- Error canvas geometry ----
    private const double ErrWidthPx  = 540.0;
    private const double ErrHeightPx = 200.0;
    private const double ErrMargin   = 32.0;

    public MainWindowViewModel()
    {
        ReferencePoints = new AvaloniaList<Point>();
        ActualPoints    = new AvaloniaList<Point>();
        ErrorPoints     = new AvaloniaList<Point>();
        GridLines       = new AvaloniaList<GridLineMarker>();
        ErrGridLines    = new AvaloniaList<GridLineMarker>();

        XyBoxLeft = XyMargin;
        XyBoxTop = XyMargin;
        XyBoxWidth = XyWidthPx - 2 * XyMargin;
        XyBoxHeight = XyHeightPx - 2 * XyMargin;

        ErrBoxLeft = ErrMargin;
        ErrBoxTop = ErrMargin / 2;
        ErrBoxWidth = ErrWidthPx - 2 * ErrMargin;
        ErrBoxHeight = ErrHeightPx - ErrMargin;

        ResetParametersToDefaults();
        Run();
    }

    // ===== Parameter properties (auto-Run on change) =====
    private double _m;          public double M          { get => _m;          set => SetField(ref _m,          value); }
    private double _izz;        public double Izz        { get => _izz;        set => SetField(ref _izz,        value); }
    private double _cp;         public double CorneringPower { get => _cp;     set => SetField(ref _cp,         value); }
    private double _h;          public double H          { get => _h;          set => SetField(ref _h,          value); }
    private double _tc;         public double Tc         { get => _tc;         set => SetField(ref _tc,         value); }
    private double _totalTime;  public double TotalTime  { get => _totalTime;  set => SetField(ref _totalTime,  value); }
    private double _targetSpd;  public double TargetSpeed { get => _targetSpd; set => SetField(ref _targetSpd,  value); }
    private double _kyP;        public double KyP        { get => _kyP;        set => SetField(ref _kyP,        value); }
    private double _kyI;        public double KyI        { get => _kyI;        set => SetField(ref _kyI,        value); }
    private double _kpsiP;      public double KpsiP      { get => _kpsiP;      set => SetField(ref _kpsiP,      value); }
    private double _kpsiI;      public double KpsiI      { get => _kpsiI;      set => SetField(ref _kpsiI,      value); }
    private double _krDamping;  public double KrDamping  { get => _krDamping;  set => SetField(ref _krDamping,  value); }
    private double _nLim;       public double NMomentLimit { get => _nLim;     set => SetField(ref _nLim,       value); }
    private double _fxLim;      public double FxLimit    { get => _fxLim;      set => SetField(ref _fxLim,      value); }
    private double _iLim;       public double ErrorIntegralLimit { get => _iLim; set => SetField(ref _iLim,     value); }
    private int    _lookahead;  public int    LookaheadIndex { get => _lookahead; set => SetField(ref _lookahead, value); }
    private double _y0Off;      public double InitialYOffset { get => _y0Off;  set => SetField(ref _y0Off,      value); }
    private double _hd0;        public double InitialHeadingDeg { get => _hd0; set => SetField(ref _hd0,        value); }
    private double _str1;       public double Straight1Len { get => _str1;     set => SetField(ref _str1,       value); }
    private double _radius;     public double Radius     { get => _radius;     set => SetField(ref _radius,     value); }
    private double _str2;       public double Straight2Len { get => _str2;     set => SetField(ref _str2,       value); }
    private double _ds;         public double Ds         { get => _ds;         set => SetField(ref _ds,         value); }

    // ===== Plot render state =====
    public AvaloniaList<Point>          ReferencePoints { get; }
    public AvaloniaList<Point>          ActualPoints    { get; }
    public AvaloniaList<Point>          ErrorPoints     { get; }
    public AvaloniaList<GridLineMarker> GridLines       { get; }
    public AvaloniaList<GridLineMarker> ErrGridLines    { get; }

    public double XyBoxLeft   { get; }
    public double XyBoxTop    { get; }
    public double XyBoxWidth  { get; }
    public double XyBoxHeight { get; }

    public double ErrBoxLeft   { get; }
    public double ErrBoxTop    { get; }
    public double ErrBoxWidth  { get; }
    public double ErrBoxHeight { get; }

    // Marker positions as separate X/Y for easy Canvas.Left/Top binding.
    private double _startMarkerX; public double StartMarkerX { get => _startMarkerX; set => SetField(ref _startMarkerX, value); }
    private double _startMarkerY; public double StartMarkerY { get => _startMarkerY; set => SetField(ref _startMarkerY, value); }
    private double _endMarkerX;   public double EndMarkerX   { get => _endMarkerX;   set => SetField(ref _endMarkerX,   value); }
    private double _endMarkerY;   public double EndMarkerY   { get => _endMarkerY;   set => SetField(ref _endMarkerY,   value); }

    // ===== Tick labels (CLAUDE.md item 3) =====
    private string _refY0  = ""; public string RefY0  { get => _refY0;  set => SetField(ref _refY0,  value); }
    private string _refY25 = ""; public string RefY25 { get => _refY25; set => SetField(ref _refY25, value); }
    private string _refY50 = ""; public string RefY50 { get => _refY50; set => SetField(ref _refY50, value); }
    private string _refY75 = ""; public string RefY75 { get => _refY75; set => SetField(ref _refY75, value); }
    private string _refY100 = ""; public string RefY100 { get => _refY100; set => SetField(ref _refY100, value); }

    private string _refX0  = ""; public string RefX0  { get => _refX0;  set => SetField(ref _refX0,  value); }
    private string _refX25 = ""; public string RefX25 { get => _refX25; set => SetField(ref _refX25, value); }
    private string _refX50 = ""; public string RefX50 { get => _refX50; set => SetField(ref _refX50, value); }
    private string _refX75 = ""; public string RefX75 { get => _refX75; set => SetField(ref _refX75, value); }
    private string _refX100 = ""; public string RefX100 { get => _refX100; set => SetField(ref _refX100, value); }

    // Error plot ticks
    private string _errY0 = ""; public string ErrY0 { get => _errY0; set => SetField(ref _errY0, value); }
    private string _errY50 = ""; public string ErrY50 { get => _errY50; set => SetField(ref _errY50, value); }
    private string _errY100 = ""; public string ErrY100 { get => _errY100; set => SetField(ref _errY100, value); }
    private string _errX0 = ""; public string ErrX0 { get => _errX0; set => SetField(ref _errX0, value); }
    private string _errX50 = ""; public string ErrX50 { get => _errX50; set => SetField(ref _errX50, value); }
    private string _errX100 = ""; public string ErrX100 { get => _errX100; set => SetField(ref _errX100, value); }

    private string _metricsText = "";
    public string MetricsText { get => _metricsText; set => SetField(ref _metricsText, value); }
    private string _statusMessage = "Ready";
    public string StatusMessage { get => _statusMessage; set => SetField(ref _statusMessage, value); }

    // ===== Commands =====
    public void Run()
    {
        var cfg = new TrackConfig
        {
            M = M, Izz = Izz, CorneringPower = CorneringPower,
            H = H, Tc = Tc, TotalTime = TotalTime, TargetSpeed = TargetSpeed,
            KyP = KyP, KyI = KyI, KpsiP = KpsiP, KpsiI = KpsiI,
            KrDamping = KrDamping,
            NMomentLimit = NMomentLimit, FxLimit = FxLimit,
            ErrorIntegralLimit = ErrorIntegralLimit,
            LookaheadIndex = LookaheadIndex,
            InitialYOffset = InitialYOffset, InitialHeadingDeg = InitialHeadingDeg,
            Straight1Len = Straight1Len, Radius = Radius,
            Straight2Len = Straight2Len, Ds = Ds,
        };

        ReferencePath refPath;
        TrackSimulationResult sim;
        try
        {
            refPath = TrackSolver.MakeReference(cfg);
            sim     = TrackSolver.Simulate(cfg);
        }
        catch (Exception ex)
        {
            StatusMessage = $"Simulation failed: {ex.Message}";
            return;
        }

        // ---- XY plot ranges (equal aspect — match Qt look) ----
        double xMin = double.PositiveInfinity, xMax = double.NegativeInfinity;
        double yMin = double.PositiveInfinity, yMax = double.NegativeInfinity;
        void Extend(double[] xs, double[] ys)
        {
            int n = Math.Min(xs.Length, ys.Length);
            for (int i = 0; i < n; ++i)
            {
                if (xs[i] < xMin) xMin = xs[i]; if (xs[i] > xMax) xMax = xs[i];
                if (ys[i] < yMin) yMin = ys[i]; if (ys[i] > yMax) yMax = ys[i];
            }
        }
        Extend(refPath.X, refPath.Y);
        Extend(sim.X, sim.Y);
        if (!double.IsFinite(xMin)) { xMin = 0; xMax = 1; yMin = 0; yMax = 1; }
        double padX = 0.05 * Math.Max(1e-6, xMax - xMin);
        double padY = 0.05 * Math.Max(1e-6, yMax - yMin);
        xMin -= padX; xMax += padX; yMin -= padY; yMax += padY;

        // Equal aspect: shrink the smaller-range axis so distances render correctly.
        double dx = xMax - xMin;
        double dy = yMax - yMin;
        double scale = Math.Min(XyBoxWidth / dx, XyBoxHeight / dy);
        double usedW = dx * scale;
        double usedH = dy * scale;
        double pxL = XyBoxLeft + (XyBoxWidth  - usedW) / 2.0;
        double pxT = XyBoxTop  + (XyBoxHeight - usedH) / 2.0;

        Point Map(double x, double y) => new(
            pxL + (x - xMin) / dx * usedW,
            pxT + (1.0 - (y - yMin) / dy) * usedH);

        ReferencePoints.Clear();
        for (int i = 0; i < refPath.X.Length; ++i)
            ReferencePoints.Add(Map(refPath.X[i], refPath.Y[i]));
        ActualPoints.Clear();
        for (int i = 0; i < sim.X.Length; ++i)
            ActualPoints.Add(Map(sim.X[i], sim.Y[i]));

        var start = Map(sim.X[0], sim.Y[0]);
        var end   = Map(sim.X[^1], sim.Y[^1]);
        // Offset by -4 so the 8px ellipse is centred on the (x, y) point.
        StartMarkerX = start.X - 4; StartMarkerY = start.Y - 4;
        EndMarkerX   = end.X   - 4; EndMarkerY   = end.Y   - 4;

        // ---- Grid lines (CLAUDE.md item 2: Line + StartPoint/EndPoint) ----
        GridLines.Clear();
        for (int i = 0; i <= 4; ++i)
        {
            double f = i / 4.0;
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(pxL,            pxT + f * usedH),
                End   = new Point(pxL + usedW,    pxT + f * usedH),
            });
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(pxL + f * usedW, pxT),
                End   = new Point(pxL + f * usedW, pxT + usedH),
            });
        }

        // ---- Tick labels (CLAUDE.md item 3) ----
        string Y(double frac) => (yMin + frac * dy).ToString("G4", CultureInfo.InvariantCulture);
        string X(double frac) => (xMin + frac * dx).ToString("G4", CultureInfo.InvariantCulture);
        RefY0   = Y(0.0); RefY25 = Y(0.25); RefY50 = Y(0.5); RefY75 = Y(0.75); RefY100 = Y(1.0);
        RefX0   = X(0.0); RefX25 = X(0.25); RefX50 = X(0.5); RefX75 = X(0.75); RefX100 = X(1.0);

        // ---- Error time-series plot ----
        int n_ = sim.T.Length;
        var pe = new double[n_];
        double tMin = sim.T[0], tMax = sim.T[^1];
        double peMin = double.PositiveInfinity, peMax = double.NegativeInfinity;
        for (int i = 0; i < n_; ++i)
        {
            double exi = sim.X[i] - sim.XRef[i];
            double eyi = sim.Y[i] - sim.YRef[i];
            pe[i] = Math.Sqrt(exi * exi + eyi * eyi);
            if (pe[i] < peMin) peMin = pe[i];
            if (pe[i] > peMax) peMax = pe[i];
        }
        if (tMax - tMin < 1e-12) tMax = tMin + 1;
        if (peMax - peMin < 1e-12) peMax = peMin + 1;
        double peSpan = peMax - peMin;
        peMin -= 0.05 * peSpan; peMax += 0.05 * peSpan;
        double tSpan = tMax - tMin;
        double peSpanFull = peMax - peMin;

        ErrorPoints.Clear();
        for (int i = 0; i < n_; ++i)
        {
            double fx = (sim.T[i] - tMin) / tSpan;
            double fy = (pe[i] - peMin) / peSpanFull;
            ErrorPoints.Add(new Point(
                ErrBoxLeft + fx * ErrBoxWidth,
                ErrBoxTop  + (1.0 - fy) * ErrBoxHeight));
        }

        ErrGridLines.Clear();
        for (int i = 0; i <= 4; ++i)
        {
            double f = i / 4.0;
            ErrGridLines.Add(new GridLineMarker
            {
                Start = new Point(ErrBoxLeft,                  ErrBoxTop + f * ErrBoxHeight),
                End   = new Point(ErrBoxLeft + ErrBoxWidth,    ErrBoxTop + f * ErrBoxHeight),
            });
            ErrGridLines.Add(new GridLineMarker
            {
                Start = new Point(ErrBoxLeft + f * ErrBoxWidth, ErrBoxTop),
                End   = new Point(ErrBoxLeft + f * ErrBoxWidth, ErrBoxTop + ErrBoxHeight),
            });
        }
        string EY(double frac) => (peMin + frac * peSpanFull).ToString("G4", CultureInfo.InvariantCulture);
        string EX(double frac) => (tMin  + frac * tSpan).ToString("G4", CultureInfo.InvariantCulture);
        ErrY0   = EY(0.0); ErrY50  = EY(0.5); ErrY100 = EY(1.0);
        ErrX0   = EX(0.0); ErrX50  = EX(0.5); ErrX100 = EX(1.0);

        // ---- Metrics ----
        MetricsText =
            $"samples              : {n_}\n" +
            $"reference points     : {refPath.X.Length}\n" +
            $"path_error_rms [m]   : {sim.PathErrorRms:F6}\n" +
            $"path_error_max [m]   : {sim.PathErrorMax:F6}\n" +
            $"e_y_rms        [m]   : {sim.EyRms:F6}\n" +
            $"e_y_max        [m]   : {sim.EyMaxAbs:F6}\n" +
            $"e_psi_rms      [rad] : {sim.EpsiRms:F6}\n" +
            $"e_psi_max      [rad] : {sim.EpsiMaxAbs:F6}\n" +
            $"max|n_moment|        : {sim.NMomentMaxAbs:F6}";

        StatusMessage = $"OK — {n_} samples, rms err {sim.PathErrorRms:F4} m";
    }

    public void ResetAndRun()
    {
        ResetParametersToDefaults();
        Run();
    }

    // ===== Internals =====
    private void ResetParametersToDefaults()
    {
        var c = TrackConfig.Default();
        M = c.M; Izz = c.Izz; CorneringPower = c.CorneringPower;
        H = c.H; Tc = c.Tc; TotalTime = c.TotalTime; TargetSpeed = c.TargetSpeed;
        KyP = c.KyP; KyI = c.KyI; KpsiP = c.KpsiP; KpsiI = c.KpsiI;
        KrDamping = c.KrDamping;
        NMomentLimit = c.NMomentLimit; FxLimit = c.FxLimit;
        ErrorIntegralLimit = c.ErrorIntegralLimit;
        LookaheadIndex = c.LookaheadIndex;
        InitialYOffset = c.InitialYOffset;
        InitialHeadingDeg = c.InitialHeadingDeg;
        Straight1Len = c.Straight1Len; Radius = c.Radius;
        Straight2Len = c.Straight2Len; Ds = c.Ds;
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    // Names that should trigger an automatic Run() when set.
    private static readonly HashSet<string> ParamNames = new()
    {
        nameof(M), nameof(Izz), nameof(CorneringPower),
        nameof(H), nameof(Tc), nameof(TotalTime), nameof(TargetSpeed),
        nameof(KyP), nameof(KyI), nameof(KpsiP), nameof(KpsiI), nameof(KrDamping),
        nameof(NMomentLimit), nameof(FxLimit), nameof(ErrorIntegralLimit),
        nameof(LookaheadIndex),
        nameof(InitialYOffset), nameof(InitialHeadingDeg),
        nameof(Straight1Len), nameof(Radius), nameof(Straight2Len), nameof(Ds),
    };

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        if (name != null && ParamNames.Contains(name)) Run();
        return true;
    }
}
