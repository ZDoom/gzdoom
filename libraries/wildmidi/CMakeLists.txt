cmake_minimum_required( VERSION 2.8.7 )

make_release_only()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ZD_FASTMATH_FLAG}")

include( CheckFunctionExists )

CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
if( NOT STRICMP_EXISTS )
	add_definitions( -Dstricmp=strcasecmp )
endif()

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if( NOT STRNICMP_EXISTS )
	add_definitions( -Dstrnicmp=strncasecmp )
endif()

include_directories( wildmidi )

file( GLOB HEADER_FILES
	wildmidi/*.h
	)
add_library( wildmidi STATIC
	file_io.cpp
	gus_pat.cpp
	reverb.cpp
	wildmidi_lib.cpp
	wm_error.cpp
	)
target_link_libraries( wildmidi )
