cmake_minimum_required( VERSION 2.8.7 )

if( NOT CMAKE_CROSSCOMPILING )
	set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" )

	add_executable( lemon lemon.c )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} lemon PARENT_SCOPE )
endif()

# Lemon wants lempar.c in its directory
add_custom_command( TARGET lemon
	POST_BUILD
	COMMAND echo $<TARGET_FILE_DIR:lemon>
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c $<TARGET_FILE_DIR:lemon> )	
