# zmusic-obj doesn't actually build anything itself, but rather all sources are
# added as interface sources.  Thus whatever links to zmusic-obj will be in
# charge of compiling.  As a result any properties set on zmusic-obj should be
# interface.
add_library(zmusic-obj INTERFACE)
target_sources(zmusic-obj
INTERFACE
	loader/i_module.cpp
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
	midisources/midisource_mids.cpp
	streamsources/music_dumb.cpp
	streamsources/music_gme.cpp
	streamsources/music_libsndfile.cpp
	streamsources/music_libxmp.cpp
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
	zmusic/critsec.cpp
	loader/test.c
)

file(GLOB HEADER_FILES
	zmusic/*.h
	loader/*.h
	mididevices/*.h
	midisources/*.h
	musicformats/*.h
	musicformats/win32/*.h
	decoder/*.h
	streamsources/*.h
	../thirdparty/*.h
	../include/*.h
)
target_sources(zmusic-obj INTERFACE ${HEADER_FILES})

target_compile_features(zmusic-obj INTERFACE cxx_std_11)
#set_target_properties(zmusic-obj PROPERTIES LINKER_LANGUAGE CXX)

require_stricmp(zmusic-obj INTERFACE)
require_strnicmp(zmusic-obj INTERFACE)

if(NOT WIN32 AND NOT APPLE)
	find_package(Threads)
	target_link_libraries(zmusic-obj INTERFACE Threads::Threads)
	determine_package_config_dependency(ZMUSIC_PACKAGE_DEPENDENCIES TARGET Threads::Threads MODULE Threads)
endif()

if ("vcpkg-libsndfile" IN_LIST VCPKG_MANIFEST_FEATURES)
	set(DYN_SNDFILE 0)
else()
	option(DYN_SNDFILE "Dynamically load libsndfile" ON)
endif()

if(DYN_SNDFILE)
	target_compile_definitions(zmusic-obj INTERFACE HAVE_SNDFILE DYN_SNDFILE)
else()
	find_package(SndFile)

	if(SNDFILE_FOUND)
		target_compile_definitions(zmusic-obj INTERFACE HAVE_SNDFILE)
		target_link_libraries(zmusic-obj INTERFACE SndFile::sndfile)
		determine_package_config_dependency(ZMUSIC_PACKAGE_DEPENDENCIES TARGET SndFile::sndfile MODULE SndFile)
	endif()
endif()

if ("vcpkg-libsndfile" IN_LIST VCPKG_MANIFEST_FEATURES)
	set(DYN_MPG123 0)
else()
	option(DYN_MPG123 "Dynamically load libmpg123" ON)
endif()
if(DYN_MPG123)
	target_compile_definitions(zmusic-obj INTERFACE HAVE_MPG123 DYN_MPG123)
elseif(NOT ("vcpkg-libsndfile" IN_LIST VCPKG_MANIFEST_FEATURES))
	find_package(MPG123)

	if(MPG123_FOUND)
		target_compile_definitions(zmusic-obj INTERFACE HAVE_MPG123)
		target_link_libraries(zmusic-obj INTERFACE mpg123)
		determine_package_config_dependency(ZMUSIC_PACKAGE_DEPENDENCIES TARGET mpg123 MODULE MPG123)
	endif()
endif()

# System MIDI support
if(WIN32)
	target_compile_definitions(zmusic-obj INTERFACE HAVE_SYSTEM_MIDI)
	target_link_libraries(zmusic-obj INTERFACE winmm)
	target_sources(zmusic-obj INTERFACE mididevices/music_win_mididevice.cpp)
elseif(NOT APPLE)
	find_package(ALSA)
    if(ALSA_FOUND)
		target_compile_definitions(zmusic-obj INTERFACE HAVE_SYSTEM_MIDI)
		target_sources(zmusic-obj
		INTERFACE
			mididevices/music_alsa_mididevice.cpp
			mididevices/music_alsa_state.cpp
		)
		target_link_libraries(zmusic-obj INTERFACE ALSA::ALSA)
		determine_package_config_dependency(ZMUSIC_PACKAGE_DEPENDENCIES TARGET ALSA::ALSA MODULE ALSA)
	endif()
endif()

if(WIN32)
	target_sources(zmusic-obj
	INTERFACE
		musicformats/win32/i_cd.cpp
		musicformats/win32/helperthread.cpp
	)
endif()

target_link_libraries(zmusic-obj INTERFACE dumb gme libxmp miniz ${CMAKE_DL_LIBS})

target_include_directories(zmusic-obj
INTERFACE
	../include
	${CMAKE_CURRENT_SOURCE_DIR}
	zmusic
)

propagate_object_links(zmusic-obj)

add_library(zmusic)
add_library(ZMusic::zmusic ALIAS zmusic)
add_library(zmusiclite)
add_library(ZMusic::zmusiclite ALIAS zmusiclite)

use_fast_math(zmusic)
use_fast_math(zmusiclite)

# Although zmusic-obj puts the public include directory in our private include
# list, we need to add it to the interface include directories for consumers.
target_include_directories(zmusic INTERFACE $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${ZMusic_SOURCE_DIR}/include>)
target_include_directories(zmusiclite INTERFACE $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${ZMusic_SOURCE_DIR}/include>)

target_link_libraries_hidden(zmusic zmusic-obj adl oplsynth opn timidity timidityplus wildmidi fluidsynth)
target_link_libraries_hidden(zmusiclite zmusic-obj fluidsynth)

target_compile_definitions(zmusic PUBLIC $<$<STREQUAL:$<TARGET_PROPERTY:zmusic,TYPE>,STATIC_LIBRARY>:ZMUSIC_STATIC>)
target_compile_definitions(zmusiclite PRIVATE ZMUSIC_LITE=1 PUBLIC $<$<STREQUAL:$<TARGET_PROPERTY:zmusiclite,TYPE>,STATIC_LIBRARY>:ZMUSIC_STATIC>)

set_target_properties(zmusic zmusiclite
PROPERTIES
	MACOSX_RPATH ON
	PUBLIC_HEADER ../include/zmusic.h
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

if (VCPKG_TOOLCHAIN)
	x_vcpkg_install_local_dependencies(TARGETS zmusic zmusiclite DESTINATION ".")
endif()

if(ZMUSIC_INSTALL)
	install(TARGETS zmusic EXPORT ZMusicFullTargets
	PUBLIC_HEADER
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
		COMPONENT devel
	LIBRARY
		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		COMPONENT full
		NAMELINK_COMPONENT devel
	)

	install(TARGETS zmusiclite EXPORT ZMusicLiteTargets
	PUBLIC_HEADER
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
		COMPONENT devel
	LIBRARY
		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		COMPONENT lite
		NAMELINK_COMPONENT devel
	)

	install(EXPORT ZMusicFullTargets
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ZMusic"
		NAMESPACE ZMusic::
		COMPONENT devel
	)

	install(EXPORT ZMusicLiteTargets
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ZMusic"
		NAMESPACE ZMusic::
		COMPONENT devel
	)
endif()

if( MSVC )
	option( ZMUSIC_GENERATE_MAPFILE "Generate .map file for debugging." OFF )

	if( ZMUSIC_GENERATE_MAPFILE )
		target_link_options(zmusic PRIVATE "/MAP")
		target_link_options(zmusiclite PRIVATE "/MAP")
	endif() 
endif()

source_group("MIDI Devices" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/mididevices/.+")
source_group("MIDI Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/midisources/.+")
source_group("Music Formats" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/musicformats/.+")
source_group("Music Formats\\Win32" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/musicformats/win32/.+")
source_group("ZMusic Core" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/zmusic/.+")
source_group("Sound Decoding" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/decoder/.+")
source_group("Stream Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/streamsources/.+")
source_group("Third Party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/.+")
source_group("Public Interface" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/../include/.+")
