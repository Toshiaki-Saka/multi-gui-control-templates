"""
app_pyside6.py — PySide6 GUI for the path-tracking demo.

Layout:
  left   : parameter sliders (gains, geometry, scenario)
  centre : tab widget with 4 plots — XY, states, errors, inputs
  bottom : metrics readout + Run / Reset / Save CSV
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

import numpy as np
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui  import QFontDatabase
from PySide6.QtWidgets import (
    QApplication, QDoubleSpinBox, QFileDialog, QFormLayout, QGroupBox,
    QHBoxLayout, QLabel, QMainWindow, QPlainTextEdit, QPushButton,
    QScrollArea, QSpinBox, QStatusBar, QSplitter, QTabWidget,
    QVBoxLayout, QWidget,
)

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import track_core as tc


# --------------------------- plot canvases ---------------------------

class XyCanvas(FigureCanvas):
    def __init__(self) -> None:
        self.figure = Figure(figsize=(7, 7))
        super().__init__(self.figure)
        self.ax = self.figure.add_subplot()

    def show(self, ref: tc.ReferencePath, sim: tc.Simulation) -> None:
        self.ax.cla()
        self.ax.plot(ref.x, ref.y, "--", label="Reference path")
        self.ax.plot(sim.x, sim.y, label="Actual path")
        self.ax.scatter(sim.x[0], sim.y[0], label="Start", zorder=5)
        self.ax.scatter(sim.x[-1], sim.y[-1], label="End", zorder=5)
        self.ax.set_aspect("equal", adjustable="box")
        self.ax.set_xlabel("X [m]"); self.ax.set_ylabel("Y [m]")
        self.ax.set_title("Path tracking — XY")
        self.ax.grid(True); self.ax.legend(loc="best", fontsize=8)
        self.figure.tight_layout(); self.draw_idle()


class StatesCanvas(FigureCanvas):
    def __init__(self) -> None:
        self.figure = Figure(figsize=(11, 8))
        super().__init__(self.figure)

    def show(self, sim: tc.Simulation) -> None:
        self.figure.clear()
        ax = self.figure.add_subplot(321); ax.plot(sim.t, sim.u); ax.set_ylabel("u [m/s]"); ax.grid()
        ax = self.figure.add_subplot(322); ax.plot(sim.t, sim.v); ax.set_ylabel("v [m/s]"); ax.grid()
        ax = self.figure.add_subplot(323); ax.plot(sim.t, sim.r); ax.set_ylabel("r [rad/s]"); ax.grid()
        ax = self.figure.add_subplot(324)
        ax.plot(sim.t, sim.psi, label="psi")
        ax.plot(sim.t, sim.psi_ref, "--", label="psi_ref")
        ax.set_ylabel("psi [rad]"); ax.legend(fontsize=8); ax.grid()
        ax = self.figure.add_subplot(325); ax.plot(sim.t, sim.beta); ax.set_ylabel("beta [rad]"); ax.set_xlabel("Time [s]"); ax.grid()
        ax = self.figure.add_subplot(326); ax.plot(sim.t, np.hypot(sim.u, sim.v)); ax.set_ylabel("speed [m/s]"); ax.set_xlabel("Time [s]"); ax.grid()
        self.figure.tight_layout(); self.draw_idle()


class ErrorsCanvas(FigureCanvas):
    def __init__(self) -> None:
        self.figure = Figure(figsize=(11, 7))
        super().__init__(self.figure)

    def show(self, sim: tc.Simulation) -> None:
        self.figure.clear()
        ax = self.figure.add_subplot(311); ax.plot(sim.t, sim.path_error); ax.set_ylabel("Path error [m]"); ax.grid()
        ax = self.figure.add_subplot(312); ax.plot(sim.t, sim.ey);         ax.set_ylabel("e_y [m]");        ax.grid()
        ax = self.figure.add_subplot(313); ax.plot(sim.t, sim.epsi);       ax.set_ylabel("e_psi [rad]");    ax.set_xlabel("Time [s]"); ax.grid()
        self.figure.tight_layout(); self.draw_idle()


class InputsCanvas(FigureCanvas):
    def __init__(self) -> None:
        self.figure = Figure(figsize=(11, 7))
        super().__init__(self.figure)

    def show(self, sim: tc.Simulation, cfg: tc.TrackConfig) -> None:
        self.figure.clear()
        ax = self.figure.add_subplot(211)
        ax.plot(sim.t, sim.n_moment); ax.set_ylabel("n_moment"); ax.grid()
        ax.axhline( cfg.n_moment_limit, color="k", lw=0.5, ls=":")
        ax.axhline(-cfg.n_moment_limit, color="k", lw=0.5, ls=":")
        ax = self.figure.add_subplot(212)
        ax.plot(sim.t, sim.fx); ax.set_ylabel("Fx [N]"); ax.set_xlabel("Time [s]"); ax.grid()
        ax.axhline( cfg.fx_limit, color="k", lw=0.5, ls=":")
        ax.axhline(-cfg.fx_limit, color="k", lw=0.5, ls=":")
        self.figure.tight_layout(); self.draw_idle()


# --------------------------- main window -----------------------------

class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(
            f"Path-tracking PI simulation (Python + PySide6) — {tc.version()}")
        self.resize(1320, 820)

        # Debounce so dragging a slider doesn't fire 9000-step sims at
        # every tiny tick; 30 ms feels live but cheap enough.
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(30)
        self._timer.timeout.connect(self._run)

        self._sim: tc.Simulation | None = None
        self._cfg: tc.TrackConfig | None = None
        self._ref: tc.ReferencePath | None = None

        self._build_ui()
        self._reset_defaults()
        self._run()

    # ---- spin-box factories ----
    def _spin(self, lo, hi, val, step, decimals=4) -> QDoubleSpinBox:
        b = QDoubleSpinBox(); b.setRange(lo, hi); b.setSingleStep(step)
        b.setDecimals(decimals); b.setValue(val); b.setMinimumWidth(110)
        return b
    def _int_spin(self, lo, hi, val) -> QSpinBox:
        b = QSpinBox(); b.setRange(lo, hi); b.setValue(val); b.setMinimumWidth(110)
        return b

    def _build_ui(self) -> None:
        central = QWidget(); self.setCentralWidget(central)

        # ---- parameter panel ----
        params = QWidget()
        pl = QVBoxLayout(params); pl.setContentsMargins(6, 6, 6, 6); pl.setSpacing(6)

        # Plant
        plant = QGroupBox("Plant"); pf = QFormLayout(plant)
        self.m   = self._spin(0.001, 100.0, 0.1,  0.01)
        self.izz = self._spin(0.001, 100.0, 1.0,  0.05)
        self.cp  = self._spin(0.0,   1000.0, 20.0, 1.0)
        pf.addRow("m",                self.m)
        pf.addRow("izz",              self.izz)
        pf.addRow("cornering_power",  self.cp)
        pl.addWidget(plant)

        # Sampling
        samp = QGroupBox("Sampling"); sf = QFormLayout(samp)
        self.h         = self._spin(1e-6, 0.1, 1e-4, 1e-5, decimals=6)
        self.tc        = self._spin(1e-6, 1.0, 1e-3, 1e-4, decimals=6)
        self.totalTime = self._spin(0.01, 100.0, 0.90, 0.05)
        self.targetSpd = self._spin(0.0, 50.0, 1.0, 0.05)
        sf.addRow("h [s]",            self.h)
        sf.addRow("tc [s]",           self.tc)
        sf.addRow("total_time [s]",   self.totalTime)
        sf.addRow("target_speed",     self.targetSpd)
        pl.addWidget(samp)

        # Gains
        gains = QGroupBox("Gains"); gf = QFormLayout(gains)
        self.kyp        = self._spin(0.0, 5000.0, 400.0, 5.0, decimals=2)
        self.kyi        = self._spin(0.0, 5000.0, 0.0, 0.5, decimals=2)
        self.kpsip      = self._spin(0.0, 5000.0, 200.0, 5.0, decimals=2)
        self.kpsii      = self._spin(0.0, 5000.0, 0.0, 0.5, decimals=2)
        self.krDamping  = self._spin(0.0, 1000.0, 20.0, 1.0, decimals=2)
        gf.addRow("ky_p",       self.kyp)
        gf.addRow("ky_i",       self.kyi)
        gf.addRow("kpsi_p",     self.kpsip)
        gf.addRow("kpsi_i",     self.kpsii)
        gf.addRow("kr_damping", self.krDamping)
        pl.addWidget(gains)

        # Limits
        lim = QGroupBox("Limits"); lf = QFormLayout(lim)
        self.nLim    = self._spin(1.0, 100000.0, 500.0, 10.0, decimals=2)
        self.fxLim   = self._spin(0.01, 1000.0, 5.0, 0.5, decimals=2)
        self.iLim    = self._spin(0.0, 100.0, 0.2, 0.05)
        self.lookahead = self._int_spin(0, 1000, 60)
        lf.addRow("n_moment_limit",     self.nLim)
        lf.addRow("fx_limit",            self.fxLim)
        lf.addRow("error_integral_limit", self.iLim)
        lf.addRow("lookahead_index",     self.lookahead)
        pl.addWidget(lim)

        # Scenario
        sc = QGroupBox("Scenario"); sf2 = QFormLayout(sc)
        self.y0Off    = self._spin(-1.0, 1.0, -0.03, 0.005)
        self.hd0      = self._spin(-90.0, 90.0, 3.0, 0.1)
        self.str1     = self._spin(0.0, 10.0, 0.30, 0.01)
        self.radius   = self._spin(0.01, 10.0, 0.20, 0.01)
        self.str2     = self._spin(0.0, 10.0, 0.30, 0.01)
        self.ds       = self._spin(1e-4, 1.0, 0.002, 1e-4, decimals=5)
        sf2.addRow("initial_y_offset",    self.y0Off)
        sf2.addRow("initial_heading_deg", self.hd0)
        sf2.addRow("straight1_len",       self.str1)
        sf2.addRow("radius",              self.radius)
        sf2.addRow("straight2_len",       self.str2)
        sf2.addRow("ds",                  self.ds)
        pl.addWidget(sc)

        pl.addStretch(1)

        scroll = QScrollArea(); scroll.setWidget(params); scroll.setWidgetResizable(True)
        scroll.setMinimumWidth(360)

        # ---- centre: tabs with plots ----
        self.xy_canvas    = XyCanvas()
        self.states_canvas = StatesCanvas()
        self.errors_canvas = ErrorsCanvas()
        self.inputs_canvas = InputsCanvas()
        self.tabs = QTabWidget()
        self.tabs.addTab(self.xy_canvas,     "XY")
        self.tabs.addTab(self.states_canvas, "States")
        self.tabs.addTab(self.errors_canvas, "Errors")
        self.tabs.addTab(self.inputs_canvas, "Inputs")

        # ---- bottom: metrics + buttons ----
        bot = QWidget(); bl = QVBoxLayout(bot)
        bl.setContentsMargins(6, 6, 6, 6); bl.setSpacing(4)
        mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        self.metrics = QPlainTextEdit(); self.metrics.setReadOnly(True)
        self.metrics.setFont(mono); self.metrics.setMaximumHeight(140)
        bl.addWidget(self.metrics)

        btns = QHBoxLayout()
        self.run_btn   = QPushButton("Run")
        self.reset_btn = QPushButton("Reset defaults")
        self.save_btn  = QPushButton("Save CSV + summary…")
        btns.addWidget(self.run_btn)
        btns.addWidget(self.reset_btn)
        btns.addWidget(self.save_btn)
        btns.addStretch(1)
        self.status = QLabel("Ready"); self.status.setFont(mono)
        btns.addWidget(self.status)
        bl.addLayout(btns)

        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(scroll); split.addWidget(self.tabs)
        split.setStretchFactor(0, 0); split.setStretchFactor(1, 1)
        split.setSizes([380, 940])

        outer = QVBoxLayout(central); outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(split, 1); outer.addWidget(bot)
        self.setStatusBar(QStatusBar())

        # ---- wire up ----
        widgets = [self.m, self.izz, self.cp,
                   self.h, self.tc, self.totalTime, self.targetSpd,
                   self.kyp, self.kyi, self.kpsip, self.kpsii, self.krDamping,
                   self.nLim, self.fxLim, self.iLim,
                   self.y0Off, self.hd0,
                   self.str1, self.radius, self.str2, self.ds]
        for w in widgets:
            w.valueChanged.connect(lambda _v: self._timer.start())
        self.lookahead.valueChanged.connect(lambda _v: self._timer.start())

        self.run_btn.clicked.connect(self._run)
        self.reset_btn.clicked.connect(self._on_reset)
        self.save_btn.clicked.connect(self._on_save)

    def _reset_defaults(self) -> None:
        c = tc.TrackConfig.default()
        self.m.setValue(c.m); self.izz.setValue(c.izz); self.cp.setValue(c.cornering_power)
        self.h.setValue(c.h); self.tc.setValue(c.tc); self.totalTime.setValue(c.total_time)
        self.targetSpd.setValue(c.target_speed)
        self.kyp.setValue(c.ky_p); self.kyi.setValue(c.ky_i)
        self.kpsip.setValue(c.kpsi_p); self.kpsii.setValue(c.kpsi_i)
        self.krDamping.setValue(c.kr_damping)
        self.nLim.setValue(c.n_moment_limit); self.fxLim.setValue(c.fx_limit)
        self.iLim.setValue(c.error_integral_limit)
        self.lookahead.setValue(c.lookahead_index)
        self.y0Off.setValue(c.initial_y_offset); self.hd0.setValue(c.initial_heading_deg)
        self.str1.setValue(c.straight1_len); self.radius.setValue(c.radius)
        self.str2.setValue(c.straight2_len); self.ds.setValue(c.ds)

    def _on_reset(self) -> None:
        self._reset_defaults(); self._run()

    def _on_save(self) -> None:
        if self._sim is None or self._cfg is None: return
        d = QFileDialog.getExistingDirectory(self, "Save to folder",
                                              str(Path.cwd()))
        if not d: return
        outdir = Path(d)
        # CSV
        csv_path = outdir / "tracking_log.csv"
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
            s = self._sim
            pe = s.path_error
            for i in range(len(s.t)):
                w.writerow([
                    s.t[i], s.x[i], s.y[i], s.psi[i],
                    s.u[i], s.v[i], s.r[i], s.beta[i],
                    s.ey[i], s.epsi[i], s.n_moment[i], s.fx[i],
                    s.x_ref[i], s.y_ref[i], s.psi_ref[i], pe[i],
                ])
        # Summary
        (outdir / "tracking_summary.txt").write_text(
            self.metrics.toPlainText(), encoding="utf-8")
        self.status.setText(f"Saved to {outdir}")

    def _cfg_from_ui(self) -> tc.TrackConfig:
        return tc.TrackConfig(
            m=self.m.value(), izz=self.izz.value(), cornering_power=self.cp.value(),
            h=self.h.value(), tc=self.tc.value(), total_time=self.totalTime.value(),
            target_speed=self.targetSpd.value(),
            ky_p=self.kyp.value(), ky_i=self.kyi.value(),
            kpsi_p=self.kpsip.value(), kpsi_i=self.kpsii.value(),
            kr_damping=self.krDamping.value(),
            n_moment_limit=self.nLim.value(),
            fx_limit=self.fxLim.value(),
            error_integral_limit=self.iLim.value(),
            lookahead_index=int(self.lookahead.value()),
            initial_y_offset=self.y0Off.value(),
            initial_heading_deg=self.hd0.value(),
            straight1_len=self.str1.value(), radius=self.radius.value(),
            straight2_len=self.str2.value(), ds=self.ds.value(),
        )

    def _run(self) -> None:
        cfg = self._cfg_from_ui()
        try:
            ref = tc.make_reference(cfg)
            sim = tc.simulate(cfg)
        except Exception as exc:
            self.status.setText(f"Simulation failed: {exc}")
            return
        self._cfg, self._ref, self._sim = cfg, ref, sim
        self.xy_canvas    .show(ref, sim)
        self.states_canvas.show(sim)
        self.errors_canvas.show(sim)
        self.inputs_canvas.show(sim, cfg)

        self.metrics.setPlainText(
            f"samples              : {len(sim.t)}\n"
            f"reference points     : {len(ref.x)}\n"
            f"path_error_rms [m]   : {sim.path_error_rms:.6f}\n"
            f"path_error_max [m]   : {sim.path_error_max:.6f}\n"
            f"e_y_rms        [m]   : {sim.ey_rms:.6f}\n"
            f"e_y_max        [m]   : {sim.ey_max:.6f}\n"
            f"e_psi_rms      [rad] : {sim.epsi_rms:.6f}\n"
            f"e_psi_max      [rad] : {sim.epsi_max:.6f}\n"
            f"max|n_moment|        : {sim.nmoment_max:.6f}"
        )
        self.status.setText(
            f"OK — {len(sim.t)} samples, "
            f"rms err {sim.path_error_rms:.4f} m"
        )


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow(); w.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
