// Native/TdofSolver.cs — managed wrapper around the C++ tdof core.

using System;
using System.Runtime.InteropServices;

namespace TdofAvalonia.Native;

public sealed class TdofConfig
{
    public double M   { get; set; } = 0.01;
    public double C   { get; set; } = 0.015;
    public double K   { get; set; } = 1.0;
    public double Kp  { get; set; } = 2.0;
    public double Ki  { get; set; } = 10.0;
    public double Kd  { get; set; } = 0.1;
    public double Ref { get; set; } = 10.0;
    public double TEnd { get; set; } = 2.0;
    public double Dt  { get; set; } = 0.01;

    public static TdofConfig Default()
    {
        var c = new TdofConfigNative();
        TdofCoreNative.DefaultConfig(ref c);
        return new TdofConfig
        {
            M = c.M, C = c.C, K = c.K,
            Kp = c.Kp, Ki = c.Ki, Kd = c.Kd,
            Ref = c.Ref, TEnd = c.TEnd, Dt = c.Dt,
        };
    }

    internal TdofConfigNative ToNative() => new()
    {
        M = M, C = C, K = K, Kp = Kp, Ki = Ki, Kd = Kd,
        Ref = Ref, TEnd = TEnd, Dt = Dt,
    };
}

public sealed class TransferFunction
{
    public required double[] Num { get; init; }
    public required double[] Den { get; init; }
}

public sealed class TdofSimulationResult
{
    public required double[] T      { get; init; }
    public required double[] R      { get; init; }
    public required double[] Z      { get; init; }
    public required double[] YPid   { get; init; }
    public required double[] Y2Dof  { get; init; }
}

public enum TdofSystem { Plant = 0, Pid = 1, Filter = 2, ClosedLoop = 3 }

public static class TdofSolver
{
    public static string Version()
    {
        var p = TdofCoreNative.Version();
        return Marshal.PtrToStringAnsi(p) ?? "tdof_core (unknown)";
    }

    public static TransferFunction GetTf(TdofConfig cfg, TdofSystem which)
    {
        var native = cfg.ToNative();
        const int cap = 32;
        var num = new double[cap];
        var den = new double[cap];
        int nN = cap, nD = cap;
        int ok = TdofCoreNative.GetTf(ref native, (int)which,
                                      num, ref nN, den, ref nD);
        if (ok == 0)
            throw new InvalidOperationException("tdof_core_get_tf failed");
        var numOut = new double[nN];
        var denOut = new double[nD];
        Array.Copy(num, numOut, nN);
        Array.Copy(den, denOut, nD);
        return new TransferFunction { Num = numOut, Den = denOut };
    }

    public static TdofSimulationResult Simulate(TdofConfig cfg)
    {
        var native = cfg.ToNative();
        var handle = TdofCoreNative.Simulate(ref native);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("tdof_core_simulate failed");
        try
        {
            int n = TdofCoreNative.SimLength(handle);
            double[] Copy(Func<IntPtr, double[], int, int> fn)
            {
                var buf = new double[n];
                fn(handle, buf, n);
                return buf;
            }
            return new TdofSimulationResult
            {
                T     = Copy(TdofCoreNative.SimCopyTime),
                R     = Copy(TdofCoreNative.SimCopyR),
                Z     = Copy(TdofCoreNative.SimCopyZ),
                YPid  = Copy(TdofCoreNative.SimCopyYPid),
                Y2Dof = Copy(TdofCoreNative.SimCopyY2Dof),
            };
        }
        finally { TdofCoreNative.FreeSimulation(handle); }
    }
}
