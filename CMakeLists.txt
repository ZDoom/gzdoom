cmake_minimum_required( VERSION 2.8.7 )
project(GZDoom)

if( COMMAND cmake_policy )
	if( POLICY CMP0011 )
		cmake_policy( SET CMP0011 NEW )
	endif()
	if( POLICY CMP0054 )
		cmake_policy( SET CMP0054 NEW )
	endif()
endif()

list( APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} )
include( CreateLaunchers )
include( FindPackageHandleStandardArgs )

# Support cross compiling
option( FORCE_CROSSCOMPILE "Turn on cross compiling." NO )
if( FORCE_CROSSCOMPILE )
	set( CMAKE_CROSSCOMPILING TRUE )
endif()

if(CMAKE_CROSSCOMPILING)
	set(IMPORT_EXECUTABLES "IMPORTFILE-NOTFOUND" CACHE FILEPATH "Export file from native build.")
	include(${IMPORT_EXECUTABLES})
endif()

# Simplify pk3 building, add_pk3(filename srcdirectory)
function( add_pk3 PK3_NAME PK3_DIR )
	# Generate target name. Just use "pk3" for main pk3 target.
	string( REPLACE "." "_" PK3_TARGET ${PK3_NAME} )
	if( ${PK3_TARGET} STREQUAL "zdoom_pk3" )
		set( PK3_TARGET "pk3" )
	endif()

	if( NOT ZDOOM_OUTPUT_OLDSTYLE )
		add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} $<TARGET_FILE_DIR:zdoom>/${PK3_NAME}
			DEPENDS zipdir )
	else()
		add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			DEPENDS zipdir )
	endif()

		# Touch the zipdir executable here so that the pk3s are forced to
		# rebuild each time since their dependecy has "changed."
		add_custom_target( ${PK3_TARGET} ALL
			COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
			DEPENDS ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} )
endfunction()

# Macro for building libraries without debugging information
macro( make_release_only )
	set( CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_RELEASE} )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELEASE} )
	string( REPLACE "/MT " "/MTd " CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_RELEASE} )
	set( CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_RELEASE} )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELEASE} )
	string( REPLACE "/MT " "/MTd " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_RELEASE} )
endmacro()

IF( NOT CMAKE_BUILD_TYPE )
	SET( CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE )
ENDIF()

set( ZDOOM_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory where zdoom.pk3 and the executable will be created." )
set( ZDOOM_EXE_NAME "gzdoom" CACHE FILEPATH "Name of the executable to create" )
if( MSVC )
	# Allow the user to use ZDOOM_OUTPUT_DIR as a single release point.
	# Use zdoom, zdoomd, zdoom64, and zdoomd64 for the binary names
	option( ZDOOM_OUTPUT_OLDSTYLE "Don't use Release/Debug directories." OFF )
else()
	set( ZDOOM_OUTPUT_OLDSTYLE OFF )
endif()

# Replacement variables for a possible long list of C/C++ compilers compatible with GCC
if( "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
	set( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE TRUE )
else()
	set( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE FALSE )
endif()

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
	set( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE TRUE )
else()
	set( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE FALSE )
endif()

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	set( PROFILE 0 CACHE BOOL "Enable profiling with gprof for Debug and RelWithDebInfo build types." )
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

option( NO_FMOD "Disable FMODEx sound support" OFF )
option( NO_OPENAL "Disable OpenAL sound support" OFF )

find_package( BZip2 )
find_package( JPEG )
find_package( ZLIB )
# GME
find_path( GME_INCLUDE_DIR gme/gme.h )
find_library( GME_LIBRARIES gme )
mark_as_advanced( GME_INCLUDE_DIR GME_LIBRARIES )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( GME
	REQUIRED_VARS GME_LIBRARIES GME_INCLUDE_DIR
)

if( MSVC )
	# Eliminate unreferenced functions and data
	# Perform identical COMDAT folding
	set( REL_LINKER_FLAGS "/opt:ref /opt:icf /nodefaultlib:msvcrt /TSAWARE /LARGEADDRESSAWARE" )

	# String pooling
	# Function-level linking
	# Disable run-time type information
	set( ALL_C_FLAGS "/GF /Gy /GR-" )
	
	if( CMAKE_SIZEOF_VOID_P MATCHES "4")
		# SSE2 option (to allow x87 in 32 bit and disallow extended feature sets which have not yet been checked for precision)
		option (ZDOOM_USE_SSE2	"Use SSE2 instruction set")
		if (ZDOOM_USE_SSE2)
			set( ALL_C_FLAGS "${ALL_C_FLAGS} /arch:SSE2")
		else ()
			if (MSVC_VERSION GREATER 1699)
				# On Visual C++ 2012 and later SSE2 is the default, so we need to switch it off explicitly
				set( ALL_C_FLAGS "${ALL_C_FLAGS} /arch:IA32")
			endif ()
		endif ()
	else()
		set( ALL_C_FLAGS "${ALL_C_FLAGS} /arch:SSE2")
	endif()
	
	# Avoid CRT DLL dependancies in release builds, optionally generate assembly output for checking crash locations.
	option( ZDOOM_GENERATE_ASM "Generate assembly output." OFF )
	if( ZDOOM_GENERATE_ASM )
		set( REL_C_FLAGS "/MT /Oy /Oi /FAcs" )
	else()
		set( REL_C_FLAGS "/MT /Oy /Oi" )
	endif()

	
	# Debug allocations in debug builds
	set( DEB_C_FLAGS "/D _CRTDBG_MAP_ALLOC /MTd" )

	# Disable warnings for unsecure CRT functions from VC8+
	if( MSVC_VERSION GREATER 1399 )
		set( ALL_C_FLAGS "${ALL_C_FLAGS} /wd4996" )
	endif()
	
	# The CMake configurations set /GR and /MD by default, which conflict with our settings.
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MDd " " " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MDd " " " CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG} )
	string(REPLACE " /GR" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )
else()
	set( REL_LINKER_FLAGS "" )
	set( ALL_C_FLAGS "-ffp-contract=off" )
	set( REL_C_FLAGS "" )
	set( DEB_C_FLAGS "" )

	# If we're compiling with a custom GCC on the Mac (which we know since g++-4.2 doesn't support C++11) statically link libgcc.
	if( APPLE AND "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" )
		set( ALL_C_FLAGS "-static-libgcc" )
	endif()
endif()

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
option(FORCE_INTERNAL_GME "Use internal gme")

# Fast math flags, required by some subprojects
set( ZD_FASTMATH_FLAG "" )
if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	set( ZD_FASTMATH_FLAG "-ffast-math -ffp-contract=fast" )
elseif( MSVC )
	set( ZD_FASTMATH_FLAG "/fp:fast" )
endif()

if( ZLIB_FOUND AND NOT FORCE_INTERNAL_ZLIB )
	message( STATUS "Using system zlib, includes found at ${ZLIB_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal zlib" )
	add_subdirectory( zlib )
	set( ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib )
	set( ZLIB_LIBRARIES z )
	set( ZLIB_LIBRARY z )
endif()

if( JPEG_FOUND AND NOT FORCE_INTERNAL_JPEG )
	message( STATUS "Using system jpeg library, includes found at ${JPEG_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal jpeg library" )
	add_subdirectory( jpeg-6b )
	set( JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6b )
	set( JPEG_LIBRARIES jpeg )
	set( JPEG_LIBRARY jpeg )
endif()

if( BZIP2_FOUND AND NOT FORCE_INTERNAL_BZIP2 )
	message( STATUS "Using system bzip2 library, includes found at ${BZIP2_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal bzip2 library" )
	add_subdirectory( bzip2 )
	set( BZIP2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bzip2" )
	set( BZIP2_LIBRARIES bz2 )
	set( BZIP2_LIBRARY bz2 )
endif()

if( GME_FOUND AND NOT FORCE_INTERNAL_GME )
	message( STATUS "Using system gme library, includes found at ${GME_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal gme library" )
	add_subdirectory( game-music-emu )
	set( GME_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game-music-emu" )
	set( GME_LIBRARIES gme )
endif()

set( LZMA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lzma/C" )

if( NOT CMAKE_CROSSCOMPILING )
	if( NOT CROSS_EXPORTS )
		set( CROSS_EXPORTS "" )
	endif()
endif()

add_subdirectory( lzma )
add_subdirectory( tools )
add_subdirectory( dumb )
add_subdirectory( gdtoa )
add_subdirectory( wadsrc )
add_subdirectory( wadsrc_bm )
add_subdirectory( wadsrc_lights )
add_subdirectory( src )

if( NOT WIN32 AND NOT APPLE )
	add_subdirectory( output_sdl )
endif()

if( NOT CMAKE_CROSSCOMPILING )
	export(TARGETS ${CROSS_EXPORTS} FILE "${CMAKE_BINARY_DIR}/ImportExecutables.cmake" )
endif()
