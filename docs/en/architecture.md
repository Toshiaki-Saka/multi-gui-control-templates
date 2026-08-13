# Architecture — one core, three GUIs

日本語版: [../ja/architecture.md](../ja/architecture.md)

## 1. The pattern

Every example in this repository is built the same way:

```
                      ┌───────────────────────────────┐
                      │  <slug>_core  (C++17, shared) │
                      │  simulation + metrics         │
                      └──────────────┬────────────────┘
                                     │  flat C ABI (extern "C")
             ┌───────────────────────┼───────────────────────┐
             │                       │                       │
     direct link (C++)        P/Invoke (C#)             ctypes (Python)
             │                       │                       │
      ┌──────┴──────┐        ┌───────┴───────┐       ┌───────┴────────┐
      │  Qt6 widget │        │ Avalonia MVVM │       │ PySide6 GUI    │
      │  <slug>_qt  │        │ <Title>Avalonia│      │ app_pyside6.py │
      └─────────────┘        └───────────────┘       └───────┬────────┘
                                                             │
                                                     ┌───────┴────────┐
                                                     │ shared gallery │
                                                     │ gallery_app.py │
                                                     └────────────────┘
```

The core owns **all** numerics. No frontend re-implements a controller, an
integrator or a metric — they differ only in how they draw and how they collect
parameters. That is what makes the four examples comparable: swapping the core
swaps the physics, and the three GUI bindings stay structurally identical.

## 2. Repository layout

```
multi-gui-control-templates/
├── CMakeLists.txt              # umbrella build: 4 cores -> build/lib, + CTest
├── build_and_run.ps1           # Windows one-shot: build + launch any frontend
├── docs/                       # this documentation (en / ja)
├── gui/python/                 # shared launcher, works across all examples
│   ├── libloader.py            #   resolves each <NAME>_CORE_LIB before import
│   ├── bindings/               #   one ctypes module per core
│   ├── adapters.py             #   native output -> uniform RunResult
│   └── gallery_app.py          #   matplotlib gallery
├── tests/test_examples.py      # cross-example smoke test (CTest target)
└── examples/                   # the interchangeable part; each keeps its history
    └── <name>/
        ├── core/               #   C++17 shared library + C ABI header
        │   ├── include/<slug>_core.h
        │   ├── src/
        │   └── tools/smoke_test.cpp
        ├── frontend_qt/        #   Qt6 Widgets app (links the core directly)
        ├── frontend_avalonia/  #   .NET 8 + Avalonia 11 app (P/Invoke)
        ├── frontend_python/    #   PySide6 GUI + matplotlib batch script
        ├── docs/               #   per-example theory/API notes
        └── build_and_run.ps1   #   per-example driver
```

## 3. The C ABI contract

Each core follows the same five-part shape. `<slug>` is `pid`, `track`, `tdof`
or `msd`.

| Part | Signature shape | Purpose |
|------|-----------------|---------|
| Config struct | `typedef struct <Name>Config { double …; int32_t …; }` | Plain-old-data, no pointers, no C++ types |
| Defaults | `void <slug>_core_default_config(<Name>Config*)` | Every frontend starts from the same numbers |
| Run | `<Name>Simulation* <slug>_core_simulate(const <Name>Config*)` | Returns an opaque handle, `NULL` on invalid input |
| Read back | `int32_t <slug>_core_sim_length(const <Name>Simulation*)` and `int32_t <slug>_core_sim_copy_<field>(const <Name>Simulation*, double* buf, int32_t cap)` | Caller allocates; a copy returns 0 if `cap < length` |
| Free | `void <slug>_core_free_simulation(<Name>Simulation*)` | Ownership returns to the core |

Design rules that make the ABI bindable from three languages:

- **Only `double`, `int32_t`, opaque pointers and `const char*` cross the
  boundary.** No `std::vector`, no `bool`, no structs by value in return
  position. C# `[StructLayout(LayoutKind.Sequential)]` and Python
  `ctypes.Structure` then map field-for-field.
- **The caller owns the output buffers.** `copy_*` never allocates; it fills a
  buffer the caller sized with `sim_length()`. This avoids cross-allocator
  frees, which is the classic way a P/Invoke or ctypes binding crashes on
  Windows.
- **Failure is `NULL` or `0`, never an exception.** `two_dof` is the only core
  that can throw internally (Eigen); it catches everything at the ABI boundary
  and converts it to `NULL`.
- **Symbol visibility is explicit.** `__declspec(dllexport)` under
  `<SLUG>_CORE_BUILD` on Windows, `__attribute__((visibility("default")))`
  elsewhere, with `CMAKE_CXX_VISIBILITY_PRESET hidden` so nothing else leaks.
- **`<slug>_core_version()`** returns a static string, useful as a smoke check
  that the frontend loaded the library it thinks it did.

Full signatures: [c-abi-reference.md](c-abi-reference.md).

## 4. How each frontend binds

### Qt6 (C++)

`frontend_qt/CMakeLists.txt` calls `add_subdirectory(../core)` when the core
target does not exist yet, then `target_link_libraries(<slug>_qt PRIVATE
<slug>_core Qt6::Core Qt6::Gui Qt6::Widgets)`. The header is consumed directly,
so there is no marshalling layer at all. Plotting is hand-written
`QPainter` code in `Widgets.cpp` — no charting dependency.

On Windows, core and exe are emitted into the same directory so the loader finds
the DLL; on Linux the target sets `BUILD_RPATH "$ORIGIN;$ORIGIN/../core"`.

### Avalonia (C#)

`Native/<Name>CoreNative.cs` declares the raw `[DllImport("<slug>_core")]`
entry points and the `[StructLayout(LayoutKind.Sequential)]` config struct;
`Native/<Name>Solver.cs` wraps them in an idiomatic, `IDisposable`-free API that
copies into `double[]` and frees the handle in a `finally`. The view model binds
to those arrays and the view renders them with Avalonia primitives
(`Polyline`/`Path`) — again no charting package.

The `.csproj` copies `..\..\core\build\<slug>_core.dll` (plus the `.so`/`.dylib`
variants) into the output directory with `CopyToOutputDirectory=PreserveNewest`,
guarded by an `Exists(...)` condition. `build_and_run.ps1` copies the DLL to
that exact flat path for this reason, and copies it again into `bin\...` as a
fallback.

### Python (PySide6 + matplotlib)

`frontend_python/<slug>_core.py` is a self-contained ctypes binding: it declares
`restype`/`argtypes` for every entry point, defines dataclasses mirroring the C
structs, and returns NumPy arrays built with
`np.frombuffer(buf, dtype=np.float64).copy()`. The `.copy()` matters — the
ctypes buffer is stack-scoped, and the un-copied view would dangle.

Library lookup order in the binding:

1. `$<SLUG>_CORE_LIB`
2. `../core/build/`, `../core/build/Release/`, `../core/build/Debug/`
3. the binding's own directory and its parents
4. `ctypes.util.find_library("<slug>_core")`

### Shared gallery

`gui/python/` generalises the above one level further:

- `libloader.py` runs at import time, searches both the per-example build trees
  and the umbrella `build/lib` output, picks the **newest** match by mtime, and
  exports the four `<NAME>_CORE_LIB` variables.
- `bindings/` holds a copy of each per-example ctypes module, so the gallery
  does not depend on `examples/*/frontend_python` being on `sys.path`.
- `adapters.py` normalises every core's output into one structure:

  ```python
  RunResult(name, description, plots=[Plot(title, xlabel, ylabel,
                                           traces=[Trace(label, x, y, style)],
                                           equal_aspect=False)],
            metrics={...})
  ```

  This is the whole point of the template: `gallery_app.py` never mentions PID,
  yaw rate or spring constants — it draws `RunResult`s.

## 5. Numerical parity as a contract

Each core is a port of a Python reference script, and each keeps a smoke test
that pins the ported numbers:

| Example | Reference script | Agreement |
|---------|------------------|-----------|
| `pid` | `pid_advanced_simulation.py` | exact recurrence, reproduced step for step |
| `pi_path_tracking` | `planar_path_tracking_pi_tuned.py` | trajectory within 5 × 10⁻¹⁰ m; all six printed metrics match to 6 decimals |
| `two_dof` | `two_degree_of_freedom…py` (python-control) | ≈ 5 × 10⁻⁹ on the step responses |
| `mass_spring_damper` | `mass_spring_damper_forced_response.py` (`scipy.odeint`) | ≤ 2 × 10⁻⁷ at `dt` = 10⁻³ s |

Because of that, the ports keep some structure that looks redundant in C++ —
most visibly `pi_path_tracking`'s per-state RK4, which provably collapses to
forward Euler. See [algorithms.md](algorithms.md) §3.7 for why it is kept.

## 6. Adding a fifth example

1. Create `examples/<name>/core` with `include/<slug>_core.h`, `src/`, and a
   `CMakeLists.txt` modelled on an existing one (shared library, hidden
   visibility, `<SLUG>_CORE_BUILD`, smoke test guarded by
   `if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)`).
2. Follow the ABI shape in §3 — `default_config`, `simulate`, `sim_length`,
   `sim_copy_*`, `free_simulation`, `version`.
3. Add `add_subdirectory(examples/<name>/core)` to the root `CMakeLists.txt`.
4. Add the slug to the `$Examples` table at the top of `build_and_run.ps1`
   (`Dir`, `Avalonia`, `EnvVar`) — nothing else in the script is example-aware.
5. Add the entry to `gui/python/libloader.py::_EXAMPLES`, drop the ctypes module
   into `gui/python/bindings/`, and write a `run_<name>()` adapter returning a
   `RunResult` in `gui/python/adapters.py`.
6. Frontends: copy an existing `frontend_qt` / `frontend_avalonia` /
   `frontend_python` and re-point the names. The plotting code is the part worth
   copying; the binding layer is mechanical.

Steps 4 and 5 are what make it appear in `build_and_run.ps1 all`, in the
gallery dropdown and in `ctest`.
