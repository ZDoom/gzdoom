cmake_minimum_required(VERSION 3.13...3.19)

if (VCPKG_LIBSNDFILE)
	list(APPEND VCPKG_MANIFEST_FEATURES "vcpkg-libsndfile")
endif()

project(ZMusic
	VERSION 1.2.0
	LANGUAGES C CXX
)

if (VCPKG_TOOLCHAIN)
	if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
 		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 	endif()

	option(VCPKG_LIBSNDFILE "Import libsndfile from vcpkg" OFF)
else()
	set(VCPKG_MANIFEST_FEATURES)
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)
include(ZUtility)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	# This project is being built standalone

	# Give user option to build shared or static
	option(BUILD_SHARED_LIBS "Build shared libraries" ON)

	# Enable install rules
	set(ZMUSIC_INSTALL ON)
else()
	# This project is being vendored by another project, set option default if
	# the parent project doesn't provide them.

	if(NOT DEFINED BUILD_SHARED_LIBS)
		set(BUILD_SHARED_LIBS ON)
	endif()

	# Although install rules can be avoided with EXCLUDE_FROM_ALL on
	# add_subdirectory, the EXPORT rules may place certain usage requirements on
	# targets shared between the two projects.
	if(NOT DEFINED ZMUSIC_INSTALL)
		set(ZMUSIC_INSTALL OFF)
	endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE)
endif()

if(MSVC AND CMAKE_VERSION VERSION_LESS 3.15)
	message(WARNING "Some things may be misconfigured. Please update to CMake >= 3.15 with Visual Studio.")
endif()
if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix" FORCE)
endif()

if(MSVC)
	#  optionally generate assembly output for checking crash locations.
	option(ZMUSIC_GENERATE_ASM "Generate assembly output." OFF)
	if(ZMUSIC_GENERATE_ASM)
		add_compile_options("/FAcs")
	endif()

	add_compile_options(
		"/GF" # String pooling
		"/Gy" # Function-level linking
		"/GR-" # Disable run-time type information
		"/permissive-"
		"/Oy" "/Oi" "/GS-"

		# Disable warnings for unsecure CRT functions from VC8+
		"/wd4996"
	)

	add_compile_definitions(
		"UNICODE"
		"_UNICODE"
		"_WIN32_WINNT=0x0600"
		# Debug allocations in debug builds
		$<$<CONFIG:DEBUG>:_CRTDBG_MAP_ALLOC>
	)

	add_link_options(
		"/opt:ref" # Eliminate unreferenced functions and data
		"/opt:icf" # Perform identical COMDAT folding
		"/nodefaultlib:msvcrt"
		$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:/TSAWARE>
		"/LARGEADDRESSAWARE"
	)

	# RelWithDebInfo uses /Ob1 by default instead of Ob2 like Release
	string(REPLACE "/Ob1 " "/Ob2 " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )

	# The CMake configurations set /GR by default, which conflict with our settings.
	# CMake 3.20 fixes the need to do this
	string(REPLACE " /GR" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

	check_cxx_compiler_flag("/std:c++14" COMPILER_SUPPORTS_CXX14)
	if(COMPILER_SUPPORTS_CXX14)
		set(FLAG_CPP14 "/std:c++14")
	endif()
	if(MSVC_VERSION LESS 1910)
		unset(COMPILER_SUPPORTS_CXX14) # MSVC older than 2017 fails to build YMFM
	endif()
else()
	add_compile_options("-ffp-contract=off")

	if(APPLE)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			# With standard Apple tools -stdlib=libc++ needs to be specified in order to get
			# C++11 support using SDKs 10.7 and 10.8.
			add_compile_options("-stdlib=libc++")
			add_link_options("-stdlib=libc++")
		elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			# If we're compiling with a custom GCC on the Mac (which we know since g++-4.2 doesn't support C++11) statically link libgcc.
			add_compile_options("-static-libgcc")
		endif()
	endif()

	check_cxx_compiler_flag("-std=c++14" COMPILER_SUPPORTS_CXX14)
	if(COMPILER_SUPPORTS_CXX14)
		set(FLAG_CPP14 "-std=c++14")
	endif()
endif()

if(COMPILER_SUPPORTS_CXX14)
	message("== Your C++ compiler supports C++14, YMFM emulators will be ENABLED")
else()
	message("== Your C++ compiler does NOT supports C++14, YMFM emulators will be DISABLED")
endif()

# Initialize our list of find_package dependencies for configure_package_config_file
set(ZMUSIC_PACKAGE_DEPENDENCIES "" CACHE INTERNAL "")

if (WIN32 AND MINGW)
	add_compile_definitions(-D_UNICODE -DUNICODE)
	add_compile_definitions(-D__USE_MINGW_ANSI_STDIO=1)
endif()

add_subdirectory(thirdparty)
add_subdirectory(source)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/ZMusicConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
	cmake/ZMusicConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/ZMusicConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake
)
if(ZMUSIC_INSTALL)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ZMusicConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/ZMusicConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ZMusic
		COMPONENT devel
	)
endif()

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(CPACK_PACKAGE_CONTACT "First Last <example@example.com>" CACHE STRING "Contact info for archive maintainer.")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GZDoom's music system as a standalone library")
	string(TOLOWER "${PROJECT_NAME}" CPACK_PACKAGE_NAME)
	set(CPACK_PACKAGE_NAME "lib${CPACK_PACKAGE_NAME}")
	# Use same prefix for packaging for consistency
	set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
	set(CPACK_STRIP_FILES ON)
	set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
	set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
	set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})

	set(CPACK_COMPONENT_DEVEL_DEPENDS Full Lite)

	if(WIN32)
		set(CPACK_GENERATOR "ZIP")
	else()
		set(CPACK_GENERATOR "DEB")
		set(CPACK_DEB_COMPONENT_INSTALL ON)
		set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
		set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
		set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://zdoom.org")
		set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
		set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
		set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
		set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")

		set(CPACK_DEBIAN_FULL_PACKAGE_NAME "${CPACK_PACKAGE_NAME}${PROJECT_VERSION_MAJOR}")
		set(CPACK_DEBIAN_LITE_PACKAGE_NAME "${CPACK_PACKAGE_NAME}lite${PROJECT_VERSION_MAJOR}")

		set(CPACK_DEBIAN_DEVEL_PACKAGE_NAME "${CPACK_PACKAGE_NAME}-dev")
		set(CPACK_DEBIAN_DEVEL_PACKAGE_SECTION "libdevel")
	endif()

	include(CPack)
endif()
