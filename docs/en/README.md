# multi-gui-control-templates — Documentation

**English** · [日本語](../ja/README.md)

This repository is a template collection for one recurring pattern:

> **one control core exposed through a C ABI, driven by three independent GUIs
> (Qt6 C++ / Avalonia C# / Python PySide6) plus a shared Python gallery.**

Four control problems are implemented against that pattern, each as a
self-contained shared library under `examples/<name>/core`.

## Modules

Each module has its own folder with a full write-up — theory, C ABI and
frontend implementation notes.

| Module | Subject | Documentation |
|--------|---------|---------------|
| **pid** | PID control of a 1-DOF attitude loop | [pid/](pid/README.md) |
| **mass_spring_damper** | Mass-spring-damper forced response | [mass_spring_damper/](mass_spring_damper/README.md) |
| **pi_path_tracking** | PI path following of a 3-DOF planar vehicle | [pi_path_tracking/](pi_path_tracking/README.md) |
| **two_dof** | 2-DOF control vs plain PID | [two_dof/](two_dof/README.md) |

## Cross-cutting pages

| Page | What it covers |
|------|----------------|
| [build-and-run.md](build-and-run.md) | Complete reference for `build_and_run.ps1`, plus manual build/run commands for every core, every frontend, the umbrella CMake build, CTest and the shared gallery |
| [architecture.md](architecture.md) | Repository layout, the C ABI contract, how each frontend binds to it, how the shared library is located at run time, and how to add a fifth example |
| [algorithms.md](algorithms.md) | Conventions shared by all four cores and a side-by-side comparison of their numerical methods (per-module derivations live in the module folders) |
| [c-abi-reference.md](c-abi-reference.md) | Rules shared by the four cores and the binding layers (per-module symbol reference lives in the module folders) |

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
