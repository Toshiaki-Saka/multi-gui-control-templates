@echo off
REM build_all.bat — build the C++ core, smoke test, and Qt6 frontend (Windows / MSVC).
REM Requirements: CMake on PATH and a Visual Studio C++ toolchain.
REM For the Qt6 frontend, set CMAKE_PREFIX_PATH to your Qt 6 installation:
REM   set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
setlocal enableextensions enabledelayedexpansion

set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%

echo ==^> Building C++ core
if not exist "%ROOT%\core\build" mkdir "%ROOT%\core\build"
cmake -S "%ROOT%\core" -B "%ROOT%\core\build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (echo CMake configure failed & exit /b 1)
cmake --build "%ROOT%\core\build" --config Release
if errorlevel 1 (echo CMake build failed & exit /b 1)

echo ==^> Running C++ smoke test
cmake --build "%ROOT%\core\build" --target msd_core_smoke --config Release
if errorlevel 1 (echo Smoke test build failed & exit /b 1)
"%ROOT%\core\build\Release\msd_core_smoke.exe"
if errorlevel 1 (echo Smoke test FAILED & exit /b 1)

echo.
echo ==^> Building Qt6 frontend
if not exist "%ROOT%\frontend_qt\build" mkdir "%ROOT%\frontend_qt\build"
cmake -S "%ROOT%\frontend_qt" -B "%ROOT%\frontend_qt\build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo    ^(Qt6 build skipped — CMake could not find Qt6.^)
    echo    Set CMAKE_PREFIX_PATH to your Qt 6 installation, e.g.:
    echo      set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
    goto :pyhint
)
cmake --build "%ROOT%\frontend_qt\build" --config Release
if errorlevel 1 (echo Qt6 build failed & goto :pyhint)
echo    -^> %ROOT%\frontend_qt\build\Release\msd_qt.exe

:pyhint
echo.
echo ==^> Python frontends:
echo     pip install -r %ROOT%\frontend_python\requirements.txt
echo     python %ROOT%\frontend_python\app_pyside6.py      (GUI)
echo     python %ROOT%\frontend_python\app_matplotlib.py   (CLI)
echo.
echo ==^> Avalonia frontend (.NET 8 required):
echo     cd %ROOT%\frontend_avalonia\MsdAvalonia
echo     dotnet run -c Release

endlocal
