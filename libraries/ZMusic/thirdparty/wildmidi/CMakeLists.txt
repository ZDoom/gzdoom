add_library(wildmidi OBJECT
	file_io.cpp
	gus_pat.cpp
	reverb.cpp
	wildmidi_lib.cpp
	wm_error.cpp
)

file(GLOB HEADER_FILES wildmidi/*.h)
target_sources(wildmidi PRIVATE ${HEADER_FILES})

set_target_properties(wildmidi
PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
)

target_include_directories(wildmidi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE wildmidi)

use_fast_math(wildmidi)
require_stricmp(wildmidi PRIVATE)
require_strnicmp(wildmidi PRIVATE)

if(COMPILER_IS_GNUC_COMPATIBLE)
	target_compile_options(wildmidi
	PRIVATE
		"-Wall" "-Wextra" "-Wno-unused-parameter" "-fomit-frame-pointer"
	)
endif()
