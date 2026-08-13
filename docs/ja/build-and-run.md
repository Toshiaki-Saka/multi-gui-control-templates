# ビルドと実行

English: [../en/build-and-run.md](../en/build-and-run.md)

本ページでは、このリポジトリのコードをビルド・起動するすべての方法を説明します。

1. [`build_and_run.ps1`](#1-build_and_runps1windows-ワンショット) — Windows 用のワンショット実行スクリプト（全機能・全フロントエンド）
2. [統合 CMake ビルド](#2-統合-cmake-ビルド4-コア) — 4 コア + CTest（プラットフォーム非依存）
3. [example ごとの手動コマンド](#3-example-ごとの手動コマンド) — コア / Qt6 / Avalonia / Python
4. [共通 Python ギャラリー](#4-共通-python-ギャラリー)
5. [前提ツールとトラブルシューティング](#5-前提ツール)

---

## 1. `build_and_run.ps1`（Windows ワンショット）

リポジトリ直下の `build_and_run.ps1` は、指定した example について C/C++ コア・
Qt6 フロントエンド・C# Avalonia フロントエンド・Python 依存関係をビルドし、
選択したフロントエンドを別プロセスとして起動します。

### 1.1 シグネチャ

```powershell
.\build_and_run.ps1 [-Example <name>] [-Target <0-3>] [-BuildType <cfg>]
                    [-Qt6Path <path>] [-SkipBuild] [-SkipPyDeps] [-List]
```

`-Example` が第 1 位置引数、`-Target` が第 2 位置引数なので、
`.\build_and_run.ps1 pid 1` は
`.\build_and_run.ps1 -Example pid -Target 1` と同じ意味になります。

### 1.2 パラメータ

| パラメータ | 既定値 | 意味 |
|-----------|--------|------|
| `-Example <name>` | `all` | 対象 example。`msd` / `track` / `pid` / `tdof`、フルネーム（`mass_spring_damper` など）、`all`。大文字小文字は区別しません。 |
| `-Target <0-3>` | `0` | フロントエンド。`1` = Qt6、`2` = Avalonia、`3` = Python、`0` = 全て。 |
| `-BuildType <cfg>` | `Release` | `cmake --config` と `dotnet -c` の両方に渡されます。`Release` / `Debug` / `RelWithDebInfo` / `MinSizeRel`。 |
| `-Qt6Path <path>` | 自動検出 | Qt 6 のインストール接頭辞（`share\Qt6\Qt6Config.cmake` または `lib\cmake\Qt6\Qt6Config.cmake` を含む階層）。 |
| `-SkipBuild` | off | ビルドを行わず、既存の成果物を起動するだけ。 |
| `-SkipPyDeps` | off | `pip install -r requirements.txt` をスキップ。 |
| `-List` | off | example 一覧を表示して終了（ビルドも起動もしない）。 |

### 1.3 命名規則

すべての成果物名は slug から導出されるため、スクリプト側に example 固有の
分岐はありません。

| slug | フルネーム | コア DLL | Qt6 exe | Avalonia プロジェクト | Python 環境変数 |
|------|-----------|----------|---------|---------------------|----------------|
| `msd` | `mass_spring_damper` | `msd_core.dll` | `msd_qt.exe` | `MsdAvalonia` | `MSD_CORE_LIB` |
| `track` | `pi_path_tracking` | `track_core.dll` | `track_qt.exe` | `TrackAvalonia` | `TRACK_CORE_LIB` |
| `pid` | `pid` | `pid_core.dll` | `pid_qt.exe` | `PidAvalonia` | `PID_CORE_LIB` |
| `tdof` | `two_dof` | `tdof_core.dll` | `tdof_qt.exe` | `TdofAvalonia` | `TDOF_CORE_LIB` |

### 1.4 スクリプトの処理内容（example ごとに順番に実行）

| 段階 | 処理 |
|------|------|
| 0. キャッシュ整理 | `CMAKE_HOME_DIRECTORY` がソースディレクトリと一致しない `CMakeCache.txt` を削除。さらに、キャッシュされた `Z_VCPKG_POWERSHELL_PATH` が存在しない `pwsh.exe` を指している場合は `frontend_qt\build` ツリーごと削除（PowerShell の更新により、vcpkg が生成した `.vcxproj` に焼き込まれたパスが無効化されるため）。 |
| 1. コア | `cmake -S core -B core\build` + `cmake --build --config <cfg> -j`。生成された `<slug>_core.dll` を `core\build\<cfg>\`（MSVC マルチ構成）または `core\build\`（Ninja / MinGW）から解決し、`.csproj` の `Exists(...)` 条件が参照する `core\build\` 直下にもコピー。 |
| 2. Qt6 | `frontend_qt` を `-DCMAKE_PREFIX_PATH=<Qt6Path>` で configure。初回 configure 時のみ `-DQt6_DIR=...` を付与し、vcpkg ツリーであれば `-DCMAKE_TOOLCHAIN_FILE=...\vcpkg.cmake` と `-DVCPKG_TARGET_TRIPLET=...` も追加。ビルド後にコア DLL を exe の横へコピーし、`windeployqt --no-translations --no-system-d3d-compiler` を実行、さらに `windeployqt` が拾わない vcpkg のサードパーティ DLL（`double-conversion`, `pcre2-16`, `z`, `zstd`, `harfbuzz`, `freetype`, `libpng16`, `bz2`, `md4c`, `brotlidec`, `brotlicommon`, `jpeg62`, `libcrypto-3-x64`）を補完。 |
| 3. Avalonia | `dotnet restore` + `dotnet build -c <cfg> --no-restore`。csproj のコピーが効いていない場合は `bin\<cfg>\net8.0\` へコア DLL を手動コピー。 |
| 4. Python | `python -m pip install -r frontend_python\requirements.txt`（`-SkipPyDeps` 指定時を除く）。 |
| 5. 起動 | 選択したフロントエンドを `Start-Process` で起動。Avalonia は exe が無ければ `dotnet run --no-build` にフォールバック。Python 起動前に `<SLUG>_CORE_LIB` へ DLL の絶対パスを設定。 |

Qt6 は「見つからなければ Qt6 のビルドだけスキップ」という挙動です。Qt 6 が
無い環境でも、コア・Avalonia・Python の経路はそのまま動作します。

### 1.5 コマンド例 — 全機能

#### 基本

```powershell
.\build_and_run.ps1                      # 4 example × 3 フロントエンド
.\build_and_run.ps1 -List                # example 一覧を表示して終了
.\build_and_run.ps1 pid                  # pid の 3 フロントエンドすべて
.\build_and_run.ps1 mass_spring_damper   # フルネーム指定も可
```

#### 全 example × 全フロントエンド

```powershell
# pid — PID 姿勢制御
.\build_and_run.ps1 pid 1                # Qt6      -> pid_qt.exe
.\build_and_run.ps1 pid 2                # Avalonia -> PidAvalonia.exe
.\build_and_run.ps1 pid 3                # Python   -> app_pyside6.py
.\build_and_run.ps1 pid                  # 上記すべて

# track — PI 経路追従
.\build_and_run.ps1 track 1              # Qt6      -> track_qt.exe
.\build_and_run.ps1 track 2              # Avalonia -> TrackAvalonia.exe
.\build_and_run.ps1 track 3              # Python   -> app_pyside6.py
.\build_and_run.ps1 pi_path_tracking     # 上記すべて

# tdof — 2 自由度制御 vs PID
.\build_and_run.ps1 tdof 1               # Qt6      -> tdof_qt.exe
.\build_and_run.ps1 tdof 2               # Avalonia -> TdofAvalonia.exe
.\build_and_run.ps1 tdof 3               # Python   -> app_pyside6.py
.\build_and_run.ps1 two_dof              # 上記すべて

# msd — 質量-ばね-ダンパ
.\build_and_run.ps1 msd 1                # Qt6      -> msd_qt.exe
.\build_and_run.ps1 msd 2                # Avalonia -> MsdAvalonia.exe
.\build_and_run.ps1 msd 3                # Python   -> app_pyside6.py
.\build_and_run.ps1 mass_spring_damper   # 上記すべて

# 1 フロントエンドで 4 example を横断
.\build_and_run.ps1 all 1                # 全 example の Qt6
.\build_and_run.ps1 all 2                # 全 example の Avalonia
.\build_and_run.ps1 all 3                # 全 example の PySide6
```

#### ビルド構成

```powershell
.\build_and_run.ps1 tdof -BuildType Debug
.\build_and_run.ps1 all  -BuildType RelWithDebInfo
.\build_and_run.ps1 pid 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
.\build_and_run.ps1 pid 1 -Qt6Path "C:\vcpkg\installed\x64-windows"
```

#### ビルドの省略

```powershell
.\build_and_run.ps1 pid 1 -SkipBuild            # 起動のみ
.\build_and_run.ps1 all -SkipBuild              # ビルド済みのものを一括起動
.\build_and_run.ps1 msd 3 -SkipPyDeps           # pip install なし
.\build_and_run.ps1 all 3 -SkipPyDeps
.\build_and_run.ps1 tdof -BuildType Debug -SkipPyDeps
```

#### 名前付きパラメータ（順不同）

```powershell
.\build_and_run.ps1 -Example track -Target 2
.\build_and_run.ps1 -Example all -Target 1 -BuildType Release `
                    -Qt6Path "C:\Qt\6.9.0\msvc2022_64"
.\build_and_run.ps1 -Example pid -Target 3 -SkipBuild -SkipPyDeps
```

### 1.6 成果物の場所

`<ex> = examples\<フルネーム>`、`<cfg> = -BuildType` として：

| 成果物 | パス |
|--------|------|
| コア DLL（ビルド出力） | `<ex>\core\build\<cfg>\<slug>_core.dll` |
| コア DLL（csproj 用のフラットコピー） | `<ex>\core\build\<slug>_core.dll` |
| Qt6 実行ファイル | `<ex>\frontend_qt\build\<cfg>\<slug>_qt.exe` |
| Avalonia 実行ファイル | `<ex>\frontend_avalonia\<Proj>\bin\<cfg>\net8.0\<Proj>.exe` |
| Python GUI | `<ex>\frontend_python\app_pyside6.py` |
| Python バッチスクリプト | `<ex>\frontend_python\app_matplotlib.py` |

### 1.7 example ごとのスクリプト

各 example にも（統合前のリポジトリ由来の）`build_and_run.ps1` があります。
オプションはルート版から `-Example` / `-List` を除いたものです。

```powershell
cd examples\pid
.\build_and_run.ps1 1                       # Qt6 のみ
.\build_and_run.ps1 -BuildType Debug
```

`examples/mass_spring_damper` にはさらに `build_all.bat`（Windows / MSVC）と
`build_all.sh`（POSIX）があり、コアのビルド → C++ スモークテスト → Qt6
フロントエンド（Qt 6 が見つかる場合）の順に実行します。

---

## 2. 統合 CMake ビルド（4 コア）

ルートの `CMakeLists.txt` は 4 つのコアを `build/lib` にまとめて出力します。
共通ギャラリーと横断テストはここを探索します。Qt6 / Avalonia
フロントエンドはビルドされません。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

`two_dof` には **Eigen 3.3 以降**（`Eigen/Dense` と
`unsupported/Eigen/MatrixFunctions`）が必要です。CMake はまず
`Eigen3::Eigen` パッケージを探し、無ければ `/usr/include/eigen3`、
`/usr/local/include/eigen3`、`/opt/homebrew/include/eigen3` を探索します。
Windows では `vcpkg install eigen3` を行い、vcpkg のツールチェーンファイルを
指定して configure してください。

### 登録されるテスト

| テスト | 登録元 | 備考 |
|--------|--------|------|
| `cross_example_python` | ルート `CMakeLists.txt` | `tests/test_examples.py` を実行し、共通アダプタ経由で 4 コアすべてを走らせて非自明な時系列が返ることを確認します。ライブラリが無い example は FAIL ではなく SKIP。Windows ではテストの `PATH` 先頭に `build/lib` が追加されます。 |
| `smoke` | `examples/pi_path_tracking/core` | `track_core_smoke`。`TRACK_BUILD_TESTS=ON`（既定）で無条件に登録されるため、統合ビルドからも実行されます。 |

`pid` / `two_dof` / `mass_spring_damper` のコアはスモークテストを
`if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)` でガードしているため、
そのコアを単体でトップレベル構成した場合のみ登録されます。

```sh
cmake -S examples/pid/core -B examples/pid/core/build -DCMAKE_BUILD_TYPE=Release
cmake --build examples/pid/core/build --config Release
ctest --test-dir examples/pid/core/build --build-config Release --output-on-failure
# -> pid_core_smoke
```

`examples/two_dof/core`（`tdof_core_smoke`）、
`examples/mass_spring_damper/core`（`msd_core_smoke`）も同様です。

---

## 3. example ごとの手動コマンド

§1.3 の slug / フルネーム対応表に読み替えてください。以下は `pid` の例です。

### 3.1 コアのみ

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

### 3.2 Qt6 フロントエンド

`frontend_qt/CMakeLists.txt` は、コアターゲットが未定義であれば
`add_subdirectory(../core)` でコアを取り込みます。したがってフロントエンドを
ビルドすればコアも同時にビルドされます。

```powershell
cmake -S examples\pid\frontend_qt -B examples\pid\frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build examples\pid\frontend_qt\build --config Release -j
.\examples\pid\frontend_qt\build\Release\pid_qt.exe
```

vcpkg の Qt を使う場合は、**初回** configure 時に `Qt6_DIR` を config
ディレクトリへ向けます（以降はキャッシュされます）。

```powershell
cmake -S examples\pid\frontend_qt -B examples\pid\frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DQt6_DIR="C:\vcpkg\installed\x64-windows\share\Qt6" `
      -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
      -DVCPKG_TARGET_TRIPLET=x64-windows
```

`msd_qt` と `track_qt` は CMake が `windeployqt` を発見できればポストビルドで
自動実行します。`pid_qt` と `tdof_qt` は `build_and_run.ps1`（または手動の
`windeployqt`）による配置に依存します。

```sh
# POSIX
cmake -S examples/pid/frontend_qt -B examples/pid/frontend_qt/build \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr
cmake --build examples/pid/frontend_qt/build -j
./examples/pid/frontend_qt/build/pid_qt
```

### 3.3 Avalonia（C#）フロントエンド

.NET 8 SDK が必要です。`.csproj` は
`..\..\core\build\<slug>_core.dll`（および `.so` / `.dylib`）が存在する場合に
出力ディレクトリへコピーするため、先にコアをビルドしてください。

```powershell
dotnet restore examples\pid\frontend_avalonia\PidAvalonia
dotnet build   examples\pid\frontend_avalonia\PidAvalonia -c Release --no-restore
.\examples\pid\frontend_avalonia\PidAvalonia\bin\Release\net8.0\PidAvalonia.exe

# 1 コマンドで実行する場合
dotnet run --project examples\pid\frontend_avalonia\PidAvalonia -c Release
```

プロジェクトディレクトリ：`PidAvalonia` / `TrackAvalonia` / `TdofAvalonia` /
`MsdAvalonia`。

### 3.4 Python フロントエンド

各 example には Python フロントエンドが 2 種類あり、同じディレクトリに
ctypes バインディング（`<slug>_core.py`）が置かれています。

| スクリプト | 種類 |
|-----------|------|
| `app_pyside6.py` | 対話的な PySide6 GUI（`build_and_run.ps1 <ex> 3` が起動するのはこちら） |
| `app_matplotlib.py` | 元の参照スクリプトを再現するバッチ / CLI 版。`frontend_python/output*/` に PNG・CSV を出力 |

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

環境変数の設定は必須ではありません。バインディングは
`../core/build/`、`../core/build/Release/`、`../core/build/Debug/`、自身の
ディレクトリ、最後に `ctypes.util.find_library()` の順に探索します。ただし
特定のビルド構成を確実に指定したい場合は環境変数が最も確実です。

example ごとの環境変数： `PID_CORE_LIB` / `TRACK_CORE_LIB` /
`TDOF_CORE_LIB` / `MSD_CORE_LIB`。

---

## 4. 共通 Python ギャラリー

`gui/python/gallery_app.py` は共通アダプタ層（`gui/python/adapters.py`）を通じて
**4 コアすべて**を駆動し、matplotlib で描画します。必要な依存は `numpy` と
`matplotlib` だけで、PySide6 は不要です。

```sh
cd gui/python
pip install -r requirements.txt

python gallery_app.py                       # 既定は pid
python gallery_app.py --example pid
python gallery_app.py --example pi_path_tracking
python gallery_app.py --example two_dof
python gallery_app.py --example mass_spring_damper
python gallery_app.py --save gallery.png    # ヘッドレスで 4 題材を 1 枚に描画
```

`gui/python/libloader.py` はインポート時に実行され、次の順でコアライブラリを
解決します。設定済みの `<NAME>_CORE_LIB` → 
`examples/<name>/core/build[/Release|Debug|RelWithDebInfo]` → 
`build/` および `build/lib/`（統合ビルドの出力、同じ構成サブディレクトリを含む）。
複数見つかった場合は **mtime が最も新しいもの**が採用されるため、example
単体の再ビルドが古い統合ビルドより優先されます。

横断テストも同じ経路を使います。

```sh
python tests/test_examples.py
```

---

## 5. 前提ツール

| ツール | 用途 | 備考 |
|--------|------|------|
| CMake 3.16 以降 | コア、Qt6 フロントエンド | |
| C++17 コンパイラ | コア、Qt6 フロントエンド | Windows では MSVC 2022（`Visual Studio 17 2022`、`-A x64`）、他は GCC / Clang |
| Eigen 3.3 以降 | `two_dof` のコアのみ | 行列指数関数に `unsupported/Eigen/MatrixFunctions` が必要 |
| Qt 6.2 以降（Core / Gui / Widgets） | Qt6 フロントエンド | 通常の Qt インストールまたは vcpkg の `qtbase` |
| .NET 8 SDK | Avalonia フロントエンド | Avalonia 11.0.10 |
| Python 3.9 以降 | Python フロントエンド、ギャラリー、テスト | `numpy` / `matplotlib`、`app_pyside6.py` には加えて PySide6 |

### トラブルシューティング

**「Qt6 が見つかりません」と表示され Qt6 がスキップされる。**
`share\Qt6\Qt6Config.cmake` または `lib\cmake\Qt6\Qt6Config.cmake` を含む
接頭辞を `-Qt6Path` で指定してください。Qt6 以外はこの状態でもビルドされます。

**Qt6 の exe が起動直後に終了する / DLL 不足のエラーが出る。**
`windeployqt` が見つかっていません。Qt の `bin` ディレクトリを `PATH` に通して
`build_and_run.ps1` を再実行するか、exe に対して手動で `windeployqt` を実行して
ください。vcpkg の Qt を使う場合は §1.4 のサードパーティ DLL も exe の横に必要です。

**vcpkg の Qt ビルド中に「指定されたパスが見つかりません」。**
PowerShell の更新で `pwsh.exe` の位置が変わり、旧パスが `CMakeCache.txt` の
`Z_VCPKG_POWERSHELL_PATH` と生成された `.vcxproj` のポストビルドに焼き込まれた
ままになっています。`build_and_run.ps1` はこれを検出して `frontend_qt\build` を
削除します。手動で対処する場合も同ディレクトリを削除して再構成してください。

**リポジトリを移動した後に `CMakeCache.txt` が陳腐化する。**
`CMAKE_HOME_DIRECTORY` が一致しないキャッシュはスクリプトが削除します。手動の
場合は該当する `build` ディレクトリを削除してください。

**`Could not locate the <name>_core shared library`。**
Python バインディングが DLL / SO を見つけられていません。コアをビルドするか、
`<SLUG>_CORE_LIB` に絶対パスを設定してください。`ctest` はこの状態を FAIL では
なく SKIP として扱う設計です。

**`two_dof` の configure で `Eigen3 not found`。**
`libeigen3-dev`（Debian / Ubuntu）、`eigen`（Homebrew / Arch）、
`vcpkg install eigen3` のいずれかを導入し、`-DCMAKE_PREFIX_PATH` か vcpkg の
ツールチェーンファイルを指定してください。

### 継続的インテグレーション

`.github/workflows/ci.yml` は `windows-latest` と `ubuntu-latest` で実行され、
`gui/python/requirements.txt` を導入 → 統合プロジェクトを Release で configure
＆ビルド → `ctest` を実行します。GUI フロントエンドは CI ではビルドされません。
