add_library(timidity OBJECT
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

file(GLOB HEADER_FILES timidity/*.h)
target_sources(timidity PRIVATE ${HEADER_FILES})

target_include_directories(timidity INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE timidity)

set_target_properties(timidity
PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
)

use_fast_math(timidity)
require_stricmp(timidity PRIVATE)

if(COMPILER_IS_GNUC_COMPATIBLE)
	target_compile_options(timidity
	PRIVATE
		"-Wno-unused-function" "-Wno-unused-variable"
	)
endif()
