cmake_minimum_required( VERSION 2.4 )

if( CMAKE_COMPILER_IS_GNUC )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fomit-frame-pointer" )
endif( CMAKE_COMPILER_IS_GNUC )

add_definitions( -DBZ_NO_STDIO )
add_library( bz2
    blocksort.c
    bzlib.c
    compress.c
    crctable.c
    decompress.c
    huffman.c
    randtable.c )
target_link_libraries( bz2 )
