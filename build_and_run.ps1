# build_and_run.ps1  (統一シェル / ルート版)
# --------------------------------------------------------------------------
# examples/ 配下の各サンプル (mass_spring_damper / pi_path_tracking / pid /
# two_dof) について、C/C++ コア (<slug>_core.dll)・Qt6・C# Avalonia・Python を
# ビルドし、指定したフロントエンドを起動する。
#
# 各 example は共通の命名規則で導出できる:
#   core dll     = <slug>_core.dll        (例: pid_core.dll)
#   Qt6 実行体   = <slug>_qt.exe          (例: pid_qt.exe)
#   Avalonia proj= <Title>Avalonia        (例: PidAvalonia)
#   Python env   = <SLUG>_CORE_LIB         (例: PID_CORE_LIB)
#
# 使い方:
#   .\build_and_run.ps1                       # 全 example × 全フロントエンド
#   .\build_and_run.ps1 pid                   # pid の全フロントエンド
#   .\build_and_run.ps1 pid 1                 # pid の Qt6 のみ
#   .\build_and_run.ps1 msd 3                 # mass_spring_damper の Python のみ
#   .\build_and_run.ps1 all 2                 # 全 example の Avalonia のみ
#   .\build_and_run.ps1 pid 1 -SkipBuild      # ビルドせず起動のみ
#   .\build_and_run.ps1 -List                 # example 一覧を表示して終了
#   .\build_and_run.ps1 tdof -BuildType Debug
#   .\build_and_run.ps1 pid -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
#
# Example 名は slug でもフルネームでも可:
#   msd  = mass_spring_damper
#   track= pi_path_tracking
#   pid  = pid
#   tdof = two_dof
# --------------------------------------------------------------------------
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

# ── Qt6 パスの自動検出 (全 example 共通で 1 回だけ) ──────────────────────────
if ($LaunchQt -and -not $SkipBuild -and -not $Qt6Path) {
    $candidates = @(
        "C:\vcpkg\installed\x64-windows",
        "C:\Qt\6.9.0\msvc2022_64",
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.0\msvc2022_64"
    )
    foreach ($c in $candidates) {
        if (Test-Path "$c\share\Qt6\Qt6Config.cmake") {
            $Qt6Path = $c
            Write-Host "Qt6 を自動検出: $Qt6Path" -ForegroundColor DarkCyan
            break
        }
    }
    if (-not $Qt6Path) {
        Warn "Qt6 が見つかりません。-Qt6Path で指定するか Qt をインストールしてください。Qt6 のビルドはスキップします。"
    }
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
            Clear-StaleCmakeCache "$exRoot\frontend_qt\build" "$exRoot\frontend_qt"
            $qtArgs = Get-CmakeConfigArgs "$exRoot\frontend_qt\build" $Qt6Path
            cmake -S "$exRoot\frontend_qt" -B "$exRoot\frontend_qt\build" -DCMAKE_BUILD_TYPE=$BuildType @qtArgs
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
