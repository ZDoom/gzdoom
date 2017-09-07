cmake_minimum_required( VERSION 2.8.7 )

include(precompiled_headers)

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
include( TargetArch )

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
option( DYN_SNDFILE "Dynamically load libsndfile" ON )
option( DYN_MPG123 "Dynamically load libmpg123" ON )

if( APPLE )
    option( OSX_COCOA_BACKEND "Use native Cocoa backend instead of SDL" ON )
endif()

target_architecture(ZDOOM_TARGET_ARCH)

if( ${ZDOOM_TARGET_ARCH} MATCHES "x86_64" )
	set( X64 64 )
endif()


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
	
	
	if( ( MSVC14 AND NOT CMAKE_GENERATOR_TOOLSET STREQUAL "v140_xp" ) OR	# For VS 2015.
		( MSVC15 AND NOT CMAKE_GENERATOR_TOOLSET STREQUAL "v141_xp" ) )		# For VS 2017.
		# for modern Windows SDKs the DirectX headers should be available by default.
		set( DX_dinput8_LIBRARY dinput8 )
	else()
		
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

		# Modern versions of the Windows SDK include dinput8.lib. Unfortunately,
		# CMake cannot find these libraries via find_library.
		if( NOT DX_dinput8_LIBRARY )
			# If we got this far, assume dinput8.lib is in the system library path.
			set( DX_dinput8_LIBRARY dinput8 )
		endif()

	endif()

	set( ZDOOM_LIBS
		opengl32
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

else()
	if( APPLE )
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
	if ( NOT DYN_OPENAL )	# DYN_OPENAL uses local copies of the headers.
		find_package( OpenAL )
		mark_as_advanced(CLEAR OPENAL_INCLUDE_DIR)
		if( OPENAL_INCLUDE_DIR )
			include_directories( ${OPENAL_INCLUDE_DIR} )
			mark_as_advanced(CLEAR OPENAL_LIBRARY)
			if( OPENAL_LIBRARY )
				set( ZDOOM_LIBS ${OPENAL_LIBRARY} ${ZDOOM_LIBS} )
			else()
				set( NO_OPENAL ON )
			endif()
		else()
			set( NO_OPENAL ON )
		endif()
	else()
		add_definitions( -DDYN_OPENAL )
	endif()
endif()

if( NO_OPENAL )
	add_definitions( -DNO_OPENAL=1 )

	set(MPG123_FOUND NO)
	set(SNDFILE_FOUND NO)
else()
	# Search for libSndFile

	if ( NOT DYN_SNDFILE )
		find_package( SndFile )
	endif()

	# Search for libmpg123

	if ( NOT DYN_MPG123 )
		find_package( MPG123 )
	endif()
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

if( X64 )
	set( HAVE_MMX 1 )
else( X64 )
	set( SAFE_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

	if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmmx")
	endif( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )

	CHECK_CXX_SOURCE_COMPILES("#include <mmintrin.h>
		int main(void) { __m64 v = _m_from_int(0); }"
		HAVE_MMX)

	set( CMAKE_CXX_FLAGS ${SAFE_CMAKE_CXX_FLAGS} )
endif( X64 )

CHECK_CXX_SOURCE_COMPILES("#include <ppl.h>
	int main() { concurrency::parallel_for(0, 1, 1, [](int) { } ); }"
	HAVE_PARALLEL_FOR)

if( NOT HAVE_PARALLEL_FOR )
	CHECK_CXX_SOURCE_COMPILES("#include <dispatch/dispatch.h>
		int main() { dispatch_apply(1, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t) { }); }"
		HAVE_DISPATCH_APPLY)
endif()

# Set up flags for MSVC
if (MSVC)
	set( CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}" )
endif (MSVC)

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

	# Support for the GCC/Clang sanitizers.
	set( WITH_ASAN 0 CACHE BOOL "Enable the Address Sanitizer")
	if( NOT CMAKE_COMPILER_IS_GNUCXX )
		set( WITH_MSAN 0 CACHE BOOL "Enable the Memory Sanitizer")
	endif( NOT CMAKE_COMPILER_IS_GNUCXX )
	set( WITH_UBSAN 0 CACHE BOOL "Enable the Undefined Behavior Sanitizer")
	if( WITH_MSAN )
		if ( WITH_ASAN OR WITH_UBSAN )
			message( SEND_ERROR "You can't use MSAN with either ASAN or UBSAN." )
		endif ( WITH_ASAN OR WITH_UBSAN )
	endif( WITH_MSAN )

	set( SANITIZER_FLAG "" )
	if( WITH_ASAN )
		set( SANITIZER_FLAG "-fsanitize=address" )
		if ( WITH_UBSAN )
			set( SANITIZER_FLAG "${SANITIZER_FLAG},undefined" )
		endif( WITH_UBSAN )
	elseif( WITH_MSAN )
		set( SANITIZER_FLAG "-fsanitize=memory" )
	elseif( WITH_UBSAN )
		set( SANITIZER_FLAG "-fsanitize=undefined" )
	endif( WITH_ASAN )

	set( CMAKE_CXX_FLAGS "${SANITIZER_FLAG} ${CMAKE_CXX_FLAGS}" )
	set( CMAKE_C_FLAGS "${SANITIZER_FLAG} ${CMAKE_C_FLAGS}" )

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

	# ARM processors (Raspberry Pi, et al) - enable ARM NEON support.
	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
		set (USE_ARMV8 0 CACHE BOOL "Use ARMv8 instructions - Raspberry Pi 3")
		if (USE_ARMV8)
			set( CMAKE_CXX_FLAGS "-mcpu=cortex-a53  -mfpu=neon-fp-armv8 -mtune=cortex-a53 -mhard-float -DNO_SSE ${CMAKE_CXX_FLAGS}" )
		else ()
			set( CMAKE_CXX_FLAGS "-mfpu=neon -DNO_SSE ${CMAKE_CXX_FLAGS}" )
		endif ()
	endif ()

	if( NOT X64 AND NOT CAN_DO_MFPMATH )
		set( CMAKE_C_FLAGS "-DNO_SSE ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-DNO_SSE ${CMAKE_CXX_FLAGS}" )
	endif()

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

# Update gitinfo.h

add_custom_target( revision_check ALL
	COMMAND updaterevision src/gitinfo.h
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	DEPENDS updaterevision )

# Libraries ZDoom needs

message( STATUS "Fluid synth libs: ${FLUIDSYNTH_LIBRARIES}" )
set( ZDOOM_LIBS ${ZDOOM_LIBS} "${ZLIB_LIBRARIES}" "${JPEG_LIBRARIES}" "${BZIP2_LIBRARIES}" "${GME_LIBRARIES}" "${CMAKE_DL_LIBS}" )
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
endif()

# Start defining source files for ZDoom
set( PLAT_WIN32_SOURCES
	sound/mididevices/music_win_mididevice.cpp
	win32/eaxedit.cpp
	win32/critsec.cpp
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
	win32/i_system.cpp
	win32/i_specialpaths.cpp
	win32/st_start.cpp
	win32/win32gliface.cpp
	win32/win32video.cpp )
set( PLAT_POSIX_SOURCES
	posix/i_cd.cpp
	posix/i_steam.cpp )
set( PLAT_SDL_SOURCES
	posix/sdl/crashcatcher.c
	posix/sdl/critsec.cpp
	posix/sdl/hardware.cpp
	posix/sdl/i_gui.cpp
	posix/sdl/i_input.cpp
	posix/sdl/i_joystick.cpp
	posix/sdl/i_main.cpp
	posix/sdl/i_system.cpp
	posix/sdl/i_timer.cpp
	posix/sdl/sdlvideo.cpp
	posix/sdl/sdlglvideo.cpp
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

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES} ${PLAT_POSIX_SOURCES} ${PLAT_OSX_SOURCES} )

	set_source_files_properties( posix/osx/zdoom.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
	set_source_files_properties( posix/osx/iwadpicker_cocoa.mm PROPERTIES COMPILE_FLAGS -fobjc-exceptions )
else()
	set( SYSTEM_SOURCES_DIR posix posix/sdl )
	set( SYSTEM_SOURCES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES} )
	set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} )
endif()

if( HAVE_MMX )
	add_definitions( -DHAVE_MMX=1 )

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES}
		gl/hqnx_asm/hq2x_asm.cpp
		gl/hqnx_asm/hq3x_asm.cpp
		gl/hqnx_asm/hq4x_asm.cpp
		gl/hqnx_asm/hqnx_asm_Image.cpp)

	if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set_source_files_properties(
			gl/hqnx_asm/hq2x_asm.cpp
			gl/hqnx_asm/hq3x_asm.cpp
			gl/hqnx_asm/hq4x_asm.cpp
			gl/textures/gl_hqresize.cpp
			PROPERTIES COMPILE_FLAGS "-mmmx" )
	endif( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
endif( HAVE_MMX )

if( HAVE_PARALLEL_FOR )
	add_definitions( -DHAVE_PARALLEL_FOR=1 )
elseif( HAVE_DISPATCH_APPLY )
	add_definitions( -DHAVE_DISPATCH_APPLY=1 )
else()
	option( NO_OPENMP "Disable usage of OpenMP" OFF )

	if( NOT NO_OPENMP )
		include( FindOpenMP )

		if( OPENMP_FOUND )
			set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}" )
			set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
		endif( OPENMP_FOUND )
	endif( NOT NO_OPENMP )
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
	g_statusbar/*.h
	g_inventory/*.h
	intermission/*.h
	menu/*.h
	sound/oplsynth/*.h
	sound/oplsynth/dosbox/*.h
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
	scripting/backend/*.h
	scripting/decorate/*.h
	scripting/zscript/*.h
	scripting/vm/*.h
	sound/timidity/*.h
	sound/wildmidi/*.h
	xlat/*.h
	swrenderer/*.h
	swrenderer/drawers/*.h
	swrenderer/scene/*.h
	swrenderer/segments/*.h
	swrenderer/line/*.h
	swrenderer/plane/*.h
	swrenderer/things/*.h
	swrenderer/viewport/*.h
	polyrenderer/*.h
	polyrenderer/math/*.h
	polyrenderer/drawers/*.h
	polyrenderer/scene/*.h
	gl/*.h
	gl/api/*.h
	gl/data/*.h
	gl/dynlights/*.h
	gl/hqnx/*.h
	gl/hqnx_asm/*.h
	gl/xbr/*.h
	gl/models/*.h
	gl/renderer/*.h
	gl/scene/*.h
	gl/stereo3d/*.h
	gl/shaders/*.h
	gl/system/*.h
	gl/textures/*.h
	gl/utility/*.h
	*.h
)

set ( SWRENDER_SOURCES
	swrenderer/r_swcanvas.cpp
	swrenderer/r_swcolormaps.cpp
	swrenderer/r_swrenderer.cpp
	swrenderer/r_memory.cpp
	swrenderer/r_renderthread.cpp
	swrenderer/drawers/r_draw_pal.cpp
	swrenderer/drawers/r_draw_rgba.cpp
	swrenderer/drawers/r_thread.cpp
	swrenderer/scene/r_3dfloors.cpp
	swrenderer/scene/r_light.cpp
	swrenderer/scene/r_opaque_pass.cpp
	swrenderer/scene/r_portal.cpp
	swrenderer/scene/r_scene.cpp
	swrenderer/scene/r_translucent_pass.cpp
	swrenderer/viewport/r_drawerargs.cpp
	swrenderer/viewport/r_skydrawer.cpp
	swrenderer/viewport/r_spandrawer.cpp
	swrenderer/viewport/r_spritedrawer.cpp
	swrenderer/viewport/r_viewport.cpp
	swrenderer/viewport/r_walldrawer.cpp
	swrenderer/line/r_line.cpp
	swrenderer/line/r_farclip_line.cpp
	swrenderer/line/r_walldraw.cpp
	swrenderer/line/r_wallsetup.cpp
	swrenderer/line/r_fogboundary.cpp
	swrenderer/line/r_renderdrawsegment.cpp
	swrenderer/segments/r_clipsegment.cpp
	swrenderer/segments/r_drawsegment.cpp
	swrenderer/segments/r_portalsegment.cpp
	swrenderer/things/r_visiblesprite.cpp
	swrenderer/things/r_visiblespritelist.cpp
	swrenderer/things/r_voxel.cpp
	swrenderer/things/r_particle.cpp
	swrenderer/things/r_playersprite.cpp
	swrenderer/things/r_sprite.cpp
	swrenderer/things/r_wallsprite.cpp
	swrenderer/things/r_decal.cpp
	swrenderer/plane/r_visibleplane.cpp
	swrenderer/plane/r_visibleplanelist.cpp
	swrenderer/plane/r_skyplane.cpp
	swrenderer/plane/r_planerenderer.cpp
	swrenderer/plane/r_flatplane.cpp
	swrenderer/plane/r_slopeplane.cpp
)

set( POLYRENDER_SOURCES
	polyrenderer/poly_renderer.cpp
	polyrenderer/poly_renderthread.cpp
	polyrenderer/scene/poly_scene.cpp
	polyrenderer/scene/poly_portal.cpp
	polyrenderer/scene/poly_cull.cpp
	polyrenderer/scene/poly_decal.cpp
	polyrenderer/scene/poly_particle.cpp
	polyrenderer/scene/poly_plane.cpp
	polyrenderer/scene/poly_playersprite.cpp
	polyrenderer/scene/poly_wall.cpp
	polyrenderer/scene/poly_wallsprite.cpp
	polyrenderer/scene/poly_sprite.cpp
	polyrenderer/scene/poly_sky.cpp
	polyrenderer/scene/poly_light.cpp
	polyrenderer/drawers/poly_buffer.cpp
	polyrenderer/drawers/poly_triangle.cpp
	polyrenderer/drawers/poly_draw_args.cpp
	polyrenderer/drawers/screen_triangle.cpp
	polyrenderer/math/tri_matrix.cpp
)

# These files will be flagged as "headers" so that they appear in project files
# without being compiled.
set( NOT_COMPILED_SOURCE_FILES
	${OTHER_SYSTEM_SOURCES}
	${SWRENDER_SOURCES}
	${POLYRENDER_SOURCES}
	sc_man_scanner.h
	sc_man_scanner.re
	g_statusbar/sbarinfo_commands.cpp
	xlat/xlat_parser.y
	xlat_parser.c
	xlat_parser.h
	scripting/zscript/zcc-parse.lemon
	zcc-parse.c
	zcc-parse.h
)

# This is disabled for now because I cannot find a way to give the .pch file a different name.
# Visual C++ 2015 seems hell-bent of only allowing one .pch file with the same name as the executable.
#enable_precompiled_headers( g_pch2.h FASTMATH_PCH_SOURCES )

# Enable fast math for some sources
set( FASTMATH_SOURCES
	swrenderer/r_all.cpp
	polyrenderer/poly_all.cpp
	sound/oplsynth/opl_mus_player.cpp
	sound/mpg123_decoder.cpp
	sound/music_midi_base.cpp
	sound/oalsound.cpp
	sound/sndfile_decoder.cpp
	sound/mididevices/music_timiditypp_mididevice.cpp
	gl/data/gl_matrix.cpp
	gl/utility/gl_clock.cpp
	gl/renderer/gl_2ddrawer.cpp
	gl/hqnx/init.cpp
	gl/hqnx/hq2x.cpp
	gl/hqnx/hq3x.cpp
	gl/hqnx/hq4x.cpp
	gl/xbr/xbrz.cpp
	gl/xbr/xbrz_old.cpp
	gl/scene/gl_bsp.cpp
	gl/scene/gl_fakeflat.cpp
	gl/scene/gl_clipper.cpp
	gl/scene/gl_decal.cpp
	gl/scene/gl_drawinfo.cpp
	gl/scene/gl_flats.cpp
	gl/scene/gl_walls.cpp
	gl/scene/gl_sprite.cpp
	gl/scene/gl_skydome.cpp
	gl/scene/gl_renderhacks.cpp
	gl/scene/gl_weapon.cpp
	gl/scene/gl_scene.cpp
	gl/scene/gl_sky.cpp
	gl/scene/gl_portal.cpp
	gl/scene/gl_walls_draw.cpp
	gl/scene/gl_vertex.cpp
	gl/scene/gl_spritelight.cpp
	gl/dynlights/gl_dynlight1.cpp
	gl/system/gl_load.c
	gl/models/gl_models.cpp
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
	c_functions.cpp
	cmdlib.cpp
	colormatcher.cpp
	compatibility.cpp
	configfile.cpp
	ct_chat.cpp
	cycler.cpp
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
	p_secnodes.cpp
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
	p_xlat.cpp
	parsecontext.cpp
	po_man.cpp
	portal.cpp
	r_utility.cpp
	r_sky.cpp
	r_videoscale.cpp
	s_advsound.cpp
	s_environment.cpp
	s_playlist.cpp
	s_sndseq.cpp
	s_sound.cpp
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
	g_inventory/a_keys.cpp
	g_inventory/a_pickups.cpp
	g_inventory/a_weapons.cpp
	g_shared/a_action.cpp
	g_shared/a_decals.cpp
	g_shared/a_dynlight.cpp
	g_shared/a_dynlightdata.cpp
	g_shared/a_flashfader.cpp
	g_shared/a_lightning.cpp
	g_shared/a_morph.cpp
	g_shared/a_quake.cpp
	g_shared/a_specialspot.cpp
	g_shared/hudmessages.cpp
	g_shared/shared_hud.cpp
	g_statusbar/sbarinfo.cpp
	g_statusbar/sbar_mugshot.cpp
	g_statusbar/shared_sbar.cpp
	gl/compatibility/gl_20.cpp
	gl/data/gl_data.cpp
	gl/data/gl_portaldata.cpp
	gl/data/gl_setup.cpp
	gl/data/gl_vertexbuffer.cpp
	gl/dynlights/gl_glow.cpp
	gl/dynlights/gl_lightbuffer.cpp
	gl/dynlights/gl_aabbtree.cpp
	gl/dynlights/gl_shadowmap.cpp
	gl/models/gl_models_md3.cpp
	gl/models/gl_models_md2.cpp
	gl/models/gl_voxels.cpp
	gl/renderer/gl_quaddrawer.cpp
	gl/renderer/gl_renderer.cpp
	gl/renderer/gl_renderstate.cpp
	gl/renderer/gl_renderbuffers.cpp
	gl/renderer/gl_lightdata.cpp
	gl/renderer/gl_postprocess.cpp
	gl/renderer/gl_postprocessstate.cpp
	gl/shaders/gl_shader.cpp
	gl/shaders/gl_texshader.cpp
	gl/shaders/gl_shaderprogram.cpp
	gl/shaders/gl_postprocessshader.cpp
	gl/shaders/gl_shadowmapshader.cpp
	gl/shaders/gl_presentshader.cpp
	gl/shaders/gl_present3dRowshader.cpp
	gl/shaders/gl_bloomshader.cpp
	gl/shaders/gl_ambientshader.cpp
	gl/shaders/gl_blurshader.cpp
	gl/shaders/gl_colormapshader.cpp
	gl/shaders/gl_tonemapshader.cpp
	gl/shaders/gl_lensshader.cpp
	gl/shaders/gl_fxaashader.cpp
	gl/stereo3d/gl_stereo3d.cpp
	gl/stereo3d/gl_stereo_cvars.cpp
	gl/stereo3d/gl_stereo_leftright.cpp
	gl/stereo3d/scoped_view_shifter.cpp
	gl/stereo3d/gl_anaglyph.cpp
	gl/stereo3d/gl_quadstereo.cpp
	gl/stereo3d/gl_sidebyside3d.cpp
	gl/stereo3d/gl_interleaved3d.cpp
	gl/system/gl_interface.cpp
	gl/system/gl_framebuffer.cpp
	gl/system/gl_swframebuffer.cpp
	gl/system/gl_swwipe.cpp
	gl/system/gl_debug.cpp
	gl/system/gl_menu.cpp
	gl/system/gl_wipe.cpp
	gl/textures/gl_hwtexture.cpp
	gl/textures/gl_texture.cpp
	gl/textures/gl_material.cpp
	gl/textures/gl_hirestex.cpp
	gl/textures/gl_samplers.cpp
	gl/textures/gl_translate.cpp
	gl/textures/gl_hqresize.cpp
	menu/joystickmenu.cpp
	menu/loadsavemenu.cpp
	menu/menu.cpp
	menu/menudef.cpp
	menu/messagebox.cpp
	menu/optionmenu.cpp
	menu/playermenu.cpp
	menu/videomenu.cpp
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
	textures/backdroptexture.cpp
	textures/shadertexture.cpp
	textures/texture.cpp
	textures/texturemanager.cpp
	textures/tgatexture.cpp
	textures/warptexture.cpp
	textures/skyboxtexture.cpp
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
	intermission/intermission.cpp
	intermission/intermission_parse.cpp
	r_data/colormaps.cpp
	r_data/r_translate.cpp
	r_data/sprites.cpp
	r_data/voxels.cpp
	r_data/renderstyle.cpp
	r_data/r_interpolate.cpp
	r_data/r_vanillatrans.cpp
	scripting/symbols.cpp
	scripting/types.cpp
	scripting/thingdef.cpp
	scripting/thingdef_data.cpp
	scripting/thingdef_properties.cpp
	scripting/backend/codegen.cpp
	scripting/backend/scopebarrier.cpp
	scripting/backend/dynarrays.cpp
	scripting/backend/vmbuilder.cpp
	scripting/backend/vmdisasm.cpp
	scripting/decorate/olddecorations.cpp
	scripting/decorate/thingdef_exp.cpp
	scripting/decorate/thingdef_parse.cpp
	scripting/decorate/thingdef_states.cpp
	scripting/vm/vmexec.cpp
	scripting/vm/vmframe.cpp
	scripting/zscript/ast.cpp
	scripting/zscript/zcc_compile.cpp
	scripting/zscript/zcc_parser.cpp
	sfmt/SFMT.cpp
	sound/i_music.cpp
	sound/i_sound.cpp
	sound/mididevices/music_opldumper_mididevice.cpp
	sound/mididevices/music_opl_mididevice.cpp
	sound/mididevices/music_pseudo_mididevice.cpp
	sound/mididevices/music_fluidsynth_mididevice.cpp
	sound/mididevices/music_softsynth_mididevice.cpp
	sound/mididevices/music_timidity_mididevice.cpp
	sound/mididevices/music_wildmidi_mididevice.cpp
	sound/musicformats/music_cd.cpp
	sound/musicformats/music_dumb.cpp
	sound/musicformats/music_gme.cpp
	sound/musicformats/music_libsndfile.cpp
	sound/musicformats/music_mus_midiout.cpp
	sound/musicformats/music_smf_midiout.cpp
	sound/musicformats/music_hmi_midiout.cpp
	sound/musicformats/music_xmi_midiout.cpp
	sound/musicformats/music_midistream.cpp
	sound/musicformats/music_opl.cpp
	sound/musicformats/music_stream.cpp
	sound/oplsynth/fmopl.cpp
	sound/oplsynth/musicblock.cpp
	sound/oplsynth/oplio.cpp
	sound/oplsynth/dosbox/opl.cpp
	sound/oplsynth/OPL3.cpp
	sound/oplsynth/nukedopl3.cpp
	sound/timidity/common.cpp
	sound/timidity/instrum.cpp
	sound/timidity/instrum_dls.cpp
	sound/timidity/instrum_font.cpp
	sound/timidity/instrum_sf2.cpp
	sound/timidity/mix.cpp
	sound/timidity/playmidi.cpp
	sound/timidity/resample.cpp
	sound/timidity/timidity.cpp
	sound/wildmidi/file_io.cpp
	sound/wildmidi/gus_pat.cpp
	sound/wildmidi/reverb.cpp
	sound/wildmidi/wildmidi_lib.cpp
	sound/wildmidi/wm_error.cpp
	events.cpp
	GuillotineBinPack.cpp
	SkylineBinPack.cpp
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
	g_statusbar
	g_shared
	g_inventory
	sound
	textures
	sound/oplsynth
	sound/timidity
	sound/wildmidi
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

	if( ZDOOM_GENERATE_MAPFILE )
		set( LINKERSTUFF "${LINKERSTUFF} /MAP" )
	endif()
	set_target_properties(zdoom PROPERTIES LINK_FLAGS ${LINKERSTUFF})

	add_custom_command(TARGET zdoom POST_BUILD
		COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\win32\\zdoom.exe.manifest\" -outputresource:\"$<TARGET_FILE:zdoom>\"\;\#1
		COMMENT "Adding manifest..."
	)

	create_default_target_launcher( zdoom WORKING_DIRECTORY ${ZDOOM_OUTPUT_DIR} )
endif()

if( NOT WIN32 AND NOT APPLE )
	FILE( WRITE ${CMAKE_CURRENT_BINARY_DIR}/link-make "if [ ! -e ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME} ]; then ln -sf ${CMAKE_CURRENT_BINARY_DIR}/${ZDOOM_EXE_NAME} ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME}; fi" )
	add_custom_command( TARGET zdoom POST_BUILD
		COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/link-make
		COMMAND /bin/sh -c ${CMAKE_CURRENT_BINARY_DIR}/link-make )
endif()

add_custom_command(TARGET zdoom POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/soundfont/gzdoom.sf2 $<TARGET_FILE_DIR:zdoom>)

if( CMAKE_COMPILER_IS_GNUCXX )
	# GCC misoptimizes this file
	set_source_files_properties( oplsynth/fmopl.cpp PROPERTIES COMPILE_FLAGS "-fno-tree-dominator-opts -fno-tree-fre" )
endif()
if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	# Need to enable intrinsics for these files.
	if( SSE_MATTERS )
		set_source_files_properties(
			gl/system/gl_swframebuffer.cpp
			polyrenderer/poly_all.cpp
			swrenderer/r_all.cpp
			x86.cpp
			PROPERTIES COMPILE_FLAGS "-msse2 -mmmx" )
	endif()
endif()

if( APPLE )
	set_target_properties(zdoom PROPERTIES
		LINK_FLAGS "-framework Carbon -framework Cocoa -framework IOKit -framework OpenGL"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/posix/osx/zdoom-info.plist" )

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
source_group("Audio Files\\OPL Synth" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/oplsynth/.+")
source_group("Audio Files\\OPL Synth\\DOSBox" FILES sound/oplsynth/dosbox/opl.cpp sound/oplsynth/dosbox/opl.h)
source_group("Audio Files\\Timidity" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/timidity/.+")
source_group("Audio Files\\WildMidi" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/wildmidi/.+")
source_group("Audio Files\\MIDI Devices" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/mididevices/.+")
source_group("Audio Files\\Music formats" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/musicformats/.+")
source_group("External\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/math/.+")
source_group("External\\RapidJSON" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/.+")
source_group("External\\SFMT" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sfmt/.+")
source_group("FraggleScript" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/fragglescript/.+")
source_group("Intermission" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/intermission/.+")
source_group("Inventory" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_inventory/.+")
source_group("Menu" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/menu/.+")
source_group("OpenGL Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/.+")
source_group("OpenGL Renderer\\Data" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/data/.+")
source_group("OpenGL Renderer\\Dynamic Lights" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/dynlights/.+")
source_group("OpenGL Renderer\\HQ Resize" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/hqnx/.+")
source_group("OpenGL Renderer\\HQ Resize MMX version" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/hqnx_asm/.+")
source_group("OpenGL Renderer\\XBRZ" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/hqnx_asm/.+")
source_group("OpenGL Renderer\\Models" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/models/.+")
source_group("OpenGL Renderer\\Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/renderer/.+")
source_group("OpenGL Renderer\\Scene" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/scene/.+")
source_group("OpenGL Renderer\\Stereo3D" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/stereo3d/.+")
source_group("OpenGL Renderer\\Shaders" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/shaders/.+")
source_group("OpenGL Renderer\\System" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/system/.+")
source_group("OpenGL Renderer\\Textures" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/textures/.+")
source_group("OpenGL Renderer\\Utilities" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/utility/.+")
source_group("Software Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/.+")
source_group("Software Renderer\\Drawers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/drawers/.+")
source_group("Software Renderer\\Scene" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/scene/.+")
source_group("Software Renderer\\Segments" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/segments/.+")
source_group("Software Renderer\\Line" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/line/.+")
source_group("Software Renderer\\Plane" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/plane/.+")
source_group("Software Renderer\\Things" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/things/.+")
source_group("Software Renderer\\Viewport" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/swrenderer/viewport/.+")
source_group("Poly Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/polyrenderer/.+")
source_group("Poly Renderer\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/polyrenderer/math/.+")
source_group("Poly Renderer\\Drawers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/polyrenderer/drawers/.+")
source_group("Poly Renderer\\Scene" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/polyrenderer/scene/.+")
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
source_group("Scripting\\Decorate frontend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/decorate/.+")
source_group("Scripting\\ZScript frontend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/zscript/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h)
source_group("Scripting\\Compiler backend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/backend/.+")
source_group("Scripting\\VM" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/vm/.+")
source_group("Scripting" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/.+")
source_group("Shared Game" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_shared/.+")
source_group("Statusbar" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_statusbar/.+")
source_group("Versioning" FILES version.h win32/zdoom.rc)
source_group("Xlat" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/xlat/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h)
source_group("Source Files" FILES ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h sc_man_scanner.re)
