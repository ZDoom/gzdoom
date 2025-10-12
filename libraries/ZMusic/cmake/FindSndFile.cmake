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

if(NOT SNDFILE_INCLUDE_DIR)
	find_path(SNDFILE_INCLUDE_DIR NAMES sndfile.h)
endif()

if(NOT SNDFILE_LIBRARY)
	find_library(SNDFILE_LIBRARY NAMES sndfile sndfile-1)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(SndFile DEFAULT_MSG SNDFILE_LIBRARY SNDFILE_INCLUDE_DIR)

if(SNDFILE_FOUND)
	add_library(sndfile UNKNOWN IMPORTED GLOBAL)
	set_target_properties(sndfile
	PROPERTIES
		IMPORTED_LOCATION "${SNDFILE_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${SNDFILE_INCLUDE_DIR}"
	)

	add_library(SndFile::sndfile ALIAS sndfile)

	# Legacy variables
	set(SNDFILE_INCLUDE_DIRS "${SNDFILE_INCLUDE_DIR}")
	set(SNDFILE_LIBRARIES "${SNDFILE_LIBRARY}")
endif()
