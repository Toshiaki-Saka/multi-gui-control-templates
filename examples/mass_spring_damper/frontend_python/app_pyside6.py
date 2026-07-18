"""
app_pyside6.py — PySide6 GUI for MSD parameter-sweep simulation.

Layout:
  left   : list of cases (add / remove / reset)
  centre : selected case editor (m, c, k, F, ω, x0, v0, derived ωn, ζ)
  right  : overlay plot of all cases — position vs time
  bottom : sampling controls (dt, stop) + Run + Save CSV + Save PNG
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path
from typing import List, Optional

import numpy as np
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui  import QColor, QFontDatabase
from PySide6.QtWidgets import (
    QApplication, QCheckBox, QComboBox, QDoubleSpinBox, QFileDialog,
    QFormLayout, QGroupBox, QHBoxLayout, QInputDialog, QLabel,
    QListWidget, QListWidgetItem, QMainWindow, QMessageBox,
    QPlainTextEdit, QPushButton, QSplitter, QStatusBar, QVBoxLayout,
    QWidget,
)

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import msd_core as mc


# Colour cycle picked to match the Python reference (matplotlib default).
_COLOURS = [
    "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
    "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf",
]


def make_label(case: mc.MsdCase) -> str:
    return (f"m={case.m:g}, c={case.c:g}, k={case.k:g}, "
            f"F={case.force_amplitude:g}sin({case.force_omega:g}t), "
            f"ωn={case.omega_n:.2f}, ζ={case.zeta:.2f}")


class SweepCanvas(FigureCanvas):
    """Overlay plot — one trace per case."""
    def __init__(self) -> None:
        self.figure = Figure(figsize=(8, 5))
        super().__init__(self.figure)
        self.ax = self.figure.add_subplot()

    def show_sweep(self, traces: List[tuple], stop: float, show_force: bool):
        """traces is a list of (case, sim, enabled). Disabled traces are skipped."""
        self.figure.clear()
        ax = self.figure.add_subplot()
        for i, (case, sim, enabled) in enumerate(traces):
            if not enabled or sim is None:
                continue
            colour = _COLOURS[i % len(_COLOURS)]
            ax.plot(sim.t, sim.x, label=make_label(case), color=colour)
            if show_force:
                ax.plot(sim.t, sim.force,
                        color=colour, ls=":", alpha=0.45,
                        label=f"force ({case.name})")
        ax.set_xlim(0, stop)
        ax.set_xlabel("t [s]"); ax.set_ylabel("x [m]")
        ax.set_title("Mass-Spring-Damper Forced Response — Parameter Sweep")
        ax.grid(True); ax.legend(fontsize=7, loc="best")
        self.figure.tight_layout(); self.draw_idle()


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(
            f"MSD forced response (Python + PySide6) — {mc.version()}"
        )
        self.resize(1320, 800)

        self._cases: List[mc.MsdCase] = list(mc.default_cases())
        self._sims:  List[Optional[mc.Simulation]] = [None] * len(self._cases)
        self._enabled: List[bool] = [True] * len(self._cases)
        # Debounce so editing a spinbox doesn't fire on every keystroke.
        self._debounce = QTimer(self); self._debounce.setSingleShot(True)
        self._debounce.setInterval(50)
        self._debounce.timeout.connect(self._run_all)

        # Used to suppress callbacks while we mutate the editor programmatically.
        self._loading_editor = False

        self._build_ui()
        self._refresh_case_list(select_index=0)
        self._run_all()

    # ---- UI builders ----
    def _spin(self, lo, hi, val, step, decimals=4) -> QDoubleSpinBox:
        b = QDoubleSpinBox(); b.setRange(lo, hi); b.setSingleStep(step)
        b.setDecimals(decimals); b.setValue(val); b.setMinimumWidth(110)
        return b

    def _build_ui(self) -> None:
        central = QWidget(); self.setCentralWidget(central)

        # ===== left: case list =====
        left = QWidget(); ll = QVBoxLayout(left)
        ll.setContentsMargins(6, 6, 6, 6); ll.setSpacing(4)
        ll.addWidget(QLabel("Cases"))
        self.case_list = QListWidget()
        self.case_list.currentRowChanged.connect(self._on_case_selected)
        self.case_list.itemChanged.connect(self._on_case_check_toggled)
        ll.addWidget(self.case_list, 1)
        btn_row = QHBoxLayout()
        self.add_btn    = QPushButton("Add")
        self.dup_btn    = QPushButton("Duplicate")
        self.rm_btn     = QPushButton("Remove")
        self.reset_btn  = QPushButton("Reset")
        for b in (self.add_btn, self.dup_btn, self.rm_btn, self.reset_btn):
            btn_row.addWidget(b)
        ll.addLayout(btn_row)
        self.add_btn.clicked.connect(self._on_add_case)
        self.dup_btn.clicked.connect(self._on_duplicate_case)
        self.rm_btn.clicked.connect(self._on_remove_case)
        self.reset_btn.clicked.connect(self._on_reset_cases)

        # ===== centre: case editor =====
        mid = QWidget(); ml = QVBoxLayout(mid)
        ml.setContentsMargins(6, 6, 6, 6); ml.setSpacing(6)

        name_row = QHBoxLayout()
        name_row.addWidget(QLabel("Name"))
        self.name_edit = QComboBox(); self.name_edit.setEditable(True)
        name_row.addWidget(self.name_edit, 1)
        ml.addLayout(name_row)

        plant = QGroupBox("Plant"); pf = QFormLayout(plant)
        self.ed_m = self._spin(1e-4, 1000.0, 1.0, 0.05, 4)
        self.ed_c = self._spin(0.0,  1000.0, 2.0, 0.05, 4)
        self.ed_k = self._spin(0.0,  1e6,    5.0, 0.1,  4)
        pf.addRow("m [kg]",   self.ed_m)
        pf.addRow("c [N·s/m]", self.ed_c)
        pf.addRow("k [N/m]",   self.ed_k)
        ml.addWidget(plant)

        force = QGroupBox("External force  f(t) = F·sin(ω·t)")
        ff = QFormLayout(force)
        self.ed_F = self._spin(-1000.0, 1000.0, 0.5, 0.05, 4)
        self.ed_w = self._spin(0.0,    1000.0, 2.0, 0.05, 4)
        ff.addRow("F", self.ed_F); ff.addRow("ω", self.ed_w)
        ml.addWidget(force)

        init = QGroupBox("Initial state"); inf = QFormLayout(init)
        self.ed_x0 = self._spin(-100.0, 100.0, 0.0, 0.01, 4)
        self.ed_v0 = self._spin(-100.0, 100.0, 0.0, 0.01, 4)
        inf.addRow("x0", self.ed_x0); inf.addRow("v0", self.ed_v0)
        ml.addWidget(init)

        derived = QGroupBox("Derived"); df = QFormLayout(derived)
        self.lbl_wn   = QLabel("0.000"); df.addRow("ωn [rad/s]", self.lbl_wn)
        self.lbl_zeta = QLabel("0.000"); df.addRow("ζ",          self.lbl_zeta)
        self.lbl_x_end = QLabel("—"); df.addRow("x(final) [m]", self.lbl_x_end)
        self.lbl_v_end = QLabel("—"); df.addRow("v(final) [m/s]", self.lbl_v_end)
        self.lbl_x_max = QLabel("—"); df.addRow("max |x| [m]", self.lbl_x_max)
        ml.addWidget(derived)

        ml.addStretch(1)

        # ===== right: plot =====
        right = QWidget(); rl = QVBoxLayout(right)
        rl.setContentsMargins(6, 6, 6, 6); rl.setSpacing(4)
        self.canvas = SweepCanvas()
        rl.addWidget(self.canvas, 1)

        # Sampling + actions bar
        bot = QWidget(); bl = QHBoxLayout(bot); bl.setContentsMargins(0, 0, 0, 0)
        bl.addWidget(QLabel("dt [s]"))
        self.ed_dt = self._spin(1e-6, 1.0, 0.001, 0.0005, decimals=6)
        bl.addWidget(self.ed_dt)
        bl.addWidget(QLabel("stop [s]"))
        self.ed_stop = self._spin(0.1, 1000.0, 10.0, 0.5, decimals=3)
        bl.addWidget(self.ed_stop)
        self.cb_force = QCheckBox("show force (dotted)")
        bl.addWidget(self.cb_force)
        bl.addStretch(1)
        self.run_btn  = QPushButton("Run")
        self.save_csv = QPushButton("Save CSV…")
        self.save_png = QPushButton("Save PNG…")
        bl.addWidget(self.run_btn); bl.addWidget(self.save_csv); bl.addWidget(self.save_png)
        rl.addWidget(bot)

        self.run_btn.clicked.connect(self._run_all)
        self.save_csv.clicked.connect(self._on_save_csv)
        self.save_png.clicked.connect(self._on_save_png)
        self.ed_dt.valueChanged.connect(lambda _v: self._debounce.start())
        self.ed_stop.valueChanged.connect(lambda _v: self._debounce.start())
        self.cb_force.toggled.connect(lambda _v: self._refresh_plot())

        # ===== assemble =====
        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(left); split.addWidget(mid); split.addWidget(right)
        split.setSizes([260, 320, 740])
        split.setStretchFactor(0, 0); split.setStretchFactor(1, 0); split.setStretchFactor(2, 1)

        outer = QVBoxLayout(central); outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(split, 1)

        # Status bar
        self.status_lbl = QLabel("Ready")
        mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        self.status_lbl.setFont(mono)
        sb = QStatusBar(); sb.addWidget(self.status_lbl); self.setStatusBar(sb)

        # Editor wiring (rebound when the selected case changes)
        for w in (self.ed_m, self.ed_c, self.ed_k, self.ed_F, self.ed_w,
                  self.ed_x0, self.ed_v0):
            w.valueChanged.connect(self._on_editor_changed)
        self.name_edit.editTextChanged.connect(self._on_name_changed)

    # ---- case-list management ----
    def _refresh_case_list(self, select_index: int = -1) -> None:
        self.case_list.blockSignals(True)
        self.case_list.clear()
        for i, case in enumerate(self._cases):
            it = QListWidgetItem(case.name)
            it.setFlags(it.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            it.setCheckState(Qt.CheckState.Checked if self._enabled[i]
                              else Qt.CheckState.Unchecked)
            self.case_list.addItem(it)
        self.case_list.blockSignals(False)
        if 0 <= select_index < self.case_list.count():
            self.case_list.setCurrentRow(select_index)
        elif self.case_list.count() > 0:
            self.case_list.setCurrentRow(0)

    def _current_case_index(self) -> int:
        return self.case_list.currentRow()

    def _on_case_selected(self, row: int) -> None:
        if row < 0 or row >= len(self._cases): return
        case = self._cases[row]
        self._loading_editor = True
        self.name_edit.setEditText(case.name)
        self.ed_m.setValue(case.m); self.ed_c.setValue(case.c)
        self.ed_k.setValue(case.k); self.ed_F.setValue(case.force_amplitude)
        self.ed_w.setValue(case.force_omega)
        self.ed_x0.setValue(case.x0); self.ed_v0.setValue(case.v0)
        self._loading_editor = False
        self._update_derived_labels(row)

    def _on_case_check_toggled(self, item: QListWidgetItem) -> None:
        row = self.case_list.row(item)
        if 0 <= row < len(self._enabled):
            self._enabled[row] = (item.checkState() == Qt.CheckState.Checked)
            self._refresh_plot()

    def _on_editor_changed(self, _value: float) -> None:
        if self._loading_editor: return
        row = self._current_case_index()
        if row < 0: return
        case = self._cases[row]
        case.m = self.ed_m.value(); case.c = self.ed_c.value()
        case.k = self.ed_k.value()
        case.force_amplitude = self.ed_F.value()
        case.force_omega     = self.ed_w.value()
        case.x0 = self.ed_x0.value(); case.v0 = self.ed_v0.value()
        self._debounce.start()

    def _on_name_changed(self, text: str) -> None:
        if self._loading_editor: return
        row = self._current_case_index()
        if row < 0: return
        self._cases[row].name = text
        # Update the list item label (not the checkbox state).
        it = self.case_list.item(row)
        if it is not None:
            self.case_list.blockSignals(True)
            it.setText(text)
            self.case_list.blockSignals(False)
        self._refresh_plot()

    def _on_add_case(self) -> None:
        self._cases.append(mc.MsdCase.default())
        self._cases[-1].name = f"case {len(self._cases)}"
        self._sims.append(None)
        self._enabled.append(True)
        self._refresh_case_list(select_index=len(self._cases) - 1)
        self._run_all()

    def _on_duplicate_case(self) -> None:
        row = self._current_case_index()
        if row < 0: return
        src = self._cases[row]
        dup = mc.MsdCase(name=src.name + " (copy)",
                         m=src.m, c=src.c, k=src.k,
                         force_amplitude=src.force_amplitude,
                         force_omega=src.force_omega,
                         x0=src.x0, v0=src.v0)
        self._cases.insert(row + 1, dup)
        self._sims.insert(row + 1, None)
        self._enabled.insert(row + 1, True)
        self._refresh_case_list(select_index=row + 1)
        self._run_all()

    def _on_remove_case(self) -> None:
        row = self._current_case_index()
        if row < 0 or len(self._cases) <= 1: return
        del self._cases[row]; del self._sims[row]; del self._enabled[row]
        new_sel = min(row, len(self._cases) - 1)
        self._refresh_case_list(select_index=new_sel)
        self._refresh_plot()

    def _on_reset_cases(self) -> None:
        self._cases = list(mc.default_cases())
        self._sims = [None] * len(self._cases)
        self._enabled = [True] * len(self._cases)
        self._refresh_case_list(select_index=0)
        self._run_all()

    # ---- simulation + plot ----
    def _run_all(self) -> None:
        sampling = mc.SamplingConfig(dt=self.ed_dt.value(),
                                     stop=self.ed_stop.value())
        try:
            self._sims = [mc.simulate(c, sampling) for c in self._cases]
        except Exception as exc:
            self.status_lbl.setText(f"Simulation failed: {exc}")
            return
        n_samp = len(self._sims[0].t) if self._sims else 0
        self.status_lbl.setText(
            f"OK — {len(self._cases)} cases, {n_samp} samples each"
        )
        self._update_derived_labels(self._current_case_index())
        self._refresh_plot()

    def _refresh_plot(self) -> None:
        traces = list(zip(self._cases, self._sims, self._enabled))
        self.canvas.show_sweep(traces, self.ed_stop.value(),
                               self.cb_force.isChecked())

    def _update_derived_labels(self, row: int) -> None:
        if row < 0 or row >= len(self._cases): return
        case = self._cases[row]
        self.lbl_wn.setText(f"{case.omega_n:.4f}")
        self.lbl_zeta.setText(f"{case.zeta:.4f}")
        sim = self._sims[row] if row < len(self._sims) else None
        if sim is None:
            self.lbl_x_end.setText("—")
            self.lbl_v_end.setText("—")
            self.lbl_x_max.setText("—")
        else:
            self.lbl_x_end.setText(f"{sim.final_x:+.6f}")
            self.lbl_v_end.setText(f"{sim.final_v:+.6f}")
            self.lbl_x_max.setText(f"{sim.max_abs_x:.6f}")

    # ---- save ----
    def _on_save_csv(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Save sweep CSV",
            str(Path.cwd() / "msd_sweep.csv"),
            "CSV (*.csv);;All files (*)")
        if not path: return
        header = ["time_s"]
        for c in self._cases:
            header += [f"{c.name}__x", f"{c.name}__v", f"{c.name}__f"]
        n = len(self._sims[0].t) if self._sims else 0
        with open(path, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(header)
            for i in range(n):
                row = [self._sims[0].t[i]]
                for s in self._sims:
                    row += [s.x[i], s.v[i], s.force[i]]
                w.writerow(row)
        self.status_lbl.setText(f"Saved: {path}")

    def _on_save_png(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Save plot",
            str(Path.cwd() / "msd_sweep.png"),
            "PNG (*.png);;All files (*)")
        if not path: return
        self.canvas.figure.savefig(path, dpi=150, bbox_inches="tight")
        self.status_lbl.setText(f"Saved: {path}")


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow(); w.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
