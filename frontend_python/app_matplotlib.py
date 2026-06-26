"""
app_matplotlib.py — drop-in replacement for pid_advanced_simulation.py.

Same matplotlib-Slider UI as the reference script, but the PID
simulation runs in C++ via the pid_core shared library.
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.widgets import Slider, Button

import pid_core as pc


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "output"


def _run(theta_start, theta_goal, offset, time_length, kp, ki, kd):
    return pc.simulate(pc.PidConfig(
        theta_start=theta_start, theta_goal=theta_goal,
        offset=offset, time_length=int(time_length),
        kp=kp, ki=ki, kd=kd,
    ))


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"=== {pc.version()} ===")

    # Initial values (match the Python reference).
    init = dict(theta_start=0.0, theta_goal=90.0, offset=0.0,
                time_length=150, kp=0.10, ki=0.01, kd=0.20)

    fig, ax = plt.subplots(figsize=(10, 6))
    plt.subplots_adjust(left=0.12, bottom=0.42)

    sim = _run(**init)

    target_line = ax.axhline(init["theta_goal"], color="red", linestyle="dashed",
                             label="Target")
    response_line, = ax.plot(sim.t, sim.theta, color="blue", label="PID")
    ax.set_xlabel("t"); ax.set_ylabel("theta")
    ax.set_title(f"final theta = {sim.final:.3f}")
    ax.grid(); ax.legend(loc="lower right")
    ax.set_xlim(0, init["time_length"])
    y_min = min(sim.min, init["theta_start"], init["theta_goal"]) - 20
    y_max = max(sim.max, init["theta_start"], init["theta_goal"]) + 20
    ax.set_ylim(y_min, y_max)

    # ---- sliders ----
    s_axes = {
        "theta_start": plt.axes([0.20, 0.32, 0.65, 0.03]),
        "theta_goal":  plt.axes([0.20, 0.28, 0.65, 0.03]),
        "offset":      plt.axes([0.20, 0.24, 0.65, 0.03]),
        "time_length": plt.axes([0.20, 0.20, 0.65, 0.03]),
        "kp":          plt.axes([0.20, 0.16, 0.65, 0.03]),
        "ki":          plt.axes([0.20, 0.12, 0.65, 0.03]),
        "kd":          plt.axes([0.20, 0.08, 0.65, 0.03]),
    }
    sliders = {
        "theta_start": Slider(s_axes["theta_start"], "theta_start", 0.0, 359.0,
                              valinit=init["theta_start"]),
        "theta_goal":  Slider(s_axes["theta_goal"],  "theta_goal",  0.0, 359.0,
                              valinit=init["theta_goal"]),
        "offset":      Slider(s_axes["offset"],      "offset",      0.0, 100.0,
                              valinit=init["offset"], valstep=0.01),
        "time_length": Slider(s_axes["time_length"], "time_length", 10, 2000,
                              valinit=init["time_length"], valstep=1),
        "kp":          Slider(s_axes["kp"], "kp", 0.0, 1.5,
                              valinit=init["kp"], valstep=0.001),
        "ki":          Slider(s_axes["ki"], "ki", 0.0, 1.5,
                              valinit=init["ki"], valstep=0.001),
        "kd":          Slider(s_axes["kd"], "kd", 0.0, 1.5,
                              valinit=init["kd"], valstep=0.001),
    }

    reset_ax = plt.axes([0.02, 0.08, 0.10, 0.05])
    save_ax  = plt.axes([0.02, 0.16, 0.10, 0.05])
    button_reset = Button(reset_ax, "Reset")
    button_save  = Button(save_ax,  "Save")

    def update(_):
        sim = _run(
            theta_start=sliders["theta_start"].val,
            theta_goal=sliders["theta_goal"].val,
            offset=sliders["offset"].val,
            time_length=int(sliders["time_length"].val),
            kp=sliders["kp"].val, ki=sliders["ki"].val, kd=sliders["kd"].val,
        )
        response_line.set_xdata(sim.t)
        response_line.set_ydata(sim.theta)
        target_line.set_ydata([sliders["theta_goal"].val,
                               sliders["theta_goal"].val])
        ax.set_xlim(0, sliders["time_length"].val)
        y_min = min(sim.min, sliders["theta_start"].val,
                    sliders["theta_goal"].val) - 20
        y_max = max(sim.max, sliders["theta_start"].val,
                    sliders["theta_goal"].val) + 20
        if y_min == y_max:
            y_max = y_min + 1
        ax.set_ylim(y_min, y_max)
        ax.set_title(f"final theta = {sim.final:.3f}")
        fig.canvas.draw_idle()

    def reset(_):
        for s in sliders.values():
            s.reset()

    def save(_):
        path = OUTPUT_DIR / "pid_interactive_response.png"
        fig.savefig(path, dpi=150, bbox_inches="tight")
        print(f"Saved: {path}")

    for s in sliders.values():
        s.on_changed(update)
    button_reset.on_clicked(reset)
    button_save.on_clicked(save)

    plt.show()


if __name__ == "__main__":
    main()
