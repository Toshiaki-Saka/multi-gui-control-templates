# pi_path_tracking — PI path following

[Docs index](../README.md) · [algorithms overview](../algorithms.md) · [日本語](../../ja/pi_path_tracking/README.md)

A 3-DOF planar vehicle following a straight → 90° arc → straight reference path, using a look-ahead lateral/heading PI controller with yaw-rate damping and a separate longitudinal speed PI.

## Documents

| Document | Contents |
|----------|----------|
| [theory.md](theory.md) | Vehicle dynamics, reference-path geometry, error definitions, both controllers, and why the per-state RK4 collapses to forward Euler |
| [api.md](api.md) | `track_core` C ABI: lifecycle, `TrackConfig`, reference path, 15 per-sample channels, aggregate metrics, C usage example |
| [python.md](python.md) | Python bindings: `TrackConfig`/`ReferencePath`/`Simulation`, a parameter sweep, the two GUI scripts |

## At a glance

| | |
|---|---|
| Source | `examples/pi_path_tracking/core/` |
| Shared library | `track_core` (`track_core.dll` / `libtrack_core.so` / `libtrack_core.dylib`) |
| Qt6 executable | `track_qt` |
| Avalonia project | `TrackAvalonia` |
| Python env var | `TRACK_CORE_LIB` |

## Run it

```powershell
# from the repository root (1 = Qt6, 2 = Avalonia, 3 = Python)
.uild_and_run.ps1 track 1
.uild_and_run.ps1 track       # all three frontends
```

```sh
# shared gallery, any platform
cd gui/python && python gallery_app.py --example pi_path_tracking
```

Full build documentation: [../build-and-run.md](../build-and-run.md).
