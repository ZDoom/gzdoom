make_release_only()

# Only dav1d used.
# Also without the encoder file write.c.
add_library(avif STATIC alpha.c
    avif.c
    colr.c
    codec_dav1d.c
    diag.c
    exif.c
    io.c
    mem.c
    obu.c
    rawdata.c
    read.c
    reformat.c
    reformat_libsharpyuv.c
    reformat_libyuv.c
    scale.c
    stream.c
    utils.c)

if(UNIX)
    # Find out if we have threading available
    set(CMAKE_THREAD_PREFER_PTHREADS ON)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)
    target_link_libraries(avif PRIVATE m Threads::Threads)
endif()

target_compile_definitions(avif PRIVATE -DAVIF_CODEC_DAV1D=1 -DAVIF_LIBYUV_ENABLED=1)
if (MSVC)
    target_compile_options(avif PRIVATE /utf-8)
endif()
target_link_libraries(avif PRIVATE PkgConfig::dav1d)
if (NOT APPLE OR (APPLE AND VCPKG_TOOLCHAIN))
    if (TARGET yuv)
        target_link_libraries(avif PRIVATE yuv)
    else()
        target_link_libraries(avif PRIVATE ${LIBYUV_LIBRARIES})
        target_include_directories(avif PRIVATE ${LIBYUV_INCLUDE_DIR})
    endif()
endif()