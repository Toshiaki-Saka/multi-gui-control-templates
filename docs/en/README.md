# multi-gui-control-templates — Documentation (English)

日本語版は [../ja/README.md](../ja/README.md) にあります。

This repository is a template collection for one recurring pattern:

> **one control core exposed through a C ABI, driven by three independent GUIs
> (Qt6 C++ / Avalonia C# / Python PySide6) plus a shared Python gallery.**

Four control problems are implemented against that pattern, each as a
self-contained shared library under `examples/<name>/core`.

| slug | example directory | subject | core library |
|------|-------------------|---------|--------------|
| `pid` | `examples/pid` | PID control of a 1-DOF attitude loop | `pid_core` |
| `track` | `examples/pi_path_tracking` | PI path following of a 3-DOF planar vehicle | `track_core` |
| `tdof` | `examples/two_dof` | 2-DOF control vs. plain PID step response | `tdof_core` |
| `msd` | `examples/mass_spring_damper` | Mass-spring-damper forced response | `msd_core` |

## Pages

| Page | What it covers |
|------|----------------|
| [build-and-run.md](build-and-run.md) | Complete reference for `build_and_run.ps1`, plus manual build/run commands for every core, every frontend, the umbrella CMake build, CTest and the shared gallery |
| [architecture.md](architecture.md) | Repository layout, the C ABI contract, how each frontend binds to it, how the shared library is located at run time, and how to add a fifth example |
| [algorithms.md](algorithms.md) | Detailed algorithm documentation: plant models, controllers, discretisation, integrators, error metrics and the reasoning behind each numerical choice |
| [c-abi-reference.md](c-abi-reference.md) | Every exported struct and function of the four cores, with ownership and buffer-size rules |

### Per-example implementation notes

All documentation lives under `docs/` — nothing is kept beside the examples.
These pages are English-only and cover one example and one frontend each.

| Document | Contents |
|----------|----------|
| [examples/mass_spring_damper/architecture.md](../examples/mass_spring_damper/architecture.md) | Layer diagram, data flow, design decisions |
| [examples/mass_spring_damper/build.md](../examples/mass_spring_damper/build.md) | Detailed build and environment setup |
| [examples/mass_spring_damper/avalonia-notes.md](../examples/mass_spring_damper/avalonia-notes.md) | Avalonia 11 implementation tips |
| [examples/mass_spring_damper/avalonia-debug-polylines.md](../examples/mass_spring_damper/avalonia-debug-polylines.md) | Debugging log for the Avalonia polyline renderer |
| [examples/pi_path_tracking/api.md](../examples/pi_path_tracking/api.md) | Complete `track_core` C ABI reference with a usage example |
| [examples/pi_path_tracking/python.md](../examples/pi_path_tracking/python.md) | Python bindings guide for `track_core` |
| [examples/pid/avalonia-notes.md](../examples/pid/avalonia-notes.md) | Avalonia 11 gotchas: DLL search path, P/Invoke, rendering |

## Quick start (Windows)

```powershell
# everything: 4 examples x 3 frontends
.\build_and_run.ps1

# one example, one frontend (1 = Qt6, 2 = Avalonia, 3 = Python)
.\build_and_run.ps1 pid 1
```

## Quick start (any platform, cores + tests + gallery)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure

cd gui/python
pip install -r requirements.txt
python gallery_app.py
```

## License

Apache License 2.0 — see [../../LICENSE](../../LICENSE).
