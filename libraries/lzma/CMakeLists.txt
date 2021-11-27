cmake_minimum_required( VERSION 3.1.0 )

make_release_only()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_7ZIP_PPMD_SUPPPORT" )

find_package(Threads)

add_library( lzma STATIC
	C/7zArcIn.c
	C/7zBuf.c
	C/7zCrc.c
	C/7zCrcOpt.c
	C/7zDec.c
	C/7zStream.c
	C/Bcj2.c
	C/Bra.c
	C/Bra86.c
	C/BraIA64.c
	C/CpuArch.c
	C/Delta.c
	C/LzFind.c
	C/LzFindMt.c
	C/LzFindOpt.c
	C/Lzma2Dec.c
	C/LzmaDec.c
	C/LzmaEnc.c
	C/Ppmd7.c
	C/Ppmd7Dec.c
	C/Threads.c
)
target_link_libraries( lzma Threads::Threads )
