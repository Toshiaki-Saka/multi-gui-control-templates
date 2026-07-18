// Native/PidSolver.cs — managed wrapper around pid_core.

using System;
using System.Runtime.InteropServices;

namespace PidAvalonia.Native;

public sealed class PidConfig
{
    public double ThetaStart    { get; set; } = 0.0;
    public double ThetaGoal     { get; set; } = 90.0;
    public double Offset        { get; set; } = 0.0;
    public int    TimeLength    { get; set; } = 150;
    public double Kp            { get; set; } = 0.10;
    public double Ki            { get; set; } = 0.01;
    public double Kd            { get; set; } = 0.20;
    public double Dt            { get; set; } = 1.0;
    public double IntegralClamp { get; set; } = 0.0;  // 0 = disabled
    public double OutputClamp   { get; set; } = 0.0;  // 0 = disabled

    public static PidConfig Default()
    {
        var c = new PidConfigNative();
        PidCoreNative.DefaultConfig(ref c);
        return new PidConfig
        {
            ThetaStart = c.ThetaStart, ThetaGoal = c.ThetaGoal,
            Offset = c.Offset, TimeLength = c.TimeLength,
            Kp = c.Kp, Ki = c.Ki, Kd = c.Kd,
            Dt = c.Dt, IntegralClamp = c.IntegralClamp, OutputClamp = c.OutputClamp,
        };
    }

    internal PidConfigNative ToNative() => new()
    {
        ThetaStart = ThetaStart, ThetaGoal = ThetaGoal,
        Offset = Offset, TimeLength = TimeLength,
        Kp = Kp, Ki = Ki, Kd = Kd,
        Dt = Dt, IntegralClamp = IntegralClamp, OutputClamp = OutputClamp,
    };
}

public sealed class PidSimulationResult
{
    public required double[] T          { get; init; }
    public required double[] Theta      { get; init; }
    public required double   FinalTheta { get; init; }
    public required double   MaxTheta   { get; init; }
    public required double   MinTheta   { get; init; }
}

public static class PidSolver
{
    public static string Version()
    {
        var p = PidCoreNative.Version();
        return Marshal.PtrToStringAnsi(p) ?? "pid_core (unknown)";
    }

    public static PidSimulationResult Simulate(PidConfig cfg)
    {
        var native = cfg.ToNative();
        var handle = PidCoreNative.Simulate(ref native);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("pid_core_simulate failed");
        try
        {
            int n = PidCoreNative.SimLength(handle);
            var t  = new double[n];
            var th = new double[n];
            PidCoreNative.SimCopyTime (handle, t,  n);
            PidCoreNative.SimCopyTheta(handle, th, n);
            return new PidSimulationResult
            {
                T          = t,
                Theta      = th,
                FinalTheta = PidCoreNative.SimFinalTheta(handle),
                MaxTheta   = PidCoreNative.SimMaxTheta(handle),
                MinTheta   = PidCoreNative.SimMinTheta(handle),
            };
        }
        finally { PidCoreNative.FreeSimulation(handle); }
    }
}
