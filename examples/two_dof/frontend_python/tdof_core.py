"""
tdof_core.py — Python bindings for the C++ 2-DOF comparison core.

Loads libtdof_core.so / .dll / .dylib via ctypes.

Search order:
  1. $TDOF_CORE_LIB
  2. ../core/build/libtdof_core.*
  3. ctypes.util.find_library('tdof_core')
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
        names = ["tdof_core.dll"]
    elif sys.platform == "darwin":
        names = ["libtdof_core.dylib"]
    else:
        names = ["libtdof_core.so"]
    out: List[Path] = []
    env = os.environ.get("TDOF_CORE_LIB")
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
    found = ctypes.util.find_library("tdof_core")
    if found:
        return ctypes.CDLL(found)
    raise OSError(
        "Could not locate the tdof_core shared library. "
        "Build it first (see core/CMakeLists.txt) or set TDOF_CORE_LIB."
    )


_lib = _load_library()


class _TdofConfig(ctypes.Structure):
    _fields_ = [
        ("m",     ctypes.c_double),
        ("c",     ctypes.c_double),
        ("k",     ctypes.c_double),
        ("kp",    ctypes.c_double),
        ("ki",    ctypes.c_double),
        ("kd",    ctypes.c_double),
        ("ref",   ctypes.c_double),
        ("t_end", ctypes.c_double),
        ("dt",    ctypes.c_double),
    ]


_lib.tdof_core_version.restype = ctypes.c_char_p
_lib.tdof_core_version.argtypes = []

_lib.tdof_core_default_config.restype = None
_lib.tdof_core_default_config.argtypes = [ctypes.POINTER(_TdofConfig)]

_lib.tdof_core_get_tf.restype = ctypes.c_int32
_lib.tdof_core_get_tf.argtypes = [
    ctypes.POINTER(_TdofConfig), ctypes.c_int32,
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_int32),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_int32),
]

_lib.tdof_core_simulate.restype = ctypes.c_void_p
_lib.tdof_core_simulate.argtypes = [ctypes.POINTER(_TdofConfig)]

_lib.tdof_core_free_simulation.restype = None
_lib.tdof_core_free_simulation.argtypes = [ctypes.c_void_p]

_lib.tdof_core_sim_length.restype = ctypes.c_int32
_lib.tdof_core_sim_length.argtypes = [ctypes.c_void_p]

for _name in ("time", "r", "z", "y_pid", "y_2dof"):
    fn = getattr(_lib, f"tdof_core_sim_copy_{_name}")
    fn.restype = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]


def version() -> str:
    return _lib.tdof_core_version().decode("utf-8")


@dataclass
class TdofConfig:
    m: float = 1.0e-2
    c: float = 1.5e-2
    k: float = 1.0
    kp: float = 2.0
    ki: float = 10.0
    kd: float = 0.1
    ref: float = 10.0
    t_end: float = 2.0
    dt: float = 0.01

    @classmethod
    def default(cls) -> "TdofConfig":
        c = _TdofConfig()
        _lib.tdof_core_default_config(ctypes.byref(c))
        return cls(m=c.m, c=c.c, k=c.k, kp=c.kp, ki=c.ki, kd=c.kd,
                   ref=c.ref, t_end=c.t_end, dt=c.dt)

    def _to_c(self) -> _TdofConfig:
        return _TdofConfig(m=self.m, c=self.c, k=self.k,
                           kp=self.kp, ki=self.ki, kd=self.kd,
                           ref=self.ref, t_end=self.t_end, dt=self.dt)


# Which-index → system name
PLANT, PID, FILTER, CLOSED_LOOP = 0, 1, 2, 3


@dataclass
class TransferFunction:
    num: np.ndarray
    den: np.ndarray

    def __str__(self) -> str:
        return f"num={list(self.num)}  den={list(self.den)}"


def get_tf(which: int, cfg: Optional[TdofConfig] = None) -> TransferFunction:
    """Return the numerator/denominator of one of the systems.

    which: 0=Plant, 1=PID, 2=Filter, 3=ClosedLoop.
    """
    if cfg is None:
        cfg = TdofConfig.default()
    cfg_c = cfg._to_c()
    cap = 32
    num = (ctypes.c_double * cap)()
    den = (ctypes.c_double * cap)()
    nN = ctypes.c_int32(cap)
    nD = ctypes.c_int32(cap)
    ok = _lib.tdof_core_get_tf(ctypes.byref(cfg_c), int(which),
                               num, ctypes.byref(nN),
                               den, ctypes.byref(nD))
    if not ok:
        raise RuntimeError("tdof_core_get_tf failed")
    return TransferFunction(
        num=np.frombuffer(num, dtype=np.float64)[: nN.value].copy(),
        den=np.frombuffer(den, dtype=np.float64)[: nD.value].copy(),
    )


@dataclass
class Simulation:
    """All signals scaled by cfg.ref (display scale)."""
    t: np.ndarray
    r: np.ndarray
    z: np.ndarray
    y_pid: np.ndarray
    y_2dof: np.ndarray


def simulate(cfg: Optional[TdofConfig] = None) -> Simulation:
    if cfg is None:
        cfg = TdofConfig.default()
    cfg_c = cfg._to_c()
    handle = _lib.tdof_core_simulate(ctypes.byref(cfg_c))
    if not handle:
        raise RuntimeError("tdof_core_simulate failed (check config)")
    try:
        n = _lib.tdof_core_sim_length(handle)

        def _copy(name: str) -> np.ndarray:
            buf = (ctypes.c_double * n)()
            getattr(_lib, f"tdof_core_sim_copy_{name}")(handle, buf, n)
            return np.frombuffer(buf, dtype=np.float64).copy()

        return Simulation(
            t=_copy("time"),
            r=_copy("r"),
            z=_copy("z"),
            y_pid=_copy("y_pid"),
            y_2dof=_copy("y_2dof"),
        )
    finally:
        _lib.tdof_core_free_simulation(handle)


__all__ = [
    "version", "TdofConfig", "TransferFunction", "Simulation",
    "get_tf", "simulate",
    "PLANT", "PID", "FILTER", "CLOSED_LOOP",
]
