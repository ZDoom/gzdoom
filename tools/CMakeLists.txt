cmake_minimum_required( VERSION 2.4 )

add_subdirectory( lemon )
add_subdirectory( re2c )
if( WIN32 )
	add_subdirectory( fixrtext )
endif( WIN32 )
add_subdirectory( updaterevision )
add_subdirectory( zipdir )

set( CROSS_EXPORTS ${CROSS_EXPORTS} PARENT_SCOPE )
