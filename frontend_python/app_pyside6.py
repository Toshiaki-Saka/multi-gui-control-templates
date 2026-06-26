"""
app_pyside6.py — PySide6 GUI for the 1-DOF PID vs 2-DOF-like comparison.

Layout:
  left   : plant / PID / scenario parameters
  centre : reference-signal plot (r vs filtered z) + output plot (PID vs 2DOF)
  right  : transfer-function readout + metrics log
"""

from __future__ import annotations

import sys

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui  import QFontDatabase
from PySide6.QtWidgets import (
    QApplication, QDoubleSpinBox, QFormLayout, QGroupBox, QHBoxLayout,
    QLabel, QMainWindow, QPlainTextEdit, QPushButton, QSplitter,
    QStatusBar, QVBoxLayout, QWidget,
)

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import tdof_core as tc


class ComparisonCanvas(FigureCanvas):
    """Two side-by-side axes: reference signals and output responses."""

    def __init__(self) -> None:
        self.figure = Figure(figsize=(9, 5))
        super().__init__(self.figure)
        self.ax_ref = self.figure.add_subplot(1, 2, 1)
        self.ax_out = self.figure.add_subplot(1, 2, 2)

    def show_simulation(self, sim: tc.Simulation) -> None:
        self.ax_ref.cla(); self.ax_out.cla()

        self.ax_ref.plot(sim.t, sim.r, lw=2, label="Original Reference")
        self.ax_ref.plot(sim.t, sim.z, lw=2, label="Filtered Reference")
        self.ax_ref.grid(True); self.ax_ref.legend(fontsize=8)
        self.ax_ref.set_xlabel("Time [s]"); self.ax_ref.set_ylabel("input")
        self.ax_ref.set_title("Reference Signal Comparison")

        self.ax_out.plot(sim.t, sim.y_pid,  lw=2, label="PID")
        self.ax_out.plot(sim.t, sim.y_2dof, lw=2, label="2DOF-like")
        self.ax_out.grid(True); self.ax_out.legend(fontsize=8)
        self.ax_out.set_xlabel("Time [s]"); self.ax_out.set_ylabel("output")
        self.ax_out.set_title("Output Response Comparison")

        self.figure.tight_layout()
        self.draw_idle()


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(
            f"2-DOF control comparison (Python + PySide6) — {tc.version()}")
        self.resize(1320, 720)
        self._build_ui()
        self._reset_defaults()
        self._run()

    def _spin(self, lo, hi, val, step, decimals=4) -> QDoubleSpinBox:
        b = QDoubleSpinBox(); b.setRange(lo, hi); b.setSingleStep(step)
        b.setDecimals(decimals); b.setValue(val); b.setMinimumWidth(110)
        return b

    def _build_ui(self) -> None:
        central = QWidget(); self.setCentralWidget(central)

        # ---- left: parameters ----
        left = QWidget(); ll = QVBoxLayout(left)
        ll.setContentsMargins(8, 8, 8, 8); ll.setSpacing(6)

        pg = QGroupBox("Plant  P(s) = 1 / (m s² + c s + k)")
        pf = QFormLayout(pg)
        self.m = self._spin(1e-4, 100.0, 0.01,  0.005)
        self.c = self._spin(0.0,  100.0, 0.015, 0.005)
        self.k = self._spin(0.0,  100.0, 1.0,   0.1)
        pf.addRow("m", self.m); pf.addRow("c", self.c); pf.addRow("k", self.k)
        ll.addWidget(pg)

        gg = QGroupBox("PID gains")
        gf = QFormLayout(gg)
        self.kp = self._spin(0.0, 1000.0, 2.0,  0.1)
        self.ki = self._spin(0.0, 1000.0, 10.0, 0.5)
        self.kd = self._spin(0.0, 1000.0, 0.1,  0.01)
        gf.addRow("kp", self.kp); gf.addRow("ki", self.ki); gf.addRow("kd", self.kd)
        ll.addWidget(gg)

        sg = QGroupBox("Scenario")
        sf = QFormLayout(sg)
        self.ref   = self._spin(-1000.0, 1000.0, 10.0, 0.5)
        self.t_end = self._spin(0.1, 100.0, 2.0, 0.5)
        self.dt    = self._spin(1e-4, 1.0, 0.01, 0.005)
        sf.addRow("ref",   self.ref)
        sf.addRow("t_end", self.t_end)
        sf.addRow("dt",    self.dt)
        ll.addWidget(sg)

        btn = QHBoxLayout()
        self.run_btn   = QPushButton("Run")
        self.reset_btn = QPushButton("Reset defaults")
        btn.addWidget(self.run_btn); btn.addWidget(self.reset_btn)
        ll.addLayout(btn)
        ll.addStretch(1)

        # ---- centre: plots ----
        centre = QWidget(); cl = QVBoxLayout(centre)
        cl.setContentsMargins(8, 8, 8, 8)
        self.canvas = ComparisonCanvas()
        cl.addWidget(self.canvas, 1)

        # ---- right: TF readout + log ----
        right = QWidget(); rl = QVBoxLayout(right)
        rl.setContentsMargins(8, 8, 8, 8); rl.setSpacing(8)
        mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        self.tf_box = QPlainTextEdit(); self.tf_box.setReadOnly(True)
        self.tf_box.setFont(mono); self.tf_box.setMaximumHeight(220)
        rl.addWidget(QLabel("Transfer functions"))
        rl.addWidget(self.tf_box)
        self.log = QPlainTextEdit(); self.log.setReadOnly(True)
        self.log.setFont(mono)
        rl.addWidget(QLabel("Metrics"))
        rl.addWidget(self.log, 1)

        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(left); split.addWidget(centre); split.addWidget(right)
        split.setStretchFactor(0, 0); split.setStretchFactor(1, 2)
        split.setStretchFactor(2, 1); split.setSizes([320, 640, 360])
        outer = QVBoxLayout(central); outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(split, 1)

        self.status_lbl = QLabel("Ready")
        sb = QStatusBar(); sb.addWidget(self.status_lbl); self.setStatusBar(sb)

        self.run_btn.clicked.connect(self._run)
        self.reset_btn.clicked.connect(self._on_reset)

    def _reset_defaults(self) -> None:
        c = tc.TdofConfig.default()
        self.m.setValue(c.m); self.c.setValue(c.c); self.k.setValue(c.k)
        self.kp.setValue(c.kp); self.ki.setValue(c.ki); self.kd.setValue(c.kd)
        self.ref.setValue(c.ref); self.t_end.setValue(c.t_end); self.dt.setValue(c.dt)

    def _on_reset(self) -> None:
        self._reset_defaults(); self._run()

    def _cfg(self) -> tc.TdofConfig:
        return tc.TdofConfig(
            m=self.m.value(), c=self.c.value(), k=self.k.value(),
            kp=self.kp.value(), ki=self.ki.value(), kd=self.kd.value(),
            ref=self.ref.value(), t_end=self.t_end.value(), dt=self.dt.value(),
        )

    @staticmethod
    def _fmt_tf(name: str, tf: tc.TransferFunction) -> str:
        def poly(cs):
            return "  ".join(f"{v:g}" for v in cs)
        return f"{name}:\n  num [{poly(tf.num)}]\n  den [{poly(tf.den)}]\n"

    def _run(self) -> None:
        cfg = self._cfg()
        try:
            sim = tc.simulate(cfg)
        except Exception as exc:
            self.status_lbl.setText(f"Simulation failed: {exc}")
            return

        self.canvas.show_simulation(sim)

        # TF readout
        txt = ""
        txt += self._fmt_tf("Plant P",      tc.get_tf(tc.PLANT, cfg))
        txt += self._fmt_tf("PID K1",       tc.get_tf(tc.PID, cfg))
        txt += self._fmt_tf("Filter K2",    tc.get_tf(tc.FILTER, cfg))
        txt += self._fmt_tf("Closed Gyz",   tc.get_tf(tc.CLOSED_LOOP, cfg))
        self.tf_box.setPlainText(txt)

        # Metrics
        ref = cfg.ref
        def overshoot(y):
            peak = np.max(y) if ref >= 0 else np.min(y)
            return 100.0 * (peak / ref - 1.0)
        self.log.setPlainText(
            f"samples       : {len(sim.t)}\n"
            f"y_pid  peak    : {np.max(sim.y_pid):.4f}\n"
            f"y_pid  final   : {sim.y_pid[-1]:.4f}\n"
            f"y_pid  overshoot: {overshoot(sim.y_pid):.1f}%\n"
            f"y_2dof peak    : {np.max(sim.y_2dof):.4f}\n"
            f"y_2dof final   : {sim.y_2dof[-1]:.4f}\n"
            f"y_2dof overshoot: {overshoot(sim.y_2dof):.1f}%\n"
            f"z final        : {sim.z[-1]:.4f}"
        )
        self.status_lbl.setText("OK")


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow(); w.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
