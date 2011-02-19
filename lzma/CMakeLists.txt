cmake_minimum_required( VERSION 2.4 )

if( CMAKE_COMPILER_IS_GNUC )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fomit-frame-pointer" )
endif( CMAKE_COMPILER_IS_GNUC )

set( LZMA_FILES
	C/Archive/7z/7zDecode.c
	C/Archive/7z/7zExtract.c
	C/Archive/7z/7zHeader.c
	C/Archive/7z/7zIn.c
	C/Archive/7z/7zItem.c
	C/7zBuf.c
	C/7zCrc.c
	C/7zStream.c
    C/Alloc.c
    C/Bcj2.c
    C/Bra86.c
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
