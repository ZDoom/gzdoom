add_library(oplsynth OBJECT
	fmopl.cpp
	musicblock.cpp
	nukedopl3.cpp
	opl_mus_player.cpp
	OPL3.cpp
	oplio.cpp
	dosbox/opl.cpp
)

file(GLOB HEADER_FILES oplsynth/*.h)
target_sources(oplsynth PRIVATE ${HEADER_FILES})

target_include_directories(oplsynth INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE oplsynth)

set_target_properties(oplsynth
PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
)

use_fast_math(oplsynth)
require_stricmp(oplsynth PRIVATE)
require_strnicmp(oplsynth PRIVATE)

if(COMPILER_IS_GNUC_COMPATIBLE)
	target_compile_options(oplsynth
	PRIVATE
		"-Wall" "-Wextra" "-Wno-unused-parameter" "-fomit-frame-pointer" 
	)
endif()
