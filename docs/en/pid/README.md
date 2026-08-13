# pid — PID attitude control

[Docs index](../README.md) · [algorithms overview](../algorithms.md) · [日本語](../../ja/pid/README.md)

PID control of a 1-DOF attitude loop whose plant is a pure accumulator — the smallest closed loop that still shows P, I and D behaviour, with optional anti-windup and actuator clamps.

![PID interactive demo](screenshot.png)

## Documents

| Document | Contents |
|----------|----------|
| [theory.md](theory.md) | Control law, exact execution order, the role of `dt`, validity rules, defaults and stability |
| [api.md](api.md) | `pid_core` C ABI: `PidConfig`, simulate/copy/free, convenience metrics |
| [avalonia-notes.md](avalonia-notes.md) | Avalonia 11 pitfalls hit while building the C# frontend |

## At a glance

| | |
|---|---|
| Source | `examples/pid/core/` |
| Shared library | `pid_core` (`pid_core.dll` / `libpid_core.so` / `libpid_core.dylib`) |
| Qt6 executable | `pid_qt` |
| Avalonia project | `PidAvalonia` |
| Python env var | `PID_CORE_LIB` |

## Run it

```powershell
# from the repository root (1 = Qt6, 2 = Avalonia, 3 = Python)
.uild_and_run.ps1 pid 1
.uild_and_run.ps1 pid       # all three frontends
```

```sh
# shared gallery, any platform
cd gui/python && python gallery_app.py --example pid
```

Full build documentation: [../build-and-run.md](../build-and-run.md).
