cmake_minimum_required( VERSION 2.4 )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" )

# Disable warnings for << operator precedence (4554) and
# unreferenced labels (4102) from VC
if( MSVC )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4554 /wd4102" )
endif( MSVC )

if( CMAKE_COMPILER_IS_GNUCXX )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra" )
endif( CMAKE_COMPILER_IS_GNUCXX )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )
add_definitions( -DINFNAN_CHECK -DMULTIPLE_THREADS )

if( NOT MSVC )
	add_executable( arithchk arithchk.c )
	add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/arith.h 
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/arithchk >${CMAKE_CURRENT_BINARY_DIR}/arith.h
		DEPENDS arithchk )

	add_executable( qnan qnan.c arith.h )
	add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gd_qnan.h
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/qnan >${CMAKE_CURRENT_BINARY_DIR}/gd_qnan.h
		DEPENDS qnan )
		
	set( GEN_FP_FILES arith.h gd_qnan.h )
	set( GEN_FP_DEPS ${CMAKE_CURRENT_BINARY_DIR}/arith.h ${CMAKE_CURRENTY_BINARY_DIR}/gd_qnan.h )
endif( NOT MSVC )

add_library( gdtoa
	${GEN_FP_FILES}
	dmisc.c
	dtoa.c
	g_Qfmt.c
	g__fmt.c
	g_ddfmt.c
	g_dfmt.c
	g_ffmt.c
	g_xLfmt.c
	g_xfmt.c
	gdtoa.c
	gethex.c
	gmisc.c
	hd_init.c
	hexnan.c
	misc.c
	smisc.c
	strtoIQ.c
	strtoId.c
	strtoIdd.c
	strtoIf.c
	strtoIg.c
	strtoIx.c
	strtoIxL.c
	strtod.c
	strtodI.c
	strtodg.c
	strtof.c
	strtopQ.c
	strtopd.c
	strtopdd.c
	strtopf.c
	strtopx.c
	strtopxL.c
	strtorQ.c
	strtord.c
	strtordd.c
	strtorf.c
	strtorx.c
	strtorxL.c
	sum.c
	ulp.c)
target_link_libraries( gdtoa )
if( GEN_FP_DEPS )
	add_dependencies( gdtoa ${GEN_FP_DEPS} )
endif( GEN_FP_DEPS )

