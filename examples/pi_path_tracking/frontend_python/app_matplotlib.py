"""
app_matplotlib.py — drop-in replacement for planar_path_tracking_pi_tuned.py.

Produces the same files in output_path_tracking_pi_tuned/, but the
heavy-lifting simulation runs in C++ via the track_core shared library.
"""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

import track_core as tc


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "output_path_tracking_pi_tuned"


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print("=" * 80)
    print("平面運動モデル: 横偏差・ヨー偏差 PI 経路追従シミュレーション（調整済み）")
    print("C++ core:", tc.version())
    print("=" * 80)

    cfg = tc.TrackConfig.default()

    print("Config:")
    print(f"  h                  = {cfg.h:.6f} [s]")
    print(f"  tc                 = {cfg.tc:.6f} [s]")
    print(f"  total_time         = {cfg.total_time:.3f} [s]")
    print(f"  target_speed       = {cfg.target_speed:.3f} [m/s]")
    print(f"  lookahead_index    = {cfg.lookahead_index}")
    print()
    print("Gains:")
    print(f"  ky_p       = {cfg.ky_p}")
    print(f"  ky_i       = {cfg.ky_i}")
    print(f"  kpsi_p     = {cfg.kpsi_p}")
    print(f"  kpsi_i     = {cfg.kpsi_i}")
    print(f"  kr_damping = {cfg.kr_damping}")
    print()

    ref = tc.make_reference(cfg)
    print("求解を開始します...")
    sim = tc.simulate(cfg)
    print("求解が完了しました。")

    path_error = sim.path_error
    summary = f"""Path Tracking PI Simulation Tuned Summary
=========================================

Gains
-----
ky_p       = {cfg.ky_p}
ky_i       = {cfg.ky_i}
kpsi_p     = {cfg.kpsi_p}
kpsi_i     = {cfg.kpsi_i}
kr_damping = {cfg.kr_damping}

Config
------
h                 = {cfg.h}
tc                = {cfg.tc}
total_time        = {cfg.total_time}
target_speed      = {cfg.target_speed}
lookahead_index   = {cfg.lookahead_index}
n_moment_limit    = {cfg.n_moment_limit}
initial_y_offset  = {cfg.initial_y_offset}
initial_heading_deg = {cfg.initial_heading_deg}

Evaluation
----------
path_error_rms [m]     = {sim.path_error_rms:.6f}
path_error_max [m]     = {sim.path_error_max:.6f}
e_y_rms [m]            = {sim.ey_rms:.6f}
e_y_max [m]            = {sim.ey_max:.6f}
e_psi_rms [rad]        = {sim.epsi_rms:.6f}
e_psi_max [rad]        = {sim.epsi_max:.6f}
max |n_moment|         = {sim.nmoment_max:.6f}

Comment
-------
This tuned version uses P feedback for lateral error and yaw error,
with yaw-rate damping. Integral gains are intentionally set to zero first.
"""
    summary_path = OUTPUT_DIR / "tracking_summary.txt"
    summary_path.write_text(summary, encoding="utf-8")
    print(summary)
    print(f"Saved: {summary_path}")

    # ----- CSV -----
    csv_path = OUTPUT_DIR / "tracking_log.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "time_s", "x_m", "y_m", "psi_rad",
            "u_mps", "v_mps", "r_radps", "beta_rad",
            "e_y_m", "e_psi_rad",
            "n_moment", "fx",
            "x_ref_m", "y_ref_m", "psi_ref_rad",
            "path_error_m",
        ])
        for i in range(len(sim.t)):
            w.writerow([
                sim.t[i], sim.x[i], sim.y[i], sim.psi[i],
                sim.u[i], sim.v[i], sim.r[i], sim.beta[i],
                sim.ey[i], sim.epsi[i],
                sim.n_moment[i], sim.fx[i],
                sim.x_ref[i], sim.y_ref[i], sim.psi_ref[i],
                path_error[i],
            ])
    print(f"Saved: {csv_path}")

    # ----- Plot 1: XY trajectory -----
    fig = plt.figure(figsize=(8, 8))
    ax = fig.add_subplot()
    ax.plot(ref.x, ref.y, "--", label="Reference path")
    ax.plot(sim.x, sim.y, label="Actual path")
    ax.scatter(sim.x[0], sim.y[0], label="Start", zorder=5)
    ax.scatter(sim.x[-1], sim.y[-1], label="End", zorder=5)
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("X [m]"); ax.set_ylabel("Y [m]")
    ax.set_title("Path Tracking Result - Tuned (C++ core)")
    ax.grid(); ax.legend()
    fig.tight_layout()
    p = OUTPUT_DIR / "path_tracking_xy.png"
    fig.savefig(p, dpi=150, bbox_inches="tight")
    print(f"Saved: {p}")

    # ----- Plot 2: states -----
    fig = plt.figure(figsize=(14, 10))
    ax = fig.add_subplot(321); ax.plot(sim.t, sim.u); ax.set_ylabel("u [m/s]"); ax.grid()
    ax = fig.add_subplot(322); ax.plot(sim.t, sim.v); ax.set_ylabel("v [m/s]"); ax.grid()
    ax = fig.add_subplot(323); ax.plot(sim.t, sim.r); ax.set_ylabel("r [rad/s]"); ax.grid()
    ax = fig.add_subplot(324)
    ax.plot(sim.t, sim.psi, label="psi")
    ax.plot(sim.t, sim.psi_ref, "--", label="psi_ref")
    ax.set_ylabel("psi [rad]"); ax.legend(); ax.grid()
    ax = fig.add_subplot(325); ax.plot(sim.t, sim.beta); ax.set_ylabel("beta [rad]"); ax.set_xlabel("Time [s]"); ax.grid()
    ax = fig.add_subplot(326); ax.plot(sim.t, np.hypot(sim.u, sim.v)); ax.set_ylabel("speed [m/s]"); ax.set_xlabel("Time [s]"); ax.grid()
    fig.tight_layout()
    p = OUTPUT_DIR / "path_tracking_states.png"
    fig.savefig(p, dpi=150, bbox_inches="tight")
    print(f"Saved: {p}")

    # ----- Plot 3: errors -----
    fig = plt.figure(figsize=(14, 8))
    ax = fig.add_subplot(311); ax.plot(sim.t, path_error); ax.set_ylabel("Path error [m]"); ax.grid()
    ax = fig.add_subplot(312); ax.plot(sim.t, sim.ey);     ax.set_ylabel("e_y [m]");        ax.grid()
    ax = fig.add_subplot(313); ax.plot(sim.t, sim.epsi);   ax.set_ylabel("e_psi [rad]");    ax.set_xlabel("Time [s]"); ax.grid()
    fig.tight_layout()
    p = OUTPUT_DIR / "path_tracking_errors.png"
    fig.savefig(p, dpi=150, bbox_inches="tight")
    print(f"Saved: {p}")

    # ----- Plot 4: inputs -----
    fig = plt.figure(figsize=(14, 8))
    ax = fig.add_subplot(211); ax.plot(sim.t, sim.n_moment); ax.set_ylabel("n_moment"); ax.grid()
    ax = fig.add_subplot(212); ax.plot(sim.t, sim.fx);       ax.set_ylabel("Fx [N]"); ax.set_xlabel("Time [s]"); ax.grid()
    fig.tight_layout()
    p = OUTPUT_DIR / "path_tracking_inputs.png"
    fig.savefig(p, dpi=150, bbox_inches="tight")
    print(f"Saved: {p}")

    plt.show()


if __name__ == "__main__":
    main()
