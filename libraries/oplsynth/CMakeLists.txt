cmake_minimum_required( VERSION 2.8.7 )

use_fast_math()
require_stricmp()
require_strnicmp()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

include_directories( oplsynth )

file( GLOB HEADER_FILES
	oplsynth/*.h
	)
add_library( oplsynth STATIC
	fmopl.cpp
	musicblock.cpp
	nukedopl3.cpp
	opl_mus_player.cpp
	OPL3.cpp
	oplio.cpp
	dosbox/opl.cpp
	)
target_link_libraries( oplsynth )
