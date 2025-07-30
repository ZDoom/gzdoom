# - Find mpg123
# Find the native mpg123 includes and library
#
#  MPG123_INCLUDE_DIR - where to find mpg123.h
#  MPG123_LIBRARY     - Path to mpg123 library.
#  MPG123_FOUND       - True if mpg123 found.

if(MPG123_INCLUDE_DIR AND MPG123_LIBRARY)
  # Already in cache, be silent
  set(MPG123_FIND_QUIETLY TRUE)
endif(MPG123_INCLUDE_DIR AND MPG123_LIBRARY)

if(NOT MPG123_INCLUDE_DIR)
	find_path(MPG123_INCLUDE_DIR mpg123.h
		PATHS "${MPG123_DIR}"
		PATH_SUFFIXES include
	)
endif()

if(NOT MPG123_LIBRARY)
	find_library(MPG123_LIBRARY NAMES mpg123 mpg123-0
		PATHS "${MPG123_DIR}"
		PATH_SUFFIXES lib
	)
endif()

# handle the QUIETLY and REQUIRED arguments and set MPG123_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPG123 DEFAULT_MSG MPG123_LIBRARY MPG123_INCLUDE_DIR)

if(MPG123_FOUND)
	add_library(mpg123 UNKNOWN IMPORTED)
	set_target_properties(mpg123
	PROPERTIES
		IMPORTED_LOCATION "${MPG123_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${MPG123_INCLUDE_DIR}"
	)

	# Legacy variables
	set(MPG123_INCLUDE_DIRS ${MPG123_INCLUDE_DIR})
	set(MPG123_LIBRARIES ${MPG123_LIBRARY})
endif()
