# build_and_run.ps1  (統一シェル / ルート版)
# ==========================================================================
# examples/ 配下の 4 サンプル (mass_spring_damper / pi_path_tracking / pid /
# two_dof) について、C/C++ コア (<slug>_core.dll)・Qt6・C# Avalonia・Python を
# ビルドし、指定したフロントエンドを起動する Windows 用ワンショットスクリプト。
#
# 詳細ドキュメント:
#   docs/ja/build-and-run.md   (日本語 / このスクリプトの完全リファレンス)
#   docs/en/build-and-run.md   (English)
#   docs/ja/algorithms.md      (各 example のアルゴリズム詳細)
#   docs/en/algorithms.md
#
# --------------------------------------------------------------------------
# 1. このスクリプトが行うこと (Example ごとに順番に実行)
# --------------------------------------------------------------------------
#   (0) 前処理  : 古い CMakeCache / stale な pwsh パスを検出して自動再構成
#   (1) core    : examples/<dir>/core を cmake configure + build
#                 → <slug>_core.dll を core\build\<BuildType>\ に生成し、
#                   csproj が参照する core\build\ 直下にもコピー
#   (2) Qt6     : examples/<dir>/frontend_qt を cmake configure + build
#                 → <slug>_qt.exe を生成し、core dll をその横にコピー、
#                   windeployqt で Qt DLL を配置、vcpkg 依存 DLL を補完
#   (3) Avalonia: dotnet restore + dotnet build (net8.0)
#                 → bin\<BuildType>\net8.0\<Proj>.exe、core dll をコピー
#   (4) Python  : frontend_python\requirements.txt を pip install
#   (5) 起動    : 選択したフロントエンドを Start-Process で別プロセス起動
#                 Python 起動時は <SLUG>_CORE_LIB に dll の絶対パスを設定
#
# 前提ツール:
#   cmake / Visual Studio 2022 (C++)  … core と Qt6 のビルドに必須
#   Qt 6.2 以降 または vcpkg の qtbase … Qt6 フロントエンド (無ければ自動スキップ)
#   .NET 8 SDK (dotnet)                … Avalonia フロントエンド
#   Python 3.9 以降 (python)           … PySide6 フロントエンド
#
# --------------------------------------------------------------------------
# 2. 命名規則 (Example ごとの成果物はすべて slug から導出される)
# --------------------------------------------------------------------------
#   core dll     = <slug>_core.dll        (例: pid_core.dll)
#   Qt6 実行体   = <slug>_qt.exe          (例: pid_qt.exe)
#   Avalonia proj= <Title>Avalonia        (例: PidAvalonia)
#   Python env   = <SLUG>_CORE_LIB        (例: PID_CORE_LIB)
#
#   slug   フルネーム            core dll         Qt6 exe        Avalonia
#   -----  -------------------  ---------------  -------------  --------------
#   msd    mass_spring_damper   msd_core.dll     msd_qt.exe     MsdAvalonia
#   track  pi_path_tracking     track_core.dll   track_qt.exe   TrackAvalonia
#   pid    pid                  pid_core.dll     pid_qt.exe     PidAvalonia
#   tdof   two_dof              tdof_core.dll    tdof_qt.exe    TdofAvalonia
#
#   Example 名は slug でもフルネームでも指定可 (大文字小文字は区別しない):
#     .\build_and_run.ps1 msd   ==   .\build_and_run.ps1 mass_spring_damper
#
# --------------------------------------------------------------------------
# 3. パラメータ
# --------------------------------------------------------------------------
#   -Example <name>    対象 example。msd/track/pid/tdof/フルネーム/all
#                      省略時は all (4 example すべて)。第 1 位置引数。
#   -Target <0-3>      0(省略)=全フロントエンド, 1=Qt6, 2=Avalonia, 3=Python。
#                      第 2 位置引数。
#   -BuildType <cfg>   Release(既定) / Debug / RelWithDebInfo / MinSizeRel。
#                      cmake --config と dotnet -c の両方に渡される。
#   -Qt6Path <path>    Qt6 のインストール接頭辞。省略時は次の順で自動検出:
#                        C:\vcpkg\installed\x64-windows
#                        C:\Qt\6.9.0\msvc2022_64
#                        C:\Qt\6.8.0\msvc2022_64
#                        C:\Qt\6.7.0\msvc2022_64
#                      見つからない場合、Qt6 のビルドのみスキップされる。
#   -SkipBuild         ビルドを一切行わず、既存の成果物を起動するだけ。
#   -SkipPyDeps        pip install (requirements.txt) をスキップ。
#   -List              example 一覧を表示して終了 (ビルドも起動もしない)。
#
# --------------------------------------------------------------------------
# 4. コマンド例 — 全機能 × 全フロントエンド
# --------------------------------------------------------------------------
# 4.1 基本
#   .\build_and_run.ps1                       # 全 example × 全フロントエンド
#   .\build_and_run.ps1 -List                 # example 一覧を表示して終了
#   .\build_and_run.ps1 pid                   # pid の全フロントエンド
#   .\build_and_run.ps1 mass_spring_damper    # フルネーム指定も可
#
# 4.2 Example × フロントエンド (Target: 1=Qt6 / 2=Avalonia / 3=Python)
#   # --- pid : PID による 1 自由度姿勢制御 ---
#   .\build_and_run.ps1 pid 1                 # Qt6      (pid_qt.exe)
#   .\build_and_run.ps1 pid 2                 # Avalonia (PidAvalonia.exe)
#   .\build_and_run.ps1 pid 3                 # Python   (app_pyside6.py)
#   .\build_and_run.ps1 pid                   # 上記 3 つすべて
#
#   # --- track : PI 経路追従 (pi_path_tracking) ---
#   .\build_and_run.ps1 track 1               # Qt6      (track_qt.exe)
#   .\build_and_run.ps1 track 2               # Avalonia (TrackAvalonia.exe)
#   .\build_and_run.ps1 track 3               # Python   (app_pyside6.py)
#   .\build_and_run.ps1 pi_path_tracking      # 上記 3 つすべて
#
#   # --- tdof : 2 自由度制御 vs PID (two_dof) ---
#   .\build_and_run.ps1 tdof 1                # Qt6      (tdof_qt.exe)
#   .\build_and_run.ps1 tdof 2                # Avalonia (TdofAvalonia.exe)
#   .\build_and_run.ps1 tdof 3                # Python   (app_pyside6.py)
#   .\build_and_run.ps1 two_dof               # 上記 3 つすべて
#
#   # --- msd : 質量-ばね-ダンパ強制応答 (mass_spring_damper) ---
#   .\build_and_run.ps1 msd 1                 # Qt6      (msd_qt.exe)
#   .\build_and_run.ps1 msd 2                 # Avalonia (MsdAvalonia.exe)
#   .\build_and_run.ps1 msd 3                 # Python   (app_pyside6.py)
#   .\build_and_run.ps1 mass_spring_damper    # 上記 3 つすべて
#
#   # --- 全 example を 1 フロントエンドで横断 ---
#   .\build_and_run.ps1 all 1                 # 4 example の Qt6 を一括
#   .\build_and_run.ps1 all 2                 # 4 example の Avalonia を一括
#   .\build_and_run.ps1 all 3                 # 4 example の Python を一括
#
# 4.3 ビルド構成の切り替え
#   .\build_and_run.ps1 tdof -BuildType Debug          # Debug でビルド/起動
#   .\build_and_run.ps1 all  -BuildType RelWithDebInfo
#   .\build_and_run.ps1 pid 1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
#   .\build_and_run.ps1 pid 1 -Qt6Path "C:\vcpkg\installed\x64-windows"
#
# 4.4 ビルドの省略 / 高速化
#   .\build_and_run.ps1 pid 1 -SkipBuild               # 起動のみ (再ビルドなし)
#   .\build_and_run.ps1 all -SkipBuild                 # 全部を起動のみ
#   .\build_and_run.ps1 msd 3 -SkipPyDeps              # pip install を省略
#   .\build_and_run.ps1 all 3 -SkipPyDeps              # 依存導入済みの一括起動
#   .\build_and_run.ps1 tdof -BuildType Debug -SkipPyDeps
#
# 4.5 名前付き引数での明示指定 (順不同で書ける)
#   .\build_and_run.ps1 -Example track -Target 2
#   .\build_and_run.ps1 -Example all -Target 1 -BuildType Release `
#                       -Qt6Path "C:\Qt\6.9.0\msvc2022_64"
#   .\build_and_run.ps1 -Example pid -Target 3 -SkipBuild -SkipPyDeps
#
# --------------------------------------------------------------------------
# 5. 成果物の場所 (<ex> = examples\<フルネーム>, <cfg> = -BuildType)
# --------------------------------------------------------------------------
#   core dll  : <ex>\core\build\<cfg>\<slug>_core.dll
#               <ex>\core\build\<slug>_core.dll            (csproj 参照用コピー)
#   Qt6 exe   : <ex>\frontend_qt\build\<cfg>\<slug>_qt.exe
#   Avalonia  : <ex>\frontend_avalonia\<Proj>\bin\<cfg>\net8.0\<Proj>.exe
#   Python    : <ex>\frontend_python\app_pyside6.py  (GUI)
#               <ex>\frontend_python\app_matplotlib.py (CLI/バッチ、本script対象外)
#
# --------------------------------------------------------------------------
# 6. このスクリプトが扱わないもの (手動コマンド)
# --------------------------------------------------------------------------
#   # ルートの統合ビルド (4 コアを build\lib へ) と横断テスト
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   ctest --test-dir build --build-config Release --output-on-failure
#
#   # 共通 GUI ギャラリー (4 題材をドロップダウンで切替)
#   cd gui\python
#   pip install -r requirements.txt
#   python gallery_app.py                      # 対話表示
#   python gallery_app.py --example two_dof    # 1 題材だけ表示
#   python gallery_app.py --save out.png       # 全題材をヘッドレス描画
#
#   # matplotlib 版フロントエンド (CSV/PNG 出力)
#   python examples\pid\frontend_python\app_matplotlib.py
#
# --------------------------------------------------------------------------
# 7. トラブルシューティング
# --------------------------------------------------------------------------
#   * "Qt6 が見つかりません"      : -Qt6Path で接頭辞を指定 (share\Qt6 または
#                                   lib\cmake\Qt6 に Qt6Config.cmake がある階層)。
#   * Qt6 exe が DLL 不足で起動しない: windeployqt が見つからない環境。Qt の
#                                   bin\windeployqt.exe を PATH に通して再実行。
#   * "指定されたパスが見つかりません" (vcpkg): PowerShell 更新で pwsh パスが
#                                   陳腐化したケース。本スクリプトが自動検出して
#                                   frontend_qt\build を再構成する。
#   * Python 側で core dll が見つからない: -SkipBuild 時は既存 dll を探すだけ。
#                                   先に core をビルドするか <SLUG>_CORE_LIB を
#                                   手動で設定する。
# ==========================================================================
param(
    # example セレクタ: msd/track/pid/tdof/フルネーム/all(省略時=all)
    [string]$Example    = "all",

    # 1=Qt6, 2=Avalonia, 3=Python, 0(省略)=全て
    [int]$Target        = 0,

    [string]$BuildType  = "Release",

    # Qt6 のインストールパス (省略時は自動検出)
    [string]$Qt6Path    = "",

    [switch]$SkipBuild,    # ビルド全体をスキップ
    [switch]$SkipPyDeps,   # pip install をスキップ
    [switch]$List          # example 一覧を表示して終了
)

$ErrorActionPreference = "Stop"
$ROOT = $PSScriptRoot

# ── example 定義テーブル ─────────────────────────────────────────────────────
# Key = slug。Dir=examples/配下のフォルダ名, Avalonia=csproj ディレクトリ,
# EnvVar=Python が core dll を探す環境変数名。
$Examples = [ordered]@{
    "msd"   = @{ Dir = "mass_spring_damper"; Avalonia = "MsdAvalonia";   EnvVar = "MSD_CORE_LIB"   }
    "track" = @{ Dir = "pi_path_tracking";   Avalonia = "TrackAvalonia"; EnvVar = "TRACK_CORE_LIB" }
    "pid"   = @{ Dir = "pid";                Avalonia = "PidAvalonia";   EnvVar = "PID_CORE_LIB"   }
    "tdof"  = @{ Dir = "two_dof";            Avalonia = "TdofAvalonia";  EnvVar = "TDOF_CORE_LIB"  }
}

# ── ヘルパー ─────────────────────────────────────────────────────────────────
function Head($msg) { Write-Host "`n######## $msg ########" -ForegroundColor Magenta }
function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "    OK : $msg" -ForegroundColor Green }
function Warn($msg) { Write-Host "    WARN: $msg" -ForegroundColor Yellow }
function Die($msg)  { Write-Host "`nERROR: $msg" -ForegroundColor Red; exit 1 }

function Require($cmd) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Die "'$cmd' が見つかりません。インストールして PATH を通してください。"
    }
}

if ($List) {
    Write-Host "利用可能な example:" -ForegroundColor Cyan
    foreach ($k in $Examples.Keys) {
        Write-Host ("  {0,-6} -> examples/{1}" -f $k, $Examples[$k].Dir)
    }
    exit 0
}

# ── example セレクタを slug 群へ解決 ─────────────────────────────────────────
function Resolve-Examples([string]$sel) {
    if ($sel -eq "all" -or $sel -eq "0" -or [string]::IsNullOrWhiteSpace($sel)) {
        return @($Examples.Keys)
    }
    $s = $sel.ToLower()
    if ($Examples.Contains($s)) { return @($s) }
    # フルネーム一致も許可
    foreach ($k in $Examples.Keys) {
        if ($Examples[$k].Dir -ieq $sel) { return @($k) }
    }
    Die "未知の example '$sel'。'-List' で一覧を確認してください。"
}

$selectedSlugs = Resolve-Examples $Example

# ── Target 番号を起動フラグに変換 ────────────────────────────────────────────
switch ($Target) {
    1 { $LaunchQt = $true;  $LaunchCSharp = $false; $LaunchPython = $false }
    2 { $LaunchQt = $false; $LaunchCSharp = $true;  $LaunchPython = $false }
    3 { $LaunchQt = $false; $LaunchCSharp = $false; $LaunchPython = $true  }
    default {
        if ($Target -ne 0) { Die "Target は 1(Qt6), 2(Avalonia), 3(Python) で指定してください。" }
        $LaunchQt = $true; $LaunchCSharp = $true; $LaunchPython = $true
    }
}

# ── Qt6Config.cmake を含むディレクトリを探す ────────────────────────────────
# vcpkg レイアウト   : <prefix>\share\Qt6\Qt6Config.cmake
# 標準 Qt インストール: <prefix>\lib\cmake\Qt6\Qt6Config.cmake
function Find-Qt6Dir([string]$prefix) {
    foreach ($rel in @("share\Qt6", "lib\cmake\Qt6")) {
        if (Test-Path "$prefix\$rel\Qt6Config.cmake") { return "$prefix\$rel" }
    }
    return $null
}

# ── Qt6 パスの自動検出 (全 example 共通で 1 回だけ) ──────────────────────────
if ($LaunchQt -and -not $SkipBuild -and -not $Qt6Path) {
    $candidates = @(
        "C:\vcpkg\installed\x64-windows",
        "C:\Qt\6.9.0\msvc2022_64",
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.0\msvc2022_64"
    )
    foreach ($c in $candidates) {
        if (Find-Qt6Dir $c) {
            $Qt6Path = $c
            Write-Host "Qt6 を自動検出: $Qt6Path" -ForegroundColor DarkCyan
            break
        }
    }
    if (-not $Qt6Path) {
        Warn "Qt6 が見つかりません。-Qt6Path で指定するか Qt をインストールしてください。Qt6 のビルドはスキップします。"
    }
}

# ── Qt6 configure 用の追加引数を組み立てる ──────────────────────────────────
# vcpkg の Qt6 は CMAKE_PREFIX_PATH だけでは解決できないため、Qt6_DIR を直接指定し、
# vcpkg ツリーなら vcpkg ツールチェーンファイルと triplet も付与する。
# ツールチェーン/Qt6_DIR は初回 configure 時のみ有効 (キャッシュ後は変更不可)。
function Get-QtExtraArgs([string]$prefix, [string]$buildDir) {
    $extra = @()
    if (Test-Path "$buildDir\CMakeCache.txt") { return ,$extra }  # 既存キャッシュは触らない
    $qt6dir = Find-Qt6Dir $prefix
    if ($qt6dir) { $extra += "-DQt6_DIR=$qt6dir" }
    # vcpkg ツリー判定: ...\vcpkg\installed\<triplet>
    if ($prefix -match '^(?<root>.*\\vcpkg)\\installed\\(?<triplet>[^\\]+)') {
        $tc = "$($Matches.root)\scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $tc) {
            $extra += "-DCMAKE_TOOLCHAIN_FILE=$tc"
            $extra += "-DVCPKG_TARGET_TRIPLET=$($Matches.triplet)"
        }
    }
    return ,$extra
}

# ── cmake 共通ヘルパー ────────────────────────────────────────────────────────
function Clear-StaleCmakeCache($buildDir, $sourceDir) {
    $cache = "$buildDir\CMakeCache.txt"
    if (-not (Test-Path $cache)) { return }
    $cached = (Get-Content $cache | Select-String "^CMAKE_HOME_DIRECTORY").Line
    if ($cached -and -not ($cached -match [Regex]::Escape($sourceDir.Replace('\','/')))) {
        Write-Host "    古い CMakeCache を削除します: $cache" -ForegroundColor DarkYellow
        Remove-Item $cache -Force
    }
}

# ── stale pwsh パスの検出 (vcpkg toolchain 用) ───────────────────────────────
# vcpkg の CMake toolchain は configure 時に find_program(pwsh) の絶対パスを
# CMakeCache (Z_VCPKG_POWERSHELL_PATH) と生成 vcxproj の post-build (applocal.ps1)
# に焼き込む。PowerShell は WindowsApps 配下にバージョン別フォルダで入るため、
# 更新されると旧パスが消え「指定されたパスが見つかりません」でビルドが失敗する。
# find_program はキャッシュ済みパスを再検証しないので、パスが実在しなければ
# ビルドディレクトリごと削除し、次回 configure で現行 pwsh を再検出させる。
function Clear-StalePwshCache([string]$buildDir) {
    $cache = "$buildDir\CMakeCache.txt"
    if (-not (Test-Path $cache)) { return }
    $line = (Get-Content $cache | Select-String "^Z_VCPKG_POWERSHELL_PATH").Line
    if (-not $line) { return }
    $p = ($line -replace '^[^=]*=', '').Trim()
    if ($p -and -not (Test-Path $p)) {
        Write-Host "    stale pwsh パスを検出 ($p)。$buildDir を再構成します。" -ForegroundColor DarkYellow
        Remove-Item $buildDir -Recurse -Force
    }
}

function Get-CmakeConfigArgs($buildDir, $prefixPath) {
    $result = @()
    if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
        $result += "-G", "Visual Studio 17 2022", "-A", "x64"
    }
    if ($prefixPath) { $result += "-DCMAKE_PREFIX_PATH=$prefixPath" }
    return ,$result
}

# windeployqt が必要とする既知の vcpkg サードパーティ DLL
$QtThirdPartyDlls = @(
    "double-conversion.dll", "pcre2-16.dll", "z.dll", "zstd.dll",
    "harfbuzz.dll", "freetype.dll", "libpng16.dll", "bz2.dll", "md4c.dll",
    "brotlidec.dll", "brotlicommon.dll", "jpeg62.dll", "libcrypto-3-x64.dll"
)

# ════════════════════════════════════════════════════════════════════════════
#  1 つの example をビルド + 起動する
# ════════════════════════════════════════════════════════════════════════════
function Invoke-Example([string]$slug) {
    $cfg      = $Examples[$slug]
    $exRoot   = Join-Path $ROOT "examples\$($cfg.Dir)"
    $coreDll  = "$slug`_core.dll"       # 例: pid_core.dll
    $qtExeNm  = "$slug`_qt.exe"         # 例: pid_qt.exe
    $avProj   = $cfg.Avalonia           # 例: PidAvalonia
    $envVar   = $cfg.EnvVar             # 例: PID_CORE_LIB

    Head "$slug  (examples/$($cfg.Dir))"

    if (-not (Test-Path $exRoot)) { Warn "ディレクトリが見つかりません: $exRoot"; return }

    # ── ビルド ──────────────────────────────────────────────────────────────
    if (-not $SkipBuild) {
        Require "cmake"

        # (1) C/C++ コア
        Step "C/C++ core をビルド中 ($BuildType)"
        Clear-StaleCmakeCache "$exRoot\core\build" "$exRoot\core"
        $coreArgs = Get-CmakeConfigArgs "$exRoot\core\build" ""
        cmake -S "$exRoot\core" -B "$exRoot\core\build" -DCMAKE_BUILD_TYPE=$BuildType @coreArgs
        if ($LASTEXITCODE -ne 0) { Die "$slug core cmake configure 失敗" }
        cmake --build "$exRoot\core\build" --config $BuildType -j
        if ($LASTEXITCODE -ne 0) { Die "$slug core cmake build 失敗" }

        $builtDll = @(
            "$exRoot\core\build\$BuildType\$coreDll",   # MSVC multi-config
            "$exRoot\core\build\$coreDll"               # Ninja / MinGW
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
        if (-not $builtDll) { Die "$coreDll が見つかりません" }
        Ok "core → $builtDll"

        # csproj は ..\..\core\build\<slug>_core.dll を参照するので直下にもコピー
        $coreFlat = "$exRoot\core\build\$coreDll"
        if ($builtDll -ne $coreFlat) { Copy-Item $builtDll $coreFlat -Force; Ok "$coreDll → $coreFlat (csproj 用)" }

        # (2) Qt6
        if ($LaunchQt -and $Qt6Path) {
            Step "Qt6 フロントエンドをビルド中 ($BuildType)"
            Clear-StalePwshCache  "$exRoot\frontend_qt\build"
            Clear-StaleCmakeCache "$exRoot\frontend_qt\build" "$exRoot\frontend_qt"
            $qtArgs   = Get-CmakeConfigArgs "$exRoot\frontend_qt\build" $Qt6Path
            $qtExtra  = Get-QtExtraArgs $Qt6Path "$exRoot\frontend_qt\build"
            cmake -S "$exRoot\frontend_qt" -B "$exRoot\frontend_qt\build" -DCMAKE_BUILD_TYPE=$BuildType @qtArgs @qtExtra
            if ($LASTEXITCODE -ne 0) { Die "$slug Qt6 cmake configure 失敗" }
            cmake --build "$exRoot\frontend_qt\build" --config $BuildType -j
            if ($LASTEXITCODE -ne 0) { Die "$slug Qt6 cmake build 失敗" }

            $qtExe = @(
                "$exRoot\frontend_qt\build\$BuildType\$qtExeNm",
                "$exRoot\frontend_qt\build\$qtExeNm"
            ) | Where-Object { Test-Path $_ } | Select-Object -First 1
            if ($qtExe) {
                $qtDir = Split-Path $qtExe
                Copy-Item $builtDll $qtDir -Force
                Ok "$coreDll → $qtDir"

                $windeployqt = @(
                    "$Qt6Path\tools\Qt6\bin\windeployqt.exe",
                    "$Qt6Path\bin\windeployqt.exe"
                ) | Where-Object { Test-Path $_ } | Select-Object -First 1
                if ($windeployqt) {
                    & $windeployqt --no-translations --no-system-d3d-compiler "$qtExe"
                    Ok "windeployqt 完了"
                    $vcpkgBin = "$Qt6Path\bin"
                    if (Test-Path $vcpkgBin) {
                        $copied = @()
                        foreach ($dll in $QtThirdPartyDlls) {
                            if (-not (Test-Path "$qtDir\$dll") -and (Test-Path "$vcpkgBin\$dll")) {
                                Copy-Item "$vcpkgBin\$dll" $qtDir -Force; $copied += $dll
                            }
                        }
                        if ($copied.Count -gt 0) { Ok "vcpkg 依存 DLL を補完: $($copied -join ', ')" }
                    }
                } else {
                    Warn "windeployqt が見つかりません。Qt DLL が不足する場合は手動でコピーしてください。"
                }
            }
            Ok "Qt6 ビルド完了"
        } elseif ($LaunchQt) {
            Warn "Qt6 のビルドをスキップ (Qt6Path 未設定)"
        }

        # (3) C# / Avalonia
        if ($LaunchCSharp) {
            Require "dotnet"
            $csprojDir = "$exRoot\frontend_avalonia\$avProj"
            Step "C# Avalonia をビルド中 ($BuildType)"
            dotnet restore "$csprojDir"
            if ($LASTEXITCODE -ne 0) { Die "$slug dotnet restore 失敗" }
            dotnet build "$csprojDir" -c $BuildType --no-restore
            if ($LASTEXITCODE -ne 0) { Die "$slug dotnet build 失敗" }

            $avaloniaOut = "$csprojDir\bin\$BuildType\net8.0"
            if (-not (Test-Path "$avaloniaOut\$coreDll") -and $builtDll) {
                Copy-Item $builtDll "$avaloniaOut\$coreDll" -Force
                Ok "$coreDll → $avaloniaOut (手動コピー)"
            }
            Ok "Avalonia ビルド完了"
        }

        # (4) Python 依存関係
        if ($LaunchPython -and -not $SkipPyDeps) {
            Require "python"
            Step "Python 依存関係をインストール中"
            python -m pip install -r "$exRoot\frontend_python\requirements.txt"
            if ($LASTEXITCODE -ne 0) { Die "$slug pip install 失敗" }
            Ok "pip install 完了"
        }
    } # if (-not $SkipBuild)

    # ── 起動 ──────────────────────────────────────────────────────────────────
    $coreDllForLaunch = @(
        "$exRoot\core\build\$BuildType\$coreDll",
        "$exRoot\core\build\$coreDll"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    Step "フロントエンドを起動します"

    if ($LaunchQt) {
        $qtExe = @(
            "$exRoot\frontend_qt\build\$BuildType\$qtExeNm",
            "$exRoot\frontend_qt\build\$qtExeNm"
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($qtExe) {
            Start-Process $qtExe -WorkingDirectory (Split-Path $qtExe)
            Ok "Qt6 起動 : $qtExe"
        } else { Warn "Qt6 実行ファイルが見つかりません (先にビルドしてください)" }
    }

    if ($LaunchCSharp) {
        $csprojDir = "$exRoot\frontend_avalonia\$avProj"
        $avaloniaExe = @(
            "$csprojDir\bin\$BuildType\net8.0\$avProj.exe",
            "$csprojDir\bin\$BuildType\net8.0\win-x64\$avProj.exe"
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($avaloniaExe) {
            Start-Process $avaloniaExe -WorkingDirectory (Split-Path $avaloniaExe)
            Ok "Avalonia 起動 : $avaloniaExe"
        } else {
            Start-Process "dotnet" `
                -ArgumentList "run --project `"$csprojDir`" -c $BuildType --no-build" `
                -WorkingDirectory $csprojDir
            Ok "Avalonia 起動 (dotnet run フォールバック)"
        }
    }

    if ($LaunchPython) {
        $pyScript = "$exRoot\frontend_python\app_pyside6.py"
        if (Test-Path $pyScript) {
            if ($coreDllForLaunch) { Set-Item -Path "Env:$envVar" -Value $coreDllForLaunch }
            Start-Process "python" -ArgumentList "`"$pyScript`"" -WorkingDirectory "$exRoot\frontend_python"
            Ok "Python (PySide6) 起動 : $pyScript"
            if ($coreDllForLaunch) { Ok "  $envVar=$coreDllForLaunch" }
        } else { Warn "Python スクリプトが見つかりません : $pyScript" }
    }
}

# ════════════════════════════════════════════════════════════════════════════
#  実行
# ════════════════════════════════════════════════════════════════════════════
foreach ($slug in $selectedSlugs) { Invoke-Example $slug }

$launched = @(
    if ($LaunchQt)     { "Qt6" }
    if ($LaunchCSharp) { "Avalonia" }
    if ($LaunchPython) { "Python" }
) -join ", "
Write-Host "`n完了: [$($selectedSlugs -join ', ')] / [$launched] を処理しました。" -ForegroundColor Green
