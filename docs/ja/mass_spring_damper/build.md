# ビルドガイド

[モジュール概要](README.md) · [理論](theory.md) · [C ABI](api.md) · [English](../../en/mass_spring_damper/build.md)

> このページのパスは `examples/mass_spring_damper/` からの相対パスです。
> リポジトリ全体を一括でビルドする場合は
> [../build-and-run.md](../build-and-run.md) を参照してください。

## 1. C++ コアライブラリ

コアに**外部依存はありません** — C++17 コンパイラと CMake だけです。

### 必要環境

| ツール | 最小バージョン |
|--------|--------------|
| CMake | 3.16 |
| MSVC | Visual Studio 2019 (v142) |
| GCC | 10 |
| Clang | 12 |

### Windows — Visual Studio / MSVC

```powershell
cmake -S core -B core\build -G "Visual Studio 17 2022" -A x64
cmake --build core\build --config Release
```

DLL は `core\build\Release\msd_core.dll` に生成されます。

### Linux / macOS — Makefile または Ninja

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Release
cmake --build core/build -j
```

共有ライブラリは `core/build/libmsd_core.so`（Linux）または
`core/build/libmsd_core.dylib`（macOS）に生成されます。

### スモークテスト

```bash
cmake --build core/build --target msd_core_smoke    # Windows では --config Release を追加
./core/build/msd_core_smoke                          # Linux/macOS
# core\build\Release\msd_core_smoke.exe              # Windows
```

期待される出力：

```
=== msd_core 1.0.0 ===
Sampling: dt=0.0010, stop=10.00
--- baseline           n=10001  x(end)= -0.0211549  ...  ALL OK.
```

---

## 2. Qt6（C++）フロントエンド

### 必要環境

| ツール | 備考 |
|--------|------|
| Qt 6.2 以降 | Core / Gui / Widgets モジュール |
| コアと同じコンパイラ | MSVC 2019+、GCC 10+、Clang 12+ |
| CMake 3.16 以降 | |

### Windows — Visual Studio / MSVC

```powershell
cmake -S frontend_qt -B frontend_qt\build `
      -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build frontend_qt\build --config Release
# 実行ファイル: frontend_qt\build\Release\msd_qt.exe
```

`windeployqt` は CMake のポストビルドコマンドから自動的に実行されるため、
必要な Qt DLL は `msd_qt.exe` の横に配置されます。vcpkg でビルドした Qt を
使う場合は、 `<vcpkg_root>/installed/x64-windows/bin/` からサードパーティ DLL
を手動でコピーしてください。

### Linux / macOS

```bash
cmake -S frontend_qt -B frontend_qt/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.0/gcc_64   # 環境に合わせて調整
cmake --build frontend_qt/build -j
./frontend_qt/build/msd_qt
```

### `build_and_run.ps1` を使う場合（Windows）

```powershell
.\build_and_run.ps1 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
```

---

## 3. Avalonia（C#）フロントエンド

### 必要環境

- .NET SDK 8.0 以降（ `dotnet --version` で確認）
- 初回の NuGet パッケージ復元にインターネット接続
- 先に C++ コアをビルドしておくこと（ `.csproj` が DLL を自動コピーします）

### ビルドと実行

```bash
# 先に C++ コアをビルドする（上記 1 章）

cd frontend_avalonia/MsdAvalonia
dotnet run -c Release
```

`.csproj` には条件付きの `<None>` 項目があり、ビルド時にコアのビルド
ディレクトリから `msd_core.dll` / `libmsd_core.so` / `libmsd_core.dylib` を
`bin/<config>/net8.0/` へコピーします。

### 単体発行（Windows）

```powershell
cd frontend_avalonia\MsdAvalonia
dotnet publish -c Release -r win-x64 --self-contained
```

出力： `bin\Release\net8.0\win-x64\publish\MsdAvalonia.exe`

---

## 4. Python フロントエンド

### 必要環境

| パッケージ | バージョン |
|-----------|-----------|
| Python | 3.9 以降 |
| numpy | 1.22 以降 |
| matplotlib | 3.5 以降 |
| PySide6 | 6.4 以降（ `app_pyside6.py` のみ） |

### 依存関係のインストール

```bash
pip install -r frontend_python/requirements.txt
```

### 実行

```bash
# Python のローダーは ../core/build/ を自動で探索します。
# 必要なら環境変数で上書きしてください：
#   export MSD_CORE_LIB=/path/to/libmsd_core.so   (Linux/macOS)
#   set MSD_CORE_LIB=path\to\msd_core.dll          (Windows)

python frontend_python/app_matplotlib.py   # CLI: frontend_python/output/ に PNG を保存
python frontend_python/app_pyside6.py      # GUI
```

---

## 5. ワンショット補助スクリプト

### Windows PowerShell（`build_and_run.ps1`）

```powershell
.\build_and_run.ps1 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"  # Qt6 フロントエンド
.\build_and_run.ps1 2              # コア + Avalonia をビルドして Avalonia を起動
.\build_and_run.ps1 3              # コア + Python 依存を整えて PySide6 を起動
.\build_and_run.ps1 0              # 全フロントエンドをビルドして起動
.\build_and_run.ps1 2 -SkipBuild   # 再ビルドせず Avalonia を起動
.\build_and_run.ps1 2 -BuildType Debug
```

> これは `examples/mass_spring_damper/build_and_run.ps1`（example 単位）です。
> リポジトリ直下のスクリプトは第 1 引数が example 名になります
> （ `.\build_and_run.ps1 msd 1` ）。

### Windows バッチ（`build_all.bat`）

```bat
build_all.bat
```

C++ コアをビルドし、スモークテストを実行し、Qt6 フロントエンドのビルドを
試みます（Qt6 が見つからない場合はスキップされます）。

### Linux / macOS シェル（`build_all.sh`）

```bash
./build_all.sh
```

C++ コアをビルドし、Python と Avalonia フロントエンドの手順を表示します。

---

## 6. 環境変数

| 変数 | 説明 |
|------|------|
| `MSD_CORE_LIB` | 共有ライブラリのパスを上書き（Python ローダー用） |

---

## 7. よくある問題

### `msd_core.dll` が見つからない（Avalonia）

`.csproj` は `Condition="Exists(...)"` を使ってビルド時に DLL をコピーします。
コピーされていない場合：

```powershell
# 手動でコピー
copy core\build\Release\msd_core.dll frontend_avalonia\MsdAvalonia\bin\Release\net8.0\
```

あるいは、これを自動で処理する `build_and_run.ps1` を使ってください。

### `msd_core.dll` が見つからない（Python）

`MSD_CORE_LIB` に DLL のフルパスを設定します。

```powershell
$env:MSD_CORE_LIB = "$(Resolve-Path core\build\Release\msd_core.dll)"
python frontend_python\app_pyside6.py
```

### `dotnet` コマンドが見つからない

[.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) をインストール
し、 `dotnet` が `PATH` に含まれることを確認してください。

### CMake がコンパイラを見つけられない（Windows）

**Visual Studio Developer Command Prompt** から実行するか、
[Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
を「C++ ビルドツール」ワークロード付きでインストールしてください。
