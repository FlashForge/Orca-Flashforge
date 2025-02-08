set WP=%CD%

set debug=OFF
set debuginfo=OFF
if "%1"=="debug" set debug=ON
if "%2"=="debug" set debug=ON
if "%1"=="debuginfo" set debuginfo=ON
if "%2"=="debuginfo" set debuginfo=ON
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

cd deps
mkdir %build_dir%
cd %build_dir%
set DEPS=%CD%/Orca-Flashforge_dep
if "%1"=="slicer" (
    GOTO :slicer
)
echo "building deps.."
cmake ../ -G "Visual Studio 16 2019" -DDESTDIR="%CD%/Orca-Flashforge_dep" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target deps -- -m

echo cmake ../ -G "Visual Studio 16 2019" -A x64 -DDESTDIR="%CD%/OrcaSlicer_dep" -DCMAKE_BUILD_TYPE=%build_type% -DDEP_DEBUG=%debug% -DORCA_INCLUDE_DEBUG_INFO=%debuginfo%
cmake ../ -G "Visual Studio 16 2019" -A x64 -DDESTDIR="%CD%/OrcaSlicer_dep" -DCMAKE_BUILD_TYPE=%build_type% -DDEP_DEBUG=%debug% -DORCA_INCLUDE_DEBUG_INFO=%debuginfo%
cmake --build . --config %build_type% --target deps -- -m

if "%1"=="deps" exit /b 0

:slicer
echo "building Flash Slicer..."
cd %WP%
mkdir %build_dir%
cd %build_dir%

cmake .. -G "Visual Studio 16 2019" -DBBL_RELEASE_TO_PUBLIC=1 -DCMAKE_PREFIX_PATH="%DEPS%/usr/local" -DCMAKE_INSTALL_PREFIX="./Orca-Flashforge" -DCMAKE_BUILD_TYPE=Release -DWIN10SDK_PATH="C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0"
cmake --build . --config Release --target ALL_BUILD -- -m
cd ..
call run_gettext.bat
cd %build_dir%
cmake --build . --target install --config %build_type%
