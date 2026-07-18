// Native/TrackCoreNative.cs — raw P/Invoke declarations for track_core.

using System;
using System.Runtime.InteropServices;

namespace TrackAvalonia.Native;

[StructLayout(LayoutKind.Sequential)]
public struct TrackConfigNative
{
    public double M;
    public double Izz;
    public double CorneringPower;
    public double H;
    public double Tc;
    public double TotalTime;
    public double TargetSpeed;
    public double KyP;
    public double KyI;
    public double KpsiP;
    public double KpsiI;
    public double KrDamping;
    public double NMomentLimit;
    public double FxLimit;
    public double ErrorIntegralLimit;
    public int    LookaheadIndex;
    public double InitialYOffset;
    public double InitialHeadingDeg;
    public double Straight1Len;
    public double Radius;
    public double Straight2Len;
    public double Ds;
}

internal static class TrackCoreNative
{
    public const string Lib = "track_core";

    [DllImport(Lib, EntryPoint = "track_core_version",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Version();

    [DllImport(Lib, EntryPoint = "track_core_default_config",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultConfig(ref TrackConfigNative cfg);

    // Reference path
    [DllImport(Lib, EntryPoint = "track_core_make_reference",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr MakeReference(ref TrackConfigNative cfg);

    [DllImport(Lib, EntryPoint = "track_core_free_reference",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeReference(IntPtr handle);

    [DllImport(Lib, EntryPoint = "track_core_ref_length",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int RefLength(IntPtr handle);

    [DllImport(Lib, EntryPoint = "track_core_ref_copy_x",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int RefCopyX(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "track_core_ref_copy_y",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int RefCopyY(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "track_core_ref_copy_psi",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int RefCopyPsi(IntPtr handle, [Out] double[] buf, int bufLen);

    // Simulation
    [DllImport(Lib, EntryPoint = "track_core_simulate",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Simulate(ref TrackConfigNative cfg);

    [DllImport(Lib, EntryPoint = "track_core_free_simulation",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeSimulation(IntPtr handle);

    [DllImport(Lib, EntryPoint = "track_core_sim_length",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimLength(IntPtr handle);

    [DllImport(Lib, EntryPoint = "track_core_sim_copy_time",    CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyTime   (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_x",       CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyX      (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_y",       CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyY      (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_psi",     CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyPsi    (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_u",       CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyU      (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_v",       CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyV      (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_r",       CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyR      (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_beta",    CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyBeta   (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_ey",      CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyEy     (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_epsi",    CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyEpsi   (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_nmoment", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyNMoment(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_fx",      CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyFx     (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_x_ref",   CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyXRef   (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_y_ref",   CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyYRef   (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "track_core_sim_copy_psi_ref", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyPsiRef (IntPtr h, [Out] double[] b, int n);

    [DllImport(Lib, EntryPoint = "track_core_sim_path_error_rms", CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimPathErrorRms(IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_path_error_max", CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimPathErrorMax(IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_ey_rms",         CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimEyRms        (IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_ey_max_abs",     CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimEyMaxAbs     (IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_epsi_rms",       CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimEpsiRms      (IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_epsi_max_abs",   CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimEpsiMaxAbs   (IntPtr h);
    [DllImport(Lib, EntryPoint = "track_core_sim_nmoment_max_abs", CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimNMomentMaxAbs(IntPtr h);
}
