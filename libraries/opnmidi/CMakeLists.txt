cmake_minimum_required( VERSION 2.8.7 )

make_release_only()
use_fast_math()

# we play with out own sequencer
add_definitions(-DOPNMIDI_DISABLE_MIDI_SEQUENCER)

# Disable OPNMIDI's experimental yet emulator (using of it has some issues and missing notes in playback)
add_definitions(-DOPNMIDI_DISABLE_GX_EMULATOR)

add_library( opn STATIC
	chips/gens_opn2.cpp
	chips/gens/Ym2612_Emu.cpp
	chips/mame/mame_ym2612fm.c
	chips/mame_opn2.cpp
	chips/nuked_opn2.cpp
	chips/nuked/ym3438.c
	opnmidi.cpp
	opnmidi_load.cpp
	opnmidi_midiplay.cpp
	opnmidi_opn2.cpp
	opnmidi_private.cpp
	wopn/wopn_file.c	)
target_link_libraries( opn )
