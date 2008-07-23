cmake_minimum_required( VERSION 2.6 )
project( dehsupp )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parse.h ${CMAKE_CURRENT_BINARY_DIR}/parse.c
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/parse.y ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND lemon parse.y
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS lemon parse.y )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
	COMMAND re2c -s -o ${CMAKE_CURRENT_BINARY_DIR}/scanner.c ${CMAKE_CURRENT_SOURCE_DIR}/scanner.re
	DEPENDS re2c scanner.re )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( dehsupp dehsupp.c parse.c scanner.c )
add_dependencies( dehsupp ${CMAKE_CURRENT_BINARY_DIR}/parse.c ${CMAKE_CURRENT_BINARY_DIR}/scanner.c )
