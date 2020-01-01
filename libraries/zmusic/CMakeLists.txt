cmake_minimum_required( VERSION 2.8.7 )

use_fast_math()
require_stricmp()
require_strnicmp()

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

include( CheckFunctionExists )

find_package( ALSA )
if (WIN32 OR ALSA_FOUND)
    add_definitions( -DHAVE_SYSTEM_MIDI )
endif()

if( DYN_SNDFILE)
	add_definitions( -DHAVE_SNDFILE -DDYN_SNDFILE )
else()
	find_package( SndFile )

	if( SNDFILE_FOUND )
		add_definitions( -DHAVE_SNDFILE )
	endif()
endif()

if( DYN_MPG123)
	add_definitions( -DHAVE_MPG123 -DDYN_MPG123 )
else()
	find_package( MPG123 )

	if( MPG123_FOUND )
		add_definitions( -DHAVE_MPG123 )
	endif()
endif()

if( DYN_FLUIDSYNTH )
	add_definitions( -DHAVE_FLUIDSYNTH -DDYN_FLUIDSYNTH )
else()
	find_package( FluidSynth )

	if( FLUIDSYNTH_FOUND )
		add_definitions( -DHAVE_FLUIDSYNTH )
	endif()
endif()


include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/dumb/include" "${ZLIB_INCLUDE_DIR}" "${ADL_INCLUDE_DIR}" "${OPN_INCLUDE_DIR}" "${TIMIDITYPP_INCLUDE_DIR}" "${TIMIDITY_INCLUDE_DIR}" "${WILDMIDI_INCLUDE_DIR}" "${OPLSYNTH_INCLUDE_DIR}" "${GME_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" )

if (WIN32)
    set( PLAT_SOURCES
        mididevices/music_win_mididevice.cpp
        musicformats/win32/i_cd.cpp
        musicformats/win32/helperthread.cpp
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    if (ALSA_FOUND)
        set( PLAT_SOURCES
            mididevices/music_alsa_mididevice.cpp
            mididevices/music_alsa_state.cpp
        )
    endif()
endif()


file( GLOB HEADER_FILES
	zmusic/*.h
	mididevices/*.h
	midisources/*.h
	musicformats/*.h
	musicformats/win32/*.h
	decoder/*.h
	streamsources/*.h
	thirdparty/*.h
	)
add_library( zmusic STATIC
	${HEADER_FILES}
	i_module.cpp
	mididevices/music_base_mididevice.cpp
	mididevices/music_adlmidi_mididevice.cpp
	mididevices/music_opl_mididevice.cpp
	mididevices/music_opnmidi_mididevice.cpp
	mididevices/music_timiditypp_mididevice.cpp
	mididevices/music_fluidsynth_mididevice.cpp
	mididevices/music_softsynth_mididevice.cpp
	mididevices/music_timidity_mididevice.cpp
	mididevices/music_wildmidi_mididevice.cpp
	mididevices/music_wavewriter_mididevice.cpp
	midisources/midisource.cpp
	midisources/midisource_mus.cpp
	midisources/midisource_smf.cpp
	midisources/midisource_hmi.cpp
	midisources/midisource_xmi.cpp
	streamsources/music_dumb.cpp
	streamsources/music_gme.cpp
	streamsources/music_libsndfile.cpp
	streamsources/music_opl.cpp
	streamsources/music_xa.cpp
	musicformats/music_stream.cpp
	musicformats/music_midi.cpp
	musicformats/music_cd.cpp
	decoder/sounddecoder.cpp
	decoder/sndfile_decoder.cpp
	decoder/mpg123_decoder.cpp
	zmusic/configuration.cpp
	zmusic/zmusic.cpp
	${PLAT_SOURCES}
	)
target_link_libraries( zmusic adl dumb gme oplsynth opn timidity timidityplus wildmidi )

if( NOT DYN_SNDFILE AND SNDFILE_FOUND )
	include_directories( "${SNDFILE_INCLUDE_DIRS}" )
	target_link_libraries( zmusic ${SNDFILE_LIBRARIES} )
endif()

if( NOT DYN_MPG123 AND MPG123_FOUND )
	include_directories( "${MPG123_INCLUDE_DIR}" )
	target_link_libraries( zmusic ${MPG123_LIBRARIES} )
endif()

if( NOT DYN_FLUIDSYNTH AND FLUIDSYNTH_FOUND )
	include_directories( "${FLUIDSYNTH_INCLUDE_DIR}" )
	target_link_libraries( zmusic ${FLUIDSYNTH_LIBRARIES} )
endif()

if(ALSA_FOUND)
	include_directories( "${ALSA_INCLUDE_DIR}" )
	target_link_libraries( zmusic ${ALSA_LIBRARIES} )
endif()

source_group("MIDI Devices" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/mididevices/.+")
source_group("MIDI Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/midisources/.+")
source_group("Music Formats" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/musicformats/.+")
source_group("Music Formats\\Win32" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/musicformats/win32/.+")
source_group("Public Interface" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/zmusic/.+")
source_group("Sound Decoding" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/decoder/.+")
source_group("Stream Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/streamsources/.+")
source_group("Third Party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/.+")
