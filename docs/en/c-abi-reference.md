# C ABI — overview

[Docs index](README.md) · [日本語](../ja/c-abi-reference.md)

Rules shared by all four cores and how each language binds to them.
**The per-symbol reference for each module lives in its own folder:**

| Library | Module | Reference |
|---------|--------|-----------|
| `pid_core` | `pid` | [pid/api.md](pid/api.md) |
| `msd_core` | `mass_spring_damper` | [mass_spring_damper/api.md](mass_spring_damper/api.md) |
| `track_core` | `pi_path_tracking` | [pi_path_tracking/api.md](pi_path_tracking/api.md) |
| `tdof_core` | `two_dof` | [two_dof/api.md](two_dof/api.md) |

---

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

## Binding layers in this repository

| Language | Location | Mechanism |
|----------|----------|-----------|
| C++ (Qt6) | `examples/<name>/frontend_qt/` | Direct link against the CMake target; the header is included as-is |
| C# (Avalonia) | `examples/<name>/frontend_avalonia/<Proj>/Native/` | `[DllImport]` + `[StructLayout(LayoutKind.Sequential)]`, wrapped by a `*Solver` class that copies into `double[]` and frees in a `finally` |
| Python (per example) | `examples/<name>/frontend_python/<slug>_core.py` | `ctypes` with explicit `restype`/`argtypes`, returning NumPy arrays via `np.frombuffer(...).copy()` |
| Python (shared gallery) | `gui/python/bindings/` | Same modules, with `gui/python/libloader.py` resolving the library path first |

See [architecture.md](architecture.md) for how these fit together.
