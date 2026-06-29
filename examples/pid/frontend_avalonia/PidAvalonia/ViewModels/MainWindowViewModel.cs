// ViewModels/MainWindowViewModel.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;

using PidAvalonia.Native;

namespace PidAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    public MainWindowViewModel()
    {
        ResetParametersToDefaults();
        Run();
    }

    // ===== Parameter properties =====
    private double _thetaStart;     public double ThetaStart    { get => _thetaStart;     set => SetField(ref _thetaStart,     value); }
    private double _thetaGoal;      public double ThetaGoal     { get => _thetaGoal;      set => SetField(ref _thetaGoal,      value); }
    private double _offset;         public double Offset        { get => _offset;         set => SetField(ref _offset,         value); }
    private int    _timeLength;     public int    TimeLength    { get => _timeLength;     set => SetField(ref _timeLength,     value); }
    private double _kp;             public double Kp            { get => _kp;             set => SetField(ref _kp,             value); }
    private double _ki;             public double Ki            { get => _ki;             set => SetField(ref _ki,             value); }
    private double _kd;             public double Kd            { get => _kd;             set => SetField(ref _kd,             value); }
    private double _dt;             public double Dt            { get => _dt;             set => SetField(ref _dt,             value); }
    private double _integralClamp;  public double IntegralClamp { get => _integralClamp;  set => SetField(ref _integralClamp,  value); }
    private double _outputClamp;    public double OutputClamp   { get => _outputClamp;    set => SetField(ref _outputClamp,    value); }

    // ===== Raw simulation data (consumed by PlotControl) =====
    private double[] _simT     = Array.Empty<double>();
    private double[] _simTheta = Array.Empty<double>();
    private double   _plotYMin;
    private double   _plotYMax = 1;
    private double   _plotXMax = 1;
    private double   _thetaGoalValue;

    public double[] SimT          { get => _simT;          private set => SetField(ref _simT,          value); }
    public double[] SimTheta      { get => _simTheta;      private set => SetField(ref _simTheta,      value); }
    public double   PlotYMin      { get => _plotYMin;      private set => SetField(ref _plotYMin,      value); }
    public double   PlotYMax      { get => _plotYMax;      private set => SetField(ref _plotYMax,      value); }
    public double   PlotXMax      { get => _plotXMax;      private set => SetField(ref _plotXMax,      value); }
    public double   ThetaGoalValue{ get => _thetaGoalValue;private set => SetField(ref _thetaGoalValue,value); }

    // ===== Display text =====
    private string _titleText     = "";      public string TitleText     { get => _titleText;     set => SetField(ref _titleText,     value); }
    private string _statusMessage = "Ready"; public string StatusMessage { get => _statusMessage; set => SetField(ref _statusMessage, value); }

    // ===== Commands =====
    public void Run()
    {
        var cfg = new PidConfig
        {
            ThetaStart = ThetaStart, ThetaGoal = ThetaGoal, Offset = Offset,
            TimeLength = TimeLength, Kp = Kp, Ki = Ki, Kd = Kd,
            Dt = Dt, IntegralClamp = IntegralClamp, OutputClamp = OutputClamp,
        };

        PidSimulationResult sim;
        try { sim = PidSolver.Simulate(cfg); }
        catch (Exception ex) { StatusMessage = $"Simulation failed: {ex.Message}"; return; }

        double yMin = sim.MinTheta;
        double yMax = sim.MaxTheta;
        if (cfg.ThetaGoal < yMin) yMin = cfg.ThetaGoal;
        if (cfg.ThetaGoal > yMax) yMax = cfg.ThetaGoal;
        yMin -= 20; yMax += 20;
        if (yMax - yMin < 1e-9) yMax = yMin + 1;

        SimT           = sim.T;
        SimTheta       = sim.Theta;
        PlotYMin       = yMin;
        PlotYMax       = yMax;
        PlotXMax       = Math.Max(1e-9, (double)cfg.TimeLength * cfg.Dt);
        ThetaGoalValue = cfg.ThetaGoal;

        TitleText     = $"final theta = {sim.FinalTheta:F3}";
        StatusMessage = $"final={sim.FinalTheta:F3}  max={sim.MaxTheta:F3}  min={sim.MinTheta:F3}";
    }

    public void ResetAndRun()
    {
        ResetParametersToDefaults();
        Run();
    }

    // ===== Internals =====
    private void ResetParametersToDefaults()
    {
        var c = PidConfig.Default();
        ThetaStart = c.ThetaStart;
        ThetaGoal  = c.ThetaGoal;
        Offset     = c.Offset;
        TimeLength = c.TimeLength;
        Kp = c.Kp; Ki = c.Ki; Kd = c.Kd;
        Dt = c.Dt; IntegralClamp = c.IntegralClamp; OutputClamp = c.OutputClamp;
    }

    // ===== INotifyPropertyChanged =====
    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    private static readonly HashSet<string> ParamNames = new()
    {
        nameof(ThetaStart), nameof(ThetaGoal), nameof(Offset),
        nameof(TimeLength), nameof(Kp), nameof(Ki), nameof(Kd),
        nameof(Dt), nameof(IntegralClamp), nameof(OutputClamp),
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
