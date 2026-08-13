# mass_spring_damper — forced response

[Docs index](../README.md) · [algorithms overview](../algorithms.md) · [日本語](../../ja/mass_spring_damper/README.md)

A 1-DOF mass-spring-dashpot driven by a sinusoidal force, integrated with fixed-step RK4 and swept over five parameter cases that span the underdamped, overdamped and near-resonant regimes.

## Documents

| Document | Contents |
|----------|----------|
| [theory.md](theory.md) | Equation of motion, state-space form, derived quantities, RK4 derivation, the case sweep and references |
| [api.md](api.md) | `msd_core` C ABI: `MsdCase`, `MsdSamplingConfig`, simulate/copy/free |
| [architecture.md](architecture.md) | Layer diagram, data flow through all three frontends, design decisions |
| [build.md](build.md) | Detailed per-frontend build and environment setup |
| [avalonia-notes.md](avalonia-notes.md) | Avalonia 11 implementation tips |
| [avalonia-debug-polylines.md](avalonia-debug-polylines.md) | Systematic troubleshooting when the plot polylines do not render |

## At a glance

| | |
|---|---|
| Source | `examples/mass_spring_damper/core/` |
| Shared library | `msd_core` (`msd_core.dll` / `libmsd_core.so` / `libmsd_core.dylib`) |
| Qt6 executable | `msd_qt` |
| Avalonia project | `MsdAvalonia` |
| Python env var | `MSD_CORE_LIB` |

## Run it

```powershell
# from the repository root (1 = Qt6, 2 = Avalonia, 3 = Python)
.uild_and_run.ps1 msd 1
.uild_and_run.ps1 msd       # all three frontends
```

```sh
# shared gallery, any platform
cd gui/python && python gallery_app.py --example mass_spring_damper
```

Full build documentation: [../build-and-run.md](../build-and-run.md).
