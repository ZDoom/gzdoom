make_release_only()

add_library(adl OBJECT)

target_sources(adl
PRIVATE
	adlmidi_midiplay.cpp
	adlmidi_opl3.cpp
	adlmidi_private.cpp
	adlmidi.cpp
	adlmidi_load.cpp
	inst_db.cpp
	chips/opal_opl3.cpp
	chips/opal/opal.c
	chips/dosbox/dbopl.cpp
	chips/nuked_opl3_v174.cpp
	chips/java_opl3.cpp
	chips/dosbox_opl3.cpp
	chips/nuked_opl3.cpp
	chips/nuked/nukedopl3_174.c
	chips/nuked/nukedopl3.c
	chips/ym3812_lle.cpp
	chips/ym3812_lle/nopl2.c
	chips/ym3812_lle/nuked_fmopl2.c
	chips/ymf262_lle.cpp
	chips/ymf262_lle/nopl3.c
	chips/ymf262_lle/nuked_fmopl3.c
	chips/esfmu_opl3.cpp
	chips/esfmu/esfm.c
	chips/esfmu/esfm_registers.c
	chips/mame_opl2.cpp
	# chips/mame/mame_fmopl.cpp  # Duplicate of ../oplsynth/fmopl.cpp
	wopl/wopl_file.c
)

if(COMPILER_SUPPORTS_CXX14)
	set(YMFM_SOURCES
		chips/ymfm_opl2.cpp
		chips/ymfm_opl3.cpp
		chips/ymfm/ymfm_adpcm.cpp
		chips/ymfm/ymfm_misc.cpp
		chips/ymfm/ymfm_opl.cpp
		chips/ymfm/ymfm_pcm.cpp
		chips/ymfm/ymfm_ssg.cpp
	)
	if(DEFINED FLAG_CPP14)
		set_source_files_properties(${YMFM_SOURCES} COMPILE_FLAGS ${FLAG_CPP14})
	endif()
	target_sources(adl PRIVATE ${YMFM_SOURCES})
else()
	target_compile_definitions(adl PUBLIC -DADLMIDI_DISABLE_YMFM_EMULATOR)
endif()

target_compile_definitions(adl PRIVATE ADLMIDI_DISABLE_MIDI_SEQUENCER ADLMIDI_ENABLE_OPL2_LLE_EMULATOR ADLMIDI_ENABLE_OPL3_LLE_EMULATOR)

target_include_directories(adl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

use_fast_math(adl)
