cmake_minimum_required( VERSION 3.1.0 )

if( NOT CMAKE_CROSSCOMPILING )
	add_executable( xpmtosdl
		xpmtosdl.cpp )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} xpmtosdl PARENT_SCOPE )
endif()
