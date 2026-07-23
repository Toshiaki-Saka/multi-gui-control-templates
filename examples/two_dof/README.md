# tdof — 1-DOF PID vs 2-DOF Control Comparison

A C++ implementation of a 1-DOF PID vs 2-DOF-like control response comparison,
with three GUI frontends sharing a single shared-library core.

```
┌──────────────────────────────────────────────────────────────────────┐
│            C++ core  (libtdof_core.so / .dll / .dylib)               │
│            ─ Transfer-function polynomial algebra                    │
│            ─ TF → controllable canonical state space                 │
│            ─ Forced response via first-order-hold discretisation     │
│              (Eigen matrix exponential, matches python-control)      │
│                                                                      │
│   ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────────┐ │
│   │ Qt6 / C++       │  │ Avalonia / C#   │  │ PySide6 / matplotlib │ │
│   │ frontend_qt     │  │ frontend_avalonia│  │ frontend_python      │ │
│   └─────────────────┘  └─────────────────┘  └──────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

The C++ core is a small shared library with a stable C ABI.
All three frontends link to (or P/Invoke into) the same binary.

> **What this is.** An *educational* 2-DOF control demo, not a production
> control library — the textbook PID-vs-2-DOF comparison itself is something
> `python-control` or MATLAB write in a few lines. The point here is the
> **architecture**: one C++ numerical core (TF algebra → controllable canonical
> form → first-order-hold discretisation, matching `python-control` to ~5e-9)
> exposed over a stable C ABI and driven from Qt6 / Avalonia (C#) / Python.
> It shares that "one core, three GUIs" shape with the sibling `pid` and
> `pid-discretization-lab` projects.

---

## What it computes

A mass-spring-damper plant is controlled by a PID:

```
Plant            P(s)  = 1 / (m s² + c s + k)
PID controller   K1(s) = (kd s² + kp s + ki) / s
Reference filter K2(s) = (kp s + ki) / (kd s² + kp s + ki)
Closed loop      Gyz   = feedback(P · K1, 1)
```

A unit-step reference `r` is applied in two ways:

| Mode | Signal path | Output |
|------|-------------|--------|
| 1-DOF PID | `r → Gyz` | `y_pid` |
| 2-DOF-like | `r → K2 → Gyz` | `y_2dof` |

The comparison shows how reference pre-filtering (K2) reduces the aggressiveness
of the commanded reference `z = K2·r` and reshapes the transient response.

See [`docs/math.md`](docs/math.md) for a full mathematical derivation.

---

## Directory layout

```
tdof/
├── core/                            # cross-language C++ shared library
│   ├── include/tdof_core.h          #   public C ABI
│   ├── src/
│   │   ├── tf.cpp                   #   transfer-function algebra
│   │   ├── simulate.cpp             #   state-space realisation & FOH simulation
│   │   └── tdof_core.cpp            #   C ABI wrapper
│   ├── tools/smoke_test.cpp         #   CLI smoke test
│   └── CMakeLists.txt
├── frontend_python/                 # Python frontend
│   ├── tdof_core.py                 #   ctypes bindings
│   ├── app_matplotlib.py            #   drop-in for the reference script
│   ├── app_pyside6.py               #   interactive GUI
│   └── requirements.txt
├── frontend_qt/                     # Qt6 (C++) frontend
│   ├── main.cpp
│   ├── MainWindow.{hpp,cpp}
│   ├── Widgets.{hpp,cpp}
│   └── CMakeLists.txt
├── frontend_avalonia/TdofAvalonia/  # Avalonia (C# / .NET 8) frontend
│   ├── TdofAvalonia.csproj
│   ├── Native/{TdofCoreNative.cs, TdofSolver.cs}
│   ├── Models/PlotSeries.cs
│   ├── ViewModels/MainWindowViewModel.cs
│   └── Views/MainWindow.{axaml,axaml.cs}
├── docs/
│   └── math.md                      # mathematical background (LaTeX)
├── build_all.sh                     # convenience build script (Linux/macOS)
├── build_and_run.ps1                # convenience build script (Windows)
├── LICENSE
└── README.md
```

---

## Building

### Requirements

| Component | Requirement |
|-----------|-------------|
| C++ core | CMake 3.16+, C++17 compiler, **Eigen 3.3+** |
| Qt6 frontend | Qt 6.2+ |
| Avalonia frontend | .NET 8.0 SDK |
| Python frontend | Python 3.10+, see `requirements.txt` |

Install Eigen:
- **Debian/Ubuntu**: `sudo apt install libeigen3-dev`
- **Homebrew**: `brew install eigen`
- **vcpkg**: `vcpkg install eigen3`

---

### 1. C++ core library

```bash
cd core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

Produces `libtdof_core.so` / `.dylib` / `tdof_core.dll`.

Run the smoke test:

```bash
cmake --build . --target tdof_core_smoke
./tdof_core_smoke
```

Expected output:

```
=== tdof_core 1.0.0 ===
Closed Gyz:  num[0.1, 2, 10]  den[0.01, 0.115, 3, 10]
samples       : 200
y_pid  max/fin: 11.39699 / 9.99570
y_2dof max/fin: 12.04883 / 9.99500
PID overshoot : 14.0%
2DOF overshoot: 20.5%
```

---

### 2. Qt6 frontend

```bash
cd frontend_qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./tdof_qt
```

The Qt CMakeLists.txt fetches the sibling `../core` automatically.

---

### 3. Avalonia frontend (C# / .NET 8)

Build the core first, then:

```bash
cd frontend_avalonia/TdofAvalonia
dotnet run -c Release
```

The `.csproj` copies `tdof_core.dll` / `.so` / `.dylib` from `../../core/build/`
next to the executable automatically.

---

### 4. Python frontend

```bash
cd frontend_python
pip install -r requirements.txt
python3 app_matplotlib.py    # static matplotlib output
python3 app_pyside6.py       # interactive PySide6 GUI
```

Both apps search for the core library in `../core/build/` by default.
Override with the environment variable:

```bash
TDOF_CORE_LIB=/path/to/libtdof_core.so python3 app_pyside6.py
```

---

### All-in-one (Linux/macOS)

```bash
./build_all.sh
```

---

## C ABI reference

See [`core/include/tdof_core.h`](core/include/tdof_core.h) for the full API.
Key types and functions:

```c
typedef struct TdofConfig {
    double m, c, k;       /* plant: P(s) = 1/(ms²+cs+k) */
    double kp, ki, kd;    /* PID gains */
    double ref;           /* step amplitude for display scaling */
    double t_end, dt;     /* time grid [0, t_end) step dt */
} TdofConfig;

void            tdof_core_default_config(TdofConfig*);
TdofSimulation* tdof_core_simulate(const TdofConfig*);
void            tdof_core_free_simulation(TdofSimulation*);
int32_t         tdof_core_sim_length(const TdofSimulation*);
int32_t         tdof_core_sim_copy_time  (const TdofSimulation*, double*, int32_t);
int32_t         tdof_core_sim_copy_r     (const TdofSimulation*, double*, int32_t);
int32_t         tdof_core_sim_copy_z     (const TdofSimulation*, double*, int32_t);
int32_t         tdof_core_sim_copy_y_pid (const TdofSimulation*, double*, int32_t);
int32_t         tdof_core_sim_copy_y_2dof(const TdofSimulation*, double*, int32_t);
```

Polynomial coefficients are **highest-power-first** throughout.

---

## Mathematical background

See [`docs/math.md`](docs/math.md) for:

- Plant model and transfer functions
- Reference-filter derivation
- Closed-loop algebra
- Controllable canonical state-space realisation
- First-order-hold discretisation via matrix exponential

---

## License

[Apache-2.0](LICENSE)
