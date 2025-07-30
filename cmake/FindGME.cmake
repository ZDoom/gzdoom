# - Find Game Music Emulation
#
#  GME_INCLUDE_DIR - where to find gme.h
#  GME_LIBRARY     - Path to gme library.
#  GME_FOUND       - True if gme found.


if(GME_INCLUDE_DIR AND GME_LIBRARY)
  # Already in cache, be silent
  set(GME_FIND_QUIETLY TRUE)
endif()

if(NOT GME_INCLUDE_DIR)
	find_path(GME_INCLUDE_DIR gme.h)
endif()

if(NOT GME_LIBRARY)
	find_library(GME_LIBRARY NAMES gme)
endif()

# handle the QUIETLY and REQUIRED arguments and set GME_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GME DEFAULT_MSG GME_LIBRARY GME_INCLUDE_DIR)

if(GME_FOUND)
	add_library(gme UNKNOWN IMPORTED)
	set_target_properties(gme
	PROPERTIES
		IMPORTED_LOCATION "${GME_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${GME_INCLUDE_DIR}"
	)

	# Legacy variables
	set(GME_INCLUDE_DIRS ${GME_INCLUDE_DIR})
	set(GME_LIBRARIES ${GME_LIBRARY})
endif()
