# Architecture

## Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     msd_core  (C++17 shared library)                        │
│                                                                             │
│  msd_core_simulate()  ─── RK4 fixed-step integrator                         │
│  msd_core_derived()   ─── ωn, ζ from (m, c, k)                              │
│  msd_core_version()   ─── "msd_core 1.0.0"                                  │
│                                                                             │
│  Pure C ABI.  Builds as msd_core.dll / libmsd_core.so / libmsd_core.dylib  │
└────────┬──────────────────────┬──────────────────────┬──────────────────────┘
         │ direct link (C++)    │ P/Invoke (C#)        │ ctypes (Python)
         │                      │                      │
┌────────▼──────────┐  ┌────────▼────────────────┐  ┌─▼───────────────────────┐
│ frontend_qt/      │  │ frontend_avalonia/       │  │ frontend_python/        │
│ (Qt6 Widgets)     │  │ MsdAvalonia (C#/.NET)   │  │ msd_core.py  (bindings) │
│                   │  │                          │  │                         │
│ MainWindow.cpp    │  │ Native/                  │  │ app_matplotlib.py (CLI) │
│ Widgets.cpp       │  │   MsdCoreNative.cs       │  │ app_pyside6.py    (GUI) │
│ (OverlayPlot)     │  │   MsdSolver.cs           │  └─────────────────────────┘
└───────────────────┘  │                          │
                       │ MVVM                     │
                       │   ViewModels/            │
                       │   MainWindowViewModel    │
                       │                          │
                       │ View                     │
                       │   Views/MainWindow       │
                       │   (AXAML + code-behind)  │
                       └──────────────────────────┘
```

---

## Layer 1 — C++ core (`core/`)

### Public C ABI (`include/msd_core.h`)

The core exposes a **pure C ABI** so that any language with a foreign-function
interface can load it at runtime. No C++ types, exceptions, or name mangling
cross the library boundary.

Key structs:

```c
typedef struct MsdCase {
    double m, c, k;
    double force_amplitude, force_omega;
    double x0, v0;
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;    /* time step [s] */
    double stop;  /* end time [s] */
} MsdSamplingConfig;

typedef struct MsdSimulation MsdSimulation;   /* opaque handle */
```

The `MsdSimulation` handle is heap-allocated by `msd_core_simulate()` and
must be freed by `msd_core_free_simulation()`.

### Solver (`src/msd_core.cpp`)

- Uses **fixed-step 4th-order Runge-Kutta** (see
  [algorithms.md §2](../../en/algorithms.md#2-mass_spring_damper--forced-response-rk4)).
- The derivative function `deriv()` evaluates the external force at the correct
  sub-step time so the midpoint evaluations of RK4 are accurate.
- Time index is computed as `i * dt` (not accumulated with `+= dt`) to avoid
  drift from repeated floating-point addition.
- $n = \lfloor (stop + dt) / dt - \varepsilon \rfloor + 1$ reproduces NumPy's
  `arange(0, stop + dt, dt)` sample count.

### Smoke test (`tools/smoke_test.cpp`)

Runs all five default cases and checks $|x(final) - expected| \le 10^{-5}$.
Reference values were produced by the Python script using `scipy.odeint`.
Tolerance is 1e-5; actual errors are ~10⁻⁸–10⁻⁷.

---

## Layer 2a — Qt6 frontend (`frontend_qt/`)

Links directly against `msd_core` (same C++ toolchain). No FFI overhead.

### UI structure

```
MainWindow (QMainWindow)
  ├── QListWidget           — case list with enable/disable checkboxes
  ├── Editor panel          — QDoubleSpinBox for m, c, k, F, ω, x₀, v₀
  │     └─ valueChanged     — debounced 50 ms → onRun()
  ├── Derived panel         — QLabel for ωₙ, ζ, x(final), v(final), max|x|
  └── OverlayPlot           — custom QWidget, paints with QPainter
```

`OverlayPlot` is a custom `QWidget` that overrides `paintEvent`. It receives
the simulation results from `MainWindow` via `setData()` and paints axis lines,
grid, and one polyline per enabled case using `QPainter::drawPolyline`.

The debounce timer (50 ms `QTimer::singleShot`) prevents a simulation run on
every keystroke when the user types in a spinbox.

---

## Layer 2b — Python bindings (`frontend_python/msd_core.py`)

Uses Python's built-in `ctypes` module — no compiled extension required.

### Library discovery order

1. `$MSD_CORE_LIB` environment variable
2. `../core/build/`, `../core/build/Release/`, `../core/build/Debug/`
   (covering both single-config and MSVC multi-config generators)
3. `ctypes.util.find_library("msd_core")` (system path)

### High-level API

```python
case = mc.MsdCase(m=1.0, c=2.0, k=5.0, force_amplitude=0.5, force_omega=2.0)
sampling = mc.SamplingConfig(dt=0.001, stop=10.0)
sim = mc.simulate(case, sampling)
# sim.t, sim.x, sim.v, sim.force  →  numpy arrays
# sim.final_x, sim.max_abs_x      →  float scalars
```

---

## Layer 2b — C# / Avalonia frontend (`frontend_avalonia/MsdAvalonia/`)

### P/Invoke layer (`Native/`)

`MsdCoreNative.cs` — raw `[DllImport]` declarations with `CallingConvention.Cdecl`.

`MsdSolver.cs` — managed wrapper. Converts `MsdCase`/`MsdSamplingConfig` to their
native structs, calls `msd_core_simulate`, copies the arrays into managed `double[]`,
and frees the native handle in a `finally` block.

### MVVM structure

```
MainWindowViewModel
  ├── Cases              : AvaloniaList<CaseEntry>
  ├── SelectedIndex      : int  (drives the editor)
  ├── Editor*            : double properties (EditorM, EditorC, …)
  │     └─ SetField(syncToCase:true) → SyncEditorToCase() → Run()
  ├── Dt / Stop          : double (SetField runOnChange:true → Run())
  ├── Run()              : simulate all cases → RebuildPlot()
  ├── GridLines          : AvaloniaList<GridLineMarker>   (grid lines)
  └── Series             : AvaloniaList<SeriesPolyline>   (data traces)
```

**Auto-rerun design**: every parameter change calls `Run()` automatically via
`SetField`. The **Run** button is available for manual re-execution (e.g. after
editing `dt`/`stop` without pressing Enter) but is otherwise redundant.

### Plot rendering

The plot canvas is a fixed-size `<Canvas Width="700" Height="460">`.
Grid lines and data polylines are rendered via `ItemsControl` bindings:

- `GridLines` → `<Line StartPoint=… EndPoint=…>`
- `Series` → `<Polyline Points=… Stroke=…>`

`AvaloniaList<Point>` is used for `Polyline.Points` so that `Clear()`/`Add()`
triggers binding notifications (a plain `List<Point>` does not).

Axis tick labels are positioned with hardcoded `Canvas.Left`/`Canvas.Top`
coordinates that match the plot box margins. The label values (`RefX0`–`RefX100`,
`RefY0`–`RefY100`) are recomputed in `RebuildPlot()` whenever the data range
changes.

---

## Design decisions

### Why a C ABI for the core?

A pure C ABI is the lowest common denominator for cross-language FFI:

- Python loads it with `ctypes` (no compilation needed).
- C# loads it with `[DllImport]` / P/Invoke.
- Any other language (Rust, Julia, Lua, …) can do the same.

### Why fixed-step RK4?

The system is a linear, well-conditioned ODE. At `dt = 1 ms`, fixed-step RK4
gives errors of ~10⁻⁷, which is indistinguishable on a plot. Adaptive-step
solvers (like LSODA used by `scipy.odeint`) add complexity with no perceptible
benefit for this application.

### Why auto-rerun on every parameter change?

The simulation runs in under 1 ms for the default 10001-sample case, so there
is no perceptible lag. Coupling parameter changes directly to rerun eliminates
the need for the user to remember to press Run, and keeps the plot always
consistent with the editor.
