# msd — Mass-Spring-Damper Forced Response

Interactive parameter-sweep simulator for a single-degree-of-freedom
mass-spring-damper system driven by a sinusoidal external force.

The project has a **shared C++ simulation core** consumed by three independent
frontend technologies:

| Frontend | Technology | Description |
|----------|-----------|-------------|
| `frontend_qt/` | C++ + Qt6 Widgets | Interactive GUI (C++) |
| `frontend_python/app_pyside6.py` | Python + PySide6 + Matplotlib | Interactive GUI (Python) |
| `frontend_python/app_matplotlib.py` | Python + Matplotlib | Batch CLI, saves PNG |
| `frontend_avalonia/` | C# + Avalonia (.NET 8) | Interactive GUI (C#) |

## Physics

The plant equation is:

$$m\ddot{x} + c\dot{x} + kx = F\sin(\omega t)$$

The C++ core integrates it with a fixed-step RK4 scheme, reproducing the
Python reference (`scipy.integrate.odeint`) to within ~10⁻⁷ for the default
cases. See [docs/en/mass_spring_damper/theory.md](../../docs/en/mass_spring_damper/theory.md)
([日本語](../../docs/ja/mass_spring_damper/theory.md)) for the full derivation.

## Project structure

```
msd/
├── core/                    # C++17 shared library (msd_core)
│   ├── include/msd_core.h   # Public C ABI
│   ├── src/msd_core.cpp     # RK4 solver
│   ├── tools/smoke_test.cpp # Numerical regression test
│   └── CMakeLists.txt
├── frontend_qt/             # C++ / Qt6 Widgets GUI
│   ├── main.cpp
│   ├── MainWindow.{hpp,cpp}
│   ├── Widgets.{hpp,cpp}    # OverlayPlot custom widget
│   └── CMakeLists.txt       # runs windeployqt on Windows
├── frontend_python/         # Python frontends (ctypes → msd_core)
│   ├── msd_core.py          # ctypes bindings + high-level Python API
│   ├── app_matplotlib.py    # CLI: simulate and save PNG
│   ├── app_pyside6.py       # GUI: interactive parameter sweep
│   └── requirements.txt
├── frontend_avalonia/       # C# / Avalonia cross-platform GUI
│   └── MsdAvalonia/
│       ├── Native/          # P/Invoke declarations + managed wrapper
│       ├── Models/          # Avalonia data model types
│       ├── ViewModels/      # MVVM ViewModel
│       └── Views/           # AXAML layout + code-behind
├── build_all.sh             # One-shot build (Linux / macOS)
├── build_all.bat            # One-shot build (Windows, MSVC)
└── build_and_run.ps1        # Build + launch helper (Windows PowerShell)
```

## Prerequisites

### C++ core (required for all frontends)

| Tool | Minimum version |
|------|----------------|
| CMake | 3.16 |
| C++ compiler | MSVC 2019, GCC 10, or Clang 12 (C++17) |

### Qt6 frontend

| Tool | Notes |
|------|-------|
| Qt6 | 6.2+ with Core, Gui, Widgets modules |
| Same compiler as the core | MSVC, GCC, or Clang |

### Python frontends

| Package | Purpose |
|---------|---------|
| Python 3.9+ | runtime |
| numpy | array handling |
| matplotlib | plotting |
| PySide6 | GUI (only for `app_pyside6.py`) |

### Avalonia frontend

| Tool | Notes |
|------|-------|
| .NET SDK 8.0+ | `dotnet` must be on `PATH` |

## Quick start

### Windows (PowerShell)

```powershell
# Build C++ core + Qt6 frontend, then launch Qt6 GUI
.\build_and_run.ps1 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"

# Build C++ core + Avalonia, then launch Avalonia GUI
.\build_and_run.ps1 2

# Build C++ core + Python deps, then launch PySide6 GUI
.\build_and_run.ps1 3
```

### Linux / macOS

```bash
# Build the C++ core
./build_all.sh

# Run the CLI matplotlib frontend
cd frontend_python && python app_matplotlib.py

# Run the Avalonia GUI (.NET 8 required)
cd frontend_avalonia/MsdAvalonia && dotnet run -c Release
```

### Manual build (all platforms)

```bash
# 1. Build the shared library
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build -j

# 2. (Optional) Run the smoke test against Python reference values
cmake --build core/build --target msd_core_smoke
./core/build/msd_core_smoke          # Linux/macOS
# core\build\Release\msd_core_smoke.exe   (Windows)

# 3a. Qt6 frontend (Qt6 must be installed)
cmake -S frontend_qt -B frontend_qt/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build frontend_qt/build -j
./frontend_qt/build/msd_qt           # Linux/macOS
# frontend_qt\build\Release\msd_qt.exe   (Windows)

# 3b. Python frontends
pip install -r frontend_python/requirements.txt
python frontend_python/app_pyside6.py      # GUI
python frontend_python/app_matplotlib.py   # CLI (saves PNG)

# 3c. Avalonia frontend
cd frontend_avalonia/MsdAvalonia
dotnet run -c Release
```

## Using the GUI

Both GUIs present three panels:

- **Left — Cases**: list of simulation cases. Toggle visibility with the
  checkbox. **Add / Duplicate / Remove / Reset** manage the list.
- **Centre — Editor**: edit parameters (m, c, k, F, ω, x₀, v₀) for the
  selected case. Derived quantities (ωₙ, ζ, final x/v, max |x|) update
  instantly. The graph reruns automatically on every change.
- **Right — Plot**: overlay of position x(t) for all enabled cases.

> **Note:** The **Run** button re-runs simulations with the current `dt`
> and `stop` values. Changing any parameter already triggers an automatic
> rerun, so in normal use the button is not needed. It is useful after
> editing `dt`/`stop` in the number box without pressing Enter.

## Default test cases

| Name | c | k | ωₙ [rad/s] | ζ | Behaviour |
|------|---|---|------------|---|-----------|
| baseline | 2.0 | 5.0 | 2.24 | 0.45 | Underdamped |
| low damping | 0.5 | 5.0 | 2.24 | 0.11 | Lightly damped |
| high damping | 5.0 | 5.0 | 2.24 | 1.12 | Overdamped |
| stiffer spring | 2.0 | 12.0 | 3.46 | 0.29 | Higher natural frequency |
| near natural frequency | 0.5 | 5.0 | 2.24 | 0.11 | ω ≈ ωₙ (near resonance) |

All cases: m = 1 kg, F = 0.5 N, x₀ = 0 m, v₀ = 0 m/s.
Sampling defaults: dt = 1 ms, stop = 10 s (10 001 samples).

## C ABI reference

```c
typedef struct MsdCase {
    double m, c, k;            /* plant parameters */
    double force_amplitude;    /* F [N] */
    double force_omega;        /* ω [rad/s] */
    double x0, v0;             /* initial state */
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;                 /* time step [s] */
    double stop;               /* end time [s] */
} MsdSamplingConfig;

void            msd_core_default_case    (MsdCase*);
void            msd_core_default_sampling(MsdSamplingConfig*);
int32_t         msd_core_derived         (const MsdCase*, double* omega_n, double* zeta);
MsdSimulation*  msd_core_simulate        (const MsdCase*, const MsdSamplingConfig*);
void            msd_core_free_simulation (MsdSimulation*);
int32_t         msd_core_sim_length      (const MsdSimulation*);
/* copy accessors: time, position, velocity, force */
/* scalar results: final_position, final_velocity, max_abs_position, max_abs_velocity */
```

See [core/include/msd_core.h](core/include/msd_core.h) for the full API.

## Documentation

| Document | Contents |
|----------|---------|
| [theory.md](../../docs/en/mass_spring_damper/theory.md) | Equation of motion, state-space form, RK4 derivation (LaTeX) |
| [api.md](../../docs/en/mass_spring_damper/api.md) | `msd_core` C ABI reference |
| [architecture.md](../../docs/en/mass_spring_damper/architecture.md) | Layer diagram, data flow, design decisions |
| [build.md](../../docs/en/mass_spring_damper/build.md) | Detailed build and environment setup |
| [avalonia-notes.md](../../docs/en/mass_spring_damper/avalonia-notes.md) | Avalonia 11 implementation tips |
| [avalonia-debug-polylines.md](../../docs/en/mass_spring_damper/avalonia-debug-polylines.md) | Troubleshooting when plot polylines do not render |

> All documentation lives under [`docs/`](../../docs/): English in
> [`docs/en/mass_spring_damper/`](../../docs/en/mass_spring_damper/README.md), Japanese in
> [`docs/ja/mass_spring_damper/`](../../docs/ja/mass_spring_damper/README.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).
