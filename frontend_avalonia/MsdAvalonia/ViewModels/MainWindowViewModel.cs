// ViewModels/MainWindowViewModel.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using Avalonia;
using Avalonia.Collections;
using Avalonia.Media;

using MsdAvalonia.Models;
using MsdAvalonia.Native;

namespace MsdAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    // ---- Plot canvas geometry (kept in lock-step with MainWindow.axaml) ----
    private const double PlotWidthPx  = 700.0;
    private const double PlotHeightPx = 460.0;
    private const double PlotMargin   = 38.0;

    // matplotlib's default colour cycle, so the C# frontend matches.
    private static readonly Color[] kCycle =
    {
        Color.Parse("#1f77b4"), Color.Parse("#ff7f0e"), Color.Parse("#2ca02c"),
        Color.Parse("#d62728"), Color.Parse("#9467bd"), Color.Parse("#8c564b"),
        Color.Parse("#e377c2"), Color.Parse("#7f7f7f"), Color.Parse("#bcbd22"),
        Color.Parse("#17becf"),
    };

    public MainWindowViewModel()
    {
        Cases       = new AvaloniaList<CaseEntry>();
        GridLines   = new AvaloniaList<GridLineMarker>();
        Series      = new AvaloniaList<SeriesPolyline>();

        PlotBoxLeft   = PlotMargin;
        PlotBoxTop    = PlotMargin;
        PlotBoxWidth  = PlotWidthPx  - 2 * PlotMargin;
        PlotBoxHeight = PlotHeightPx - 2 * PlotMargin;

        foreach (var c in MsdSolver.DefaultCases())
            Cases.Add(WireCase(new CaseEntry(c)));
        if (Cases.Count > 0) SelectedIndex = 0;

        Dt = 0.001; Stop = 10.0;
        Run();
    }

    // ===== Case list =====
    public AvaloniaList<CaseEntry> Cases { get; }

    private int _selectedIndex = -1;
    public int SelectedIndex
    {
        get => _selectedIndex;
        set
        {
            if (_selectedIndex == value) return;
            _selectedIndex = value;
            OnPropertyChanged();
            LoadEditorFromSelection();
        }
    }

    // ===== Selected-case editor (mirrors the underlying MsdCase) =====
    private string _editorName = "";
    public string EditorName { get => _editorName; set => SetField(ref _editorName, value, syncToCase: true); }
    private double _editorM;   public double EditorM { get => _editorM; set => SetField(ref _editorM, value, syncToCase: true); }
    private double _editorC;   public double EditorC { get => _editorC; set => SetField(ref _editorC, value, syncToCase: true); }
    private double _editorK;   public double EditorK { get => _editorK; set => SetField(ref _editorK, value, syncToCase: true); }
    private double _editorF;   public double EditorF { get => _editorF; set => SetField(ref _editorF, value, syncToCase: true); }
    private double _editorW;   public double EditorW { get => _editorW; set => SetField(ref _editorW, value, syncToCase: true); }
    private double _editorX0;  public double EditorX0 { get => _editorX0; set => SetField(ref _editorX0, value, syncToCase: true); }
    private double _editorV0;  public double EditorV0 { get => _editorV0; set => SetField(ref _editorV0, value, syncToCase: true); }

    // ===== Sampling =====
    private double _dt;   public double Dt   { get => _dt;   set => SetField(ref _dt,   value, runOnChange: true); }
    private double _stop; public double Stop { get => _stop; set => SetField(ref _stop, value, runOnChange: true); }

    // ===== Plot render state =====
    public AvaloniaList<GridLineMarker> GridLines { get; }
    public AvaloniaList<SeriesPolyline> Series    { get; }
    public double PlotBoxLeft   { get; }
    public double PlotBoxTop    { get; }
    public double PlotBoxWidth  { get; }
    public double PlotBoxHeight { get; }

    // ===== Tick labels (CLAUDE.md item 3) =====
    private string _refY0 = ""; public string RefY0 { get => _refY0; set => SetField(ref _refY0, value); }
    private string _refY25 = ""; public string RefY25 { get => _refY25; set => SetField(ref _refY25, value); }
    private string _refY50 = ""; public string RefY50 { get => _refY50; set => SetField(ref _refY50, value); }
    private string _refY75 = ""; public string RefY75 { get => _refY75; set => SetField(ref _refY75, value); }
    private string _refY100 = ""; public string RefY100 { get => _refY100; set => SetField(ref _refY100, value); }
    private string _refX0 = ""; public string RefX0 { get => _refX0; set => SetField(ref _refX0, value); }
    private string _refX25 = ""; public string RefX25 { get => _refX25; set => SetField(ref _refX25, value); }
    private string _refX50 = ""; public string RefX50 { get => _refX50; set => SetField(ref _refX50, value); }
    private string _refX75 = ""; public string RefX75 { get => _refX75; set => SetField(ref _refX75, value); }
    private string _refX100 = ""; public string RefX100 { get => _refX100; set => SetField(ref _refX100, value); }

    // ===== Derived metrics (selected case) =====
    private string _omegaNText = "—"; public string OmegaNText { get => _omegaNText; set => SetField(ref _omegaNText, value); }
    private string _zetaText   = "—"; public string ZetaText   { get => _zetaText;   set => SetField(ref _zetaText,   value); }
    private string _finalXText = "—"; public string FinalXText { get => _finalXText; set => SetField(ref _finalXText, value); }
    private string _finalVText = "—"; public string FinalVText { get => _finalVText; set => SetField(ref _finalVText, value); }
    private string _maxAbsXText = "—"; public string MaxAbsXText { get => _maxAbsXText; set => SetField(ref _maxAbsXText, value); }
    private string _legendText = ""; public string LegendText { get => _legendText; set => SetField(ref _legendText, value); }

    private string _statusMessage = "Ready";
    public string StatusMessage { get => _statusMessage; set => SetField(ref _statusMessage, value); }

    // ===== Internals =====
    private readonly Dictionary<CaseEntry, MsdSimulationResult?> _sims = new();
    private bool _suppressEditorSync;

    // ===== Commands =====
    public void Run()
    {
        var sampling = new MsdSamplingConfig { Dt = Dt, Stop = Stop };
        _sims.Clear();
        int n_samples = 0;
        int fails = 0;
        foreach (var entry in Cases)
        {
            try
            {
                var s = MsdSolver.Simulate(entry.Case, sampling);
                _sims[entry] = s;
                n_samples = s.T.Length;
            }
            catch (Exception)
            {
                _sims[entry] = null;
                ++fails;
            }
        }
        StatusMessage = fails > 0
            ? $"Simulation failed for {fails} case(s)"
            : $"OK — {Cases.Count} cases, {n_samples} samples each";
        UpdateDerivedLabels();
        RebuildPlot();
    }

    public void AddCase()
    {
        var c = MsdCase.Default();
        c.Name = $"case {Cases.Count + 1}";
        Cases.Add(WireCase(new CaseEntry(c)));
        SelectedIndex = Cases.Count - 1;
        Run();
    }

    public void DuplicateSelected()
    {
        if (SelectedIndex < 0 || SelectedIndex >= Cases.Count) return;
        var src = Cases[SelectedIndex];
        var dup = new CaseEntry(src.Case.Clone());
        dup.Name = src.Name + " (copy)";
        WireCase(dup);
        Cases.Insert(SelectedIndex + 1, dup);
        SelectedIndex = SelectedIndex + 1;
        Run();
    }

    public void RemoveSelected()
    {
        if (SelectedIndex < 0 || Cases.Count <= 1) return;
        int idx = SelectedIndex;
        Cases.RemoveAt(idx);
        SelectedIndex = Math.Min(idx, Cases.Count - 1);
        Run();
    }

    public void ResetCases()
    {
        Cases.Clear();
        foreach (var c in MsdSolver.DefaultCases())
            Cases.Add(WireCase(new CaseEntry(c)));
        SelectedIndex = 0;
        Run();
    }

    // ===== Internals =====
    private CaseEntry WireCase(CaseEntry entry)
    {
        // When the user toggles "Enabled", just rebuild the plot.
        entry.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName == nameof(CaseEntry.Enabled))
                RebuildPlot();
            else if (e.PropertyName == nameof(CaseEntry.Name))
                RebuildPlot();
        };
        return entry;
    }

    private void LoadEditorFromSelection()
    {
        _suppressEditorSync = true;
        if (SelectedIndex < 0 || SelectedIndex >= Cases.Count)
        {
            EditorName = ""; EditorM = 0; EditorC = 0; EditorK = 0;
            EditorF = 0; EditorW = 0; EditorX0 = 0; EditorV0 = 0;
        }
        else
        {
            var c = Cases[SelectedIndex].Case;
            EditorName = c.Name;
            EditorM = c.M; EditorC = c.C; EditorK = c.K;
            EditorF = c.ForceAmplitude; EditorW = c.ForceOmega;
            EditorX0 = c.X0; EditorV0 = c.V0;
        }
        _suppressEditorSync = false;
        UpdateDerivedLabels();
    }

    private void SyncEditorToCase()
    {
        if (_suppressEditorSync) return;
        if (SelectedIndex < 0 || SelectedIndex >= Cases.Count) return;
        var entry = Cases[SelectedIndex];
        entry.Name = EditorName;
        entry.Case.M = EditorM; entry.Case.C = EditorC; entry.Case.K = EditorK;
        entry.Case.ForceAmplitude = EditorF; entry.Case.ForceOmega = EditorW;
        entry.Case.X0 = EditorX0; entry.Case.V0 = EditorV0;
        Run();
    }

    private void UpdateDerivedLabels()
    {
        if (SelectedIndex < 0 || SelectedIndex >= Cases.Count)
        {
            OmegaNText = ZetaText = FinalXText = FinalVText = MaxAbsXText = "—";
            return;
        }
        var entry = Cases[SelectedIndex];
        var (wn, zeta) = entry.Case.Derived();
        OmegaNText = wn.ToString("F4", CultureInfo.InvariantCulture);
        ZetaText   = zeta.ToString("F4", CultureInfo.InvariantCulture);
        if (_sims.TryGetValue(entry, out var sim) && sim != null)
        {
            FinalXText  = sim.FinalX.ToString("+0.000000;-0.000000", CultureInfo.InvariantCulture);
            FinalVText  = sim.FinalV.ToString("+0.000000;-0.000000", CultureInfo.InvariantCulture);
            MaxAbsXText = sim.MaxAbsX.ToString("F6", CultureInfo.InvariantCulture);
        }
        else
        {
            FinalXText = FinalVText = MaxAbsXText = "—";
        }
    }

    private void RebuildPlot()
    {
        Series.Clear();
        GridLines.Clear();

        // Y range from all *enabled* cases with successful sims.
        double yMin = double.PositiveInfinity, yMax = double.NegativeInfinity;
        foreach (var entry in Cases)
        {
            if (!entry.Enabled) continue;
            if (!_sims.TryGetValue(entry, out var sim) || sim == null) continue;
            for (int i = 0; i < sim.X.Length; ++i)
            {
                if (sim.X[i] < yMin) yMin = sim.X[i];
                if (sim.X[i] > yMax) yMax = sim.X[i];
            }
        }
        if (!double.IsFinite(yMin)) { yMin = -1; yMax = 1; }
        if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
        double padY = 0.05 * (yMax - yMin);
        yMin -= padY; yMax += padY;

        const double xMin = 0.0;
        double xMax = Math.Max(Stop, 1e-9);
        double dx = xMax - xMin;
        double dy = yMax - yMin;

        // Grid (5x5).
        for (int i = 0; i <= 4; ++i)
        {
            double f = i / 4.0;
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(PlotBoxLeft,                PlotBoxTop + f * PlotBoxHeight),
                End   = new Point(PlotBoxLeft + PlotBoxWidth, PlotBoxTop + f * PlotBoxHeight),
            });
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(PlotBoxLeft + f * PlotBoxWidth, PlotBoxTop),
                End   = new Point(PlotBoxLeft + f * PlotBoxWidth, PlotBoxTop + PlotBoxHeight),
            });
        }

        // Tick labels (CLAUDE.md item 3).
        string YT(double frac) => Snap((1.0 - frac) * yMax + frac * yMin, dy);
        string XT(double frac) => Snap(xMin + frac * dx, dx);
        // Y is inverted (top = max)
        RefY0   = YT(1.0); // bottom
        RefY25  = YT(0.75);
        RefY50  = YT(0.5);
        RefY75  = YT(0.25);
        RefY100 = YT(0.0); // top
        RefX0   = XT(0.0);
        RefX25  = XT(0.25);
        RefX50  = XT(0.5);
        RefX75  = XT(0.75);
        RefX100 = XT(1.0);

        // Series polylines.
        int colourIdx = 0;
        var legendLines = new List<string>();
        for (int idx = 0; idx < Cases.Count; ++idx)
        {
            var entry = Cases[idx];
            if (!entry.Enabled) { ++colourIdx; continue; }
            if (!_sims.TryGetValue(entry, out var sim) || sim == null) { ++colourIdx; continue; }
            var col = kCycle[colourIdx % kCycle.Length];
            ++colourIdx;
            var pts = new AvaloniaList<Point>();
            for (int i = 0; i < sim.T.Length; ++i)
            {
                double fx = (sim.T[i] - xMin) / dx;
                double fy = (sim.X[i] - yMin) / dy;
                pts.Add(new Point(
                    PlotBoxLeft + fx * PlotBoxWidth,
                    PlotBoxTop  + (1.0 - fy) * PlotBoxHeight));
            }
            var label = MakeLabel(entry.Case);
            Series.Add(new SeriesPolyline
            {
                Label  = label,
                Stroke = new SolidColorBrush(col),
                Points = pts,
            });
            legendLines.Add(label);
        }
        LegendText = string.Join("\n", legendLines);
    }

    private static string Snap(double v, double span)
    {
        double eps = span * 1e-9;
        if (Math.Abs(v) < eps) v = 0.0;
        return v.ToString("G4", CultureInfo.InvariantCulture);
    }

    private static string MakeLabel(MsdCase c)
    {
        var (wn, zeta) = c.Derived();
        return string.Create(CultureInfo.InvariantCulture,
            $"m={c.M:G4}, c={c.C:G4}, k={c.K:G4}, F={c.ForceAmplitude:G4}sin({c.ForceOmega:G4}t), ωn={wn:F2}, ζ={zeta:F2}");
    }

    // ===== INotifyPropertyChanged =====
    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    private bool SetField<T>(ref T field, T value,
                              [CallerMemberName] string? name = null,
                              bool syncToCase = false,
                              bool runOnChange = false)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        if (syncToCase) SyncEditorToCase();
        if (runOnChange) Run();
        return true;
    }
}
