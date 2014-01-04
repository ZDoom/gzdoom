cmake_minimum_required( VERSION 2.4 )

if( NOT CMAKE_CROSSCOMPILING )

include( CheckFunctionExists )
include( CheckTypeSize )

set( PACKAGE_NAME re2c )
set( PACKAGE_TARNAME re2c )
set( PACKAGE_VERSION 0.12.3 )
set( PACKAGE_STRING "re2c 0.12.3" )
set( PACKAGE_BUGREPORT "re2c-general@lists.sourceforge.net" )

CHECK_FUNCTION_EXISTS( strdup HAVE_STRDUP )
CHECK_FUNCTION_EXISTS( strndup HAVE_STRNDUP )

CHECK_TYPE_SIZE( char SIZEOF_CHAR )
CHECK_TYPE_SIZE( short SIZEOF_SHORT )
CHECK_TYPE_SIZE( int SIZEOF_INT )
CHECK_TYPE_SIZE( long SIZEOF_LONG )

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )
add_definitions( -DHAVE_CONFIG_H )

add_executable( re2c
	actions.cc
	code.cc
	dfa.cc
	main.cc
	mbo_getopt.cc
	parser.cc
	scanner.cc
	substr.cc
	translate.cc )

set( CROSS_EXPORTS ${CROSS_EXPORTS} re2c PARENT_SCOPE )

endif( NOT CMAKE_CROSSCOMPILING )
