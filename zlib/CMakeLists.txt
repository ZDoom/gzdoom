cmake_minimum_required( VERSION 2.4 )

if( CMAKE_COMPILER_IS_GNUC )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fomit-frame-pointer" )
endif( CMAKE_COMPILER_IS_GNUC )

add_library( z
    adler32.c
    compress.c
    crc32.c
    gzio.c
    uncompr.c
    deflate.c
    trees.c
    zutil.c
    inflate.c
    infback.c
    inftrees.c
    inffast.c )
target_link_libraries( z )
