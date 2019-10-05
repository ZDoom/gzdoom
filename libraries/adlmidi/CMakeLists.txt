cmake_minimum_required( VERSION 2.8.7 )

make_release_only()
use_fast_math()

add_definitions(-DADLMIDI_DISABLE_MIDI_SEQUENCER)

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
