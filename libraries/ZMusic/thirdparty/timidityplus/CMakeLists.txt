add_library(timidityplus OBJECT
	fft4g.cpp
	reverb.cpp
	common.cpp
	configfile.cpp
	effect.cpp
	filter.cpp
	freq.cpp
	instrum.cpp
	mblock.cpp
	mix.cpp
	playmidi.cpp
	quantity.cpp
	readmidic.cpp
	recache.cpp
	resample.cpp
	sbkconv.cpp
	sffile.cpp
	sfitem.cpp
	smplfile.cpp
	sndfont.cpp
	tables.cpp
)

target_include_directories(timidityplus INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE timiditypp)

set_target_properties(timidityplus
PROPERTIES
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
)

file(GLOB HEADER_FILES timiditypp/*.h)
target_sources(timidityplus PRIVATE ${HEADER_FILES})

use_fast_math(timidityplus)
