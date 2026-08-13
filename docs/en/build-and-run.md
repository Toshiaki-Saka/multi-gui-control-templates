# Build & Run

日本語版: [../ja/build-and-run.md](../ja/build-and-run.md)

This page documents every way to build and launch the code in this repository:

1. [`build_and_run.ps1`](#1-build_and_runps1-windows-one-shot) — the Windows one-shot driver (all features, all frontends)
2. [Umbrella CMake build](#2-umbrella-cmake-build-all-four-cores) — all four cores + CTest, any platform
3. [Manual per-example commands](#3-manual-commands-per-example) — core, Qt6, Avalonia, Python
4. [The shared Python gallery](#4-shared-python-gallery)
5. [Prerequisites and troubleshooting](#5-prerequisites) 

---

## 1. `build_and_run.ps1` (Windows one-shot)

`build_and_run.ps1` in the repository root builds the C/C++ core, the Qt6
frontend, the C# Avalonia frontend and the Python dependencies for one or all
examples, then launches the selected frontends as separate processes.

### 1.1 Signature

```powershell
.\build_and_run.ps1 [-Example <name>] [-Target <0-3>] [-BuildType <cfg>]
                    [-Qt6Path <path>] [-SkipBuild] [-SkipPyDeps] [-List]
```

`-Example` is the first positional parameter and `-Target` the second, so
`.\build_and_run.ps1 pid 1` is the same as
`.\build_and_run.ps1 -Example pid -Target 1`.

### 1.2 Parameters

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `-Example <name>` | `all` | Which example to process: `msd`, `track`, `pid`, `tdof`, a full directory name (`mass_spring_damper`, …), or `all`. Case-insensitive. |
| `-Target <0-3>` | `0` | Which frontend: `1` = Qt6, `2` = Avalonia, `3` = Python, `0` = all three. |
| `-BuildType <cfg>` | `Release` | Passed to `cmake --config` and `dotnet -c`. `Release`, `Debug`, `RelWithDebInfo`, `MinSizeRel`. |
| `-Qt6Path <path>` | auto-detect | Qt 6 install prefix (the directory containing `share\Qt6\Qt6Config.cmake` or `lib\cmake\Qt6\Qt6Config.cmake`). |
| `-SkipBuild` | off | Skip all building; launch whatever is already built. |
| `-SkipPyDeps` | off | Skip `pip install -r requirements.txt`. |
| `-List` | off | Print the example table and exit without building or launching. |

### 1.3 Naming convention

Every artifact name is derived from the slug, so the script needs no
per-example special-casing:

| slug | full name | core DLL | Qt6 exe | Avalonia project | Python env var |
|------|-----------|----------|---------|------------------|----------------|
| `msd` | `mass_spring_damper` | `msd_core.dll` | `msd_qt.exe` | `MsdAvalonia` | `MSD_CORE_LIB` |
| `track` | `pi_path_tracking` | `track_core.dll` | `track_qt.exe` | `TrackAvalonia` | `TRACK_CORE_LIB` |
| `pid` | `pid` | `pid_core.dll` | `pid_qt.exe` | `PidAvalonia` | `PID_CORE_LIB` |
| `tdof` | `two_dof` | `tdof_core.dll` | `tdof_qt.exe` | `TdofAvalonia` | `TDOF_CORE_LIB` |

### 1.4 What the script does, in order

For each selected example:

| Stage | Action |
|-------|--------|
| 0. Cache hygiene | Deletes a `CMakeCache.txt` whose `CMAKE_HOME_DIRECTORY` no longer matches the source dir, and removes the whole `frontend_qt\build` tree when the cached `Z_VCPKG_POWERSHELL_PATH` points at a `pwsh.exe` that no longer exists (a PowerShell update invalidates the path vcpkg baked into the generated `.vcxproj`). |
| 1. Core | `cmake -S core -B core\build` + `cmake --build --config <cfg> -j`, then resolves `<slug>_core.dll` from `core\build\<cfg>\` (MSVC multi-config) or `core\build\` (Ninja/MinGW) and copies it to `core\build\` so the `.csproj` `Exists(...)` conditions find it. |
| 2. Qt6 | Configures `frontend_qt` with `-DCMAKE_PREFIX_PATH=<Qt6Path>`, and on first configure also `-DQt6_DIR=...`; for a vcpkg tree it adds `-DCMAKE_TOOLCHAIN_FILE=...\vcpkg.cmake` and `-DVCPKG_TARGET_TRIPLET=...`. After building it copies the core DLL next to the exe, runs `windeployqt --no-translations --no-system-d3d-compiler`, and copies the vcpkg third-party DLLs `windeployqt` misses (`double-conversion`, `pcre2-16`, `z`, `zstd`, `harfbuzz`, `freetype`, `libpng16`, `bz2`, `md4c`, `brotlidec`, `brotlicommon`, `jpeg62`, `libcrypto-3-x64`). |
| 3. Avalonia | `dotnet restore` + `dotnet build -c <cfg> --no-restore`, then copies the core DLL into `bin\<cfg>\net8.0\` if the csproj item group did not. |
| 4. Python | `python -m pip install -r frontend_python\requirements.txt` (unless `-SkipPyDeps`). |
| 5. Launch | `Start-Process` for each selected frontend. Avalonia falls back to `dotnet run --no-build` if the exe is missing. Before launching Python it sets `<SLUG>_CORE_LIB` to the absolute DLL path. |

Qt6 handling degrades gracefully: if no Qt 6 prefix is found the script prints a
warning and skips **only** the Qt6 build; the core, Avalonia and Python paths
still run.

### 1.5 Command examples — all features

#### Basics

```powershell
.\build_and_run.ps1                      # 4 examples x 3 frontends
.\build_and_run.ps1 -List                # list examples and exit
.\build_and_run.ps1 pid                  # pid, all three frontends
.\build_and_run.ps1 mass_spring_damper   # full names work too
```

#### Every example × every frontend

```powershell
# pid — PID attitude control
.\build_and_run.ps1 pid 1                # Qt6      -> pid_qt.exe
.\build_and_run.ps1 pid 2                # Avalonia -> PidAvalonia.exe
.\build_and_run.ps1 pid 3                # Python   -> app_pyside6.py
.\build_and_run.ps1 pid                  # all three

# track — PI path tracking
.\build_and_run.ps1 track 1              # Qt6      -> track_qt.exe
.\build_and_run.ps1 track 2              # Avalonia -> TrackAvalonia.exe
.\build_and_run.ps1 track 3              # Python   -> app_pyside6.py
.\build_and_run.ps1 pi_path_tracking     # all three

# tdof — 2-DOF vs PID
.\build_and_run.ps1 tdof 1               # Qt6      -> tdof_qt.exe
.\build_and_run.ps1 tdof 2               # Avalonia -> TdofAvalonia.exe
.\build_and_run.ps1 tdof 3               # Python   -> app_pyside6.py
.\build_and_run.ps1 two_dof              # all three

# msd — mass-spring-damper
.\build_and_run.ps1 msd 1                # Qt6      -> msd_qt.exe
.\build_and_run.ps1 msd 2                # Avalonia -> MsdAvalonia.exe
.\build_and_run.ps1 msd 3                # Python   -> app_pyside6.py
.\build_and_run.ps1 mass_spring_damper   # all three

# one frontend across all four examples
.\build_and_run.ps1 all 1                # every Qt6 app
.\build_and_run.ps1 all 2                # every Avalonia app
.\build_and_run.ps1 all 3                # every PySide6 app
```

#### Build configuration

```powershell
.\build_and_run.ps1 tdof -BuildType Debug
.\build_and_run.ps1 all  -BuildType RelWithDebInfo
.\build_and_run.ps1 pid 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
.\build_and_run.ps1 pid 1 -Qt6Path "C:\vcpkg\installed\x64-windows"
```

#### Skipping work

```powershell
.\build_and_run.ps1 pid 1 -SkipBuild            # launch only
.\build_and_run.ps1 all -SkipBuild              # launch everything already built
.\build_and_run.ps1 msd 3 -SkipPyDeps           # no pip install
.\build_and_run.ps1 all 3 -SkipPyDeps
.\build_and_run.ps1 tdof -BuildType Debug -SkipPyDeps
```

#### Named parameters (any order)

```powershell
.\build_and_run.ps1 -Example track -Target 2
.\build_and_run.ps1 -Example all -Target 1 -BuildType Release `
                    -Qt6Path "C:\Qt\6.9.0\msvc2022_64"
.\build_and_run.ps1 -Example pid -Target 3 -SkipBuild -SkipPyDeps
```

### 1.6 Output locations

With `<ex> = examples\<full name>` and `<cfg> = -BuildType`:

| Artifact | Path |
|----------|------|
| Core DLL (build output) | `<ex>\core\build\<cfg>\<slug>_core.dll` |
| Core DLL (flat copy for csproj) | `<ex>\core\build\<slug>_core.dll` |
| Qt6 executable | `<ex>\frontend_qt\build\<cfg>\<slug>_qt.exe` |
| Avalonia executable | `<ex>\frontend_avalonia\<Proj>\bin\<cfg>\net8.0\<Proj>.exe` |
| Python GUI | `<ex>\frontend_python\app_pyside6.py` |
| Python batch script | `<ex>\frontend_python\app_matplotlib.py` |

### 1.7 Per-example scripts

Each example also carries its own `build_and_run.ps1` (same options minus
`-Example`/`-List`), inherited from the repository it was imported from:

```powershell
cd examples\pid
.\build_and_run.ps1 1                       # Qt6 only
.\build_and_run.ps1 -BuildType Debug
```

`examples/mass_spring_damper` additionally ships `build_all.bat` (Windows/MSVC)
and `build_all.sh` (POSIX), which build the core, run the C++ smoke test and
then build the Qt6 frontend if Qt 6 is discoverable.

---

## 2. Umbrella CMake build (all four cores)

The root `CMakeLists.txt` builds the four cores into one output directory,
`build/lib`, which is where the shared gallery and the cross-example test look
for them. It does **not** build the Qt6/Avalonia frontends.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

`two_dof` needs **Eigen 3.3+** (`Eigen/Dense` plus
`unsupported/Eigen/MatrixFunctions`); CMake looks for the `Eigen3::Eigen`
package first, then falls back to `/usr/include/eigen3`,
`/usr/local/include/eigen3` and `/opt/homebrew/include/eigen3`. On Windows,
`vcpkg install eigen3` and configure with the vcpkg toolchain file.

### Registered tests

| Test | Registered by | Notes |
|------|---------------|-------|
| `cross_example_python` | root `CMakeLists.txt` | Runs `tests/test_examples.py`, which drives all four cores through the shared adapters and asserts each returns a non-trivial series. Examples whose library is missing are skipped, not failed. On Windows the test's `PATH` is prefixed with `build/lib`. |
| `smoke` | `examples/pi_path_tracking/core` | `track_core_smoke`; registered unconditionally (`TRACK_BUILD_TESTS=ON` by default), so it also runs from the umbrella build. |

The `pid`, `two_dof` and `mass_spring_damper` cores guard their smoke tests with
`if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)`, so those tests are
registered only when that core is configured as the top-level project:

```sh
cmake -S examples/pid/core -B examples/pid/core/build -DCMAKE_BUILD_TYPE=Release
cmake --build examples/pid/core/build --config Release
ctest --test-dir examples/pid/core/build --build-config Release --output-on-failure
# -> pid_core_smoke
```

The same works with `examples/two_dof/core` (`tdof_core_smoke`) and
`examples/mass_spring_damper/core` (`msd_core_smoke`).

---

## 3. Manual commands per example

Substitute the slug/name pair from §1.3. The examples below use `pid`.

### 3.1 Core only

```powershell
cmake -S examples\pid\core -B examples\pid\core\build -G "Visual Studio 17 2022" -A x64
cmake --build examples\pid\core\build --config Release -j
# -> examples\pid\core\build\Release\pid_core.dll
```

```sh
# POSIX
cmake -S examples/pid/core -B examples/pid/core/build -DCMAKE_BUILD_TYPE=Release
cmake --build examples/pid/core/build -j
# -> examples/pid/core/build/libpid_core.so
```

### 3.2 Qt6 frontend

The `frontend_qt/CMakeLists.txt` pulls in `../core` via `add_subdirectory` when
the core target does not already exist, so building the frontend builds the core
as well.

```powershell
cmake -S examples\pid\frontend_qt -B examples\pid\frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build examples\pid\frontend_qt\build --config Release -j
.\examples\pid\frontend_qt\build\Release\pid_qt.exe
```

For a vcpkg Qt, point `Qt6_DIR` at the config directory on the **first**
configure (it is cached afterwards):

```powershell
cmake -S examples\pid\frontend_qt -B examples\pid\frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DQt6_DIR="C:\vcpkg\installed\x64-windows\share\Qt6" `
      -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
      -DVCPKG_TARGET_TRIPLET=x64-windows
```

`msd_qt` and `track_qt` run `windeployqt` themselves as a post-build step when
CMake can find it; `pid_qt` and `tdof_qt` rely on `build_and_run.ps1` (or a
manual `windeployqt`) for deployment.

```sh
# POSIX
cmake -S examples/pid/frontend_qt -B examples/pid/frontend_qt/build \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr
cmake --build examples/pid/frontend_qt/build -j
./examples/pid/frontend_qt/build/pid_qt
```

### 3.3 Avalonia (C#) frontend

Requires the .NET 8 SDK. The `.csproj` copies
`..\..\core\build\<slug>_core.dll` (or `.so`/`.dylib`) to the output directory
when it exists, so build the core first.

```powershell
dotnet restore examples\pid\frontend_avalonia\PidAvalonia
dotnet build   examples\pid\frontend_avalonia\PidAvalonia -c Release --no-restore
.\examples\pid\frontend_avalonia\PidAvalonia\bin\Release\net8.0\PidAvalonia.exe

# or, in one step
dotnet run --project examples\pid\frontend_avalonia\PidAvalonia -c Release
```

Project directories: `PidAvalonia`, `TrackAvalonia`, `TdofAvalonia`,
`MsdAvalonia`.

### 3.4 Python frontends

Each example carries two Python frontends and its own ctypes binding
(`<slug>_core.py`) next to them:

| Script | Kind |
|--------|------|
| `app_pyside6.py` | Interactive PySide6 GUI (this is what `build_and_run.ps1 <ex> 3` starts) |
| `app_matplotlib.py` | Batch/CLI reproduction of the original reference script; writes PNG/CSV under `frontend_python/output*/` |

```powershell
pip install -r examples\pid\frontend_python\requirements.txt   # numpy, matplotlib, PySide6

$env:PID_CORE_LIB = "$PWD\examples\pid\core\build\Release\pid_core.dll"
python examples\pid\frontend_python\app_pyside6.py
python examples\pid\frontend_python\app_matplotlib.py
```

```sh
# POSIX
export PID_CORE_LIB="$PWD/examples/pid/core/build/libpid_core.so"
python examples/pid/frontend_python/app_pyside6.py
```

The env var is optional: the binding also searches `../core/build/`,
`../core/build/Release/`, `../core/build/Debug/`, its own directory, and finally
`ctypes.util.find_library()`. Setting it is the reliable way to pin a specific
build configuration.

Environment variables per example: `PID_CORE_LIB`, `TRACK_CORE_LIB`,
`TDOF_CORE_LIB`, `MSD_CORE_LIB`.

---

## 4. Shared Python gallery

`gui/python/gallery_app.py` drives **all four** cores through one uniform
adapter layer (`gui/python/adapters.py`) and plots them with matplotlib. It
needs only `numpy` and `matplotlib` — no PySide6.

```sh
cd gui/python
pip install -r requirements.txt

python gallery_app.py                       # defaults to pid
python gallery_app.py --example pid
python gallery_app.py --example pi_path_tracking
python gallery_app.py --example two_dof
python gallery_app.py --example mass_spring_damper
python gallery_app.py --save gallery.png    # headless: render all four to one PNG
```

`gui/python/libloader.py` runs on import and resolves each core library, in this
order: an already-set `<NAME>_CORE_LIB`, then `examples/<name>/core/build[/Release|Debug|RelWithDebInfo]`,
then `build/` and `build/lib/` (the umbrella build output) with the same
configuration sub-directories. When several copies exist the **newest by mtime**
wins, so a fresh per-example build takes precedence over a stale umbrella build.

The cross-example test uses the same path:

```sh
python tests/test_examples.py
```

---

## 5. Prerequisites

| Tool | Needed for | Notes |
|------|-----------|-------|
| CMake ≥ 3.16 | cores, Qt6 frontends | |
| C++17 compiler | cores, Qt6 frontends | MSVC 2022 (`Visual Studio 17 2022`, `-A x64`) on Windows; GCC/Clang elsewhere |
| Eigen ≥ 3.3 | `two_dof` core only | Needs `unsupported/Eigen/MatrixFunctions` for the matrix exponential |
| Qt ≥ 6.2 (Core, Gui, Widgets) | Qt6 frontends | Standard Qt install or vcpkg `qtbase` |
| .NET 8 SDK | Avalonia frontends | Avalonia 11.0.10 |
| Python ≥ 3.9 | Python frontends, gallery, tests | `numpy`, `matplotlib`; PySide6 additionally for `app_pyside6.py` |

### Troubleshooting

**"Qt6 が見つかりません" / Qt6 build skipped.** Pass `-Qt6Path` with the prefix
that contains `share\Qt6\Qt6Config.cmake` or `lib\cmake\Qt6\Qt6Config.cmake`.
Everything except the Qt6 frontend still builds without it.

**Qt6 exe starts and immediately exits, or reports a missing DLL.**
`windeployqt` was not found. Add the Qt `bin` directory to `PATH` and rerun
`build_and_run.ps1`, or run `windeployqt` manually against the exe. With a vcpkg
Qt you also need the third-party DLLs listed in §1.4 next to the exe.

**"指定されたパスが見つかりません" during a vcpkg Qt build.** A PowerShell
update moved `pwsh.exe`, and the old absolute path is baked into
`CMakeCache.txt` (`Z_VCPKG_POWERSHELL_PATH`) and into the generated
`.vcxproj` post-build step. `build_and_run.ps1` detects this and deletes
`frontend_qt\build`; to fix it by hand, delete that directory and reconfigure.

**Stale `CMakeCache.txt` after moving the repository.** The script removes a
cache whose `CMAKE_HOME_DIRECTORY` no longer matches. Manually: delete the
`build` directory in question.

**`Could not locate the <name>_core shared library`.** The Python binding found
no DLL/SO. Build the core, or set `<SLUG>_CORE_LIB` to the absolute path.
`ctest` reports these as SKIP rather than FAIL by design.

**`Eigen3 not found`** when configuring `two_dof`. Install `libeigen3-dev`
(Debian/Ubuntu), `eigen` (Homebrew/Arch) or `vcpkg install eigen3`, then pass
`-DCMAKE_PREFIX_PATH` or the vcpkg toolchain file.

### Continuous integration

`.github/workflows/ci.yml` runs on `windows-latest` and `ubuntu-latest`:
installs `gui/python/requirements.txt`, configures and builds the umbrella
project in Release, then runs `ctest`. The GUI frontends are not built in CI.
