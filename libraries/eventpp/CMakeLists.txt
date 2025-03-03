cmake_minimum_required(VERSION 3.5)
project(eventpp VERSION 0.1.3)

add_library(eventpp INTERFACE)
set(CMAKE_CXX_STANDARD 11)

include(GNUInstallDirs)
target_include_directories(
	eventpp INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(eventpp::eventpp ALIAS eventpp)

# Checks if eventpp is the main project or if it is
# being built as a subproject (using add_subdirectory/FetchContent).
set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MAIN_PROJECT ON)
endif()

# Installation
# ------------
if (POLICY CMP0077)
	# Allow CMake 3.13+ to override options when using add_subdirectory/FetchContent.
	cmake_policy(SET CMP0077 NEW)
endif (POLICY CMP0077)

option(EVENTPP_INSTALL "Enable installation" ${MAIN_PROJECT})

if (EVENTPP_INSTALL)
	# Install the library
	install(
		TARGETS eventpp
		EXPORT eventppTargets
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)

	# Install the headers
	install(
		DIRECTORY include/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	# (Generate and) install the target import file
	install(
		EXPORT eventppTargets
		NAMESPACE eventpp::
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eventpp
	)

	# Generate the package version file
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file(
		${CMAKE_CURRENT_BINARY_DIR}/eventppConfigVersion.cmake
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY AnyNewerVersion
	)

	# Generate the package configuration file, that allows other
	# CMake projects to find the library with find_package()
	configure_package_config_file(
		cmake/eventppConfig.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/eventppConfig.cmake
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eventpp
	)

	# Install the package version and configuration files
	install(
		FILES
		${CMAKE_CURRENT_BINARY_DIR}/eventppConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/eventppConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eventpp
	)

	# Install readme and license
	install(
		FILES
		readme.md
		license
		DESTINATION ${CMAKE_INSTALL_DATADIR}/eventpp
	)
endif()
