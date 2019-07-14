cmake_minimum_required( VERSION 2.8.7 )

add_subdirectory( re2c )
add_subdirectory( lemon )
add_subdirectory( updaterevision )
add_subdirectory( zipdir )

set( CROSS_EXPORTS ${CROSS_EXPORTS} PARENT_SCOPE )
