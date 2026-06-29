# Python Bindings

`frontend_python/track_core.py` wraps the C library via `ctypes`.
It exposes three dataclasses (`TrackConfig`, `ReferencePath`, `Simulation`)
and two top-level functions (`make_reference`, `simulate`).

---

## Installation

```bash
# 1. Build the C core (see README §1)
cd core && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j

# 2. Install Python dependencies
cd ../../frontend_python
pip install -r requirements.txt   # numpy, matplotlib, PySide6
```

The loader searches for the shared library in this order:

1. `$TRACK_CORE_LIB` environment variable (absolute path)
2. `../core/build/` (single-config generators, Linux/macOS)
3. `../core/build/Release/` and `../core/build/Debug/` (MSVC multi-config)
4. `ctypes.util.find_library('track_core')` (system path)

To override:
```bash
set TRACK_CORE_LIB=C:\path\to\track_core.dll   # Windows
export TRACK_CORE_LIB=/path/to/libtrack_core.so  # Linux/macOS
```

---

## Quick Start

```python
import track_core as tc

# Run with default parameters
sim = tc.simulate()

print(f"path_error_rms = {sim.path_error_rms:.6f} m")
print(f"e_y_max        = {sim.ey_max:.6f} m")
print(f"e_psi_max      = {sim.epsi_max:.6f} rad")
```

---

## `TrackConfig`

A plain `dataclass` mirroring `TrackConfig` in C.
Field names and defaults are identical to the C struct (see [api.md](api.md)).

```python
@dataclass
class TrackConfig:
    m:               float = 0.1
    izz:             float = 1.0
    cornering_power: float = 20.0
    h:               float = 1e-4
    tc:              float = 1e-3
    total_time:      float = 0.90
    target_speed:    float = 1.0
    ky_p:            float = 400.0
    ky_i:            float = 0.0
    kpsi_p:          float = 200.0
    kpsi_i:          float = 0.0
    kr_damping:      float = 20.0
    n_moment_limit:  float = 500.0
    fx_limit:        float = 5.0
    error_integral_limit: float = 0.2
    lookahead_index: int   = 60
    initial_y_offset:     float = -0.03
    initial_heading_deg:  float =  3.0
    straight1_len:   float = 0.30
    radius:          float = 0.20
    straight2_len:   float = 0.30
    ds:              float = 0.002
```

Use `TrackConfig.default()` to read defaults from the C library itself:

```python
cfg = tc.TrackConfig.default()
cfg.radius = 0.30   # widen the arc
cfg.ky_p   = 600.0  # stronger lateral gain
sim = tc.simulate(cfg)
```

---

## `make_reference`

```python
def make_reference(cfg: TrackConfig | None = None) -> ReferencePath
```

Returns the reference path as NumPy arrays.

```python
ref = tc.make_reference()
print(f"{len(ref.x)} reference points")

import matplotlib.pyplot as plt
plt.plot(ref.x, ref.y)
plt.axis("equal")
plt.show()
```

### `ReferencePath` fields

| Field | Type | Description |
|-------|------|-------------|
| `x` | `np.ndarray` (float64) | Global X positions [m] |
| `y` | `np.ndarray` (float64) | Global Y positions [m] |
| `psi` | `np.ndarray` (float64) | Reference heading [rad] |

---

## `simulate`

```python
def simulate(cfg: TrackConfig | None = None) -> Simulation
```

Runs the full closed-loop simulation and returns results.

### `Simulation` fields

**Time-series arrays** — one value per integration step (`len = int(total_time / h)`):

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `t` | `np.ndarray` | s | Time |
| `x`, `y` | `np.ndarray` | m | Vehicle position (global) |
| `psi` | `np.ndarray` | rad | Heading |
| `u`, `v` | `np.ndarray` | m/s | Body-frame velocities |
| `r` | `np.ndarray` | rad/s | Yaw rate |
| `beta` | `np.ndarray` | rad | Side-slip angle |
| `ey` | `np.ndarray` | m | Lateral error at look-ahead |
| `epsi` | `np.ndarray` | rad | Heading error at look-ahead |
| `n_moment` | `np.ndarray` | N·m | Applied yaw moment |
| `fx` | `np.ndarray` | N | Applied longitudinal force |
| `x_ref`, `y_ref` | `np.ndarray` | m | Nearest reference point |
| `psi_ref` | `np.ndarray` | rad | Nearest reference heading |

**Computed property:**

```python
@property
def path_error(self) -> np.ndarray:
    return np.hypot(self.x - self.x_ref, self.y - self.y_ref)
```

**Aggregate metrics:**

| Field | Unit | Description |
|-------|------|-------------|
| `path_error_rms` | m | RMS distance to nearest reference point |
| `path_error_max` | m | Peak distance to nearest reference point |
| `ey_rms` | m | RMS lateral error at look-ahead |
| `ey_max` | m | Peak absolute lateral error |
| `epsi_rms` | rad | RMS heading error |
| `epsi_max` | rad | Peak absolute heading error |
| `nmoment_max` | N·m | Peak absolute yaw moment |

---

## Parameter Sweep Example

```python
import numpy as np
import track_core as tc

ky_values = np.linspace(200, 800, 13)
results = []
for ky in ky_values:
    cfg = tc.TrackConfig.default()
    cfg.ky_p = ky
    sim = tc.simulate(cfg)
    results.append((ky, sim.path_error_rms, sim.ey_max))

print(f"{'ky_p':>8}  {'path_err_rms':>14}  {'ey_max':>10}")
for ky, rms, mx in results:
    print(f"{ky:8.1f}  {rms:14.6f}  {mx:10.6f}")
```

---

## GUI Frontends

| Script | Description |
|--------|-------------|
| `app_matplotlib.py` | Batch run; saves plots to `output_path_tracking_pi_tuned/` |
| `app_pyside6.py` | Interactive GUI with parameter sliders and 4 plot tabs |

Run either after building the core:

```bash
python app_matplotlib.py   # saves figures
python app_pyside6.py      # interactive
```
