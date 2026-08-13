# Build Guide

[Module overview](README.md) · [theory](theory.md) · [C ABI](api.md) · [日本語](../../ja/mass_spring_damper/build.md)

> Paths on this page are relative to `examples/mass_spring_damper/`.
> For building the whole repository at once see [../build-and-run.md](../build-and-run.md).

## 1. C++ core library

The core has **no external dependencies** — only a C++17 compiler and CMake.

### Requirements

| Tool | Minimum version |
|------|----------------|
| CMake | 3.16 |
| MSVC | Visual Studio 2019 (v142) |
| GCC | 10 |
| Clang | 12 |

### Windows — Visual Studio / MSVC

```powershell
cmake -S core -B core\build -G "Visual Studio 17 2022" -A x64
cmake --build core\build --config Release
```

The DLL is placed at `core\build\Release\msd_core.dll`.

### Linux / macOS — Makefile or Ninja

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build -j
```

The shared library is placed at `core/build/libmsd_core.so` (Linux) or
`core/build/libmsd_core.dylib` (macOS).

### Smoke test

```bash
cmake --build core/build --target msd_core_smoke    # Windows: add --config Release
./core/build/msd_core_smoke                          # Linux/macOS
# core\build\Release\msd_core_smoke.exe              # Windows
```

Expected output:

```
=== msd_core 1.0.0 ===
Sampling: dt=0.0010, stop=10.00
--- baseline           n=10001  x(end)= -0.0211549  ...  ALL OK.
```

---

## 2. Qt6 (C++) frontend

### Requirements

| Tool | Notes |
|------|-------|
| Qt 6.2+ | Core, Gui, Widgets modules |
| Same compiler as the core | MSVC 2019+, GCC 10+, Clang 12+ |
| CMake 3.16+ | |

### Windows — Visual Studio / MSVC

```powershell
cmake -S frontend_qt -B frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build frontend_qt\build --config Release
# Executable: frontend_qt\build\Release\msd_qt.exe
```

`windeployqt` is invoked automatically via a CMake post-build command, so all
required Qt DLLs are deployed next to `msd_qt.exe`. If using vcpkg-built Qt,
copy the third-party DLLs from `<vcpkg_root>/installed/x64-windows/bin/`
manually.

### Linux / macOS

```bash
cmake -S frontend_qt -B frontend_qt/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.0/gcc_64   # adjust to your install
cmake --build frontend_qt/build -j
./frontend_qt/build/msd_qt
```

### Using `build_and_run.ps1` (Windows)

```powershell
.\build_and_run.ps1 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
```

---

## 3. Avalonia (C#) frontend

### Requirements

- .NET SDK 8.0 or later (`dotnet --version` to check)
- Internet access for the first NuGet package restore
- The C++ core must be built first (the `.csproj` copies the DLL automatically)

### Build and run

```bash
# Build the C++ core first (see Section 1 above)

cd frontend_avalonia/MsdAvalonia
dotnet run -c Release
```

The `.csproj` contains conditional `<None>` items that copy `msd_core.dll` /
`libmsd_core.so` / `libmsd_core.dylib` from the core build directory into
`bin/<config>/net8.0/` at build time.

### Standalone publish (Windows)

```powershell
cd frontend_avalonia\MsdAvalonia
dotnet publish -c Release -r win-x64 --self-contained
```

Output: `bin\Release\net8.0\win-x64\publish\MsdAvalonia.exe`

---

## 4. Python frontends

### Requirements

| Package | Version |
|---------|---------|
| Python | 3.9+ |
| numpy | ≥ 1.22 |
| matplotlib | ≥ 3.5 |
| PySide6 | ≥ 6.4 (only for `app_pyside6.py`) |

### Install dependencies

```bash
pip install -r frontend_python/requirements.txt
```

### Run

```bash
# The Python loader searches ../core/build/ automatically.
# Override with an environment variable if needed:
#   export MSD_CORE_LIB=/path/to/libmsd_core.so   (Linux/macOS)
#   set MSD_CORE_LIB=path\to\msd_core.dll          (Windows)

python frontend_python/app_matplotlib.py   # CLI: saves PNG to frontend_python/output/
python frontend_python/app_pyside6.py      # GUI
```

---

## 5. One-shot convenience scripts

### Windows PowerShell (`build_and_run.ps1`)

```powershell
.\build_and_run.ps1 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"  # Qt6 frontend
.\build_and_run.ps1 2              # Build core + Avalonia, launch Avalonia
.\build_and_run.ps1 3              # Build core + Python deps, launch PySide6
.\build_and_run.ps1 0              # Build and launch all frontends
.\build_and_run.ps1 2 -SkipBuild   # Launch Avalonia without rebuilding
.\build_and_run.ps1 2 -BuildType Debug
```

### Windows batch (`build_all.bat`)

```bat
build_all.bat
```

Builds the C++ core, runs the smoke test, and attempts to build the Qt6
frontend (skipped gracefully if Qt6 is not found).

### Linux / macOS shell (`build_all.sh`)

```bash
./build_all.sh
```

Builds the C++ core and prints instructions for the Python and Avalonia
frontends.

> These are the per-example scripts in `examples/mass_spring_damper/`. The
> repository-root `build_and_run.ps1` takes the example name as its first
> argument instead (`.uild_and_run.ps1 msd 1`).

---

## 6. Environment variables

| Variable | Description |
|----------|-------------|
| `MSD_CORE_LIB` | Override path to the shared library (Python loader) |

---

## 7. Common issues

### `msd_core.dll` not found (Avalonia)

The `.csproj` copies the DLL at build time using `Condition="Exists(...)"`.
If the copy did not happen:

```powershell
# Copy manually:
copy core\build\Release\msd_core.dll frontend_avalonia\MsdAvalonia\bin\Release\net8.0\
```

Or use `build_and_run.ps1` which handles this automatically.

### `msd_core.dll` not found (Python)

Set `MSD_CORE_LIB` to the full DLL path:

```powershell
$env:MSD_CORE_LIB = "$(Resolve-Path core\build\Release\msd_core.dll)"
python frontend_python\app_pyside6.py
```

### `dotnet` command not found

Install the [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0)
and ensure `dotnet` is on `PATH`.

### CMake can't find the compiler (Windows)

Run from a **Visual Studio Developer Command Prompt**, or install
[Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
with the "C++ build tools" workload selected.
