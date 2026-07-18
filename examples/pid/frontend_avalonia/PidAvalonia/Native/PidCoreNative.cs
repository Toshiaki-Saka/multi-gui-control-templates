// Native/PidCoreNative.cs — raw P/Invoke declarations for pid_core.

using System;
using System.Runtime.InteropServices;

namespace PidAvalonia.Native;

[StructLayout(LayoutKind.Sequential)]
public struct PidConfigNative
{
    public double ThetaStart;
    public double ThetaGoal;
    public double Offset;
    public int    TimeLength;
    public double Kp;
    public double Ki;
    public double Kd;
    public double Dt;
    public double IntegralClamp;
    public double OutputClamp;
}

internal static class PidCoreNative
{
    public const string Lib = "pid_core";

    [DllImport(Lib, EntryPoint = "pid_core_version",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Version();

    [DllImport(Lib, EntryPoint = "pid_core_default_config",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultConfig(ref PidConfigNative cfg);

    [DllImport(Lib, EntryPoint = "pid_core_simulate",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Simulate(ref PidConfigNative cfg);

    [DllImport(Lib, EntryPoint = "pid_core_free_simulation",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeSimulation(IntPtr handle);

    [DllImport(Lib, EntryPoint = "pid_core_sim_length",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimLength(IntPtr handle);

    [DllImport(Lib, EntryPoint = "pid_core_sim_copy_time",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyTime(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "pid_core_sim_copy_theta",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int SimCopyTheta(IntPtr handle, [Out] double[] buf, int bufLen);

    [DllImport(Lib, EntryPoint = "pid_core_sim_final_theta",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimFinalTheta(IntPtr handle);

    [DllImport(Lib, EntryPoint = "pid_core_sim_max_theta",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimMaxTheta(IntPtr handle);

    [DllImport(Lib, EntryPoint = "pid_core_sim_min_theta",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern double SimMinTheta(IntPtr handle);
}
