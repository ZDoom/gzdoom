cmake_minimum_required( VERSION 2.8.7 )

make_release_only()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ZD_FASTMATH_FLAG}")

include_directories( timiditypp )

add_library( timidityplus STATIC
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
target_link_libraries( timidityplus )
