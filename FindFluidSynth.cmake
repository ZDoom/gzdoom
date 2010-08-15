# - Find fluidsynth
# Find the native fluidsynth includes and library
#
#  FLUIDSYNTH_INCLUDE_DIR - where to find fluidsynth.h
#  FLUIDSYNTH_LIBRARIES   - List of libraries when using fluidsynth.
#  FLUIDSYNTH_FOUND       - True if fluidsynth found.


IF (FLUIDSYNTH_INCLUDE_DIR AND FLUIDSYNTH_LIBRARIES)
  # Already in cache, be silent
  SET(FluidSynth_FIND_QUIETLY TRUE)
ENDIF (FLUIDSYNTH_INCLUDE_DIR AND FLUIDSYNTH_LIBRARIES)

FIND_PATH(FLUIDSYNTH_INCLUDE_DIR fluidsynth.h)

FIND_LIBRARY(FLUIDSYNTH_LIBRARIES NAMES fluidsynth )
MARK_AS_ADVANCED( FLUIDSYNTH_LIBRARIES FLUIDSYNTH_INCLUDE_DIR )

# handle the QUIETLY and REQUIRED arguments and set FLUIDSYNTH_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FluidSynth DEFAULT_MSG FLUIDSYNTH_LIBRARIES FLUIDSYNTH_INCLUDE_DIR)

