"""
pid_core.py — Python bindings for the C++ PID demo core.

Loads libpid_core.so / .dll / .dylib via ctypes.

Search order:
  1. $PID_CORE_LIB
  2. ../core/build/libpid_core.*
  3. ctypes.util.find_library('pid_core')
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

import numpy as np


def _candidate_paths() -> List[Path]:
    here = Path(__file__).resolve().parent
    core_build = (here / ".." / "core" / "build").resolve()
    if sys.platform.startswith("win"):
        names = ["pid_core.dll"]
    elif sys.platform == "darwin":
        names = ["libpid_core.dylib"]
    else:
        names = ["libpid_core.so"]
    out: List[Path] = []
    env = os.environ.get("PID_CORE_LIB")
    if env:
        out.append(Path(env))
    for d in (core_build, here, here / "..", here.parent):
        for n in names:
            out.append(Path(d) / n)
    return out


def _load_library() -> ctypes.CDLL:
    for p in _candidate_paths():
        if p.is_file():
            return ctypes.CDLL(str(p))
    found = ctypes.util.find_library("pid_core")
    if found:
        return ctypes.CDLL(found)
    raise OSError(
        "Could not locate the pid_core shared library. "
        "Build it first (see core/CMakeLists.txt) or set PID_CORE_LIB."
    )


_lib = _load_library()


class _PidConfig(ctypes.Structure):
    _fields_ = [
        ("theta_start",     ctypes.c_double),
        ("theta_goal",      ctypes.c_double),
        ("offset",          ctypes.c_double),
        ("time_length",     ctypes.c_int32),
        ("kp",              ctypes.c_double),
        ("ki",              ctypes.c_double),
        ("kd",              ctypes.c_double),
        ("dt",              ctypes.c_double),
        ("integral_clamp",  ctypes.c_double),
        ("output_clamp",    ctypes.c_double),
    ]


_lib.pid_core_version.restype = ctypes.c_char_p
_lib.pid_core_version.argtypes = []

_lib.pid_core_default_config.restype = None
_lib.pid_core_default_config.argtypes = [ctypes.POINTER(_PidConfig)]

_lib.pid_core_simulate.restype = ctypes.c_void_p
_lib.pid_core_simulate.argtypes = [ctypes.POINTER(_PidConfig)]

_lib.pid_core_free_simulation.restype = None
_lib.pid_core_free_simulation.argtypes = [ctypes.c_void_p]

_lib.pid_core_sim_length.restype = ctypes.c_int32
_lib.pid_core_sim_length.argtypes = [ctypes.c_void_p]

for _name in ("time", "theta"):
    fn = getattr(_lib, f"pid_core_sim_copy_{_name}")
    fn.restype = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]

for _name in ("final_theta", "max_theta", "min_theta"):
    fn = getattr(_lib, f"pid_core_sim_{_name}")
    fn.restype = ctypes.c_double
    fn.argtypes = [ctypes.c_void_p]


def version() -> str:
    return _lib.pid_core_version().decode("utf-8")


@dataclass
class PidConfig:
    theta_start:    float = 0.0
    theta_goal:     float = 90.0
    offset:         float = 0.0
    time_length:    int   = 150
    kp:             float = 0.10
    ki:             float = 0.01
    kd:             float = 0.20
    dt:             float = 1.0
    integral_clamp: float = 0.0  # 0 = disabled
    output_clamp:   float = 0.0  # 0 = disabled

    @classmethod
    def default(cls) -> "PidConfig":
        c = _PidConfig()
        _lib.pid_core_default_config(ctypes.byref(c))
        return cls(theta_start=c.theta_start, theta_goal=c.theta_goal,
                   offset=c.offset, time_length=c.time_length,
                   kp=c.kp, ki=c.ki, kd=c.kd,
                   dt=c.dt, integral_clamp=c.integral_clamp,
                   output_clamp=c.output_clamp)

    def _to_c(self) -> _PidConfig:
        return _PidConfig(
            theta_start=float(self.theta_start),
            theta_goal=float(self.theta_goal),
            offset=float(self.offset),
            time_length=int(self.time_length),
            kp=float(self.kp), ki=float(self.ki), kd=float(self.kd),
            dt=float(self.dt),
            integral_clamp=float(self.integral_clamp),
            output_clamp=float(self.output_clamp),
        )


@dataclass
class Simulation:
    t:      np.ndarray
    theta:  np.ndarray
    final:  float
    max:    float
    min:    float


def simulate(cfg: Optional[PidConfig] = None) -> Simulation:
    if cfg is None:
        cfg = PidConfig.default()
    c = cfg._to_c()
    handle = _lib.pid_core_simulate(ctypes.byref(c))
    if not handle:
        raise RuntimeError("pid_core_simulate failed (check config)")
    try:
        n = _lib.pid_core_sim_length(handle)
        t_buf  = (ctypes.c_double * n)()
        th_buf = (ctypes.c_double * n)()
        _lib.pid_core_sim_copy_time (handle, t_buf,  n)
        _lib.pid_core_sim_copy_theta(handle, th_buf, n)
        return Simulation(
            t=np.frombuffer(t_buf,  dtype=np.float64).copy(),
            theta=np.frombuffer(th_buf, dtype=np.float64).copy(),
            final=_lib.pid_core_sim_final_theta(handle),
            max=_lib.pid_core_sim_max_theta(handle),
            min=_lib.pid_core_sim_min_theta(handle),
        )
    finally:
        _lib.pid_core_free_simulation(handle)


__all__ = ["version", "PidConfig", "Simulation", "simulate"]
