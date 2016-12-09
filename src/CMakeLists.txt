cmake_minimum_required( VERSION 2.8.7 )

include(../precompiled_headers.cmake)

if( COMMAND cmake_policy )
	cmake_policy( SET CMP0003 NEW )
endif()

include( CheckCXXSourceCompiles )
include( CheckFunctionExists )
include( CheckCXXCompilerFlag )
include( CheckIncludeFile )
include( CheckIncludeFiles )
include( CheckLibraryExists )
include( FindPkgConfig )

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	option( NO_STRIP "Do not strip Release or MinSizeRel builds" )
	# At least some versions of Xcode fail if you strip with the linker
	# instead of the separate strip utility.
	if( APPLE )
		set( NO_STRIP ON )
	endif()
endif()

option( DYN_FLUIDSYNTH "Dynamically load fluidsynth" ON )
option( DYN_OPENAL "Dynamically load OpenAL" ON )

if( APPLE )
    option( OSX_COCOA_BACKEND "Use native Cocoa backend instead of SDL" ON )
endif()

if( CMAKE_SIZEOF_VOID_P MATCHES "8" )
	set( X64 64 )
endif()

# You can either use "make install" on the FMOD distribution to put it
# in standard system locations, or you can unpack the FMOD distribution
# in the root of the zdoom tree. e.g.:
# zdoom
#   docs
#   fmodapi<version>linux[64] -or simply- fmod
#   jpeg-6b
#   ...
# The recommended method is to put it in the zdoom tree, since its
# headers are unversioned. Especially now that we can't work properly
# with anything newer than 4.26.xx, you probably don't want to use
# a system-wide version.

# Construct version numbers for searching for the FMOD library on Linux.
set( MINOR_VERSIONS "61" "50" "49" "48" "47" "46" "45" "44" "43" "42" "41"
	"40" "39" "38" "37" "36" "35" "34" "33" "32" "31" "30" "29" "28"
	"27" "26" "25" "24" "23" "22" "21" "20" "21" "19" "18" "17" "16"
	"15" "14" "13" "12" "11" "10" "09" "08" "07" "06" "05" "04" "03"
	"02" "01" "00" )
set( MAJOR_VERSIONS "44" "34" "28" "26" "24" "22" "20" )

if( NOT FMOD_DIR_VERSIONS )
	set( FMOD_DIR_VERSIONS "" )
endif()
if( NOT FMOD_VERSIONS )
	set( FMOD_VERSIONS "" )
endif()
if( NOT FMOD_LOCAL_INC_DIRS )
	set( FMOD_LOCAL_INC_DIRS "" )
endif()
if( NOT FMOD_LOCAL_LIB_DIRS )
	set( FMOD_LOCAL_LIB_DIRS "" )
endif()

set( FMOD_DIR_VERSIONS ${FMOD_DIR_VERSIONS} "../fmod" )
foreach( majver ${MAJOR_VERSIONS} )
	foreach( minver ${MINOR_VERSIONS} )
		set( FMOD_VERSIONS ${FMOD_VERSIONS} "fmodex${X64}-4.${majver}.${minver}" )
		# FMOD Ex version 4.44 unified 32-bit and 64-bit linux packages into one.
		if( NOT majver EQUAL "44" )
			set( FMOD_DIR_VERSIONS ${FMOD_DIR_VERSIONS} "${CMAKE_HOME_DIRECTORY}/fmodapi4${majver}${minver}linux${X64}" )
		else()
			set( FMOD_DIR_VERSIONS ${FMOD_DIR_VERSIONS} "${CMAKE_HOME_DIRECTORY}/fmodapi4${majver}${minver}linux" )
		endif()
	endforeach()
	foreach( dir ${FMOD_DIR_VERSIONS} )
		set( FMOD_LOCAL_INC_DIRS ${FMOD_LOCAL_INC_DIRS} "${dir}/api/inc" )
		set( FMOD_LOCAL_LIB_DIRS ${FMOD_LOCAL_LIB_DIRS} "${dir}/api/lib" )
	endforeach()
endforeach()

if( NOT ZDOOM_LIBS )
	set( ZDOOM_LIBS "" )
endif()

if( WIN32 )
	if( X64 )
		set( WIN_TYPE Win64 )
		set( XBITS x64 )
	else()
		set( WIN_TYPE Win32 )
		set( XBITS x86 )
	endif()
	
	add_definitions( -D_WIN32 )
	
	set( FMOD_SEARCH_PATHS
		"C:/Program Files/FMOD SoundSystem/FMOD Programmers API ${WIN_TYPE}/api"
		"C:/Program Files (x86)/FMOD SoundSystem/FMOD Programmers API ${WIN_TYPE}/api"
		# This next one is for Randy.
		"E:/Software/Dev/FMOD/${WIN_TYPE}/api" 
		)
	set( FMOD_INC_PATH_SUFFIXES PATH_SUFFIXES inc )
	set( FMOD_LIB_PATH_SUFFIXES PATH_SUFFIXES lib )
	
	find_path( D3D_INCLUDE_DIR d3d9.h
		PATHS ENV DXSDK_DIR
		PATH_SUFFIXES Include )
	if( NOT D3D_INCLUDE_DIR )
		# Modern versions of the Windows SDK include d3d9.h. Unfortunately,
		# CMake cannot find this file via find_path, so we check for it using
		# CHECK_INCLUDE_FILE.
		CHECK_INCLUDE_FILE( d3d9.h D3D9_H_FOUND )
		if ( NOT D3D9_H_FOUND )
			message( SEND_ERROR "Could not find DirectX 9 header files" )
		endif()
	else()
		include_directories( ${D3D_INCLUDE_DIR} )
	endif()
	
	find_path( XINPUT_INCLUDE_DIR xinput.h
		PATHS ENV DXSDK_DIR
		PATH_SUFFIXES Include )
	if( NOT XINPUT_INCLUDE_DIR )
		# Modern versions of the Windows SDK include xinput.h. Unfortunately,
		# CMake cannot find this file via find_path, so we check for it using
		# CHECK_INCLUDE_FILES. windows.h must be included before xinput.h.
		CHECK_INCLUDE_FILES( "windows.h;xinput.h" XINPUT_H_FOUND )
		if( NOT XINPUT_H_FOUND )
			message( WARNING "Could not find xinput.h. XInput will be disabled." )
			add_definitions( -DNO_XINPUT )
		endif()
	else()
		include_directories( ${XINPUT_INCLUDE_DIR} )
	endif()

	find_library( DX_dinput8_LIBRARY dinput8
		PATHS ENV DXSDK_DIR
		PATH_SUFFIXES Lib Lib/${XBITS} )
	find_library( DX_dxguid_LIBRARY dxguid
		PATHS ENV DXSDK_DIR
		PATH_SUFFIXES Lib Lib/${XBITS} )

	# Modern versions of the Windows SDK include dinput8.lib. Unfortunately,
	# CMake cannot find these libraries via find_library.
	if( NOT DX_dinput8_LIBRARY )
		# If we got this far, assume dinput8.lib is in the system library path.
		set( DX_dinput8_LIBRARY dinput8 )
	endif()

	# Modern versions of the Windows SDK do NOT include dxguid.lib. Its contents
	# were moved to dinput8.lib.
	if( NOT DX_dxguid_LIBRARY )
		message( STATUS "Could not find dxguid.lib. Build may fail on old Windows SDKs.")
	endif()

	set( ZDOOM_LIBS
		wsock32
		winmm
		"${DX_dinput8_LIBRARY}"
		ole32
		user32
		gdi32
		comctl32
		comdlg32
		ws2_32
		setupapi
		oleaut32 )

	if( NOT ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} DelayImp )
	endif()

	if( DX_dxguid_LIBRARY )
		list( APPEND ZDOOM_LIBS "${DX_dxguid_LIBRARY}" )
	endif()
else()
	if( APPLE )
		set( FMOD_SEARCH_PATHS "/Developer/FMOD Programmers API Mac/api" )
		set( FMOD_INC_PATH_SUFFIXES PATH_SUFFIXES inc )
		set( FMOD_LIB_PATH_SUFFIXES PATH_SUFFIXES lib )
		set( NO_GTK ON )
		set( DYN_GTK OFF )

		# Prevent inclusion of fp.h and FixMath.h from Carbon framework
		# Declarations from these files are not used but cause the following conflicts:
		#  - redefinition of 'FixedToFloat' and 'FloatToFixed' macros
		#  - redefinition of 'pi' as different kind of symbol
		add_definitions( -D__FP__ -D__FIXMATH__ )
	else()
		option( NO_GTK "Disable GTK+ dialogs (Not applicable to Windows)" )
		option( DYN_GTK "Load GTK+ at runtime instead of compile time" ON )

		set( FMOD_SEARCH_PATHS
			/usr/local/include
			/usr/local/include/fmodex
			/usr/include
			/usr/include/fmodex
			/opt/local/include
			/opt/local/include/fmodex
			/opt/include
			/opt/include/fmodex )
		set( FMOD_INC_PATH_SUFFIXES PATH_SUFFIXES fmodex )

		# Use GTK+ for the IWAD picker, if available.
		if( NOT NO_GTK )
			pkg_check_modules( GTK3 gtk+-3.0 )
			if( GTK3_FOUND )
				if( NOT DYN_GTK )
					set( ZDOOM_LIBS ${ZDOOM_LIBS} ${GTK3_LIBRARIES} )
				endif()
				include_directories( ${GTK3_INCLUDE_DIRS} )
				link_directories( ${GTK3_LIBRARY_DIRS} )
			else()
				pkg_check_modules( GTK2 gtk+-2.0 )
				if( GTK2_FOUND )
					if( NOT DYN_GTK )
						set( ZDOOM_LIBS ${ZDOOM_LIBS} ${GTK2_LIBRARIES} )
					endif()
					include_directories( ${GTK2_INCLUDE_DIRS} )
					link_directories( ${GTK2_LIBRARY_DIRS} )
				else()
					set( NO_GTK ON )
				endif()
			endif()
		endif()
	endif()
	
	if( NO_GTK )
		add_definitions( -DNO_GTK )
	elseif( DYN_GTK )
		add_definitions( -DDYN_GTK=1 )
	else()
		add_definitions( -DDYN_GTK=0 )
	endif()

	# Non-Windows version also needs SDL except native OS X backend
	if( NOT APPLE OR NOT OSX_COCOA_BACKEND )
		find_package( SDL2 REQUIRED )
		include_directories( "${SDL2_INCLUDE_DIR}" )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} "${SDL2_LIBRARY}" )
	endif()

	find_path( FPU_CONTROL_DIR fpu_control.h )
	if( FPU_CONTROL_DIR )
		include_directories( ${FPU_CONTROL_DIR} )
		add_definitions( -DHAVE_FPU_CONTROL )
	endif()
endif()


if( NOT NO_OPENAL )
	find_package( OpenAL )
	mark_as_advanced(CLEAR OPENAL_INCLUDE_DIR)
	if( OPENAL_INCLUDE_DIR )
		include_directories( ${OPENAL_INCLUDE_DIR} )
		if( DYN_OPENAL )
			add_definitions( -DDYN_OPENAL )
		else()
			mark_as_advanced(CLEAR OPENAL_LIBRARY)
			if( OPENAL_LIBRARY )
				set( ZDOOM_LIBS ${OPENAL_LIBRARY} ${ZDOOM_LIBS} )
			else()
				set( NO_OPENAL ON )
			endif()
		endif()
	else()
		set( NO_OPENAL ON )
	endif()
endif()

if( NOT NO_FMOD )
	# Search for FMOD include files
	if( NOT WIN32 )
		find_path( FMOD_INCLUDE_DIR fmod.hpp
			PATHS ${FMOD_LOCAL_INC_DIRS} )
	endif()

	if( NOT FMOD_INCLUDE_DIR )
		find_path( FMOD_INCLUDE_DIR fmod.hpp
			PATHS ${FMOD_SEARCH_PATHS}
			${FMOD_INC_PATH_SUFFIXES} )
	endif()

	if( FMOD_INCLUDE_DIR )
		message( STATUS "FMOD include files found at ${FMOD_INCLUDE_DIR}" )
		include_directories( "${FMOD_INCLUDE_DIR}" )
		
		if( EXISTS "${FMOD_INCLUDE_DIR}/fmod_common.h" )
			set( FMOD_STUDIO YES )
			set( FMOD_VERSION_FILE "fmod_common.h" )
		else()
			set( FMOD_STUDIO NO )
			set( FMOD_VERSION_FILE "fmod.h" )
		endif()

		file( STRINGS "${FMOD_INCLUDE_DIR}/${FMOD_VERSION_FILE}" FMOD_VERSION_LINE REGEX "^#define[ \t]+FMOD_VERSION[ \t]+0x[0-9]+$" )
		string( REGEX REPLACE "^#define[ \t]+FMOD_VERSION[ \t]+0x([0-9][0-9][0-9][0-9])([0-9][0-9])([0-9][0-9])$" "\\1.\\2.\\3" FMOD_VERSION "${FMOD_VERSION_LINE}" )
		message( STATUS "FMOD version: ${FMOD_VERSION}" )

		# FMOD Ex didn't hide xiph symbols in the past (not applicable to Win32 since symbols are hidden by default there).
		if( NOT WIN32 AND NOT FMOD_STUDIO AND NOT NO_OPENAL AND "${FMOD_VERSION}" VERSION_LESS "4.36.00")
			message( SEND_ERROR "Use of FMOD Ex ${FMOD_VERSION} with OpenAL will result in crashes. Either update FMOD to 4.36 or later or set NO_OPENAL." )
		endif()
	else()
		message( STATUS "Could not find FMOD include files" )
		set( NO_FMOD ON )
	endif()
endif()

if( NOT NO_FMOD )
	# Decide on the name of the FMOD library we want to use.
	if( NOT FMOD_LIB_NAME AND MSVC )
		set( FMOD_LIB_NAME fmodex${X64}_vc )
	endif()

	if( NOT FMOD_LIB_NAME AND BORLAND )
		set( FMOD_LIB_NAME fmodex${X64}_bc )
	endif()

	if( NOT FMOD_LIB_NAME )
		set( FMOD_LIB_NAME fmodex${X64} )
	endif()

	# Search for FMOD library
	if( WIN32 OR APPLE )
		find_library( FMOD_LIBRARY ${FMOD_LIB_NAME}
			PATHS ${FMOD_SEARCH_PATHS}
			${FMOD_LIB_PATH_SUFFIXES} )
	else()
		find_library( FMOD_LIBRARY
			NAMES ${FMOD_VERSIONS}
			PATHS ${FMOD_LOCAL_LIB_DIRS} )
	endif()

	if( FMOD_LIBRARY )
		message( STATUS "FMOD library found at ${FMOD_LIBRARY}" )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} "${FMOD_LIBRARY}" )
	else()
		message( STATUS "Could not find FMOD library" )
		set( NO_FMOD ON )
	endif()
endif()

if( NO_FMOD )
	add_definitions( -DNO_FMOD=1 )
endif()
if( NO_OPENAL )
	add_definitions( -DNO_OPENAL=1 )

	set(MPG123_FOUND NO)
	set(SNDFILE_FOUND NO)
else()
	# Search for libSndFile

	find_package( SndFile )

	# Search for libmpg123

	find_package( MPG123 )
endif()

# Search for FluidSynth

find_package( FluidSynth )

# Decide on SSE setup

set( SSE_MATTERS NO )

# with global use of SSE 2 we do not need special handling for selected files
if (NOT ZDOOM_USE_SSE2)
	# SSE only matters on 32-bit targets. We check compiler flags to know if we can do it.
	if( CMAKE_SIZEOF_VOID_P MATCHES "4" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES ppc )
		CHECK_CXX_COMPILER_FLAG( "-msse2 -mfpmath=sse" CAN_DO_MFPMATH )
		CHECK_CXX_COMPILER_FLAG( -arch:SSE2 CAN_DO_ARCHSSE2 )
		if( CAN_DO_MFPMATH )
			set( SSE1_ENABLE "-msse -mfpmath=sse" )
			set( SSE2_ENABLE "-msse2 -mfpmath=sse" )
			set( SSE_MATTERS YES )
		elseif( CAN_DO_ARCHSSE2 )
			set( SSE1_ENABLE -arch:SSE )
			set( SSE2_ENABLE -arch:SSE2 )
			set( SSE_MATTERS YES )
		endif()
	endif()
endif()

if( SSE_MATTERS )
	if( WIN32 )
		set( BACKPATCH 1 CACHE BOOL "Enable backpatching." )
	else()
		CHECK_FUNCTION_EXISTS(mprotect HAVE_MPROTECT)
		if( HAVE_MPROTECT )
			set( BACKPATCH 1 CACHE BOOL "Enable backpatching." )
		else()
			set( BACKPATCH 0 )
		endif()
	endif()
	set( SSE 1 CACHE BOOL "Build SSE and SSE2 versions of key code." )
else()
	set( BACKPATCH 0 )
endif()

# Set up flags for GCC

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	if( PROFILE )
		set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg" )
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg" )
		set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg" )
		set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg" )
	endif()

	set( REL_CXX_FLAGS "-fno-rtti" )
	if( NOT PROFILE AND NOT APPLE )
		# On OS X frame pointers are required for exception handling, at least with Clang
		set( REL_CXX_FLAGS "${REL_CXX_FLAGS} -fomit-frame-pointer" )
	endif()
	set( CMAKE_CXX_FLAGS_RELEASE "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" )

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.5")
		set( CMAKE_C_FLAGS "-Wno-unused-result ${CMAKE_C_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-Wno-unused-result ${CMAKE_CXX_FLAGS}" )
	endif()
	if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
		if( APPLE OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.6" )
			set( CMAKE_CXX_FLAGS "-Wno-inconsistent-missing-override ${CMAKE_CXX_FLAGS}" )
		endif()
	endif()
	set( CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_C_FLAGS}" )
	set( CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_CXX_FLAGS}" )

	# Use the highest C++ standard available since VS2015 compiles with C++14
	# but we only require C++11.  The recommended way to do this in CMake is to
	# probably to use target_compile_features, but I don't feel like maintaining
	# a list of features we use.
	CHECK_CXX_COMPILER_FLAG( "-std=gnu++14" CAN_DO_CPP14 )
	if ( CAN_DO_CPP14 )
		set ( CMAKE_CXX_FLAGS "-std=gnu++14 ${CMAKE_CXX_FLAGS}" )
	else ()
		CHECK_CXX_COMPILER_FLAG( "-std=gnu++1y" CAN_DO_CPP1Y )
		if ( CAN_DO_CPP1Y )
			set ( CMAKE_CXX_FLAGS "-std=gnu++1y ${CMAKE_CXX_FLAGS}" )
		else ()
			CHECK_CXX_COMPILER_FLAG( "-std=gnu++11" CAN_DO_CPP11 )
			if ( CAN_DO_CPP11 )
				set ( CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}" )
			else ()
				CHECK_CXX_COMPILER_FLAG( "-std=gnu++0x" CAN_DO_CPP0X )
				if ( CAN_DO_CPP0X )
					set ( CMAKE_CXX_FLAGS "-std=gnu++0x ${CMAKE_CXX_FLAGS}" )
				endif ()
			endif ()
		endif ()
	endif ()

	# With standard Apple tools -stdlib=libc++ needs to be specified in order to get
	# C++11 support using SDKs 10.7 and 10.8.
	if ( APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
		set( CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++ ${CMAKE_EXE_LINKER_FLAGS}" )
	endif ()

	# Remove extra warnings when using the official DirectX headers.
	# Also, TDM-GCC 4.4.0 no longer accepts glibc-style printf formats as valid,
	# which is a royal pain. The previous version I had been using was fine with them.
	if( WIN32 )
		set( CMAKE_CXX_FLAGS "-Wno-unknown-pragmas -Wno-comment -Wno-format ${CMAKE_CXX_FLAGS}" )
	endif()

	if( NOT NO_STRIP )
		set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s" )
		set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -s" )
	endif()
endif()

# Check for functions that may or may not exist.

CHECK_FUNCTION_EXISTS( filelength FILELENGTH_EXISTS )
if( FILELENGTH_EXISTS )
	add_definitions( -DHAVE_FILELENGTH=1 )
endif()

CHECK_FUNCTION_EXISTS( strupr STRUPR_EXISTS )
if( NOT STRUPR_EXISTS )
	add_definitions( -DNEED_STRUPR=1 )
endif()

CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
if( NOT STRICMP_EXISTS )
	add_definitions( -Dstricmp=strcasecmp )
endif()

CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
if( NOT STRNICMP_EXISTS )
	add_definitions( -Dstrnicmp=strncasecmp )
endif()

if( NOT MSVC )
	add_definitions( -D__forceinline=inline )
endif()

# Fix stat in v140_xp (broken in RTM and Update 1 so far)
if( MSVC AND MSVC_VERSION EQUAL 1900 AND CMAKE_GENERATOR_TOOLSET STREQUAL "v140_xp" )
	add_definitions( -D_stat64i32=VS14Stat )
endif()

if( UNIX )
	CHECK_LIBRARY_EXISTS( rt clock_gettime "" CLOCK_GETTIME_IN_RT )
	if( NOT CLOCK_GETTIME_IN_RT )
		CHECK_FUNCTION_EXISTS( clock_gettime CLOCK_GETTIME_EXISTS )
		if( NOT CLOCK_GETTIME_EXISTS )
			message( STATUS "Could not find clock_gettime. Timing statistics will not be available." )
			add_definitions( -DNO_CLOCK_GETTIME )
		endif()
	else()
		set( ZDOOM_LIBS ${ZDOOM_LIBS} rt )
	endif()
endif()

# Flags

if( BACKPATCH )
	add_definitions( -DBACKPATCH )
endif()

# Update gitinfo.h

add_custom_target( revision_check ALL
	COMMAND updaterevision src/gitinfo.h
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	DEPENDS updaterevision )

# Libraries ZDoom needs

message( STATUS "Fluid synth libs: ${FLUIDSYNTH_LIBRARIES}" )
set( ZDOOM_LIBS ${ZDOOM_LIBS} "${ZLIB_LIBRARIES}" "${JPEG_LIBRARIES}" "${BZIP2_LIBRARIES}" "${GME_LIBRARIES}" )
include_directories( "${ZLIB_INCLUDE_DIR}" "${BZIP2_INCLUDE_DIR}" "${LZMA_INCLUDE_DIR}" "${JPEG_INCLUDE_DIR}" "${GME_INCLUDE_DIR}" )

if( SNDFILE_FOUND )
    set( ZDOOM_LIBS ${ZDOOM_LIBS} "${SNDFILE_LIBRARIES}" )
    include_directories( "${SNDFILE_INCLUDE_DIRS}" )
endif()
if( MPG123_FOUND )
    set( ZDOOM_LIBS ${ZDOOM_LIBS} "${MPG123_LIBRARIES}" )
    include_directories( "${MPG123_INCLUDE_DIR}" )
endif()
if( NOT DYN_FLUIDSYNTH )
	if( FLUIDSYNTH_FOUND )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} "${FLUIDSYNTH_LIBRARIES}" )
		include_directories( "${FLUIDSYNTH_INCLUDE_DIR}" )
	endif()
else()
	set( ZDOOM_LIBS ${ZDOOM_LIBS} ${CMAKE_DL_LIBS} )
endif()

# Start defining source files for ZDoom
set( PLAT_WIN32_SOURCES
	win32/eaxedit.cpp
	win32/fb_d3d9.cpp
	win32/fb_d3d9_wipe.cpp
	win32/fb_ddraw.cpp
	win32/hardware.cpp
	win32/helperthread.cpp
	win32/i_cd.cpp
	win32/i_crash.cpp
	win32/i_input.cpp
	win32/i_keyboard.cpp
	win32/i_mouse.cpp
	win32/i_dijoy.cpp
	win32/i_rawps2.cpp
	win32/i_xinput.cpp
	win32/i_main.cpp
	win32/i_movie.cpp
	win32/i_system.cpp
	win32/i_specialpaths.cpp
	win32/st_start.cpp
	win32/win32video.cpp )
set( PLAT_POSIX_SOURCES
	posix/i_cd.cpp
	posix/i_movie.cpp
	posix/i_steam.cpp )
set( PLAT_SDL_SOURCES
	posix/sdl/crashcatcher.c
	posix/sdl/hardware.cpp
	posix/sdl/i_gui.cpp
	posix/sdl/i_input.cpp
	posix/sdl/i_joystick.cpp
	posix/sdl/i_main.cpp
	posix/sdl/i_system.cpp
	posix/sdl/i_timer.cpp
	posix/sdl/sdlvideo.cpp
	posix/sdl/st_start.cpp )
set( PLAT_UNIX_SOURCES
	posix/unix/i_specialpaths.cpp
	posix/unix/iwadpicker_gtk.cpp )
set( PLAT_OSX_SOURCES
	posix/osx/iwadpicker_cocoa.mm
	posix/osx/i_specialpaths.mm
	posix/osx/zdoom.icns )
set( PLAT_COCOA_SOURCES
	posix/cocoa/critsec.cpp
	posix/cocoa/i_input.mm
	posix/cocoa/i_joystick.cpp
	posix/cocoa/i_main.mm
	posix/cocoa/i_main_except.cpp
	posix/cocoa/i_system.mm
	posix/cocoa/i_timer.cpp
	posix/cocoa/i_video.mm
	posix/cocoa/st_console.mm
	posix/cocoa/st_start.mm )

if( WIN32 )
	set( SYSTEM_SOURCES_DIR win32 )
	set( SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} )
	set( OTHER_SYSTEM_SOURCES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES} )
	
	set( SYSTEM_SOURCES ${SYSTEM_SOURCES} win32/zdoom.rc )
elseif( APPLE )
	if( OSX_COCOA_BACKEND )
		set( SYSTEM_SOURCES_DIR posix posix/cocoa )
		set( SYSTEM_SOURCES ${PLAT_COCOA_SOURCES} )
		set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES} )
	else()
		set( SYSTEM_SOURCES_DIR posix posix/sdl )
		set( SYSTEM_SOURCES ${PLAT_SDL_SOURCES} )
		set( PLAT_OSX_SOURCES ${PLAT_OSX_SOURCES} posix/sdl/i_system.mm )
		set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES} )
	endif()

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES} ${PLAT_POSIX_SOURCES} ${PLAT_OSX_SOURCES} "${FMOD_LIBRARY}" )

	set_source_files_properties( posix/osx/zdoom.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
	set_source_files_properties( "${FMOD_LIBRARY}" PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks )
	set_source_files_properties( posix/osx/iwadpicker_cocoa.mm PROPERTIES COMPILE_FLAGS -fobjc-exceptions )
else()
	set( SYSTEM_SOURCES_DIR posix posix/sdl )
	set( SYSTEM_SOURCES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES} )
	set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} )
endif()

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/xlat/xlat_parser.y
	DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/xlat/xlat_parser.y )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/scripting/zscript/zcc-parse.lemon
	DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/scripting/zscript/zcc-parse.lemon )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h
	COMMAND re2c --no-generation-date -s -o ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h ${CMAKE_CURRENT_SOURCE_DIR}/sc_man_scanner.re
	DEPENDS re2c ${CMAKE_CURRENT_SOURCE_DIR}/sc_man_scanner.re )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

if( SSE_MATTERS )
	if( SSE )
		set( X86_SOURCES nodebuild_classify_sse2.cpp )
		set_source_files_properties( nodebuild_classify_sse2.cpp PROPERTIES COMPILE_FLAGS "${SSE2_ENABLE}" )
	else()
		add_definitions( -DDISABLE_SSE )
	endif()
else()
	add_definitions( -DDISABLE_SSE )
	set( X86_SOURCES )
endif()

if( SNDFILE_FOUND )
    add_definitions( -DHAVE_SNDFILE )
endif()
if( MPG123_FOUND )
    add_definitions( -DHAVE_MPG123 )
endif()
if( DYN_FLUIDSYNTH )
	add_definitions( -DHAVE_FLUIDSYNTH -DDYN_FLUIDSYNTH )
elseif( FLUIDSYNTH_FOUND )
	add_definitions( -DHAVE_FLUIDSYNTH )
endif()

# Project files should be aware of the header files. We can GLOB these since
# there's generally a new cpp for every header so this file will get changed
if( WIN32 )
	set( EXTRA_HEADER_DIRS win32/*.h )
elseif( APPLE )
	if( OSX_COCOA_BACKEND )
		set( EXTRA_HEADER_DIRS posix/*.h posix/cocoa/*.h )
	else()
		set( EXTRA_HEADER_DIRS posix/*.h posix/sdl/*.h )
	endif()
else()
	set( EXTRA_HEADER_DIRS posix/*.h posix/sdl/*.h )
endif()
file( GLOB HEADER_FILES
	${EXTRA_HEADER_DIRS}
	fragglescript/*.h
	g_shared/*.h
	g_inventory/*.h
	g_strife/*.h
	intermission/*.h
	menu/*.h
	oplsynth/*.h
	oplsynth/dosbox/*.h
	posix/*.h
	posix/cocoa/*.h
	posix/sdl/*.h
	r_data/*.h
	rapidjson/*.h
	resourcefiles/*.h
	sfmt/*.h
	sound/*.h
	textures/*.h
	scripting/*.h
	scripting/codegeneration/*.h
	scripting/decorate/*.h
	scripting/zscript/*.h
	scripting/vm/*.h
	xlat/*.h
	*.h
)

# These files will be flagged as "headers" so that they appear in project files
# without being compiled.
set( NOT_COMPILED_SOURCE_FILES
	${OTHER_SYSTEM_SOURCES}
	sc_man_scanner.h
	sc_man_scanner.re
	g_shared/sbarinfo_commands.cpp
	xlat/xlat_parser.y
	xlat_parser.c
	xlat_parser.h
	scripting/zscript/zcc-parse.lemon
	zcc-parse.c
	zcc-parse.h
)

set( FASTMATH_PCH_SOURCES
	r_swrenderer.cpp
	r_3dfloors.cpp
	r_bsp.cpp
	r_draw.cpp
	r_draw_pal.cpp
	r_drawt_pal.cpp
	r_thread.cpp
	r_main.cpp
	r_plane.cpp
	r_segs.cpp
	r_sky.cpp
	r_things.cpp
	r_walldraw.cpp
	s_advsound.cpp
	s_environment.cpp
	s_playlist.cpp
	s_sndseq.cpp
	s_sound.cpp
	GuillotineBinPack.cpp
	SkylineBinPack.cpp
	intermission/intermission.cpp
	intermission/intermission_parse.cpp
	menu/colorpickermenu.cpp
	menu/joystickmenu.cpp
	menu/listmenu.cpp
	menu/loadsavemenu.cpp
	menu/menu.cpp
	menu/menudef.cpp
	menu/menuinput.cpp
	menu/messagebox.cpp
	menu/optionmenu.cpp
	menu/playerdisplay.cpp
	menu/playermenu.cpp
	menu/readthis.cpp
	menu/videomenu.cpp
	oplsynth/fmopl.cpp
	oplsynth/mlopl.cpp
	oplsynth/mlopl_io.cpp
	oplsynth/dosbox/opl.cpp
	oplsynth/OPL3.cpp
	oplsynth/nukedopl3.cpp
	timidity/common.cpp
	timidity/instrum.cpp
	timidity/instrum_dls.cpp
	timidity/instrum_font.cpp
	timidity/instrum_sf2.cpp
	timidity/mix.cpp
	timidity/playmidi.cpp
	timidity/resample.cpp
	timidity/timidity.cpp
	wildmidi/file_io.cpp
	wildmidi/gus_pat.cpp
	wildmidi/reverb.cpp
	wildmidi/wm_error.cpp
	r_data/colormaps.cpp
	r_data/r_translate.cpp
)
# This is disabled for now because I cannot find a way to give the .pch file a different name.
# Visual C++ 2015 seems hell-bent of only allowing one .pch file with the same name as the executable.
#enable_precompiled_headers( g_pch2.h FASTMATH_PCH_SOURCES )

# Enable fast math for some sources
set( FASTMATH_SOURCES
	${FASTMATH_PCH_SOURCES}
	oplsynth/music_opldumper_mididevice.cpp
	oplsynth/music_opl_mididevice.cpp
	oplsynth/opl_mus_player.cpp
	sound/fmodsound.cpp
	sound/i_music.cpp
	sound/i_sound.cpp
	sound/mpg123_decoder.cpp
	sound/music_cd.cpp
	sound/music_dumb.cpp
	sound/music_gme.cpp
	sound/music_mus_midiout.cpp
	sound/music_smf_midiout.cpp
	sound/music_hmi_midiout.cpp
	sound/music_xmi_midiout.cpp
	sound/music_midistream.cpp
	sound/music_midi_base.cpp
	sound/music_midi_timidity.cpp
	sound/music_mus_opl.cpp
	sound/music_stream.cpp
	sound/music_fluidsynth_mididevice.cpp
	sound/music_softsynth_mididevice.cpp
	sound/music_timidity_mididevice.cpp
	sound/music_wildmidi_mididevice.cpp
	sound/music_win_mididevice.cpp
	sound/oalsound.cpp
	sound/sndfile_decoder.cpp
	sound/music_pseudo_mididevice.cpp
	wildmidi/wildmidi_lib.cpp
)

set (PCH_SOURCES
	actorptrselect.cpp
	am_map.cpp
	b_bot.cpp
	b_func.cpp
	b_game.cpp
	b_move.cpp
	b_think.cpp
	bbannouncer.cpp
	c_bind.cpp
	c_cmds.cpp
	c_console.cpp
	c_consolebuffer.cpp
	c_cvars.cpp
	c_dispatch.cpp
	c_expr.cpp
	cmdlib.cpp
	colormatcher.cpp
	compatibility.cpp
	configfile.cpp
	ct_chat.cpp
	d_dehacked.cpp
	d_iwad.cpp
	d_main.cpp
	d_net.cpp
	d_netinfo.cpp
	d_protocol.cpp
	decallib.cpp
	dobject.cpp
	dobjgc.cpp
	dobjtype.cpp
	doomdef.cpp
	doomstat.cpp
	dsectoreffect.cpp
	dthinker.cpp
	edata.cpp
	f_wipe.cpp
	files.cpp
	g_doomedmap.cpp
	g_game.cpp
	g_hub.cpp
	g_level.cpp
	g_mapinfo.cpp
	g_skill.cpp
	gameconfigfile.cpp
	gi.cpp
	gitinfo.cpp
	hu_scores.cpp
	i_module.cpp
	i_net.cpp
	info.cpp
	keysections.cpp
	lumpconfigfile.cpp
	m_alloc.cpp
	m_argv.cpp
	m_bbox.cpp
	m_cheat.cpp
	m_joy.cpp
	m_misc.cpp
	m_png.cpp
	m_random.cpp
	memarena.cpp
	md5.cpp
	name.cpp
	nodebuild.cpp
	nodebuild_classify_nosse2.cpp
	nodebuild_events.cpp
	nodebuild_extract.cpp
	nodebuild_gl.cpp
	nodebuild_utility.cpp
	pathexpander.cpp
	p_3dfloors.cpp
	p_3dmidtex.cpp
	p_acs.cpp
	p_actionfunctions.cpp
	p_buildmap.cpp
	p_ceiling.cpp
	p_conversation.cpp
	p_doors.cpp
	p_effect.cpp
	p_enemy.cpp
	p_floor.cpp
	p_glnodes.cpp
	p_interaction.cpp
	p_lights.cpp
	p_linkedsectors.cpp
	p_lnspec.cpp
	p_map.cpp
	p_maputl.cpp
	p_mobj.cpp
	p_pillar.cpp
	p_plats.cpp
	p_pspr.cpp
	p_pusher.cpp
	p_saveg.cpp
	p_scroll.cpp
	p_sectors.cpp
	p_setup.cpp
	p_sight.cpp
	p_slopes.cpp
	p_spec.cpp
	p_states.cpp
	p_switch.cpp
	p_tags.cpp
	p_teleport.cpp
	p_terrain.cpp
	p_things.cpp
	p_tick.cpp
	p_trace.cpp
	p_udmf.cpp
	p_usdf.cpp
	p_user.cpp
	p_writemap.cpp
	p_xlat.cpp
	parsecontext.cpp
	po_man.cpp
	portal.cpp
	r_utility.cpp
	serializer.cpp
	sc_man.cpp
	st_stuff.cpp
	statistics.cpp
	stats.cpp
	stringtable.cpp
	teaminfo.cpp
	tempfiles.cpp
	v_blend.cpp
	v_collection.cpp
	v_draw.cpp
	v_font.cpp
	v_palette.cpp
	v_pfx.cpp
	v_text.cpp
	v_video.cpp
	w_wad.cpp
	wi_stuff.cpp
	zstrformat.cpp
	g_inventory/a_ammo.cpp
	g_inventory/a_armor.cpp
	g_inventory/a_artifacts.cpp
	g_inventory/a_health.cpp
	g_inventory/a_keys.cpp
	g_inventory/a_pickups.cpp
	g_inventory/a_puzzleitems.cpp
	g_inventory/a_weaponpiece.cpp
	g_inventory/a_weapons.cpp
	g_strife/strife_sbar.cpp
	g_shared/a_action.cpp
	g_shared/a_bridge.cpp
	g_shared/a_decals.cpp
	g_shared/a_fastprojectile.cpp
	g_shared/a_flashfader.cpp
	g_shared/a_fountain.cpp
	g_shared/a_lightning.cpp
	g_shared/a_morph.cpp
	g_shared/a_movingcamera.cpp
	g_shared/a_quake.cpp
	g_shared/a_randomspawner.cpp
	g_shared/a_sectoraction.cpp
	g_shared/a_skies.cpp
	g_shared/a_soundenvironment.cpp
	g_shared/a_soundsequence.cpp
	g_shared/a_specialspot.cpp
	g_shared/hudmessages.cpp
	g_shared/sbarinfo.cpp
	g_shared/sbar_mugshot.cpp
	g_shared/shared_hud.cpp
	g_shared/shared_sbar.cpp
	resourcefiles/ancientzip.cpp
	resourcefiles/file_7z.cpp
	resourcefiles/file_grp.cpp
	resourcefiles/file_lump.cpp
	resourcefiles/file_rff.cpp
	resourcefiles/file_wad.cpp
	resourcefiles/file_zip.cpp
	resourcefiles/file_pak.cpp
	resourcefiles/file_directory.cpp
	resourcefiles/resourcefile.cpp
	textures/animations.cpp
	textures/anim_switches.cpp
	textures/automaptexture.cpp
	textures/bitmap.cpp
	textures/buildtexture.cpp
	textures/canvastexture.cpp
	textures/ddstexture.cpp
	textures/flattexture.cpp
	textures/imgztexture.cpp
	textures/jpegtexture.cpp
	textures/multipatchtexture.cpp
	textures/patchtexture.cpp
	textures/pcxtexture.cpp
	textures/pngtexture.cpp
	textures/rawpagetexture.cpp
	textures/emptytexture.cpp
	textures/texture.cpp
	textures/texturemanager.cpp
	textures/tgatexture.cpp
	textures/warptexture.cpp
	xlat/parse_xlat.cpp
	fragglescript/t_func.cpp
	fragglescript/t_load.cpp
	fragglescript/t_oper.cpp
	fragglescript/t_parse.cpp
	fragglescript/t_prepro.cpp
	fragglescript/t_script.cpp
	fragglescript/t_spec.cpp
	fragglescript/t_variable.cpp
	fragglescript/t_cmd.cpp
	r_data/sprites.cpp
	r_data/voxels.cpp
	r_data/renderstyle.cpp
	r_data/r_interpolate.cpp
	scripting/thingdef.cpp
	scripting/thingdef_data.cpp
	scripting/thingdef_properties.cpp
	scripting/codegeneration/codegen.cpp
	scripting/decorate/olddecorations.cpp
	scripting/decorate/thingdef_exp.cpp
	scripting/decorate/thingdef_parse.cpp
	scripting/decorate/thingdef_states.cpp
	scripting/vm/vmbuilder.cpp
	scripting/vm/vmdisasm.cpp
	scripting/vm/vmexec.cpp
	scripting/vm/vmframe.cpp
	scripting/zscript/ast.cpp
	scripting/zscript/zcc_compile.cpp
	scripting/zscript/zcc_expr.cpp
	scripting/zscript/zcc_parser.cpp
	sfmt/SFMT.cpp
)
enable_precompiled_headers( g_pch.h PCH_SOURCES )

add_executable( zdoom WIN32 MACOSX_BUNDLE
	${HEADER_FILES}
	${NOT_COMPILED_SOURCE_FILES}
	__autostart.cpp
	${SYSTEM_SOURCES}
	${X86_SOURCES}
	${FASTMATH_SOURCES}
	${PCH_SOURCES}
	x86.cpp
	strnatcmp.c
	zstring.cpp
	math/asin.c
	math/atan.c
	math/const.c
	math/cosh.c
	math/exp.c
	math/isnan.c
	math/log.c
	math/log10.c
	math/mtherr.c
	math/polevl.c
	math/pow.c
	math/powi.c
	math/sin.c
	math/sinh.c
	math/sqrt.c
	math/tan.c
	math/tanh.c
	math/fastsin.cpp
	zzautozend.cpp
)

set_source_files_properties( ${FASTMATH_SOURCES} PROPERTIES COMPILE_FLAGS ${ZD_FASTMATH_FLAG} )
set_source_files_properties( xlat/parse_xlat.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c" )
set_source_files_properties( sc_man.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h" )
set_source_files_properties( ${NOT_COMPILED_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE )
if ( WIN32 )
	set_source_files_properties( win32/fb_d3d9.cpp win32/fb_d3d9_wipe.cpp PROPERTIES COMPILE_FLAGS ${ZD_FASTMATH_FLAG} )
endif()


if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
	# [BL] Solaris requires these to be explicitly linked.
	set( ZDOOM_LIBS ${ZDOOM_LIBS} nsl socket)
endif()

target_link_libraries( zdoom ${ZDOOM_LIBS} gdtoa dumb lzma )
include_directories( .
	g_strife
	g_shared
	g_inventory
	oplsynth
	sound
	textures
	timidity
	wildmidi
	xlat
	scripting
	scripting/vm
	../gdtoa
	../dumb/include
	${CMAKE_BINARY_DIR}/gdtoa
	${SYSTEM_SOURCES_DIR} )

add_dependencies( zdoom revision_check )

# Due to some quirks, we need to do this in this order
if( NOT ZDOOM_OUTPUT_OLDSTYLE )
	# RUNTIME_OUTPUT_DIRECTORY does not exist in CMake 2.4.
	# Linux distributions are slow to adopt 2.6. :(
	set_target_properties( zdoom PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZDOOM_OUTPUT_DIR} )
	set_target_properties( zdoom PROPERTIES OUTPUT_NAME ${ZDOOM_EXE_NAME} )
else()
	set_target_properties( zdoom PROPERTIES
		RUNTIME_OUTPUT_NAME ${ZDOOM_EXE_NAME}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_DEBUG ${ZDOOM_EXE_NAME}d
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_MINSIZEREL ${ZDOOM_EXE_NAME}msr
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_RELWITHDEBINFO ${ZDOOM_EXE_NAME}rd
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${ZDOOM_OUTPUT_DIR}
	)
endif()

if( MSVC )
	option( ZDOOM_GENERATE_MAPFILE "Generate .map file for debugging." OFF )
	set( LINKERSTUFF "/MANIFEST:NO" )

	if( NOT NO_FMOD )
		if( FMOD_STUDIO )
			set( LINKERSTUFF "${LINKERSTUFF} /DELAYLOAD:\"fmod${X64}.dll\"" )
		else()
			set( LINKERSTUFF "${LINKERSTUFF} /DELAYLOAD:\"fmodex${X64}.dll\"" )
		endif()
	endif()

	if( ZDOOM_GENERATE_MAPFILE )
		set( LINKERSTUFF "${LINKERSTUFF} /MAP" )
	endif()
	if( NOT NO_OPENAL )
		set( LINKERSTUFF "${LINKERSTUFF} /DELAYLOAD:\"libmpg123-0.dll\" /DELAYLOAD:\"libsndfile-1.dll\"" )
	endif()
	set_target_properties(zdoom PROPERTIES LINK_FLAGS ${LINKERSTUFF})

	add_custom_command(TARGET zdoom POST_BUILD
		COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\win32\\zdoom.exe.manifest\" -outputresource:\"$<TARGET_FILE:zdoom>\"\;\#1
		COMMENT "Adding manifest..."
	)

	create_default_target_launcher( zdoom WORKING_DIRECTORY ${ZDOOM_OUTPUT_DIR} )
endif()

if( NOT WIN32 )
	FILE( WRITE ${CMAKE_CURRENT_BINARY_DIR}/link-make "if [ ! -e ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME} ]; then ln -sf ${CMAKE_CURRENT_BINARY_DIR}/${ZDOOM_EXE_NAME} ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME}; fi" )
	add_custom_command( TARGET zdoom POST_BUILD
		COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/link-make
		COMMAND /bin/sh -c ${CMAKE_CURRENT_BINARY_DIR}/link-make )
endif()
if( CMAKE_COMPILER_IS_GNUCXX )
	# GCC misoptimizes this file
	set_source_files_properties( oplsynth/fmopl.cpp PROPERTIES COMPILE_FLAGS "-fno-tree-dominator-opts -fno-tree-fre" )
endif()
if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	# Need to enable intrinsics for this file.
	if( SSE_MATTERS )
		set_source_files_properties( x86.cpp PROPERTIES COMPILE_FLAGS "-msse2 -mmmx" )
	endif()
endif()

if( APPLE )
	set_target_properties(zdoom PROPERTIES
		LINK_FLAGS "-framework Carbon -framework Cocoa -framework IOKit -framework OpenGL"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/posix/osx/zdoom-info.plist" )

	if( NOT NO_FMOD )
		if( FMOD_STUDIO )
			set( FMOD_DYLIB libfmod.dylib )
		else()
			set( FMOD_DYLIB libfmodex.dylib )
		endif()

		# Fix fmod link so that it can be found in the app bundle.
		find_program( OTOOL otool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin" )
		find_program( INSTALL_NAME_TOOL install_name_tool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin" )
		execute_process( COMMAND "${OTOOL}" -L "${FMOD_LIBRARY}"
			COMMAND grep "${FMOD_DYLIB} (compat"
			COMMAND head -n1
			COMMAND awk "{print $1}"
			OUTPUT_VARIABLE FMOD_LINK
			OUTPUT_STRIP_TRAILING_WHITESPACE )
		add_custom_command( TARGET zdoom POST_BUILD
			COMMAND "${INSTALL_NAME_TOOL}" -change "${FMOD_LINK}" @executable_path/../Frameworks/${FMOD_DYLIB} "$<TARGET_FILE:zdoom>"
			COMMENT "Relinking FMOD Ex" )
	endif()
endif()

if( WIN32 )
	set( INSTALL_PATH . CACHE STRING "Directory where the zdoom executable will be placed during install." )
else()
	set( INSTALL_PATH bin CACHE STRING "Directory where the zdoom executable will be placed during install." )
endif()
install(TARGETS zdoom
		DESTINATION ${INSTALL_PATH}
		COMPONENT "Game executable")

source_group("Audio Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/.+")
source_group("Audio Files\\OPL Synth" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/oplsynth/.+")
source_group("Audio Files\\OPL Synth\\DOSBox" FILES oplsynth/dosbox/opl.cpp oplsynth/dosbox/opl.h)
source_group("Audio Files\\Timidity\\Headers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/timidity/.+\\.h$")
source_group("Audio Files\\Timidity\\Source" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/timidity/.+\\.cpp$")
source_group("Audio Files\\WildMidi\\Headers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/wildmidi/.+\\.h$")
source_group("Audio Files\\WildMidi\\Source" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/wildmidi/.+\\.cpp$")
source_group("External\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/math/.+")
source_group("External\\RapidJSON" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/.+")
source_group("Externak\\SFMT" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sfmt/.+")
source_group("FraggleScript" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/fragglescript/.+")
source_group("Games\\Strife Game" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_strife/.+")
source_group("Intermission" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/intermission/.+")
source_group("Inventory" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_inventory/.+")
source_group("Menu" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/menu/.+")
source_group("Render Core\\Render Headers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_.+\\.h$")
source_group("Render Core\\Render Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_.+\\.cpp$")
source_group("Render Data\\Resource Headers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_data/.+\\.h$")
source_group("Render Data\\Resource Sources" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_data/.+\\.cpp$")
source_group("Render Data\\Textures" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/textures/.+")
source_group("Render Interface" FILES r_defs.h r_renderer.h r_sky.cpp r_sky.h r_state.h r_utility.cpp r_utility.h)
source_group("Resource Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/resourcefiles/.+")
source_group("Platforms\\POSIX Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/.+")
source_group("Platforms\\Cocoa Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/cocoa/.+")
source_group("Platforms\\OS X Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/osx/.+")
source_group("Platforms\\Unix Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/unix/.+")
source_group("Platforms\\SDL Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/sdl/.+")
source_group("Platforms\\Win32 Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/win32/.+")
source_group("Scripting\\Decorate" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/decorate/.+")
source_group("Scripting\\ZScript" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/zscript/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h)
source_group("Scripting\\Code Generation" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/codegeneration/.+")
source_group("Scripting\\VM" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/vm/.+")
source_group("Scripting" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/.+")
source_group("Shared Game" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_shared/.+")
source_group("Versioning" FILES version.h win32/zdoom.rc)
source_group("Xlat" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/xlat/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h)
source_group("Source Files" FILES ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h sc_man_scanner.re)
