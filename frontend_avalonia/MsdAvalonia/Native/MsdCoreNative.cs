// Native/MsdCoreNative.cs — raw P/Invoke declarations for msd_core.

using System;
using System.Runtime.InteropServices;

namespace MsdAvalonia.Native;

[StructLayout(LayoutKind.Sequential)]
public struct MsdCaseNative
{
    public double M;
    public double C;
    public double K;
    public double ForceAmplitude;
    public double ForceOmega;
    public double X0;
    public double V0;
}

[StructLayout(LayoutKind.Sequential)]
public struct MsdSamplingNative
{
    public double Dt;
    public double Stop;
}

internal static class MsdCoreNative
{
    public const string Lib = "msd_core";

    [DllImport(Lib, EntryPoint = "msd_core_version",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Version();

    [DllImport(Lib, EntryPoint = "msd_core_default_case",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultCase(ref MsdCaseNative c);

    [DllImport(Lib, EntryPoint = "msd_core_default_sampling",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultSampling(ref MsdSamplingNative s);

    [DllImport(Lib, EntryPoint = "msd_core_derived",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int Derived(ref MsdCaseNative c,
                                     out double omegaN, out double zeta);

    [DllImport(Lib, EntryPoint = "msd_core_simulate",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Simulate(ref MsdCaseNative c, ref MsdSamplingNative s);

    [DllImport(Lib, EntryPoint = "msd_core_free_simulation",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeSimulation(IntPtr handle);

    [DllImport(Lib, EntryPoint = "msd_core_sim_length",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimLength(IntPtr handle);

    [DllImport(Lib, EntryPoint = "msd_core_sim_copy_time",     CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyTime    (IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "msd_core_sim_copy_position", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyPosition(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "msd_core_sim_copy_velocity", CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyVelocity(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "msd_core_sim_copy_force",    CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyForce   (IntPtr h, [Out] double[] b, int n);

    [DllImport(Lib, EntryPoint = "msd_core_sim_final_position",   CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimFinalPosition  (IntPtr h);
    [DllImport(Lib, EntryPoint = "msd_core_sim_final_velocity",   CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimFinalVelocity  (IntPtr h);
    [DllImport(Lib, EntryPoint = "msd_core_sim_max_abs_position", CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimMaxAbsPosition (IntPtr h);
    [DllImport(Lib, EntryPoint = "msd_core_sim_max_abs_velocity", CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimMaxAbsVelocity (IntPtr h);
}
