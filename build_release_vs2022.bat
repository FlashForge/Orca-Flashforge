@REM Orca-Flashforge build script for Windows
@echo off
set WP=%CD%

@REM Pack deps
if "%1"=="pack" (
    setlocal ENABLEDELAYEDEXPANSION 
    cd %WP%/deps/build
    for /f "tokens=2-4 delims=/ " %%a in ('date /t') do set build_date=%%c%%b%%a
    echo packing deps: Orca-Flashforge_dep_win64_!build_date!_vs2022.zip

    %WP%/tools/7z.exe a Orca-Flashforge_dep_win64_!build_date!_vs2022.zip Orca-Flashforge_dep
    exit /b 0
)

setlocal DISABLEDELAYEDEXPANSION 
cd deps
mkdir build
cd build
set DEPS=%CD%/Orca-Flashforge_dep

if "%1"=="slicer" (
    GOTO :slicer
)
echo "building deps.."


cmake ../ -G "Visual Studio 17 2022" -A x64 -DDESTDIR="%CD%/Orca-Flashforge_dep" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target deps -- -m

if "%1"=="deps" exit /b 0

:slicer
echo "building Flash Slicer..."
cd %WP%
mkdir build 
cd build

echo cmake .. -G "Visual Studio 17 2022" -A x64 -DBBL_RELEASE_TO_PUBLIC=1 -DCMAKE_PREFIX_PATH="%DEPS%/usr/local" -DCMAKE_INSTALL_PREFIX="./Orca-Flashforge" -DCMAKE_BUILD_TYPE=Release
cmake .. -G "Visual Studio 17 2022" -A x64 -DBBL_RELEASE_TO_PUBLIC=1 -DCMAKE_PREFIX_PATH="%DEPS%/usr/local" -DCMAKE_INSTALL_PREFIX="./Orca-Flashforge" -DCMAKE_BUILD_TYPE=Release -DWIN10SDK_PATH="C:/Program Files (x86)/Windows Kits/10/Include/10.0.22000.0"
cmake --build . --config Release --target ALL_BUILD -- -m
cd ..
call run_gettext.bat
cd build
cmake --build . --target install --config Release
