# - Find ZMusic
# Find the zmusic includes and library
#
#  ZMUSIC_INCLUDE_DIR - where to find zmusic.h
#  ZMUSIC_LIBRARIES   - List of libraries when using ZMusic
#  ZMUSIC_FOUND       - True if ZMusic found.

if(ZMUSIC_INCLUDE_DIR AND ZMUSIC_LIBRARIES)
    # Already in cache, be silent
    set(ZMUSIC_FIND_QUIETLY TRUE)
endif()

find_path(ZMUSIC_INCLUDE_DIR zmusic.h)

find_library(ZMUSIC_LIBRARIES NAMES zmusic)
mark_as_advanced(ZMUSIC_LIBRARIES ZMUSIC_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set ZMUSIC_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZMusic DEFAULT_MSG ZMUSIC_LIBRARIES ZMUSIC_INCLUDE_DIR)
