@REM Orca-Flashforge build script for Windows
@REM
@REM Usage:
@REM   build_release_vs2022.bat
@REM       Configure and build deps, then configure, build, and install the slicer in Release mode.
@REM
@REM   build_release_vs2022.bat slicer
@REM       Skip deps. Configure, build, and install the slicer in Release mode.
@REM
@REM   build_release_vs2022.bat slicer buildonly
@REM       Skip deps and skip CMake configure. Build and install the existing Release build directory.
@REM       Aliases: compile, noconfigure
@REM
@REM   build_release_vs2022.bat slicer debuginfo buildonly
@REM       Build and install the existing RelWithDebInfo build directory.
@REM
@REM   build_release_vs2022.bat slicer debug buildonly
@REM       Build and install the existing Debug build directory.
@REM
@REM Build type options:
@REM   debug      -> build-dbg, Debug
@REM   debuginfo  -> build-dbginfo, RelWithDebInfo
@REM   default    -> build, Release
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

set debug=OFF
set debuginfo=OFF
set build_only=OFF
for %%a in (%*) do (
    if /I "%%~a"=="debug" set debug=ON
    if /I "%%~a"=="debuginfo" set debuginfo=ON
    if /I "%%~a"=="buildonly" set build_only=ON
    if /I "%%~a"=="compile" set build_only=ON
    if /I "%%~a"=="noconfigure" set build_only=ON
)
if "%debug%"=="ON" (
    set build_type=Debug
    set build_dir=build-dbg
) else (
    if "%debuginfo%"=="ON" (
        set build_type=RelWithDebInfo
        set build_dir=build-dbginfo
    ) else (
        set build_type=Release
        set build_dir=build
    )
)
echo build type set to %build_type%

setlocal DISABLEDELAYEDEXPANSION 
cd deps
mkdir %build_dir%
cd %build_dir%
set "SIG_FLAG="
if defined ORCA_UPDATER_SIG_KEY set "SIG_FLAG=-DORCA_UPDATER_SIG_KEY=%ORCA_UPDATER_SIG_KEY%"

if "%1"=="slicer" (
    GOTO :slicer
)
echo "building deps.."

echo on
REM Set minimum CMake policy to avoid <3.5 errors
set CMAKE_POLICY_VERSION_MINIMUM=3.5
if not "%build_only%"=="ON" cmake ../ -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%build_type%
cmake --build . --config %build_type% --target deps -- -m
@echo off

if "%1"=="deps" exit /b 0

:slicer
echo "building Orca-Flashforge..."
cd %WP%
mkdir %build_dir%
cd %build_dir%

echo on
set CMAKE_POLICY_VERSION_MINIMUM=3.5
if not "%build_only%"=="ON" cmake .. -G "Visual Studio 17 2022" -A x64 -DORCA_TOOLS=ON %SIG_FLAG% -DCMAKE_BUILD_TYPE=%build_type%
cmake --build . --config %build_type% --target ALL_BUILD -- -m
@echo off
cd ..
call scripts/run_gettext.bat
cd %build_dir%
set "INSTALL_I18N_DIR=%CD%\Orca-Flashforge\resources\i18n"
if exist "%INSTALL_I18N_DIR%" (
    for /d %%D in ("%INSTALL_I18N_DIR%\OrcaSlicer_*") do (
        if exist "%%~fD\" (
            echo Removing stale i18n directory "%%~fD"
            rmdir /s /q "%%~fD"
        )
    )
)
cmake --build . --target install --config %build_type%
