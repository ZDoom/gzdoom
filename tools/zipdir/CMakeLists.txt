cmake_minimum_required( VERSION 3.1.0 )

if( NOT CMAKE_CROSSCOMPILING )
	include_directories( SYSTEM "${ZLIB_INCLUDE_DIR}" "${BZIP2_INCLUDE_DIR}" "${LZMA_INCLUDE_DIR}" )
	add_executable( zipdir
		zipdir.c )
	target_link_libraries( zipdir ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} lzma )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} zipdir PARENT_SCOPE )
endif()
