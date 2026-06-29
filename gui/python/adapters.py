"""
adapters.py — run each control example's C ABI core and normalise its native
time-series output into a common :class:`RunResult` so one GUI launcher can
display any of them.

This is the "1コア×3GUI" template made concrete on the Python side: every
example exposes a ``<name>_core_simulate()`` C ABI; the adapter wraps it and
returns a uniform, plottable structure regardless of the underlying dynamics.

``libloader`` is imported first so the ``*_CORE_LIB`` env vars point at the
freshly built shared libraries before the bindings load them.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Tuple

import numpy as np

import libloader  # noqa: F401  (primes <NAME>_CORE_LIB on import)


@dataclass
class Trace:
    label: str
    x: np.ndarray
    y: np.ndarray
    style: str = "-"


@dataclass
class Plot:
    title: str
    xlabel: str
    ylabel: str
    traces: List[Trace] = field(default_factory=list)
    equal_aspect: bool = False


@dataclass
class RunResult:
    name: str
    description: str
    plots: List[Plot] = field(default_factory=list)
    metrics: Dict[str, float] = field(default_factory=dict)


# --------------------------------------------------------------------------
# pid — 1-DOF attitude control
# --------------------------------------------------------------------------

def run_pid() -> RunResult:
    from bindings import pid_core
    sim = pid_core.simulate()
    return RunResult(
        "pid", "PID 制御による1自由度姿勢制御",
        plots=[Plot("PID attitude response", "t [s]", "θ",
                    [Trace("θ(t)", sim.t, sim.theta)])],
        metrics={"final θ": sim.final, "max θ": sim.max, "min θ": sim.min},
    )


# --------------------------------------------------------------------------
# pi_path_tracking — PI path following (XY + cross-track error)
# --------------------------------------------------------------------------

def run_pi_path_tracking() -> RunResult:
    from bindings import track_core
    sim = track_core.simulate()
    return RunResult(
        "pi_path_tracking", "PI 制御による経路追従",
        plots=[
            Plot("Path tracking", "x", "y",
                 [Trace("reference", sim.x_ref, sim.y_ref, "--"),
                  Trace("vehicle", sim.x, sim.y)],
                 equal_aspect=True),
            Plot("Cross-track error", "t [s]", "e_y",
                 [Trace("e_y(t)", sim.t, sim.ey)]),
        ],
        metrics={"path err RMS": sim.path_error_rms,
                 "path err max": sim.path_error_max},
    )


# --------------------------------------------------------------------------
# two_dof — 2-DOF vs PID step response
# --------------------------------------------------------------------------

def run_two_dof() -> RunResult:
    from bindings import tdof_core
    sim = tdof_core.simulate()
    return RunResult(
        "two_dof", "2自由度制御 vs PID のステップ応答",
        plots=[Plot("Step response", "t [s]", "y",
                    [Trace("reference", sim.t, sim.r, "--"),
                     Trace("PID", sim.t, sim.y_pid),
                     Trace("2-DOF", sim.t, sim.y_2dof)])],
    )


# --------------------------------------------------------------------------
# mass_spring_damper — overlay the reference cases
# --------------------------------------------------------------------------

def run_mass_spring_damper() -> RunResult:
    from bindings import msd_core
    cases = msd_core.default_cases()
    pos = Plot("Position response", "t [s]", "x")
    vel = Plot("Velocity response", "t [s]", "v")
    metrics: Dict[str, float] = {}
    for case in cases:
        sim = msd_core.simulate(case)
        pos.traces.append(Trace(case.name, sim.t, sim.x))
        vel.traces.append(Trace(case.name, sim.t, sim.v))
        metrics[f"{case.name} max|x|"] = sim.max_abs_x
    return RunResult(
        "mass_spring_damper", "質量・ばね・ダンパ系のステップ応答（5ケース比較）",
        plots=[pos, vel], metrics=metrics,
    )


EXAMPLES: Dict[str, Callable[[], RunResult]] = {
    "pid": run_pid,
    "pi_path_tracking": run_pi_path_tracking,
    "two_dof": run_two_dof,
    "mass_spring_damper": run_mass_spring_damper,
}


def run(name: str) -> RunResult:
    return EXAMPLES[name]()


def run_all() -> List[RunResult]:
    out: List[RunResult] = []
    for name, fn in EXAMPLES.items():
        try:
            out.append(fn())
        except Exception as exc:
            out.append(RunResult(name, f"error: {exc}"))
    return out
