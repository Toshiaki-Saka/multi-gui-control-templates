# Planar path-tracking PI demo — multi-frontend port (Windows-first)

A C++ implementation of the planar-motion + PI path-tracking simulation
from `planar_path_tracking_pi_tuned.py`, plus three GUI frontends that
share the same native core.

```
┌──────────────────────────────────────────────────────────────────────┐
│            C++ core  (track_core.dll / .so / .dylib)                 │
│            ─ 3-DOF planar vehicle (u, v, r, x, y, psi)               │
│            ─ Per-state RK4 integration                               │
│            ─ Speed PI + lateral PI + yaw-rate damping                │
│            ─ Reference path: straight → arc → straight               │
│                                                                      │
│   ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────────┐ │
│   │ Qt6 / C++       │  │ Avalonia / C#   │  │ PySide6 / matplotlib │ │
│   │ frontend_qt     │  │ frontend_avalonia│ │ frontend_python      │ │
│   └─────────────────┘  └─────────────────┘  └──────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

The C++ core matches the Python reference to all 6 printed digits on
every aggregate metric, and trajectory-wise to ~5×10⁻¹⁰ — pure
floating-point noise.

Project is **Windows-first**. CMake, `.csproj`, and the Python loader
all handle Windows conventions (DLL next to the .exe, MSVC
`Release\`/`Debug\` config subdirectories).

---

## What it computes

```
Plant (3-DOF planar vehicle):
    u_dot   = fx/m + r·v               (longitudinal velocity)
    v_dot   = fy/m - r·u               (lateral velocity)
    r_dot   = n_moment / izz           (yaw rate)
    x_dot   = u·cos(psi) - v·sin(psi)  (global X)
    y_dot   = u·sin(psi) + v·cos(psi)  (global Y)
    psi_dot = r                        (heading)
    fy      = -cornering_power · beta    where beta = atan2(v, u)

Reference path: straight (X) → 90° left arc → straight (Y),
resampled at arc-length step ds = 0.002 m.

Controller (updated every tc = 1 ms; integrator step h = 0.1 ms):
    fx       = sat(100·speed_err + 0.1·∫speed_err,  ±fx_limit)
    n_moment = sat(-ky_p·e_y    - ky_i·∫e_y
                  + kpsi_p·e_psi + kpsi_i·∫e_psi
                  - kr_damping·r,                   ±n_moment_limit)
where (e_y, e_psi) are evaluated at a look-ahead point 60 indices
ahead of the nearest reference point.
```

Defaults give a smooth left-turn through the 90° arc starting from a
3 cm lateral offset and 3° heading error.

---

## Directory layout

```
track\
├── core\                            # cross-language C++ library
│   ├── include\track_core.h
│   ├── src\track_core.cpp
│   ├── tools\smoke_test.cpp
│   └── CMakeLists.txt
├── frontend_python\
│   ├── track_core.py                # ctypes bindings
│   ├── app_matplotlib.py            # drop-in for the reference script
│   ├── app_pyside6.py               # GUI with parameter sliders + 4 tabs
│   └── requirements.txt
├── frontend_qt\
│   ├── main.cpp, MainWindow.{hpp,cpp}, Widgets.{hpp,cpp}
│   └── CMakeLists.txt               # runs windeployqt on Windows
├── frontend_avalonia\TrackAvalonia\
│   ├── TrackAvalonia.csproj         # copies track_core.dll next to .exe
│   ├── Native\{TrackCoreNative.cs, TrackSolver.cs}
│   ├── Models\GridLineMarker.cs
│   ├── ViewModels\MainWindowViewModel.cs
│   ├── Views\MainWindow.{axaml,axaml.cs}
│   └── App.axaml{,.cs}, Program.cs, app.manifest
├── build_all.bat                    # Windows convenience build
├── build_all.sh                     # Linux/macOS convenience build
└── README.md
```

---

## 1. Building the core library

Requirements: **CMake 3.16+** and a C++17 compiler. **No external
dependencies** — pure C++.

### Windows (Visual Studio / MSVC)

```powershell
cd core
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
# track_core.dll lives at  core\build\Release\track_core.dll
```

Run the smoke test:

```powershell
cmake --build . --target track_core_smoke --config Release
.\Release\track_core_smoke.exe
```

Expected output (matches the Python reference exactly):

```
=== track_core 1.0.0 ===
Reference points: 458
Simulation steps : 9000  (expect 9000)
First (x,y)  : (0.000000, -0.030000)
Last  (x,y)  : (0.501808, 0.477185)
path_error_rms = 0.013762  (expect 0.013762)
path_error_max = 0.030000  (expect 0.030000)
e_y_rms        = 0.020243
e_y_max        = 0.032639
e_psi_rms      = 0.300645
e_psi_max      = 0.579404
max|n_moment|  = 28.495415
```

### Linux / macOS

```bash
cd core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
cmake --build . --target track_core_smoke -j
./track_core_smoke
```

---

## 2. Qt6 (C++) frontend

Requirements: **Qt 6.2+** (built with the same toolchain you use for
the core), CMake 3.16+.

### Windows

```powershell
cd frontend_qt
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build . --config Release
.\Release\track_qt.exe
```

`windeployqt` runs automatically after the build, so all Qt DLLs end
up next to `track_qt.exe`. The core's `track_core.dll` is placed in
the same directory.

### Linux / macOS

```bash
cd frontend_qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./track_qt
```

The GUI has 4 tabs:
- **XY** — equal-aspect 2-D map with reference path (blue dashed) and
  actual path (orange), plus Start/End markers.
- **States** — `u, v, r, psi, beta, speed` in a 3×2 grid.
- **Errors** — `path_error, e_y, e_psi` stacked.
- **Inputs** — `n_moment, fx` with dashed saturation lines.

Drag any parameter in the left panel and the simulation re-runs after
a short debounce.

---

## 3. Avalonia (C#) frontend

Requirements: **.NET 8.0 SDK** + internet for the first NuGet restore.

```powershell
# 1) build the core (above) so track_core.dll exists at
#    core\build\Release\track_core.dll

# 2) build + run the Avalonia frontend
cd frontend_avalonia\TrackAvalonia
dotnet run -c Release
```

The csproj automatically copies `track_core.dll` from
`..\..\core\build\Release\` (and `..\..\core\build\` for single-config
generators) next to the .exe in `bin\Release\net8.0\`.

---

## 4. Python frontend

Requirements: **Python 3.10+**, NumPy, matplotlib, PySide6.

```powershell
# build the core first as above, then:
cd frontend_python
pip install -r requirements.txt

python app_matplotlib.py   # drop-in: writes output_path_tracking_pi_tuned\
python app_pyside6.py      # GUI with parameter sliders + 4 tabs
```

The Python loader searches `..\core\build\`, `..\core\build\Release\`,
and `..\core\build\Debug\` automatically. Override with
`set TRACK_CORE_LIB=path\to\track_core.dll`.

---

## Algorithm notes

The original Python pipeline is reproduced bit-for-bit:

1. **Per-state RK4**. The reference Python code calls a generic
   scalar RK4 helper six times per simulation step — once for each of
   `u, v, r, x, y, psi`. The arguments other than the integrated
   variable are kept constant (the "old" values from the previous
   step) across the four RK4 sub-steps. The C++ core does the same.
   This is *not* equivalent to a vector RK4: the cross-coupling terms
   `r·v`, `r·u`, etc. are evaluated only at the start of the step.

2. **Reference path**. Each segment is generated with `np.arange`
   semantics (`np.arange(0.0, L, ds)` excludes the endpoint). The C++
   side computes the same point counts with
   `ceil(L/ds - 1e-12)` so concatenated lengths match the Python
   reference.

3. **Nearest-point search**. Windowed local search around the previous
   index `[prev-5, prev+120)`, taking the squared-distance argmin.
   `look-ahead` then advances the target by `lookahead_index` points.

4. **Lateral error sign**. `e_y = -sin(psi_ref) · dx + cos(psi_ref) · dy`,
   i.e. positive when the vehicle is to the left of the reference
   tangent. This is why the n-moment formula uses `-ky_p · e_y`.

The smoke test verifies the resulting `path_error_rms`, `e_y_rms`,
`e_psi_rms`, and `max|n_moment|` against the Python output values.

---

## C ABI reference (`core\include\track_core.h`)

```c
typedef struct TrackConfig {
    double m, izz, cornering_power;     /* plant */
    double h, tc, total_time;           /* sampling */
    double target_speed;
    double ky_p, ky_i, kpsi_p, kpsi_i, kr_damping;
    double n_moment_limit, fx_limit, error_integral_limit;
    int32_t lookahead_index;
    double initial_y_offset, initial_heading_deg;
    double straight1_len, radius, straight2_len, ds;  /* reference */
} TrackConfig;

void track_core_default_config(TrackConfig*);

TrackReferencePath* track_core_make_reference(const TrackConfig*);
TrackSimulation*    track_core_simulate(const TrackConfig*);
/* … 15 per-sample accessors (time, x, y, psi, u, v, r, beta,
 *   ey, epsi, n_moment, fx, x_ref, y_ref, psi_ref) … */
/* … 7 aggregate metrics (path_error_rms/max, ey_rms/max,
 *   epsi_rms/max, nmoment_max_abs) … */
```

All numeric fields are `double`; sizes/indices are `int32_t`.
