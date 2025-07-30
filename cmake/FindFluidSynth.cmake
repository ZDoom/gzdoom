# - Find fluidsynth
# Find the native fluidsynth includes and library
#
#  FLUIDSYNTH_INCLUDE_DIR - where to find fluidsynth.h
#  FLUIDSYNTH_LIBRARY     - Path to fluidsynth library.
#  FLUIDSYNTH_FOUND       - True if fluidsynth found.


if(FLUIDSYNTH_INCLUDE_DIR AND FLUIDSYNTH_LIBRARY)
  # Already in cache, be silent
  set(FluidSynth_FIND_QUIETLY TRUE)
endif()

if(NOT FLUIDSYNTH_INCLUDE_DIR)
	find_path(FLUIDSYNTH_INCLUDE_DIR fluidsynth.h)
endif()

if(NOT FLUIDSYNTH_LIBRARY)
	find_library(FLUIDSYNTH_LIBRARY NAMES fluidsynth)
endif()

# handle the QUIETLY and REQUIRED arguments and set FLUIDSYNTH_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FluidSynth DEFAULT_MSG FLUIDSYNTH_LIBRARY FLUIDSYNTH_INCLUDE_DIR)

if(FLUIDSYNTH_FOUND)
	add_library(libfluidsynth UNKNOWN IMPORTED)
	set_target_properties(libfluidsynth
	PROPERTIES
		IMPORTED_LOCATION "${FLUIDSYNTH_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${FLUIDSYNTH_INCLUDE_DIR}"
	)

	# Legacy variables
	set(FLUIDSYNTH_INCLUDE_DIRS ${FLUIDSYNTH_INCLUDE_DIR})
	set(FLUIDSYNTH_LIBRARIES ${FLUIDSYNTH_LIBRARY})
endif()
