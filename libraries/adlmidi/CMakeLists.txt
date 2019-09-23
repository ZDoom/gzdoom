cmake_minimum_required( VERSION 2.8.7 )

make_release_only()

add_definitions(-DADLMIDI_DISABLE_MIDI_SEQUENCER)

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ZD_FASTMATH_FLAG}")

add_library( adl STATIC
	adldata.cpp
	adlmidi.cpp
	adlmidi_load.cpp
	adlmidi_midiplay.cpp
	adlmidi_opl3.cpp
	adlmidi_private.cpp
	chips/dosbox/dbopl.cpp
	chips/dosbox_opl3.cpp
	chips/nuked/nukedopl3_174.c
	chips/nuked/nukedopl3.c
	chips/nuked_opl3.cpp
	chips/nuked_opl3_v174.cpp
	wopl/wopl_file.c
	)
target_link_libraries( adl )
