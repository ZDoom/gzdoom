cmake_minimum_required( VERSION 2.8.7 )

make_release_only()
use_fast_math()

include( CheckFunctionExists )
include( CheckCXXCompilerFlag )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG -DDEBUGMODE=1" )

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-pointer-sign -Wno-uninitialized" )
	if( CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.5" )
		set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable" )
	endif()
endif()

CHECK_FUNCTION_EXISTS( itoa ITOA_EXISTS )
if( NOT ITOA_EXISTS )
	add_definitions( -DNEED_ITOA=1 )
endif()

include_directories( include )

add_library( dumb STATIC
    src/core/unload.c
    src/core/rendsig.c
    src/core/rendduh.c
    src/core/register.c
    src/core/readduh.c
    src/core/rawsig.c
    src/core/makeduh.c
    src/core/loadduh.c
    src/core/dumbfile.c
    src/core/duhtag.c
    src/core/duhlen.c
    src/core/atexit.c
    src/helpers/stdfile.c
    src/helpers/silence.c
    src/helpers/sampbuf.c
    src/helpers/riff.c
    src/helpers/resample.c
    src/helpers/memfile.c
    src/helpers/clickrem.c
    src/helpers/barray.c
    src/it/xmeffect.c
    src/it/readxm2.c
    src/it/readxm.c
    src/it/readstm2.c
    src/it/readstm.c
    src/it/reads3m2.c
    src/it/reads3m.c
    src/it/readriff.c
    src/it/readptm.c
    src/it/readpsm.c
    src/it/readoldpsm.c
    src/it/readokt2.c
    src/it/readokt.c
    src/it/readmtm.c
    src/it/readmod2.c
    src/it/readmod.c
    src/it/readdsmf.c
    src/it/readasy.c
    src/it/readamf2.c
    src/it/readamf.c
    src/it/readam.c
    src/it/read6692.c
    src/it/read669.c
    src/it/ptmeffect.c
    src/it/loadxm2.c
    src/it/loadxm.c
    src/it/loadstm2.c
    src/it/loadstm.c
    src/it/loads3m2.c
    src/it/loads3m.c
    src/it/loadriff2.c
    src/it/loadriff.c
    src/it/loadptm2.c
    src/it/loadptm.c
    src/it/loadpsm2.c
    src/it/loadpsm.c
    src/it/loadoldpsm2.c
    src/it/loadoldpsm.c
    src/it/loadokt2.c
    src/it/loadokt.c
    src/it/loadmtm2.c
    src/it/loadmtm.c
    src/it/loadmod2.c
    src/it/loadmod.c
    src/it/loadasy2.c
    src/it/loadasy.c
    src/it/loadamf2.c
    src/it/loadamf.c
    src/it/load6692.c
    src/it/load669.c
    src/it/itunload.c
    src/it/itrender.c
    src/it/itread2.c
    src/it/itread.c
    src/it/itorder.c
    src/it/itmisc.c
    src/it/itload2.c
    src/it/itload.c
    src/it/readany.c
    src/it/loadany2.c
    src/it/loadany.c
    src/it/readany2.c
    src/helpers/resampler.c
    src/helpers/lpc.c
)
target_link_libraries( dumb )

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	CHECK_CXX_COMPILER_FLAG( -msse DUMB_CAN_USE_SSE )

	if( DUMB_CAN_USE_SSE )
		set_source_files_properties( src/helpers/resampler.c PROPERTIES COMPILE_FLAGS -msse )
	endif()
endif()
