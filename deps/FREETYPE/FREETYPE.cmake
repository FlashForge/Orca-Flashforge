if(WIN32)
    set(library_build_shared "1")
else()
    set(library_build_shared "0")
endif()

if(WIN32)
    set(_ft_zlib_args
        -DFT_DISABLE_ZLIB=TRUE
    )
    set(_ft_zlib_depends "")
else()
    # Keep FreeType and the rest of the application on the same zlib. In a
    # static macOS build, FreeType's bundled copy may otherwise satisfy only
    # part of Assimp's zlib symbols and make compressed FBX data unreadable.
    set(_ft_zlib_args
        -DFT_DISABLE_ZLIB=FALSE
        -DFT_REQUIRE_ZLIB=TRUE
    )
    set(_ft_zlib_depends ${ZLIB_PKG})
endif()

orcaslicer_add_cmake_project(FREETYPE
    URL https://github.com/SoftFever/orca_deps/releases/download/freetype-2.12.1.tar.gz/freetype-2.12.1.tar.gz
    URL_HASH SHA256=efe71fd4b8246f1b0b1b9bfca13cfff1c9ad85930340c27df469733bbb620938
    DEPENDS ${_ft_zlib_depends}
    #"${_patch_step}"
    CMAKE_ARGS
	-DBUILD_SHARED_LIBS=${library_build_shared}
	${_ft_zlib_args}
        -DFT_DISABLE_BZIP2=TRUE
        -DFT_DISABLE_PNG=TRUE
        -DFT_DISABLE_HARFBUZZ=TRUE
        -DFT_DISABLE_BROTLI=TRUE
)

if(MSVC)
    add_debug_dep(dep_FREETYPE)
endif()
