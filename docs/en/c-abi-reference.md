# C ABI Reference

日本語版: [../ja/c-abi-reference.md](../ja/c-abi-reference.md)

Every exported symbol of the four cores. Headers live at
`examples/<name>/core/include/<slug>_core.h`.

## Common rules

- **Calling convention**: C (`extern "C"`, cdecl). C# declares
  `CallingConvention.Cdecl`; Python uses `ctypes.CDLL`.
- **Types across the boundary**: `double`, `int32_t`, opaque handle pointers,
  `const char*`. Nothing else.
- **Handles are opaque.** `*_simulate()` returns a pointer you must release with
  the matching `*_free_simulation()`. Passing `NULL` to any accessor is safe and
  returns `0` / `0.0`.
- **Buffers belong to the caller.** Every `*_copy_*()` takes
  `(handle, double* buffer, int32_t buffer_len)`, returns the number of samples
  written, and returns `0` without writing if `buffer_len` is smaller than the
  simulation length. Size the buffer with `*_sim_length()` first.
- **Errors are values.** `*_simulate()` returns `NULL` for invalid or non-finite
  configurations. Nothing throws across the boundary.
- **Export macros**: `<SLUG>_CORE_API` expands to `__declspec(dllexport)` when
  building the core with `<SLUG>_CORE_BUILD` defined, `__declspec(dllimport)`
  for consumers on Windows, and `__attribute__((visibility("default")))`
  elsewhere.

Typical usage from C:

```c
PidConfig cfg;
pid_core_default_config(&cfg);
cfg.kp = 0.2;

PidSimulation* sim = pid_core_simulate(&cfg);
if (!sim) { /* invalid configuration */ }

int32_t n = pid_core_sim_length(sim);
double* t     = malloc(n * sizeof(double));
double* theta = malloc(n * sizeof(double));
pid_core_sim_copy_time (sim, t,     n);
pid_core_sim_copy_theta(sim, theta, n);

pid_core_free_simulation(sim);
```

---

## 1. `pid_core`

Header: `examples/pid/core/include/pid_core.h`.

### Config

```c
typedef struct PidConfig {
    double  theta_start;     /* initial state                              */
    double  theta_goal;      /* setpoint                                   */
    double  offset;          /* subtracted from theta each step (bias)     */
    int32_t time_length;     /* number of samples (>= 2)                   */
    double  kp, ki, kd;      /* gains                                      */
    double  dt;              /* time step (> 0); scales the I and D terms  */
    double  integral_clamp;  /* max |error_sum|; 0 = disabled              */
    double  output_clamp;    /* max |m|;         0 = disabled              */
} PidConfig;
```

### Functions

| Function | Description |
|----------|-------------|
| `void pid_core_default_config(PidConfig*)` | Fills the defaults (θ 0 → 90, N = 150, kp 0.10, ki 0.01, kd 0, dt 1, clamps off). |
| `PidSimulation* pid_core_simulate(const PidConfig*)` | Runs the loop. `NULL` if any field is non-finite, `dt <= 0`, a clamp is negative, or `time_length < 2`. |
| `void pid_core_free_simulation(PidSimulation*)` | Releases the handle. |
| `int32_t pid_core_sim_length(const PidSimulation*)` | Sample count (= `time_length`). |
| `int32_t pid_core_sim_copy_time(const PidSimulation*, double*, int32_t)` | Time axis $`t_n = n\,\Delta t`$. |
| `int32_t pid_core_sim_copy_theta(const PidSimulation*, double*, int32_t)` | Response θ. |
| `double pid_core_sim_final_theta(const PidSimulation*)` | Last sample. |
| `double pid_core_sim_max_theta(const PidSimulation*)` | Maximum over the run. |
| `double pid_core_sim_min_theta(const PidSimulation*)` | Minimum over the run. |
| `const char* pid_core_version(void)` | Static string, e.g. `"pid_core 1.0.0"`. |

---

## 2. `msd_core`

Header: `examples/mass_spring_damper/core/include/msd_core.h`.
This core splits the plant (`MsdCase`) from the sampling grid
(`MsdSamplingConfig`) so a frontend can sweep several cases on one grid.

### Structs

```c
typedef struct MsdCase {
    double m;                 /* mass [kg], > 0            */
    double c;                 /* damping [N·s/m]           */
    double k;                 /* stiffness [N/m], >= 0     */
    double force_amplitude;   /* F [N]                     */
    double force_omega;       /* ω [rad/s]                 */
    double x0, v0;            /* initial position/velocity */
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;    /* step [s], > 0     */
    double stop;  /* end time [s], > 0 */
} MsdSamplingConfig;
```

### Functions

| Function | Description |
|----------|-------------|
| `void msd_core_default_case(MsdCase*)` | Baseline case: m 1, c 2, k 5, F 0.5, ω 2, x₀ = v₀ = 0. |
| `void msd_core_default_sampling(MsdSamplingConfig*)` | dt = 0.001 s, stop = 10 s. |
| `int32_t msd_core_derived(const MsdCase*, double* omega_n, double* zeta)` | $`\omega_n = \sqrt{k/m}`$, $`\zeta = c/(2\sqrt{mk})`$. Returns 0 if `m <= 0` or `k < 0`; ζ reported as 0 when `m·k == 0`. Either output pointer may be `NULL`. |
| `MsdSimulation* msd_core_simulate(const MsdCase*, const MsdSamplingConfig*)` | Fixed-step RK4. `NULL` on invalid input or when fewer than 2 samples result. |
| `void msd_core_free_simulation(MsdSimulation*)` | Releases the handle. |
| `int32_t msd_core_sim_length(const MsdSimulation*)` | $`N = \lfloor (\mathrm{stop}+dt)/dt - 10^{-12}\rfloor + 1`$. |
| `int32_t msd_core_sim_copy_time(…)` | Time axis. |
| `int32_t msd_core_sim_copy_position(…)` | x(t). |
| `int32_t msd_core_sim_copy_velocity(…)` | v(t). |
| `int32_t msd_core_sim_copy_force(…)` | f(t) = F sin(ωt), sampled on the same grid. |
| `double msd_core_sim_final_position(…)` / `…_final_velocity(…)` | Last sample. |
| `double msd_core_sim_max_abs_position(…)` / `…_max_abs_velocity(…)` | Peak magnitude. |
| `const char* msd_core_version(void)` | Static version string. |

---

## 3. `track_core`

Header: `examples/pi_path_tracking/core/include/track_core.h`.
The only core that exposes two handle types: the reference path can be built and
drawn without running a simulation.

### Config

```c
typedef struct TrackConfig {
    /* plant */
    double m, izz, cornering_power;
    /* integrator / sampling */
    double h, tc, total_time;
    /* speed control */
    double target_speed;
    /* tracking gains */
    double ky_p, ky_i, kpsi_p, kpsi_i, kr_damping;
    /* limits */
    double n_moment_limit, fx_limit, error_integral_limit;
    /* look-ahead */
    int32_t lookahead_index;
    /* initial conditions */
    double initial_y_offset, initial_heading_deg;
    /* reference-path geometry */
    double straight1_len, radius, straight2_len, ds;
} TrackConfig;
```

`track_core_default_config()` fills the tuned values from the reference script
(see [algorithms.md §3.6](algorithms.md#36-controllers)). All of `m`, `izz`,
`h`, `tc`, `total_time`, `radius`, `ds` and the three limits must be strictly
positive; `lookahead_index` must be ≥ 0.

### Reference path

| Function | Description |
|----------|-------------|
| `TrackReferencePath* track_core_make_reference(const TrackConfig*)` | Builds straight → 90° arc → straight, resampled at `ds`. `NULL` on invalid config. |
| `void track_core_free_reference(TrackReferencePath*)` | Releases the handle. |
| `int32_t track_core_ref_length(const TrackReferencePath*)` | Point count (458 with the defaults). |
| `int32_t track_core_ref_copy_x / _y / _psi(…)` | Path coordinates and tangent heading. |

### Simulation

| Function | Description |
|----------|-------------|
| `TrackSimulation* track_core_simulate(const TrackConfig*)` | Full closed-loop run; builds its own reference internally. `NULL` on invalid config. |
| `void track_core_free_simulation(TrackSimulation*)` | Releases the handle. |
| `int32_t track_core_sim_length(const TrackSimulation*)` | $`N = \lfloor T/h \rfloor`$ (9000 with the defaults). |

Per-sample channels, all `int32_t <name>(const TrackSimulation*, double*, int32_t)`:

| Accessor | Signal |
|----------|--------|
| `track_core_sim_copy_time` | t [s] |
| `track_core_sim_copy_x` / `_y` / `_psi` | global pose |
| `track_core_sim_copy_u` / `_v` / `_r` | body velocities and yaw rate |
| `track_core_sim_copy_beta` | side-slip angle β |
| `track_core_sim_copy_ey` / `_epsi` | lateral and heading error (at the look-ahead point) |
| `track_core_sim_copy_nmoment` / `_fx` | commanded yaw moment and longitudinal force |
| `track_core_sim_copy_x_ref` / `_y_ref` / `_psi_ref` | the **nearest** reference point at each step |

Aggregate metrics, all `double <name>(const TrackSimulation*)`:
`track_core_sim_path_error_rms`, `_path_error_max`, `_ey_rms`, `_ey_max_abs`,
`_epsi_rms`, `_epsi_max_abs`, `_nmoment_max_abs`.

`const char* track_core_version(void)` returns the static version string.

---

## 4. `tdof_core`

Header: `examples/two_dof/core/include/tdof_core.h`.
Requires Eigen at build time; the dependency does not leak into the ABI.

### Config

```c
typedef struct TdofConfig {
    double m, c, k;      /* plant P(s) = 1 / (m s^2 + c s + k) */
    double kp, ki, kd;   /* PID gains                          */
    double ref;          /* step amplitude used for display    */
    double t_end, dt;    /* time grid: [0, t_end) step dt      */
} TdofConfig;
```

`tdof_core_default_config()` fills m = 0.01, c = 0.015, k = 1.0, kp = 2,
ki = 10, kd = 0.1, ref = 10, t_end = 2 s, dt = 0.01 s.
`tdof_core_simulate()` requires `m > 0`, finite gains, `t_end > 0`, `dt > 0` and
`dt < t_end`.

### Transfer-function inspection

```c
int32_t tdof_core_get_tf(const TdofConfig* cfg, int32_t which,
                         double* num, int32_t* num_len,
                         double* den, int32_t* den_len);
```

`which`: `0` = plant $P$, `1` = PID $`K_1`$, `2` = pre-filter $`K_2`$,
`3` = closed loop $`G_{yz}`$. Coefficients come back highest-power-first.

Two-call protocol: on entry `*num_len` / `*den_len` hold the buffer capacities;
on return they hold the actual counts. Pass `NULL` buffers to query sizes only
(returns 1). Returns 0 for a bad `which`, `NULL` config/length pointers, or
buffers that are too small.

### Simulation

| Function | Description |
|----------|-------------|
| `TdofSimulation* tdof_core_simulate(const TdofConfig*)` | Runs both comparisons. `NULL` on invalid config or an internal Eigen failure. |
| `void tdof_core_free_simulation(TdofSimulation*)` | Releases the handle. |
| `int32_t tdof_core_sim_length(const TdofSimulation*)` | $`N = \lceil t_{\rm end}/dt - 10^{-12}\rceil`$ (200 with the defaults). |
| `int32_t tdof_core_sim_copy_time(…)` | Time axis — **not** scaled by `ref`. |
| `int32_t tdof_core_sim_copy_r(…)` | Reference step, scaled by `ref`. |
| `int32_t tdof_core_sim_copy_z(…)` | Pre-filtered reference $z = K_2 r$, scaled. |
| `int32_t tdof_core_sim_copy_y_pid(…)` | 1-DOF PID output $`G_{yz} r`$, scaled. |
| `int32_t tdof_core_sim_copy_y_2dof(…)` | 2-DOF output $`G_{yz} z`$, scaled. |
| `const char* tdof_core_version(void)` | Static version string. |

Signals are stored normalised to a unit step and multiplied by `ref` inside the
copy functions, so changing only the amplitude needs no re-simulation on the
core side.

---

## 5. Binding layers in this repository

| Language | Location | Mechanism |
|----------|----------|-----------|
| C++ (Qt6) | `examples/<name>/frontend_qt/` | Direct link against the CMake target; the header is included as-is |
| C# (Avalonia) | `examples/<name>/frontend_avalonia/<Proj>/Native/` | `[DllImport]` + `[StructLayout(LayoutKind.Sequential)]`, wrapped by a `*Solver` class that copies into `double[]` and frees in a `finally` |
| Python (per example) | `examples/<name>/frontend_python/<slug>_core.py` | `ctypes` with explicit `restype`/`argtypes`, returning NumPy arrays via `np.frombuffer(...).copy()` |
| Python (shared gallery) | `gui/python/bindings/` | Same modules, with `gui/python/libloader.py` resolving the library path first |

See [architecture.md](architecture.md) for how these fit together.
