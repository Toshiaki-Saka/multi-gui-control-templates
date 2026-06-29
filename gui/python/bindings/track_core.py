"""
track_core.py — Python bindings for the C++ planar-tracking core.

Loads libtrack_core.so / .dylib / track_core.dll via ctypes.

Search order:
  1. $TRACK_CORE_LIB
  2. ../core/build/{Release,Debug,}      (Windows MSVC multi-config)
  3. ctypes.util.find_library('track_core')
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
    if sys.platform.startswith("win"):
        names = ["track_core.dll"]
    elif sys.platform == "darwin":
        names = ["libtrack_core.dylib"]
    else:
        names = ["libtrack_core.so"]
    out: List[Path] = []
    env = os.environ.get("TRACK_CORE_LIB")
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
    found = ctypes.util.find_library("track_core")
    if found:
        return ctypes.CDLL(found)
    raise OSError(
        "Could not locate the track_core shared library. "
        "Build it first (see core/CMakeLists.txt) or set TRACK_CORE_LIB."
    )


_lib = _load_library()


class _TrackConfig(ctypes.Structure):
    _fields_ = [
        ("m",                   ctypes.c_double),
        ("izz",                 ctypes.c_double),
        ("cornering_power",     ctypes.c_double),
        ("h",                   ctypes.c_double),
        ("tc",                  ctypes.c_double),
        ("total_time",          ctypes.c_double),
        ("target_speed",        ctypes.c_double),
        ("ky_p",                ctypes.c_double),
        ("ky_i",                ctypes.c_double),
        ("kpsi_p",              ctypes.c_double),
        ("kpsi_i",              ctypes.c_double),
        ("kr_damping",          ctypes.c_double),
        ("n_moment_limit",      ctypes.c_double),
        ("fx_limit",            ctypes.c_double),
        ("error_integral_limit", ctypes.c_double),
        ("lookahead_index",     ctypes.c_int32),
        ("initial_y_offset",    ctypes.c_double),
        ("initial_heading_deg", ctypes.c_double),
        ("straight1_len",       ctypes.c_double),
        ("radius",              ctypes.c_double),
        ("straight2_len",       ctypes.c_double),
        ("ds",                  ctypes.c_double),
    ]


_lib.track_core_version.restype = ctypes.c_char_p
_lib.track_core_version.argtypes = []

_lib.track_core_default_config.restype = None
_lib.track_core_default_config.argtypes = [ctypes.POINTER(_TrackConfig)]

_lib.track_core_make_reference.restype = ctypes.c_void_p
_lib.track_core_make_reference.argtypes = [ctypes.POINTER(_TrackConfig)]
_lib.track_core_free_reference.restype = None
_lib.track_core_free_reference.argtypes = [ctypes.c_void_p]
_lib.track_core_ref_length.restype = ctypes.c_int32
_lib.track_core_ref_length.argtypes = [ctypes.c_void_p]
for _name in ("x", "y", "psi"):
    fn = getattr(_lib, f"track_core_ref_copy_{_name}")
    fn.restype = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]

_lib.track_core_simulate.restype = ctypes.c_void_p
_lib.track_core_simulate.argtypes = [ctypes.POINTER(_TrackConfig)]
_lib.track_core_free_simulation.restype = None
_lib.track_core_free_simulation.argtypes = [ctypes.c_void_p]
_lib.track_core_sim_length.restype = ctypes.c_int32
_lib.track_core_sim_length.argtypes = [ctypes.c_void_p]

_SIM_FIELDS = [
    "time", "x", "y", "psi", "u", "v", "r", "beta",
    "ey", "epsi", "nmoment", "fx", "x_ref", "y_ref", "psi_ref",
]
for _name in _SIM_FIELDS:
    fn = getattr(_lib, f"track_core_sim_copy_{_name}")
    fn.restype = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]

for _name in ("path_error_rms", "path_error_max",
              "ey_rms", "ey_max_abs",
              "epsi_rms", "epsi_max_abs",
              "nmoment_max_abs"):
    fn = getattr(_lib, f"track_core_sim_{_name}")
    fn.restype = ctypes.c_double
    fn.argtypes = [ctypes.c_void_p]


def version() -> str:
    return _lib.track_core_version().decode("utf-8")


@dataclass
class TrackConfig:
    m:               float = 0.1
    izz:             float = 1.0
    cornering_power: float = 20.0
    h:               float = 1.0e-4
    tc:              float = 1.0e-3
    total_time:      float = 0.90
    target_speed:    float = 1.0
    ky_p:            float = 400.0
    ky_i:            float = 0.0
    kpsi_p:          float = 200.0
    kpsi_i:          float = 0.0
    kr_damping:      float = 20.0
    n_moment_limit:  float = 500.0
    fx_limit:        float = 5.0
    error_integral_limit: float = 0.2
    lookahead_index: int   = 60
    initial_y_offset: float = -0.03
    initial_heading_deg: float = 3.0
    straight1_len:   float = 0.30
    radius:          float = 0.20
    straight2_len:   float = 0.30
    ds:              float = 0.002

    @classmethod
    def default(cls) -> "TrackConfig":
        c = _TrackConfig()
        _lib.track_core_default_config(ctypes.byref(c))
        return cls(
            m=c.m, izz=c.izz, cornering_power=c.cornering_power,
            h=c.h, tc=c.tc, total_time=c.total_time,
            target_speed=c.target_speed,
            ky_p=c.ky_p, ky_i=c.ky_i, kpsi_p=c.kpsi_p, kpsi_i=c.kpsi_i,
            kr_damping=c.kr_damping,
            n_moment_limit=c.n_moment_limit, fx_limit=c.fx_limit,
            error_integral_limit=c.error_integral_limit,
            lookahead_index=c.lookahead_index,
            initial_y_offset=c.initial_y_offset,
            initial_heading_deg=c.initial_heading_deg,
            straight1_len=c.straight1_len, radius=c.radius,
            straight2_len=c.straight2_len, ds=c.ds,
        )

    def _to_c(self) -> _TrackConfig:
        return _TrackConfig(
            m=self.m, izz=self.izz, cornering_power=self.cornering_power,
            h=self.h, tc=self.tc, total_time=self.total_time,
            target_speed=self.target_speed,
            ky_p=self.ky_p, ky_i=self.ky_i,
            kpsi_p=self.kpsi_p, kpsi_i=self.kpsi_i,
            kr_damping=self.kr_damping,
            n_moment_limit=self.n_moment_limit, fx_limit=self.fx_limit,
            error_integral_limit=self.error_integral_limit,
            lookahead_index=int(self.lookahead_index),
            initial_y_offset=self.initial_y_offset,
            initial_heading_deg=self.initial_heading_deg,
            straight1_len=self.straight1_len, radius=self.radius,
            straight2_len=self.straight2_len, ds=self.ds,
        )


@dataclass
class ReferencePath:
    x:   np.ndarray
    y:   np.ndarray
    psi: np.ndarray


def make_reference(cfg: Optional[TrackConfig] = None) -> ReferencePath:
    if cfg is None: cfg = TrackConfig.default()
    cc = cfg._to_c()
    h = _lib.track_core_make_reference(ctypes.byref(cc))
    if not h:
        raise RuntimeError("make_reference failed")
    try:
        n = _lib.track_core_ref_length(h)
        bx = (ctypes.c_double * n)()
        by = (ctypes.c_double * n)()
        bp = (ctypes.c_double * n)()
        _lib.track_core_ref_copy_x  (h, bx, n)
        _lib.track_core_ref_copy_y  (h, by, n)
        _lib.track_core_ref_copy_psi(h, bp, n)
        return ReferencePath(
            x=np.frombuffer(bx, dtype=np.float64).copy(),
            y=np.frombuffer(by, dtype=np.float64).copy(),
            psi=np.frombuffer(bp, dtype=np.float64).copy(),
        )
    finally:
        _lib.track_core_free_reference(h)


@dataclass
class Simulation:
    """All time-series arrays + the aggregate metrics."""
    t: np.ndarray
    x: np.ndarray; y: np.ndarray; psi: np.ndarray
    u: np.ndarray; v: np.ndarray; r: np.ndarray; beta: np.ndarray
    ey: np.ndarray; epsi: np.ndarray
    n_moment: np.ndarray; fx: np.ndarray
    x_ref: np.ndarray; y_ref: np.ndarray; psi_ref: np.ndarray

    path_error_rms: float
    path_error_max: float
    ey_rms: float;   ey_max:   float
    epsi_rms: float; epsi_max: float
    nmoment_max: float

    @property
    def path_error(self) -> np.ndarray:
        return np.hypot(self.x - self.x_ref, self.y - self.y_ref)


def simulate(cfg: Optional[TrackConfig] = None) -> Simulation:
    if cfg is None: cfg = TrackConfig.default()
    cc = cfg._to_c()
    handle = _lib.track_core_simulate(ctypes.byref(cc))
    if not handle:
        raise RuntimeError("simulate failed (check config)")
    try:
        n = _lib.track_core_sim_length(handle)
        def _copy(name):
            buf = (ctypes.c_double * n)()
            getattr(_lib, f"track_core_sim_copy_{name}")(handle, buf, n)
            return np.frombuffer(buf, dtype=np.float64).copy()
        return Simulation(
            t=_copy("time"),
            x=_copy("x"), y=_copy("y"), psi=_copy("psi"),
            u=_copy("u"), v=_copy("v"), r=_copy("r"), beta=_copy("beta"),
            ey=_copy("ey"), epsi=_copy("epsi"),
            n_moment=_copy("nmoment"), fx=_copy("fx"),
            x_ref=_copy("x_ref"), y_ref=_copy("y_ref"), psi_ref=_copy("psi_ref"),
            path_error_rms=_lib.track_core_sim_path_error_rms(handle),
            path_error_max=_lib.track_core_sim_path_error_max(handle),
            ey_rms=_lib.track_core_sim_ey_rms(handle),
            ey_max=_lib.track_core_sim_ey_max_abs(handle),
            epsi_rms=_lib.track_core_sim_epsi_rms(handle),
            epsi_max=_lib.track_core_sim_epsi_max_abs(handle),
            nmoment_max=_lib.track_core_sim_nmoment_max_abs(handle),
        )
    finally:
        _lib.track_core_free_simulation(handle)


__all__ = [
    "version", "TrackConfig", "ReferencePath", "Simulation",
    "make_reference", "simulate",
]
