cmake_minimum_required( VERSION 2.8.7 )

if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ZD_FASTMATH_FLAG}")

include( CheckFunctionExists )

CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
if( NOT STRICMP_EXISTS )
	add_definitions( -Dstricmp=strcasecmp )
endif()

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if( NOT STRNICMP_EXISTS )
	add_definitions( -Dstrnicmp=strncasecmp )
endif()

if( DYN_SNDFILE)
	add_definitions( -DHAVE_SNDFILE -DDYN_SNDFILE )
elseif( SNDFILE_FOUND )
	add_definitions( -DHAVE_SNDFILE )
endif()

if( DYN_MPG123)
	add_definitions( -DHAVE_MPG123 -DDYN_MPG123 )
elseif( MPG123_FOUND )
	add_definitions( -DHAVE_MPG123 )
endif()

if( DYN_FLUIDSYNTH )
	add_definitions( -DHAVE_FLUIDSYNTH -DDYN_FLUIDSYNTH )
elseif( FLUIDSYNTH_FOUND )
	add_definitions( -DHAVE_FLUIDSYNTH )
endif()


include_directories( "../libraries/dumb/include" "${ADL_INCLUDE_DIR}" "${OPN_INCLUDE_DIR}" "${TIMIDITYPP_INCLUDE_DIR}" "${TIMIDITY_INCLUDE_DIR}" "${WILDMIDI_INCLUDE_DIR}" "${OPLSYNTH_INCLUDE_DIR}" "${GME_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" )

if (WIN32)
set( PLAT_WIN32_SOURCES
	mididevices/music_win_mididevice.cpp)
endif()

file( GLOB HEADER_FILES
	zmusic/*.h
	mididevices/*.h
	midisources/*.h
	musicformats/*.h
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
	decoder/sounddecoder.cpp
	decoder/sndfile_decoder.cpp
	decoder/mpg123_decoder.cpp
	zmusic/configuration.cpp
	${PLAT_WIN32_SOURCES}
	)
target_link_libraries( zmusic )


source_group("MIDI Devices" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/mididevices/.+")
source_group("MIDI Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/midisources/.+")
source_group("Music Formats" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/musicformats/.+")
source_group("Public Interface" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/zmusic/.+")
source_group("Sound Decoding" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/decoder/.+")
source_group("Stream Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/streamsources/.+")
source_group("Third Party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/.+")
