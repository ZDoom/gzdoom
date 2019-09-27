cmake_minimum_required( VERSION 2.8.7 )

make_release_only()
use_fast_math()
require_stricmp()
require_strnicmp()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	add_compile_options( -std=c++11 )
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
