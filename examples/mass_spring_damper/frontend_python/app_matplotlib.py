"""
app_matplotlib.py — drop-in replacement for mass_spring_damper_forced_response.py.

Sweeps the 5 default cases and produces the same overlay plot, but the
simulation runs in C++ via the msd_core shared library.
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

import msd_core as mc


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "output"


def make_label(case: mc.MsdCase) -> str:
    return (f"m={case.m}, c={case.c}, k={case.k}, "
            f"F={case.force_amplitude}sin({case.force_omega}t), "
            f"ωn={case.omega_n:.2f}, ζ={case.zeta:.2f}")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"=== {mc.version()} ===")

    sampling = mc.SamplingConfig.default()
    cases = mc.default_cases()

    print("===== parameter sweep simulation =====")
    results = []
    for case in cases:
        sim = mc.simulate(case, sampling)
        results.append((make_label(case), sim))
        print()
        print(f"--- {case.name} ---")
        print(f"m = {case.m}")
        print(f"c = {case.c}")
        print(f"k = {case.k}")
        print(f"force = {case.force_amplitude} * sin({case.force_omega} * t)")
        print(f"x(final) = {sim.final_x}")
        print(f"v(final) = {sim.final_v}")

    print()
    print("===== common simulation settings =====")
    print(f"dt = {sampling.dt}")
    print(f"stop = {sampling.stop}")
    print(f"number of samples = {len(results[0][1].t)}")

    fig = plt.figure(figsize=(11, 6))
    ax = fig.add_subplot()
    for label, sim in results:
        ax.plot(sim.t, sim.x, label=label)
    ax.set_xlim(0, sampling.stop)
    ax.set_xlabel("t [s]"); ax.set_ylabel("x [m]")
    ax.set_title("Mass-Spring-Damper Forced Response - Parameter Sweep (C++ core)")
    ax.grid(True)
    ax.legend(fontsize=8)
    fig.tight_layout()
    save_path = OUTPUT_DIR / "mass_spring_damper_parameter_sweep.png"
    fig.savefig(save_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {save_path}")
    plt.show()


if __name__ == "__main__":
    main()
