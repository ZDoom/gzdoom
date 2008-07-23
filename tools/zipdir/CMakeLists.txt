cmake_minimum_required( VERSION 2.4 )
add_executable( zipdir
	zipdir.c
	ioapi.c
	zip.c )
target_link_libraries( zipdir ${ZLIB_LIBRARIES} )
