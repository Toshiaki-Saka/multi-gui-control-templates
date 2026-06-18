# build_and_run.ps1
# C++ コア (track_core.dll)、Qt6 フロントエンド、C# Avalonia をビルドし、
# 指定したフロントエンドを起動する。
#
# 使い方:
#   .\build_and_run.ps1 1        # Qt6 ビルド + 起動
#   .\build_and_run.ps1 2        # Avalonia ビルド + 起動
#   .\build_and_run.ps1 3        # Python ビルド + 起動
#   .\build_and_run.ps1 1 -SkipBuild          # Qt6 のみ起動 (ビルドスキップ)
#   .\build_and_run.ps1 -Qt6Path "C:\Qt\6.8.0\msvc2022_64"
#   .\build_and_run.ps1 -BuildType Debug
#
param(
    # 1=Qt6, 2=Avalonia, 3=Python, 0(省略)=全て
    [int]$Target        = 0,

    [string]$BuildType  = "Release",

    # Qt6 のインストールパス (省略時は自動検出)
    # 例: "C:\Qt\6.8.0\msvc2022_64"  または  "C:\vcpkg\installed\x64-windows"
    [string]$Qt6Path    = "",

    [switch]$SkipBuild,    # ビルド全体をスキップ
    [switch]$SkipPyDeps    # pip install をスキップ
)

$ErrorActionPreference = "Stop"
$ROOT = $PSScriptRoot

# ── Target 番号を起動フラグに変換 ──────────────────────────────────────────
switch ($Target) {
    1 { $LaunchQt = $true;     $LaunchCSharp = $false; $LaunchPython = $false }
    2 { $LaunchQt = $false;    $LaunchCSharp = $true;  $LaunchPython = $false }
    3 { $LaunchQt = $false;    $LaunchCSharp = $false; $LaunchPython = $true  }
    default {
        if ($Target -ne 0) {
            Write-Host "ERROR: Target は 1(Qt6), 2(Avalonia), 3(Python) で指定してください。" -ForegroundColor Red
            exit 1
        }
        $LaunchQt = $true; $LaunchCSharp = $true; $LaunchPython = $true
    }
}

# ── ヘルパー ─────────────────────────────────────────────────────────────────
function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "    OK : $msg" -ForegroundColor Green }
function Warn($msg) { Write-Host "    WARN: $msg" -ForegroundColor Yellow }
function Die($msg)  { Write-Host "`nERROR: $msg" -ForegroundColor Red; exit 1 }

function Require($cmd) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Die "'$cmd' が見つかりません。インストールして PATH を通してください。"
    }
}

# ── Qt6 パスの自動検出 ────────────────────────────────────────────────────────
if (-not $Qt6Path) {
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
        Warn "Qt6 が見つかりません。-Qt6Path で指定するか Qt をインストールしてください。Qt6 フロントエンドのビルドはスキップします。"
    }
}

# ── Qt6 vcpkg サードパーティ DLL リスト ─────────────────────────────────────
# windeployqt は Qt 本体の DLL を配置するが vcpkg 製 DLL を配置しない。
# CLAUDE_qt6_troubleshooting1.md に記載の既知の必要 DLL を手動コピーする。
$qtThirdPartyDlls = @(
    "double-conversion.dll",
    "pcre2-16.dll",
    "z.dll",
    "zstd.dll",
    "harfbuzz.dll",
    "freetype.dll",
    "libpng16.dll",
    "bz2.dll",
    "md4c.dll",
    "brotlidec.dll",
    "brotlicommon.dll",
    "jpeg62.dll",
    "libcrypto-3-x64.dll"
)

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

# ════════════════════════════════════════════════════════════════════════════
#  ビルド
# ════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuild) {

    Require "cmake"

    # ── 1. C++ コアライブラリ (track_core.dll) ───────────────────────────
    Step "C++ core をビルド中 ($BuildType)"
    Clear-StaleCmakeCache "$ROOT\core\build" "$ROOT\core"
    $coreArgs = Get-CmakeConfigArgs "$ROOT\core\build" ""
    cmake -S "$ROOT\core" -B "$ROOT\core\build" `
          -DCMAKE_BUILD_TYPE=$BuildType @coreArgs
    if ($LASTEXITCODE -ne 0) { Die "core cmake configure 失敗" }
    cmake --build "$ROOT\core\build" --config $BuildType -j
    if ($LASTEXITCODE -ne 0) { Die "core cmake build 失敗" }

    $coreDllCandidates = @(
        "$ROOT\core\build\$BuildType\track_core.dll",  # MSVC multi-config
        "$ROOT\core\build\track_core.dll"              # Ninja / MinGW
    )
    $coreDll = $coreDllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $coreDll) { Die "track_core.dll が見つかりません" }
    Ok "core ビルド完了 → $coreDll"

    # csproj は ../../core/build/track_core.dll を参照するので直下にもコピー
    $coreFlat = "$ROOT\core\build\track_core.dll"
    if ($coreDll -ne $coreFlat) {
        Copy-Item $coreDll $coreFlat -Force
        Ok "track_core.dll → $coreFlat (csproj 用)"
    }

    # ── 2. Qt6 フロントエンド ────────────────────────────────────────────
    if ($LaunchQt -and $Qt6Path) {
        Step "Qt6 フロントエンドをビルド中 ($BuildType)"
        Clear-StaleCmakeCache "$ROOT\frontend_qt\build" "$ROOT\frontend_qt"
        $qtArgs = Get-CmakeConfigArgs "$ROOT\frontend_qt\build" $Qt6Path
        cmake -S "$ROOT\frontend_qt" -B "$ROOT\frontend_qt\build" `
              -DCMAKE_BUILD_TYPE=$BuildType @qtArgs
        if ($LASTEXITCODE -ne 0) { Die "Qt6 cmake configure 失敗" }
        cmake --build "$ROOT\frontend_qt\build" --config $BuildType -j
        if ($LASTEXITCODE -ne 0) { Die "Qt6 cmake build 失敗" }

        $qtExeCandidates = @(
            "$ROOT\frontend_qt\build\$BuildType\track_qt.exe",
            "$ROOT\frontend_qt\build\track_qt.exe"
        )
        $qtExe = $qtExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($qtExe -and $coreDll) {
            $qtDir = Split-Path $qtExe

            # track_core.dll を Qt 実行ファイルと同じディレクトリにコピー
            Copy-Item $coreDll $qtDir -Force
            Ok "track_core.dll → $qtDir"

            # windeployqt で Qt 本体 DLL を配置
            $windeployqt = $null
            foreach ($p in @(
                "$Qt6Path\tools\Qt6\bin\windeployqt.exe",
                "$Qt6Path\bin\windeployqt.exe"
            )) {
                if (Test-Path $p) { $windeployqt = $p; break }
            }
            if ($windeployqt) {
                & $windeployqt --no-translations --no-system-d3d-compiler `
                               --no-opengl-sw --no-compiler-runtime "$qtExe"
                Ok "windeployqt 完了"
            } else {
                Warn "windeployqt が見つかりません。Qt DLL が不足する場合は手動でコピーしてください。"
            }

            # vcpkg サードパーティ DLL を補完コピー
            $vcpkgBin = "$Qt6Path\bin"
            $copiedCount = 0
            foreach ($dll in $qtThirdPartyDlls) {
                $src = "$vcpkgBin\$dll"
                if (Test-Path $src) {
                    Copy-Item $src $qtDir -Force
                    $copiedCount++
                }
            }
            if ($copiedCount -gt 0) {
                Ok "vcpkg サードパーティ DLL $copiedCount 個をコピーしました"
            }
        }
        Ok "Qt6 ビルド完了"
    } elseif ($LaunchQt) {
        Warn "Qt6 フロントエンドのビルドをスキップ (Qt6Path 未設定)"
    }

    # ── 3. C# / Avalonia ────────────────────────────────────────────────
    if ($LaunchCSharp) {
        Require "dotnet"
        $csprojDir = "$ROOT\frontend_avalonia\TrackAvalonia"
        Step "C# Avalonia をビルド中 ($BuildType)"
        dotnet restore "$csprojDir"
        if ($LASTEXITCODE -ne 0) { Die "dotnet restore 失敗" }
        dotnet build "$csprojDir" -c $BuildType --no-restore
        if ($LASTEXITCODE -ne 0) { Die "dotnet build 失敗" }

        # track_core.dll が出力ディレクトリにコピーされているか確認 (csproj 条件の保険)
        $avaloniaOut = "$csprojDir\bin\$BuildType\net8.0"
        if ($coreDll -and -not (Test-Path "$avaloniaOut\track_core.dll")) {
            Copy-Item $coreDll "$avaloniaOut\track_core.dll" -Force
            Ok "track_core.dll → $avaloniaOut (手動コピー)"
        }
        Ok "Avalonia ビルド完了"
    }

    # ── 4. Python 依存関係 ───────────────────────────────────────────────
    if ($LaunchPython -and -not $SkipPyDeps) {
        Require "python"
        Step "Python 依存関係をインストール中"
        python -m pip install -r "$ROOT\frontend_python\requirements.txt"
        if ($LASTEXITCODE -ne 0) { Die "pip install 失敗" }
        Ok "pip install 完了"
    }

} # if (-not $SkipBuild)

# ════════════════════════════════════════════════════════════════════════════
#  track_core.dll パスを確定 (SkipBuild 時も使用)
# ════════════════════════════════════════════════════════════════════════════
$coreDllForLaunch = @(
    "$ROOT\core\build\$BuildType\track_core.dll",
    "$ROOT\core\build\track_core.dll"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

# ════════════════════════════════════════════════════════════════════════════
#  起動
# ════════════════════════════════════════════════════════════════════════════
Step "フロントエンドを起動します"

# ── Qt6 ──────────────────────────────────────────────────────────────────
if ($LaunchQt) {
    $qtExe = @(
        "$ROOT\frontend_qt\build\$BuildType\track_qt.exe",
        "$ROOT\frontend_qt\build\track_qt.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ($qtExe) {
        Start-Process $qtExe -WorkingDirectory (Split-Path $qtExe)
        Ok "Qt6 起動 : $qtExe"
    } else {
        Warn "Qt6 実行ファイルが見つかりません (先にビルドしてください)"
    }
}

# ── C# / Avalonia ────────────────────────────────────────────────────────
if ($LaunchCSharp) {
    $csprojDir = "$ROOT\frontend_avalonia\TrackAvalonia"
    $avaloniaExe = @(
        "$csprojDir\bin\$BuildType\net8.0\TrackAvalonia.exe",
        "$csprojDir\bin\$BuildType\net8.0\win-x64\TrackAvalonia.exe"
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

# ── Python / PySide6 ─────────────────────────────────────────────────────
if ($LaunchPython) {
    $pyScript = "$ROOT\frontend_python\app_pyside6.py"
    if (Test-Path $pyScript) {
        if ($coreDllForLaunch) { $env:TRACK_CORE_LIB = $coreDllForLaunch }
        Start-Process "python" -ArgumentList "`"$pyScript`"" `
                      -WorkingDirectory "$ROOT\frontend_python"
        Ok "Python (PySide6) 起動 : $pyScript"
        if ($coreDllForLaunch) { Ok "  TRACK_CORE_LIB=$coreDllForLaunch" }
    } else {
        Warn "Python スクリプトが見つかりません : $pyScript"
    }
}

$launched = @(
    if ($LaunchQt)     { "Qt6" }
    if ($LaunchCSharp) { "Avalonia" }
    if ($LaunchPython) { "Python" }
) -join ", "
Write-Host "`n[$launched] を起動しました。" -ForegroundColor Green
