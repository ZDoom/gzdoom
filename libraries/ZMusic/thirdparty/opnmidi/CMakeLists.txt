make_release_only()

add_library(opn OBJECT)

target_sources(opn
PRIVATE
	opnmidi_load.cpp
	opnmidi_private.cpp
	opnmidi.cpp
	opnmidi_midiplay.cpp
	opnmidi_opn2.cpp
	chips/np2/fmgen_fmgen.cpp
	chips/np2/fmgen_opna.cpp
	chips/np2/fmgen_fmtimer.cpp
	chips/np2/fmgen_file.cpp
	chips/np2/fmgen_psg.cpp
	chips/mame_opn2.cpp
	chips/gens_opn2.cpp
	chips/mame_opna.cpp
	chips/np2_opna.cpp
	chips/mamefm/ymdeltat.cpp
	chips/mamefm/resampler.cpp
	chips/mamefm/fm.cpp
	chips/nuked_opn2.cpp
	chips/gens/Ym2612.cpp
	chips/nuked/ym3438.c
	chips/mamefm/emu2149.c
	chips/mame/mame_ym2612fm.c
	wopn/wopn_file.c
)

if(COMPILER_SUPPORTS_CXX14)
	set(YMFM_SOURCES
		chips/ymfm_opn2.cpp
		chips/ymfm_opna.cpp
		chips/ymfm/ymfm_opn.cpp
# Duplicates at the libADLMIDI
#		chips/ymfm/ymfm_misc.cpp
#		chips/ymfm/ymfm_pcm.cpp
#		chips/ymfm/ymfm_adpcm.cpp
#		chips/ymfm/ymfm_ssg.cpp
	)
	if(DEFINED FLAG_CPP14)
		set_source_files_properties(${YMFM_SOURCES} COMPILE_FLAGS ${FLAG_CPP14})
	endif()
	target_sources(opn PRIVATE ${YMFM_SOURCES})
else()
	target_compile_definitions(opn PUBLIC -DOPNMIDI_DISABLE_YMFM_EMULATOR)
endif()

target_compile_definitions(opn
PRIVATE
	# we play with out own sequencer
	OPNMIDI_DISABLE_MIDI_SEQUENCER
)

target_include_directories(opn PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

use_fast_math(opn)
