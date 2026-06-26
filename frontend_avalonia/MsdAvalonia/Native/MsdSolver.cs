// Native/MsdSolver.cs — managed wrapper around msd_core.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace MsdAvalonia.Native;

public sealed class MsdCase
{
    public string Name { get; set; } = "case";
    public double M    { get; set; } = 1.0;
    public double C    { get; set; } = 2.0;
    public double K    { get; set; } = 5.0;
    public double ForceAmplitude { get; set; } = 0.5;
    public double ForceOmega     { get; set; } = 2.0;
    public double X0   { get; set; } = 0.0;
    public double V0   { get; set; } = 0.0;

    public static MsdCase Default()
    {
        var n = new MsdCaseNative();
        MsdCoreNative.DefaultCase(ref n);
        return new MsdCase
        {
            Name = "baseline",
            M = n.M, C = n.C, K = n.K,
            ForceAmplitude = n.ForceAmplitude, ForceOmega = n.ForceOmega,
            X0 = n.X0, V0 = n.V0,
        };
    }

    public MsdCase Clone() => new()
    {
        Name = Name, M = M, C = C, K = K,
        ForceAmplitude = ForceAmplitude, ForceOmega = ForceOmega,
        X0 = X0, V0 = V0,
    };

    internal MsdCaseNative ToNative() => new()
    {
        M = M, C = C, K = K,
        ForceAmplitude = ForceAmplitude, ForceOmega = ForceOmega,
        X0 = X0, V0 = V0,
    };

    public (double OmegaN, double Zeta) Derived()
    {
        var n = ToNative();
        MsdCoreNative.Derived(ref n, out var w, out var z);
        return (w, z);
    }
}

public sealed class MsdSamplingConfig
{
    public double Dt   { get; set; } = 0.001;
    public double Stop { get; set; } = 10.0;

    public static MsdSamplingConfig Default()
    {
        var n = new MsdSamplingNative();
        MsdCoreNative.DefaultSampling(ref n);
        return new MsdSamplingConfig { Dt = n.Dt, Stop = n.Stop };
    }

    internal MsdSamplingNative ToNative() => new() { Dt = Dt, Stop = Stop };
}

public sealed class MsdSimulationResult
{
    public required double[] T            { get; init; }
    public required double[] X            { get; init; }
    public required double[] V            { get; init; }
    public required double[] Force        { get; init; }
    public required double   FinalX       { get; init; }
    public required double   FinalV       { get; init; }
    public required double   MaxAbsX      { get; init; }
    public required double   MaxAbsV      { get; init; }
}

public static class MsdSolver
{
    public static string Version()
    {
        var p = MsdCoreNative.Version();
        return Marshal.PtrToStringAnsi(p) ?? "msd_core (unknown)";
    }

    public static MsdSimulationResult Simulate(MsdCase c, MsdSamplingConfig s)
    {
        var cn = c.ToNative();
        var sn = s.ToNative();
        var handle = MsdCoreNative.Simulate(ref cn, ref sn);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("msd_core_simulate failed");
        try
        {
            int n = MsdCoreNative.SimLength(handle);
            var T = new double[n]; var X = new double[n];
            var V = new double[n]; var F = new double[n];
            MsdCoreNative.SimCopyTime    (handle, T, n);
            MsdCoreNative.SimCopyPosition(handle, X, n);
            MsdCoreNative.SimCopyVelocity(handle, V, n);
            MsdCoreNative.SimCopyForce   (handle, F, n);
            return new MsdSimulationResult
            {
                T = T, X = X, V = V, Force = F,
                FinalX = MsdCoreNative.SimFinalPosition (handle),
                FinalV = MsdCoreNative.SimFinalVelocity (handle),
                MaxAbsX = MsdCoreNative.SimMaxAbsPosition(handle),
                MaxAbsV = MsdCoreNative.SimMaxAbsVelocity(handle),
            };
        }
        finally { MsdCoreNative.FreeSimulation(handle); }
    }

    public static List<MsdCase> DefaultCases()
    {
        // The 5 cases from the Python reference script.
        return new List<MsdCase>
        {
            new() { Name = "baseline",               M = 1.0, C = 2.0, K = 5.0,  ForceAmplitude = 0.5, ForceOmega = 2.0 },
            new() { Name = "low damping",            M = 1.0, C = 0.5, K = 5.0,  ForceAmplitude = 0.5, ForceOmega = 2.0 },
            new() { Name = "high damping",           M = 1.0, C = 5.0, K = 5.0,  ForceAmplitude = 0.5, ForceOmega = 2.0 },
            new() { Name = "stiffer spring",         M = 1.0, C = 2.0, K = 12.0, ForceAmplitude = 0.5, ForceOmega = 2.0 },
            new() { Name = "near natural frequency", M = 1.0, C = 0.5, K = 5.0,  ForceAmplitude = 0.5, ForceOmega = 2.2 },
        };
    }
}
