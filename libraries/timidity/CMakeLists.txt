cmake_minimum_required( VERSION 2.8.7 )

make_release_only()
use_fast_math()
require_stricmp()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	add_compile_options( -std=c++11 )
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
