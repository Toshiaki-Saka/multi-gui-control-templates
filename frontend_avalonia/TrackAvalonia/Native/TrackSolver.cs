// Native/TrackSolver.cs — managed wrapper around track_core.

using System;
using System.Runtime.InteropServices;

namespace TrackAvalonia.Native;

public sealed class TrackConfig
{
    public double M               { get; set; } = 0.1;
    public double Izz             { get; set; } = 1.0;
    public double CorneringPower  { get; set; } = 20.0;
    public double H               { get; set; } = 1e-4;
    public double Tc              { get; set; } = 1e-3;
    public double TotalTime       { get; set; } = 0.90;
    public double TargetSpeed     { get; set; } = 1.0;
    public double KyP             { get; set; } = 400.0;
    public double KyI             { get; set; } = 0.0;
    public double KpsiP           { get; set; } = 200.0;
    public double KpsiI           { get; set; } = 0.0;
    public double KrDamping       { get; set; } = 20.0;
    public double NMomentLimit    { get; set; } = 500.0;
    public double FxLimit         { get; set; } = 5.0;
    public double ErrorIntegralLimit { get; set; } = 0.2;
    public int    LookaheadIndex  { get; set; } = 60;
    public double InitialYOffset  { get; set; } = -0.03;
    public double InitialHeadingDeg { get; set; } = 3.0;
    public double Straight1Len    { get; set; } = 0.30;
    public double Radius          { get; set; } = 0.20;
    public double Straight2Len    { get; set; } = 0.30;
    public double Ds              { get; set; } = 0.002;

    public static TrackConfig Default()
    {
        var c = new TrackConfigNative();
        TrackCoreNative.DefaultConfig(ref c);
        return new TrackConfig
        {
            M = c.M, Izz = c.Izz, CorneringPower = c.CorneringPower,
            H = c.H, Tc = c.Tc, TotalTime = c.TotalTime,
            TargetSpeed = c.TargetSpeed,
            KyP = c.KyP, KyI = c.KyI, KpsiP = c.KpsiP, KpsiI = c.KpsiI,
            KrDamping = c.KrDamping,
            NMomentLimit = c.NMomentLimit, FxLimit = c.FxLimit,
            ErrorIntegralLimit = c.ErrorIntegralLimit,
            LookaheadIndex = c.LookaheadIndex,
            InitialYOffset = c.InitialYOffset,
            InitialHeadingDeg = c.InitialHeadingDeg,
            Straight1Len = c.Straight1Len, Radius = c.Radius,
            Straight2Len = c.Straight2Len, Ds = c.Ds,
        };
    }

    internal TrackConfigNative ToNative() => new()
    {
        M = M, Izz = Izz, CorneringPower = CorneringPower,
        H = H, Tc = Tc, TotalTime = TotalTime, TargetSpeed = TargetSpeed,
        KyP = KyP, KyI = KyI, KpsiP = KpsiP, KpsiI = KpsiI,
        KrDamping = KrDamping,
        NMomentLimit = NMomentLimit, FxLimit = FxLimit,
        ErrorIntegralLimit = ErrorIntegralLimit,
        LookaheadIndex = LookaheadIndex,
        InitialYOffset = InitialYOffset,
        InitialHeadingDeg = InitialHeadingDeg,
        Straight1Len = Straight1Len, Radius = Radius,
        Straight2Len = Straight2Len, Ds = Ds,
    };
}

public sealed class ReferencePath
{
    public required double[] X   { get; init; }
    public required double[] Y   { get; init; }
    public required double[] Psi { get; init; }
}

public sealed class TrackSimulationResult
{
    public required double[] T       { get; init; }
    public required double[] X       { get; init; }
    public required double[] Y       { get; init; }
    public required double[] Psi     { get; init; }
    public required double[] U       { get; init; }
    public required double[] V       { get; init; }
    public required double[] R       { get; init; }
    public required double[] Beta    { get; init; }
    public required double[] Ey      { get; init; }
    public required double[] Epsi    { get; init; }
    public required double[] NMoment { get; init; }
    public required double[] Fx      { get; init; }
    public required double[] XRef    { get; init; }
    public required double[] YRef    { get; init; }
    public required double[] PsiRef  { get; init; }

    public required double PathErrorRms { get; init; }
    public required double PathErrorMax { get; init; }
    public required double EyRms        { get; init; }
    public required double EyMaxAbs     { get; init; }
    public required double EpsiRms      { get; init; }
    public required double EpsiMaxAbs   { get; init; }
    public required double NMomentMaxAbs { get; init; }
}

public static class TrackSolver
{
    public static string Version()
    {
        var p = TrackCoreNative.Version();
        return Marshal.PtrToStringAnsi(p) ?? "track_core (unknown)";
    }

    public static ReferencePath MakeReference(TrackConfig cfg)
    {
        var native = cfg.ToNative();
        var handle = TrackCoreNative.MakeReference(ref native);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("track_core_make_reference failed");
        try
        {
            int n = TrackCoreNative.RefLength(handle);
            var x = new double[n]; var y = new double[n]; var psi = new double[n];
            TrackCoreNative.RefCopyX  (handle, x,   n);
            TrackCoreNative.RefCopyY  (handle, y,   n);
            TrackCoreNative.RefCopyPsi(handle, psi, n);
            return new ReferencePath { X = x, Y = y, Psi = psi };
        }
        finally { TrackCoreNative.FreeReference(handle); }
    }

    public static TrackSimulationResult Simulate(TrackConfig cfg)
    {
        var native = cfg.ToNative();
        var handle = TrackCoreNative.Simulate(ref native);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("track_core_simulate failed");
        try
        {
            int n = TrackCoreNative.SimLength(handle);
            double[] mk() => new double[n];
            var T = mk(); var X = mk(); var Y = mk(); var Psi = mk();
            var U = mk(); var V = mk(); var R = mk(); var Beta = mk();
            var Ey = mk(); var Epsi = mk(); var NM = mk(); var Fx = mk();
            var XR = mk(); var YR = mk(); var PR = mk();
            TrackCoreNative.SimCopyTime   (handle, T,    n);
            TrackCoreNative.SimCopyX      (handle, X,    n);
            TrackCoreNative.SimCopyY      (handle, Y,    n);
            TrackCoreNative.SimCopyPsi    (handle, Psi,  n);
            TrackCoreNative.SimCopyU      (handle, U,    n);
            TrackCoreNative.SimCopyV      (handle, V,    n);
            TrackCoreNative.SimCopyR      (handle, R,    n);
            TrackCoreNative.SimCopyBeta   (handle, Beta, n);
            TrackCoreNative.SimCopyEy     (handle, Ey,   n);
            TrackCoreNative.SimCopyEpsi   (handle, Epsi, n);
            TrackCoreNative.SimCopyNMoment(handle, NM,   n);
            TrackCoreNative.SimCopyFx     (handle, Fx,   n);
            TrackCoreNative.SimCopyXRef   (handle, XR,   n);
            TrackCoreNative.SimCopyYRef   (handle, YR,   n);
            TrackCoreNative.SimCopyPsiRef (handle, PR,   n);
            return new TrackSimulationResult
            {
                T = T, X = X, Y = Y, Psi = Psi,
                U = U, V = V, R = R, Beta = Beta,
                Ey = Ey, Epsi = Epsi, NMoment = NM, Fx = Fx,
                XRef = XR, YRef = YR, PsiRef = PR,
                PathErrorRms = TrackCoreNative.SimPathErrorRms(handle),
                PathErrorMax = TrackCoreNative.SimPathErrorMax(handle),
                EyRms        = TrackCoreNative.SimEyRms(handle),
                EyMaxAbs     = TrackCoreNative.SimEyMaxAbs(handle),
                EpsiRms      = TrackCoreNative.SimEpsiRms(handle),
                EpsiMaxAbs   = TrackCoreNative.SimEpsiMaxAbs(handle),
                NMomentMaxAbs = TrackCoreNative.SimNMomentMaxAbs(handle),
            };
        }
        finally { TrackCoreNative.FreeSimulation(handle); }
    }
}
