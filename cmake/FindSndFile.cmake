# - Try to find SndFile
# Once done this will define
#
#  SNDFILE_FOUND - system has SndFile
#  SNDFILE_INCLUDE_DIRS - the SndFile include directory
#  SNDFILE_LIBRARIES - Link these to use SndFile
#
#  Copyright © 2006  Wengo
#  Copyright © 2009 Guillaume Martres
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

find_path(SNDFILE_INCLUDE_DIR NAMES sndfile.h)

find_library(SNDFILE_LIBRARY NAMES sndfile sndfile-1)

set(SNDFILE_INCLUDE_DIRS ${SNDFILE_INCLUDE_DIR})
set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SndFile DEFAULT_MSG SNDFILE_LIBRARY SNDFILE_INCLUDE_DIR)

# show the SNDFILE_INCLUDE_DIRS and SNDFILE_LIBRARIES variables only in the advanced view
mark_as_advanced(SNDFILE_INCLUDE_DIRS SNDFILE_LIBRARIES)
