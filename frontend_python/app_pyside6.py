"""
app_pyside6.py — PySide6 GUI for the interactive PID demo.

Single-panel: matplotlib canvas at top, slider grid below. Sliders
update the simulation live as you drag.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui  import QFontDatabase
from PySide6.QtWidgets import (
    QApplication, QDoubleSpinBox, QFileDialog, QGridLayout, QHBoxLayout,
    QLabel, QMainWindow, QPushButton, QSlider, QSpinBox, QVBoxLayout,
    QWidget, QStatusBar,
)

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import pid_core as pc


class ResponseCanvas(FigureCanvas):
    """One axis showing the PID response with a dashed target line."""

    def __init__(self) -> None:
        self.figure = Figure(figsize=(8, 4))
        super().__init__(self.figure)
        self.ax = self.figure.add_subplot(1, 1, 1)
        self._resp_line = None
        self._target_line = None

    def setup(self):
        self.ax.cla()
        self._target_line = self.ax.axhline(
            0.0, color="red", linestyle="dashed", label="Target")
        self._resp_line, = self.ax.plot([], [], color="blue", label="PID")
        self.ax.set_xlabel("t"); self.ax.set_ylabel("theta")
        self.ax.grid(True); self.ax.legend(loc="lower right")
        self.figure.tight_layout()

    def update_simulation(self, sim: pc.Simulation, cfg: pc.PidConfig) -> None:
        if self._resp_line is None:
            self.setup()
        self._resp_line.set_data(sim.t, sim.theta)
        self._target_line.set_ydata([cfg.theta_goal, cfg.theta_goal])
        self.ax.set_xlim(0, max(1, cfg.time_length))
        y_min = min(sim.min, cfg.theta_start, cfg.theta_goal) - 20
        y_max = max(sim.max, cfg.theta_start, cfg.theta_goal) + 20
        if y_min == y_max:
            y_max = y_min + 1.0
        self.ax.set_ylim(y_min, y_max)
        self.ax.set_title(f"final theta = {sim.final:.3f}")
        self.draw_idle()


class _LinkedSlider(QWidget):
    """A labelled slider + spin-box pair that stays in sync."""

    def __init__(self, name: str, lo: float, hi: float, step: float,
                 init: float, integer: bool = False) -> None:
        super().__init__()
        self.name = name
        self._integer = integer
        self._lo = lo
        self._hi = hi
        self._step = step

        self._label = QLabel(name)
        self._label.setMinimumWidth(90)

        self._slider = QSlider(Qt.Orientation.Horizontal)
        self._slider.setRange(0, 1000)
        self._slider.setValue(self._value_to_slider(init))

        if integer:
            self._spin = QSpinBox()
            self._spin.setRange(int(lo), int(hi))
            self._spin.setSingleStep(max(1, int(step)))
            self._spin.setValue(int(init))
        else:
            self._spin = QDoubleSpinBox()
            self._spin.setRange(lo, hi)
            self._spin.setSingleStep(step)
            decs = max(1, min(4, -int(np.floor(np.log10(step))) if step < 1 else 1))
            self._spin.setDecimals(decs)
            self._spin.setValue(init)
        self._spin.setMinimumWidth(120)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._label)
        layout.addWidget(self._slider, 1)
        layout.addWidget(self._spin)

        self._slider.valueChanged.connect(self._on_slider)
        self._spin.valueChanged.connect(self._on_spin)
        self._syncing = False
        self._callbacks = []

    def value(self) -> float:
        return float(self._spin.value())

    def set_value(self, v: float) -> None:
        self._syncing = True
        if self._integer:
            self._spin.setValue(int(v))
        else:
            self._spin.setValue(v)
        self._slider.setValue(self._value_to_slider(v))
        self._syncing = False

    def on_changed(self, fn):
        self._callbacks.append(fn)

    # ----- internal -----
    def _value_to_slider(self, v: float) -> int:
        f = (v - self._lo) / (self._hi - self._lo) if self._hi > self._lo else 0.0
        return int(round(max(0.0, min(1.0, f)) * 1000))

    def _slider_to_value(self, s: int) -> float:
        f = s / 1000.0
        v = self._lo + f * (self._hi - self._lo)
        if self._integer:
            return float(int(round(v)))
        if self._step > 0:
            v = round(v / self._step) * self._step
        return v

    def _on_slider(self, s: int) -> None:
        if self._syncing: return
        v = self._slider_to_value(s)
        self._syncing = True
        if self._integer: self._spin.setValue(int(v))
        else:             self._spin.setValue(v)
        self._syncing = False
        self._fire()

    def _on_spin(self, v) -> None:
        if self._syncing: return
        self._syncing = True
        self._slider.setValue(self._value_to_slider(float(v)))
        self._syncing = False
        self._fire()

    def _fire(self) -> None:
        for fn in self._callbacks:
            fn(self.value())


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(f"PID interactive (Python + PySide6) — {pc.version()}")
        self.resize(1100, 720)

        # Debounce timer so dragging many slider events doesn't recompute
        # every single tick (still very responsive for this tiny sim).
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(10)
        self._timer.timeout.connect(self._run)

        self._build_ui()
        self._reset_defaults()
        self._run()

    def _build_ui(self) -> None:
        central = QWidget(); self.setCentralWidget(central)
        outer = QVBoxLayout(central); outer.setContentsMargins(6, 6, 6, 6)
        outer.setSpacing(6)

        # plot area
        self.canvas = ResponseCanvas()
        outer.addWidget(self.canvas, 1)

        # sliders
        self._sliders = {
            "theta_start": _LinkedSlider("theta_start", 0.0, 359.0, 0.1, 0.0),
            "theta_goal":  _LinkedSlider("theta_goal",  0.0, 359.0, 0.1, 90.0),
            "offset":      _LinkedSlider("offset",      0.0, 100.0, 0.01, 0.0),
            "time_length": _LinkedSlider("time_length", 10, 2000, 1, 150, integer=True),
            "kp":          _LinkedSlider("kp", 0.0, 1.5, 0.001, 0.10),
            "ki":          _LinkedSlider("ki", 0.0, 1.5, 0.001, 0.01),
            "kd":          _LinkedSlider("kd", 0.0, 1.5, 0.001, 0.20),
        }
        sliders_box = QWidget()
        sl = QVBoxLayout(sliders_box)
        sl.setContentsMargins(8, 4, 8, 4); sl.setSpacing(2)
        for s in self._sliders.values():
            sl.addWidget(s)
            s.on_changed(lambda _v: self._timer.start())
        outer.addWidget(sliders_box)

        # buttons + status row
        btn_row = QHBoxLayout()
        self.reset_btn = QPushButton("Reset defaults")
        self.save_btn  = QPushButton("Save plot…")
        btn_row.addWidget(self.reset_btn); btn_row.addWidget(self.save_btn)
        btn_row.addStretch(1)
        self.status = QLabel("Ready")
        mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        self.status.setFont(mono)
        btn_row.addWidget(self.status)
        outer.addLayout(btn_row)

        sb = QStatusBar(); self.setStatusBar(sb)

        self.reset_btn.clicked.connect(self._on_reset)
        self.save_btn.clicked.connect(self._on_save)

    def _reset_defaults(self) -> None:
        cfg = pc.PidConfig.default()
        self._sliders["theta_start"].set_value(cfg.theta_start)
        self._sliders["theta_goal"].set_value(cfg.theta_goal)
        self._sliders["offset"].set_value(cfg.offset)
        self._sliders["time_length"].set_value(cfg.time_length)
        self._sliders["kp"].set_value(cfg.kp)
        self._sliders["ki"].set_value(cfg.ki)
        self._sliders["kd"].set_value(cfg.kd)

    def _on_reset(self) -> None:
        self._reset_defaults(); self._run()

    def _on_save(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Save plot",
            str(Path.cwd() / "pid_response.png"),
            "PNG (*.png);;PDF (*.pdf);;All files (*.*)")
        if not path:
            return
        self.canvas.figure.savefig(path, dpi=150, bbox_inches="tight")
        self.status.setText(f"Saved: {path}")

    def _cfg(self) -> pc.PidConfig:
        return pc.PidConfig(
            theta_start=self._sliders["theta_start"].value(),
            theta_goal=self._sliders["theta_goal"].value(),
            offset=self._sliders["offset"].value(),
            time_length=int(self._sliders["time_length"].value()),
            kp=self._sliders["kp"].value(),
            ki=self._sliders["ki"].value(),
            kd=self._sliders["kd"].value(),
        )

    def _run(self) -> None:
        cfg = self._cfg()
        try:
            sim = pc.simulate(cfg)
        except Exception as exc:
            self.status.setText(f"Simulation failed: {exc}")
            return
        self.canvas.update_simulation(sim, cfg)
        self.status.setText(
            f"final={sim.final:7.3f}  max={sim.max:7.3f}  min={sim.min:7.3f}"
        )


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow(); w.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
