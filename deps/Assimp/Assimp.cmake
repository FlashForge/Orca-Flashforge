if(CMAKE_VERSION VERSION_LESS 3.22)
    set(ASSIMP_VERSION 5.3.1)
    set(ASSIMP_URL https://github.com/assimp/assimp/archive/refs/tags/v${ASSIMP_VERSION}.tar.gz)
    set(ASSIMP_HASH SHA256=a07666be71afe1ad4bc008c2336b7c688aca391271188eb9108d0c6db1be53f1)
else()
    set(ASSIMP_VERSION 5.4.3)
    set(ASSIMP_URL https://github.com/assimp/assimp/archive/refs/tags/v${ASSIMP_VERSION}.tar.gz)
    set(ASSIMP_HASH SHA256=66dfbaee288f2bc43172440a55d0235dfc7bf885dda6435c038e8000e79582cb)
endif()

if(APPLE)
    set(ASSIMP_BUILD_ZLIB OFF)
else()
    set(ASSIMP_BUILD_ZLIB ON)
endif()

orcaslicer_add_cmake_project(Assimp
    URL ${ASSIMP_URL}
    URL_HASH ${ASSIMP_HASH}
    DEPENDS ${ZLIB_PKG}
    CMAKE_ARGS
        -DASSIMP_BUILD_TESTS=OFF
        -DASSIMP_BUILD_SAMPLES=OFF
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
        -DASSIMP_INSTALL_PDB=OFF
        -DASSIMP_INSTALL=ON
        -DASSIMP_NO_EXPORT=ON
        -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF
        -DASSIMP_BUILD_FBX_IMPORTER=ON
        -DASSIMP_BUILD_ZLIB=${ASSIMP_BUILD_ZLIB}
        -DASSIMP_WARNINGS_AS_ERRORS=OFF
        -DBUILD_WITH_STATIC_CRT=OFF
)

if(MSVC)
    add_debug_dep(dep_Assimp)
endif()
