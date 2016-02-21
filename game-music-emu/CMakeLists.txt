# CMake project definition file.
project(libgme)

include (CheckCXXCompilerFlag)

# When version is changed, also change the one in gme/gme.h to match
set(GME_VERSION 0.6.0 CACHE INTERNAL "libgme Version")

# 2.6+ always assumes FATAL_ERROR, but 2.4 and below don't.
# Of course, 2.4 might work, in which case you're welcome to drop
# down the requirement, but I can't test that.
cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)

make_release_only()

# I don't plan on debugging this, so make it a release build.
if( NOT CMAKE_BUILD_TYPE MATCHES "Release" )
	set( CMAKE_BUILD_TYPE "RelWithDebInfo" )
endif()

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra" )
	if( NOT PROFILE )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer" )
	endif()
	check_cxx_compiler_flag( -Wno-array-bounds HAVE_NO_ARRAY_BOUNDS )
	if( HAVE_NO_ARRAY_BOUNDS )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-array-bounds" )
	endif()
endif()

# Enable fast flag for GME
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ZD_FASTMATH_FLAG}" )

# Default emulators to build (all of them! ;)
# [ZDoom] No options, enable all of them by default.
#if (NOT DEFINED USE_GME_AY)
    SET(USE_GME_AY 1 BOOL "Enable support for Spectrum ZX music emulation")
#endif()

#if (NOT DEFINED USE_GME_GBS)
    SET(USE_GME_GBS 1 BOOL "Enable support for Game Boy music emulation")
#endif()

#if (NOT DEFINED USE_GME_GYM)
    SET(USE_GME_GYM 1 BOOL "Enable Sega MegaDrive/Genesis music emulation")
#endif()

#if (NOT DEFINED USE_GME_HES)
    SET(USE_GME_HES 1 BOOL "Enable PC Engine/TurboGrafx-16 music emulation")
#endif()

#if (NOT DEFINED USE_GME_KSS)
    SET(USE_GME_KSS 1 BOOL "Enable MSX or other Z80 systems music emulation")
#endif()

#if (NOT DEFINED USE_GME_NSF)
    SET(USE_GME_NSF 1 BOOL "Enable NES NSF music emulation")
#endif()

#if (NOT DEFINED USE_GME_NSFE)
    SET(USE_GME_NSFE 1 BOOL "Enable NES NSFE and NSF music emulation")
#endif()

#if (NOT DEFINED USE_GME_SAP)
    SET(USE_GME_SAP 1 BOOL "Enable Atari SAP music emulation")
#endif()

#if (NOT DEFINED USE_GME_SPC)
    SET(USE_GME_SPC 1 BOOL "Enable SNES SPC music emulation")
#endif()

#if (NOT DEFINED USE_GME_VGM)
    SET(USE_GME_VGM 1 BOOL "Enable Sega VGM/VGZ music emulation")
#endif()

#if (USE_GME_NSFE AND NOT USE_GME_NSF)
    # MESSAGE(" -- NSFE support requires NSF, enabling NSF support. --")
    SET(USE_GME_NSF 1 BOOL "Enable NES NSF music emulation")
#endif()

# Check for GCC "visibility" support.
if (CMAKE_COMPILER_IS_GNUCXX)
   check_cxx_compiler_flag (-fvisibility=hidden __LIBGME_TEST_VISIBILITY)
   set (ENABLE_VISIBILITY OFF)
   if (__LIBGME_TEST_VISIBILITY)
      # get the gcc version
      exec_program(${CMAKE_CXX_COMPILER} ARGS --version OUTPUT_VARIABLE _gcc_version_info)
      string (REGEX MATCH "[3-9]\\.[0-9]\\.[0-9]" _gcc_version "${_gcc_version_info}")

      # gcc <4.1 had poor support for symbol visibility
      if ((${_gcc_version} VERSION_GREATER "4.1") OR (${_gcc_version} VERSION_EQUAL "4.1"))
         set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
         set (ENABLE_VISIBILITY ON)
         add_definitions (-DLIBGME_VISIBILITY)

         # GCC >= 4.2 also correctly supports making inline members have hidden
         # visibility by default.
         if ((${_gcc_version} VERSION_GREATER "4.2") OR (${_gcc_version} VERSION_EQUAL "4.2"))
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
         endif()
      endif()
   endif() # test visibility
endif()

# Cache this result
set( LIBGME_HAVE_GCC_VISIBILITY ${ENABLE_VISIBILITY} CACHE BOOL "GCC support for hidden visibility")

# Shared library defined here
add_subdirectory(gme)
