"""
msd_core.py — Python bindings for the C++ MSD core.

Loads libmsd_core.so / .dylib / msd_core.dll via ctypes.

Search order:
  1. $MSD_CORE_LIB
  2. ../core/build/{Release,Debug,}      (Windows MSVC multi-config)
  3. ctypes.util.find_library('msd_core')
"""

from __future__ import annotations

import ctypes
import ctypes.util
import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

import numpy as np


def _candidate_paths() -> List[Path]:
    here = Path(__file__).resolve().parent
    if sys.platform.startswith("win"):
        names = ["msd_core.dll"]
    elif sys.platform == "darwin":
        names = ["libmsd_core.dylib"]
    else:
        names = ["libmsd_core.so"]
    out: List[Path] = []
    env = os.environ.get("MSD_CORE_LIB")
    if env:
        out.append(Path(env))
    base_dirs = [
        (here / ".." / "core" / "build").resolve(),
        (here / ".." / "core" / "build" / "Release").resolve(),
        (here / ".." / "core" / "build" / "Debug").resolve(),
        here, here / "..", here.parent,
    ]
    for d in base_dirs:
        for n in names:
            out.append(d / n)
    return out


def _load_library() -> ctypes.CDLL:
    for p in _candidate_paths():
        if p.is_file():
            return ctypes.CDLL(str(p))
    found = ctypes.util.find_library("msd_core")
    if found:
        return ctypes.CDLL(found)
    raise OSError(
        "Could not locate the msd_core shared library. "
        "Build it first (see core/CMakeLists.txt) or set MSD_CORE_LIB."
    )


_lib = _load_library()


class _MsdCase(ctypes.Structure):
    _fields_ = [
        ("m",               ctypes.c_double),
        ("c",               ctypes.c_double),
        ("k",               ctypes.c_double),
        ("force_amplitude", ctypes.c_double),
        ("force_omega",     ctypes.c_double),
        ("x0",              ctypes.c_double),
        ("v0",              ctypes.c_double),
    ]


class _MsdSampling(ctypes.Structure):
    _fields_ = [
        ("dt",   ctypes.c_double),
        ("stop", ctypes.c_double),
    ]


_lib.msd_core_version.restype = ctypes.c_char_p
_lib.msd_core_version.argtypes = []

_lib.msd_core_default_case.restype = None
_lib.msd_core_default_case.argtypes = [ctypes.POINTER(_MsdCase)]

_lib.msd_core_default_sampling.restype = None
_lib.msd_core_default_sampling.argtypes = [ctypes.POINTER(_MsdSampling)]

_lib.msd_core_derived.restype = ctypes.c_int32
_lib.msd_core_derived.argtypes = [
    ctypes.POINTER(_MsdCase),
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double),
]

_lib.msd_core_simulate.restype = ctypes.c_void_p
_lib.msd_core_simulate.argtypes = [
    ctypes.POINTER(_MsdCase), ctypes.POINTER(_MsdSampling)]

_lib.msd_core_free_simulation.restype = None
_lib.msd_core_free_simulation.argtypes = [ctypes.c_void_p]

_lib.msd_core_sim_length.restype = ctypes.c_int32
_lib.msd_core_sim_length.argtypes = [ctypes.c_void_p]

for _name in ("time", "position", "velocity", "force"):
    fn = getattr(_lib, f"msd_core_sim_copy_{_name}")
    fn.restype = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]

for _name in ("final_position", "final_velocity",
              "max_abs_position", "max_abs_velocity"):
    fn = getattr(_lib, f"msd_core_sim_{_name}")
    fn.restype = ctypes.c_double
    fn.argtypes = [ctypes.c_void_p]


def version() -> str:
    return _lib.msd_core_version().decode("utf-8")


@dataclass
class MsdCase:
    """One simulation case (plant + forcing + initial state)."""
    name:            str   = "case"
    m:               float = 1.0
    c:               float = 2.0
    k:               float = 5.0
    force_amplitude: float = 0.5
    force_omega:     float = 2.0
    x0:              float = 0.0
    v0:              float = 0.0

    @classmethod
    def default(cls) -> "MsdCase":
        c = _MsdCase(); _lib.msd_core_default_case(ctypes.byref(c))
        return cls(name="baseline",
                   m=c.m, c=c.c, k=c.k,
                   force_amplitude=c.force_amplitude,
                   force_omega=c.force_omega,
                   x0=c.x0, v0=c.v0)

    def _to_c(self) -> _MsdCase:
        return _MsdCase(
            m=float(self.m), c=float(self.c), k=float(self.k),
            force_amplitude=float(self.force_amplitude),
            force_omega=float(self.force_omega),
            x0=float(self.x0), v0=float(self.v0),
        )

    @property
    def omega_n(self) -> float:
        return math.sqrt(self.k / self.m) if self.m > 0 else 0.0

    @property
    def zeta(self) -> float:
        d = 2.0 * math.sqrt(self.m * self.k) if self.m > 0 and self.k > 0 else 0.0
        return self.c / d if d > 0.0 else 0.0


@dataclass
class SamplingConfig:
    dt:   float = 0.001
    stop: float = 10.0

    @classmethod
    def default(cls) -> "SamplingConfig":
        s = _MsdSampling(); _lib.msd_core_default_sampling(ctypes.byref(s))
        return cls(dt=s.dt, stop=s.stop)

    def _to_c(self) -> _MsdSampling:
        return _MsdSampling(dt=float(self.dt), stop=float(self.stop))


@dataclass
class Simulation:
    """Results for one case."""
    t:        np.ndarray
    x:        np.ndarray
    v:        np.ndarray
    force:    np.ndarray
    final_x:  float
    final_v:  float
    max_abs_x: float
    max_abs_v: float


def simulate(case: MsdCase,
             sampling: Optional[SamplingConfig] = None) -> Simulation:
    if sampling is None: sampling = SamplingConfig.default()
    cc = case._to_c()
    ss = sampling._to_c()
    handle = _lib.msd_core_simulate(ctypes.byref(cc), ctypes.byref(ss))
    if not handle:
        raise RuntimeError("msd_core_simulate failed (check parameters)")
    try:
        n = _lib.msd_core_sim_length(handle)
        def _copy(name):
            buf = (ctypes.c_double * n)()
            getattr(_lib, f"msd_core_sim_copy_{name}")(handle, buf, n)
            return np.frombuffer(buf, dtype=np.float64).copy()
        return Simulation(
            t=_copy("time"), x=_copy("position"),
            v=_copy("velocity"), force=_copy("force"),
            final_x=_lib.msd_core_sim_final_position(handle),
            final_v=_lib.msd_core_sim_final_velocity(handle),
            max_abs_x=_lib.msd_core_sim_max_abs_position(handle),
            max_abs_v=_lib.msd_core_sim_max_abs_velocity(handle),
        )
    finally:
        _lib.msd_core_free_simulation(handle)


def default_cases() -> List[MsdCase]:
    """The 5 cases from the Python reference script."""
    return [
        MsdCase(name="baseline",
                m=1.0, c=2.0, k=5.0,  force_amplitude=0.5, force_omega=2.0),
        MsdCase(name="low damping",
                m=1.0, c=0.5, k=5.0,  force_amplitude=0.5, force_omega=2.0),
        MsdCase(name="high damping",
                m=1.0, c=5.0, k=5.0,  force_amplitude=0.5, force_omega=2.0),
        MsdCase(name="stiffer spring",
                m=1.0, c=2.0, k=12.0, force_amplitude=0.5, force_omega=2.0),
        MsdCase(name="near natural frequency",
                m=1.0, c=0.5, k=5.0,  force_amplitude=0.5, force_omega=2.2),
    ]


__all__ = [
    "version", "MsdCase", "SamplingConfig", "Simulation",
    "simulate", "default_cases",
]
