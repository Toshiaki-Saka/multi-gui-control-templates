// Native/TdofCoreNative.cs — raw P/Invoke declarations for tdof_core.

using System;
using System.Runtime.InteropServices;

namespace TdofAvalonia.Native;

[StructLayout(LayoutKind.Sequential)]
public struct TdofConfigNative
{
    public double M;
    public double C;
    public double K;
    public double Kp;
    public double Ki;
    public double Kd;
    public double Ref;
    public double TEnd;
    public double Dt;
}

internal static class TdofCoreNative
{
    public const string Lib = "tdof_core";

    [DllImport(Lib, EntryPoint = "tdof_core_version",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Version();

    [DllImport(Lib, EntryPoint = "tdof_core_default_config",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultConfig(ref TdofConfigNative cfg);

    [DllImport(Lib, EntryPoint = "tdof_core_get_tf",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetTf(ref TdofConfigNative cfg, int which,
                                   [Out] double[] num, ref int numLen,
                                   [Out] double[] den, ref int denLen);

    [DllImport(Lib, EntryPoint = "tdof_core_simulate",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Simulate(ref TdofConfigNative cfg);

    [DllImport(Lib, EntryPoint = "tdof_core_free_simulation",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeSimulation(IntPtr handle);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_length",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimLength(IntPtr handle);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_copy_time",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyTime(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_copy_r",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyR(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_copy_z",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyZ(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_copy_y_pid",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyYPid(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "tdof_core_sim_copy_y_2dof",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyY2Dof(IntPtr handle, [Out] double[] buf, int bufLen);
}
