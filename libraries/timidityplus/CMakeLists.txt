cmake_minimum_required( VERSION 2.8.7 )

use_fast_math()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

include_directories( timiditypp )

file( GLOB HEADER_FILES
	timiditypp/*.h
	)
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
