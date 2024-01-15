make_release_only()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZ7_PPMD_SUPPORT" )

find_package(Threads)

add_library( lzma STATIC
	C/7zAlloc.c
	C/7zArcIn.c
	C/7zBuf.c
	C/7zBuf2.c
	C/7zCrc.c
	C/7zCrcOpt.c
	C/7zDec.c
	C/7zFile.c
	C/7zStream.c
	C/Alloc.c
	C/Bcj2.c
	C/Bcj2Enc.c
	C/Bra.c
	C/Bra86.c
	C/CpuArch.c
	C/Delta.c
	C/DllSecur.c
	C/LzFind.c
	C/LzFindMt.c
	C/LzFindOpt.c
	C/Lzma2Dec.c
	C/Lzma2DecMt.c
	C/Lzma2Enc.c
	C/LzmaDec.c
	C/LzmaEnc.c
	C/LzmaLib.c
	C/MtCoder.c
	C/MtDec.c
	C/Ppmd7.c
	C/Ppmd7Dec.c
	C/Ppmd7Enc.c
	C/Sha256.c
	C/Sha256Opt.c
	C/Sort.c
	C/SwapBytes.c
	C/Threads.c
	C/Xz.c
	C/XzCrc64.c
	C/XzCrc64Opt.c
	C/XzDec.c
	C/XzEnc.c
	C/XzIn.c
)
target_link_libraries( lzma Threads::Threads )
