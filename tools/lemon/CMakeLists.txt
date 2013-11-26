cmake_minimum_required( VERSION 2.4 )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" )

add_executable( lemon lemon.c )

# Lemon wants lempar.c in its directory
if( NOT NO_GENERATOR_EXPRESSIONS )
add_custom_command( TARGET lemon
	POST_BUILD
	COMMAND echo $<TARGET_FILE_DIR:lemon>
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c $<TARGET_FILE_DIR:lemon> )	
else( NOT NO_GENERATOR_EXPRESSIONS )
add_custom_command( TARGET lemon
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c ${CMAKE_CURRENT_BINARY_DIR} )
endif( NOT NO_GENERATOR_EXPRESSIONS )