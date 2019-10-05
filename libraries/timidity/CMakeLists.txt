cmake_minimum_required( VERSION 2.8.7 )

use_fast_math()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

include( CheckFunctionExists )

CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
if( NOT STRICMP_EXISTS )
	add_definitions( -Dstricmp=strcasecmp )
endif()

include_directories( timidity )

file( GLOB HEADER_FILES
	timidity/*.h
	)
add_library( timidity STATIC
	common.cpp
	instrum.cpp
	instrum_dls.cpp
	instrum_font.cpp
	instrum_sf2.cpp
	mix.cpp
	playmidi.cpp
	resample.cpp
	timidity.cpp
	)
target_link_libraries( timidity )
