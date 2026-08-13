# two_dof — 2-DOF control vs PID

[Docs index](../README.md) · [algorithms overview](../algorithms.md) · [日本語](../../ja/two_dof/README.md)

How much of a PID step response's overshoot comes from the reference path rather than the feedback loop? The same closed loop is run twice — once with a raw step, once through a reference pre-filter.

## Documents

| Document | Contents |
|----------|----------|
| [theory.md](theory.md) | Plant/PID/pre-filter transfer functions, polynomial algebra, controllable canonical form, first-order-hold discretisation via the augmented matrix exponential, and how to read the result |
| [api.md](api.md) | `tdof_core` C ABI: `TdofConfig`, the two-call transfer-function query, simulate/copy/free |

## At a glance

| | |
|---|---|
| Source | `examples/two_dof/core/` |
| Shared library | `tdof_core` (`tdof_core.dll` / `libtdof_core.so` / `libtdof_core.dylib`) |
| Qt6 executable | `tdof_qt` |
| Avalonia project | `TdofAvalonia` |
| Python env var | `TDOF_CORE_LIB` |

## Run it

```powershell
# from the repository root (1 = Qt6, 2 = Avalonia, 3 = Python)
.uild_and_run.ps1 tdof 1
.uild_and_run.ps1 tdof       # all three frontends
```

```sh
# shared gallery, any platform
cd gui/python && python gallery_app.py --example two_dof
```

Full build documentation: [../build-and-run.md](../build-and-run.md).
