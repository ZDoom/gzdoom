cmake_minimum_required( VERSION 2.4 )
message(STATUS "${ZLIB_INCLUDE_DIR}" )
message(STATUS "${BZIP2_INCLUDE_DIR}" )
message(STATUS "${LZMA_INCLUDE_DIR}" )
include_directories( "${ZLIB_INCLUDE_DIR}" "${BZIP2_INCLUDE_DIR}" "${LZMA_INCLUDE_DIR}" )
add_executable( zipdir
	zipdir.c )
target_link_libraries( zipdir ${ZLIB_LIBRARIES} bzip2 lzma )
