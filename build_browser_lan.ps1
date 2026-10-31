param(
    [int]$Jobs = 8,
    [string]$BuildDirectory = 'build-browser-lan',
    [string]$DependencyDirectory = 'deps/build-browser-lan',
    [string]$OutputDirectory = 'dist/FlashStudio-BrowserLAN',
    [string]$OriginalApplicationDirectory = "$env:ProgramFiles/Flashforge/Orca-Flashforge",
    [switch]$SkipDependencies,
    [switch]$SkipHelper
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
foreach ($tool in @('cmake', 'ninja', 'cl', 'perl')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool is required. Run this script from an x64 C++ developer shell."
    }
}
function Run-BuildStep([scriptblock]$Step) {
    & $Step
    if ($LASTEXITCODE -ne 0) { throw "Build step failed with exit code $LASTEXITCODE" }
}
# Reject a mismatched installed SDK before spending time on the native build.
Run-BuildStep { python tools/validate_flashnetwork.py $OriginalApplicationDirectory }
$env:CMAKE_BUILD_PARALLEL_LEVEL = "$Jobs"
$env:PATH = "$PSScriptRoot/tools;$env:PATH"
if (-not $SkipHelper) {
    Run-BuildStep { python -m PyInstaller --noconfirm --onefile --console --name flashstudio-browser-auth --distpath resources/browser-auth --workpath "$BuildDirectory/broker" --specpath "$BuildDirectory" resources/browser-auth/broker.py }
}
if (-not $SkipDependencies) {
    Run-BuildStep { cmake -S deps -B $DependencyDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 }
    Run-BuildStep { cmake --build $DependencyDirectory --target deps --parallel 2 }
}
$prefix = (Resolve-Path "$DependencyDirectory/OrcaSlicer_dep/usr/local").Path
Run-BuildStep { cmake -S . -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$prefix" -DBBL_RELEASE_TO_PUBLIC=1 -DSLIC3R_MSVC_COMPILE_PARALLEL=OFF -DSLIC3R_MSVC_PDB=OFF -DORCA_TOOLS=OFF -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF }
Run-BuildStep { cmake --build $BuildDirectory --parallel $Jobs }
Run-BuildStep { cmake --build $BuildDirectory --target gettext_po_to_mo }
Run-BuildStep { python localization/tools/build_japanese.py --compile }
Run-BuildStep { cmake --install $BuildDirectory --prefix $OutputDirectory }
$destination = (Resolve-Path $OutputDirectory).Path
Get-ChildItem -LiteralPath "$prefix/bin" -Recurse -File -Filter '*.dll' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}
if (-not (Test-Path -LiteralPath "$destination/WebView2Loader.dll")) {
    throw 'WebView2Loader.dll is missing. Rebuild the wxWidgets dependency before packaging.'
}
New-Item -ItemType Directory -Path "$destination/resources/licenses/Microsoft.WebView2" -Force | Out-Null
Copy-Item -LiteralPath "$prefix/share/licenses/Microsoft.WebView2/LICENSE.txt" -Destination "$destination/resources/licenses/Microsoft.WebView2" -Force
Copy-Item -LiteralPath "$prefix/share/licenses/Microsoft.WebView2/NOTICE.txt" -Destination "$destination/resources/licenses/Microsoft.WebView2" -Force
$network = Join-Path $OriginalApplicationDirectory 'FlashNetwork.dll'
if (-not (Test-Path -LiteralPath $network)) {
    throw "Build completed, but the vendor FlashNetwork.dll is required for printer connectivity. Supply -OriginalApplicationDirectory."
}
Copy-Item -LiteralPath $network -Destination $destination -Force
Copy-Item -LiteralPath (Join-Path $OriginalApplicationDirectory 'resources/data/FLASHNETWORK9.DAT') -Destination "$destination/resources/data/FLASHNETWORK9.DAT" -Force
Run-BuildStep { python tools/validate_flashnetwork.py $destination }
Get-ChildItem -LiteralPath $OriginalApplicationDirectory -File | Where-Object Name -Match '^(msvcp|vcruntime|concrt).*\.dll$' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}
New-Item -ItemType Directory -Path "$destination/data_dir" -Force | Out-Null
Write-Output "Built application: $destination/flash studio.exe"
