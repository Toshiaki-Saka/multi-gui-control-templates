#!/usr/bin/env bash
# build_all.sh — build the C++ core, smoke test, and optional frontends.
# For Windows use build_and_run.ps1 or build_all.bat.
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"

echo "==> Building C++ core"
cmake -S "$ROOT/core" -B "$ROOT/core/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/core/build" -j

echo "==> Running C++ smoke test"
cmake --build "$ROOT/core/build" --target msd_core_smoke -j
"$ROOT/core/build/msd_core_smoke"

echo "==> Building Qt6 frontend"
if cmake -S "$ROOT/frontend_qt" -B "$ROOT/frontend_qt/build" \
         -DCMAKE_BUILD_TYPE=Release 2>/dev/null; then
    cmake --build "$ROOT/frontend_qt/build" -j
    echo "    -> $ROOT/frontend_qt/build/msd_qt"
else
    echo "    (skipped — Qt6 not found; install Qt6 and set CMAKE_PREFIX_PATH)"
fi

echo
echo "==> Python frontends: ready."
echo "    Install dependencies:"
echo "      pip install -r $ROOT/frontend_python/requirements.txt"
echo "    Run:"
echo "      python $ROOT/frontend_python/app_pyside6.py      # GUI"
echo "      python $ROOT/frontend_python/app_matplotlib.py   # CLI"
echo
echo "==> Avalonia frontend (.NET 8 required):"
echo "      cd $ROOT/frontend_avalonia/MsdAvalonia"
echo "      dotnet run -c Release"
