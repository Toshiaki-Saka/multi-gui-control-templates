"""
app_matplotlib.py — drop-in replacement for
two_degree_of_freedom...py. The transfer functions and time responses
are all computed in C++ via the tdof_core shared library.
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

import tdof_core as tc

# Optional Japanese fonts (matches the reference script).
try:
    import japanize_matplotlib  # noqa: F401
except ImportError:
    pass


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "output"


def _fmt_tf(tf: tc.TransferFunction) -> str:
    def poly(cs):
        return "  ".join(f"{c:g}" for c in cs)
    return f"num: [{poly(tf.num)}]\nden: [{poly(tf.den)}]"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"=== {tc.version()} ===")

    cfg = tc.TdofConfig.default()

    print("===== Plant P =====");            print(_fmt_tf(tc.get_tf(tc.PLANT, cfg)))
    print("\n===== PID Controller K1 ====="); print(_fmt_tf(tc.get_tf(tc.PID, cfg)))
    print("\n===== Reference Filter K2 ====="); print(_fmt_tf(tc.get_tf(tc.FILTER, cfg)))
    print("\n===== Closed-loop Gyz =====");   print(_fmt_tf(tc.get_tf(tc.CLOSED_LOOP, cfg)))

    sim = tc.simulate(cfg)

    # ---------------------------------------------------------------
    # Plot (same layout as the reference)
    # ---------------------------------------------------------------
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    axes[0].plot(sim.t, sim.r, linewidth=2, label="Original Reference")
    axes[0].plot(sim.t, sim.z, linewidth=2, label="Filtered Reference")
    axes[0].grid()
    axes[0].set_xlabel("Time [s]")
    axes[0].set_ylabel("input")
    axes[0].set_title("Reference Signal Comparison")
    axes[0].legend()

    axes[1].plot(sim.t, sim.y_pid,  linewidth=2, label="PID")
    axes[1].plot(sim.t, sim.y_2dof, linewidth=2, label="2DOF-like")
    axes[1].grid()
    axes[1].set_xlabel("Time [s]")
    axes[1].set_ylabel("output")
    axes[1].set_title("Output Response Comparison")
    axes[1].legend()

    fig.tight_layout()

    save_path = OUTPUT_DIR / "two_dof_like_pid_comparison.png"
    fig.savefig(save_path, dpi=150, bbox_inches="tight")
    print(f"\nSaved: {save_path}")

    plt.show()


if __name__ == "__main__":
    main()
