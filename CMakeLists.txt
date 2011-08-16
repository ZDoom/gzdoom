cmake_minimum_required( VERSION 2.4 )

IF( NOT CMAKE_BUILD_TYPE )
	SET( CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE )
ENDIF( NOT CMAKE_BUILD_TYPE )

set( ZDOOM_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory where zdoom.pk3 and the executable will be created." )
set( ZDOOM_EXE_NAME "zdoom" CACHE FILEPATH "Name of the executable to create." )

if( CMAKE_COMPILER_IS_GNUCXX )
	set( PROFILE 0 CACHE BOOL "Enable profiling with gprof for Debug and RelWithDebInfo build types." )
endif( CMAKE_COMPILER_IS_GNUCXX )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

find_package( BZip2 )
find_package( JPEG )
find_package( ZLIB )

if( MSVC )
	# Eliminate unreferenced functions and data
	# Perform identical COMDAT folding
	set( REL_LINKER_FLAGS "/opt:ref /opt:icf /nodefaultlib:msvcrt" )

	# String pooling
	# Function-level linking
	# Disable run-time type information
	set( ALL_C_FLAGS "/GF /Gy /GR-" )
	
	# Avoid CRT DLL dependancies in release builds
	set( REL_C_FLAGS "/MT" )

	# Debug allocations in debug builds
	set( DEB_C_FLAGS "/D _CRTDBG_MAP_ALLOC" )

	# Disable warnings for unsecure CRT functions from VC8+
	if( MSVC_VERSION GREATER 1399 )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} /wd4996" )
	endif( MSVC_VERSION GREATER 1399 )
	
	# The CMake configurations set /GR and /MD by default, which conflict with our settings.
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO} )
	string(REPLACE " /GR" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )
endif( MSVC )

set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${REL_LINKER_FLAGS}" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

option(FORCE_INTERNAL_ZLIB "Use internal zlib")
option(FORCE_INTERNAL_JPEG "Use internal jpeg")
option(FORCE_INTERNAL_BZIP2 "Use internal bzip2")

if( ZLIB_FOUND AND NOT FORCE_INTERNAL_ZLIB )
	message( STATUS "Using system zlib" )
else( ZLIB_FOUND AND NOT FORCE_INTERNAL_ZLIB )
	message( STATUS "Using internal zlib" )
	add_subdirectory( zlib )
	set( ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib )
	set( ZLIB_LIBRARIES z )
	set( ZLIB_LIBRARY z )
endif( ZLIB_FOUND AND NOT FORCE_INTERNAL_ZLIB )

if( JPEG_FOUND AND NOT FORCE_INTERNAL_JPEG )
	message( STATUS "Using system jpeg library" )
else( JPEG_FOUND AND NOT FORCE_INTERNAL_JPEG )
	message( STATUS "Using internal jpeg library" )
	add_subdirectory( jpeg-6b )
	set( JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6b )
	set( JPEG_LIBRARIES jpeg )
	set( JPEG_LIBRARY jpeg )
endif( JPEG_FOUND AND NOT FORCE_INTERNAL_JPEG )

if( BZIP2_FOUND AND NOT FORCE_INTERNAL_BZIP2 )
	message( STATUS "Using system bzip2 library" )
else( BZIP2_FOUND AND NOT FORCE_INTERNAL_BZIP2 )
	message( STATUS "Using internal bzip2 library" )
	add_subdirectory( bzip2 )
	set( BZIP2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bzip2" )
	set( BZIP2_LIBRARIES bz2 )
	set( BZIP2_LIBRARY bz2 )
endif( BZIP2_FOUND AND NOT FORCE_INTERNAL_BZIP2 )

set( LZMA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lzma/C" )

add_subdirectory( lzma )
add_subdirectory( tools )
add_subdirectory( game-music-emu )
add_subdirectory( dumb )
add_subdirectory( gdtoa )
add_subdirectory( wadsrc )
add_subdirectory( src )

if( NOT WIN32 AND NOT APPLE )
	add_subdirectory( output_sdl )
endif( NOT WIN32 AND NOT APPLE )
