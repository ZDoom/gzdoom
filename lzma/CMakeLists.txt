cmake_minimum_required( VERSION 2.4 )

if( CMAKE_COMPILER_IS_GNUC )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fomit-frame-pointer" )
endif( CMAKE_COMPILER_IS_GNUC )

set( LZMA_FILES
    C/Alloc.c
    C/LzFind.c
    C/LzFindMt.c
    C/LzmaDec.c
    C/LzmaEnc.c
    C/LzmaLib.c )

if( WIN32 )
	set( LZMA_FILES ${LZMA_FILES} C/Threads.c )
endif( WIN32 )

add_library( lzma ${LZMA_FILES} )
target_link_libraries( lzma )
