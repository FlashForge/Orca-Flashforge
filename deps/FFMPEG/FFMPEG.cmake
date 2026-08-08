set(_conf_cmd ./configure)

if (MSVC)
    set(_dstdir ${DESTDIR})
    set(_source_dir "${CMAKE_BINARY_DIR}/dep_FFMPEG-prefix/src/dep_FFMPEG")
    ExternalProject_Add(dep_FFMPEG
        # URL https://github.com/bambulab/ffmpeg_prebuilts/releases/download/7.0.2/7.0.2_msvc.zip
        # URL_HASH SHA256=DF44AE6B97CE84C720695AE7F151B4A9654915D1841C68F10D62A1189E0E7181
        # 这个预编译包名字里就带 rtmp，本身已支持 RTMP，无需改动
        # URL "https://app-zh-1349496149.cos.ap-guangzhou.myqcloud.com/libs/ffmpeg/ffmpeg-8.0.1-full_build-shared.zip"
        URL "${CMAKE_CURRENT_LIST_DIR}/ffmpeg_min.zip"
        # URL_HASH SHA256=77B4F5D73FEA9C4A5BF15FC055FF12491EDE7C2764D40E92AB56D6C5ED11E1CE

        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/FFMPEG
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND
            # COMMAND ${CMAKE_COMMAND} -E make_directory "${_dstdir}/bin"
            # COMMAND ${CMAKE_COMMAND} -E make_directory "${_dstdir}/lib"
            # COMMAND ${CMAKE_COMMAND} -E make_directory "${_dstdir}/include"
            COMMAND ${CMAKE_COMMAND} -E copy_directory  "${_source_dir}/bin" "${_dstdir}/bin"
            COMMAND ${CMAKE_COMMAND} -E copy_directory  "${_source_dir}/lib" "${_dstdir}/lib"
            COMMAND ${CMAKE_COMMAND} -E copy_directory  "${_source_dir}/include" "${_dstdir}/include"
    )

else ()
    # DESTDIR already points at the final prefix (normally .../usr/local).
    # Keep these as a CMake list so every configure option is actually passed;
    # the previous concatenated _extra_cmd string was never used.
    set(_ffmpeg_extra_libs "-lpthread -lm")
    if (NOT APPLE)
        string(APPEND _ffmpeg_extra_libs " -ldl")
    endif()
    set(_extra_cmd
        "--pkg-config-flags=--static"
        "--extra-cflags=-I${DESTDIR}/include"
        "--extra-ldflags=-L${DESTDIR}/lib"
        "--extra-libs=${_ffmpeg_extra_libs}"
        "--bindir=${DESTDIR}/bin"
        --disable-autodetect
    )
    set(_ffmpeg_depends_args)
    set(_pkg_config_cmd)

    if (APPLE)
        # Keep the macOS application bundle self-contained without a separate
        # dylib collection/install-name pass. This matches the rest of the
        # dependency bundle and the upstream macOS packaging flow.
        list(APPEND _extra_cmd
            --enable-securetransport
            --disable-openssl
            --disable-videotoolbox
            --disable-audiotoolbox
            --disable-bzlib
            --disable-lzma
            --disable-iconv
        )
        set(_linkage_cmd --disable-shared --enable-static)
        set(_minos_cmd
            "CFLAGS=-mmacosx-version-min=${DEP_OSX_TARGET}"
            "LDFLAGS=-mmacosx-version-min=${DEP_OSX_TARGET}"
            )
        if (IS_CROSS_COMPILE)
            set(_cross_cmd --enable-cross-compile)
            set(_pic_cmd --enable-pic)
            if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
                set(_arch_cmd --arch=arm64)
                set(_cc_cmd "--cc=clang -arch arm64")
            else()
                set(_arch_cmd --arch=x86_64)
                set(_cc_cmd "--cc=clang -arch x86_64")
            endif()
        endif()
    else()
        # Linux packages do not collect FFmpeg shared objects. Build position-
        # independent archives so the application remains self-contained.
        list(APPEND _extra_cmd
            --enable-openssl
            --enable-pthreads
        )
        set(_pkg_config_cmd
            "PKG_CONFIG_PATH=${DESTDIR}/lib/pkgconfig:${DESTDIR}/share/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        if (OPENSSL_PKG)
            set(_ffmpeg_depends_args DEPENDS ${OPENSSL_PKG})
        endif()
        set(_linkage_cmd --disable-shared --enable-static)
        set(_pic_cmd --enable-pic)
    endif()

    set(_build_j -j)
    if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
        set(_build_j "-j$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
    endif()

    ExternalProject_Add(dep_FFMPEG
        URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n8.0.1.tar.gz
        # URL_HASH SHA256=5EB46D18D664A0CCADF7B0ADEE03BD3B7FA72893D667F36C69E202A807E6D533
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/FFMPEG
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${_minos_cmd} ${_pkg_config_cmd} ${_conf_cmd}
            ${_extra_cmd}
            ${_cross_cmd}
            ${_pic_cmd}
            ${_arch_cmd}
            ${_cc_cmd}
            --prefix="${DESTDIR}"
            ${_linkage_cmd}
            --disable-doc
            --enable-small
            --disable-outdevs
            # The RTMP control does not use libavfilter. Keeping it disabled
            # avoids pulling optional/GPL filter dependencies into releases.
            --disable-avfilter
            --disable-protocols
            # tcp → 基础网络; rtmp → RTMP 推拉流; http/https → HLS/HLS 播放; 补充 tls, httpproxy
            --enable-protocol=file,fd,pipe,rtp,udp,tcp,rtmp,http,https,tls,httpproxy,crypto,data
            --disable-muxers
            # flv → RTMP 推流; hls → HLS 输出
            --enable-muxer=rtp,flv
            --disable-encoders
            --disable-decoders
            # 补充 hevc* 解码器
            --enable-decoder=*aac*,h264*,hevc*,mp3*,mjpeg,rv*
            --disable-demuxers
            # flv → RTMP 拉流; hls + mpegts → HLS (.m3u8) 播放;
            # mpjpeg → LAN 摄像头的 multipart/x-mixed-replace MJPEG 流
            # 补充 hevc, rtp, rtsp, sdp 解复用器
            --enable-demuxer=h264,hevc,mp3,mov,flv,hls,mpegts,mpjpeg,rtp,rtsp,sdp
            --disable-zlib
            --disable-avdevice
        ${_ffmpeg_depends_args}
        BUILD_IN_SOURCE ON
        BUILD_COMMAND make ${_build_j}
        INSTALL_COMMAND make install
    )

endif()
