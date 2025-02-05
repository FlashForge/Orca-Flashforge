#!/bin/bash

set -e
set -o pipefail

while getopts ":dpa:snt:xbc:h" opt; do
  case "${opt}" in
    d )
        export BUILD_TARGET="deps"
        ;;
    p )
        export PACK_DEPS="1"
        ;;
    a )
        export ARCH="$OPTARG"
        ;;
    s )
        export BUILD_TARGET="slicer"
        ;;
    n )
        export NIGHTLY_BUILD="1"
        ;;
    t )
        export OSX_DEPLOYMENT_TARGET="$OPTARG"
        ;;
    x )
        export SLICER_CMAKE_GENERATOR="Ninja"
        export SLICER_BUILD_TARGET="all"
        export DEPS_CMAKE_GENERATOR="Ninja"
        ;;
    b )
        export BUILD_ONLY="1"
        ;;
    c )
        export BUILD_CONFIG="$OPTARG"
        ;;
    1 )
        export CMAKE_BUILD_PARALLEL_LEVEL=1
        ;;
    h ) echo "Usage: ./build_release_macos.sh [-d]"
        echo "   -d: Build deps only"
        echo "   -a: Set ARCHITECTURE (arm64 or x86_64)"
        echo "   -s: Build slicer only"
        echo "   -n: Nightly build"
        echo "   -t: Specify minimum version of the target platform, default is 11.3"
        echo "   -x: Use Ninja CMake generator, default is Xcode"
        echo "   -b: Build without reconfiguring CMake"
        echo "   -c: Set CMake build configuration, default is Release"
        echo "   -1: Use single job for building"
        exit 0
        ;;
    * )
        ;;
  esac
done

# Set defaults

if [ -z "$ARCH" ]; then
  ARCH="$(uname -m)"
  export ARCH
fi

if [ -z "$BUILD_CONFIG" ]; then
  export BUILD_CONFIG="Release"
fi

if [ -z "$BUILD_TARGET" ]; then
  export BUILD_TARGET="all"
fi

WD="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd $WD/deps
mkdir -p build_$ARCH
cd build_$ARCH
DEPS=$PWD/Orca-Flashforge_dep_$ARCH
mkdir -p $DEPS
if [ "slicer." != $BUILD_TARGET. ]; 
then
    echo "building deps..."
    echo "cmake ../ -DDESTDIR=$DEPS -DOPENSSL_ARCH=darwin64-${ARCH}-cc -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES:STRING=${ARCH}"
    cmake ../ -DDESTDIR="$DEPS" -DOPENSSL_ARCH="darwin64-${ARCH}-cc" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES:STRING=${ARCH} -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3
    cmake --build . --config Release --target deps 
    if [ "1." == "$PACK_DEPS". ];
    then
        tar -zcvf Orca-Flashforge_dep_mac_${ARCH}_$(date +"%Y%m%d").tar.gz Orca-Flashforge_dep_$ARCH
    fi
fi

if [ -z "$SLICER_BUILD_TARGET" ]; then
  export SLICER_BUILD_TARGET="ALL_BUILD"
fi

if [ -z "$DEPS_CMAKE_GENERATOR" ]; then
  export DEPS_CMAKE_GENERATOR="Unix Makefiles"
fi

cd $WD
mkdir -p build_$ARCH
cd build_$ARCH
echo "building slicer..."
cmake .. -GXcode -DBBL_RELEASE_TO_PUBLIC=1 -DCMAKE_PREFIX_PATH="$DEPS/usr/local" -DCMAKE_INSTALL_PREFIX="$PWD/Orca-Flashforge" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MACOSX_RPATH=ON -DCMAKE_INSTALL_RPATH="$DEPS/usr/local" -DCMAKE_MACOSX_BUNDLE=ON -DCMAKE_OSX_ARCHITECTURES=${ARCH} -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3
cmake --build . --config Release --target ALL_BUILD 
cd ..
./run_gettext.sh
cd build_$ARCH
mkdir -p Orca-Flashforge
cd Orca-Flashforge
rm -r ./Orca-Flashforge.app
cp -pR ../src/Release/Orca-Flashforge.app ./Orca-Flashforge.app
resources_path=$(readlink ./Orca-Flashforge.app/Contents/Resources)
rm ./Orca-Flashforge.app/Contents/Resources
cp -R $resources_path ./Orca-Flashforge.app/Contents/Resources
# delete .DS_Store file
find ./Orca-Flashforge.app/ -name '.DS_Store' -delete
# extract version
# export ver=$(grep '^#define SoftFever_VERSION' ../src/libslic3r/libslic3r_version.h | cut -d ' ' -f3)
# ver="_V${ver//\"}"
# echo $PWD
# if [ "1." != "$NIGHTLY_BUILD". ];
# then
#     ver=${ver}_dev
# fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_BUILD_DIR="$PROJECT_DIR/build_$ARCH"
DEPS_DIR="$PROJECT_DIR/deps"
DEPS_BUILD_DIR="$DEPS_DIR/build_$ARCH"
DEPS="$DEPS_BUILD_DIR/OrcaSlicer_dep_$ARCH"

# zip -FSr Orca-Flashforge${ver}_Mac_${ARCH}.zip Orca-Flashforge.app
