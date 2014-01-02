cmake_minimum_required( VERSION 2.4 )

if(NOT CMAKE_CROSSCOMPILING)
	message(STATUS "${ZLIB_INCLUDE_DIR}" )
	message(STATUS "${BZIP2_INCLUDE_DIR}" )
	message(STATUS "${LZMA_INCLUDE_DIR}" )
	include_directories( "${ZLIB_INCLUDE_DIR}" "${BZIP2_INCLUDE_DIR}" "${LZMA_INCLUDE_DIR}" )
	add_executable( zipdir
		zipdir.c )
	target_link_libraries( zipdir ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} lzma )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} zipdir PARENT_SCOPE )
endif(NOT CMAKE_CROSSCOMPILING)
