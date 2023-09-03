cmake_minimum_required( VERSION 3.16 )

if( NOT CMAKE_CROSSCOMPILING )

include( CheckFunctionExists )
include( CheckTypeSize )

if( MSVC )
	# Runtime type information is required and don't complain about uint32_t to bool conversions
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR /wd4800" )
endif()

set( PACKAGE_NAME re2c )
set( PACKAGE_TARNAME re2c )
set( PACKAGE_VERSION 0.16 )
set( PACKAGE_STRING "re2c 0.16" )
set( PACKAGE_BUGREPORT "re2c-general@lists.sourceforge.net" )

CHECK_FUNCTION_EXISTS( strdup HAVE_STRDUP )
CHECK_FUNCTION_EXISTS( strndup HAVE_STRNDUP )

CHECK_TYPE_SIZE( "0i8" SIZEOF_0I8 )
CHECK_TYPE_SIZE( "0l" SIZEOF_0L )
CHECK_TYPE_SIZE( "0ll" SIZEOF_0LL )
CHECK_TYPE_SIZE( char SIZEOF_CHAR )
CHECK_TYPE_SIZE( short SIZEOF_SHORT )
CHECK_TYPE_SIZE( int SIZEOF_INT )
CHECK_TYPE_SIZE( long SIZEOF_LONG )
CHECK_TYPE_SIZE( "long long" SIZEOF_LONG_LONG )
CHECK_TYPE_SIZE( "void *" SIZEOF_VOID_P )
CHECK_TYPE_SIZE( __int64 SIZEOF___INT_64 )

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )
add_definitions( -DHAVE_CONFIG_H )

file( GLOB SRC_HDR
	src/codegen/*.h
	src/conf/*.h
	src/ir/*.h
	src/*.h
	src/parse/*.h
	src/util/*.h )

add_executable( re2c
	${SRC_HDR}
        src/codegen/bitmap.cc
        src/codegen/emit_action.cc
        src/codegen/emit_dfa.cc
        src/codegen/label.cc
        src/codegen/go_construct.cc
        src/codegen/go_destruct.cc
        src/codegen/go_emit.cc
        src/codegen/go_used_labels.cc
        src/codegen/input_api.cc
        src/codegen/output.cc
        src/codegen/print.cc
        src/conf/msg.cc
        src/conf/opt.cc
	src/conf/parse_opts.cc
        src/conf/warn.cc
        src/ir/nfa/calc_size.cc
        src/ir/nfa/nfa.cc
        src/ir/nfa/split.cc
        src/ir/adfa/adfa.cc
        src/ir/adfa/prepare.cc
        src/ir/dfa/determinization.cc
        src/ir/dfa/fillpoints.cc
        src/ir/dfa/minimization.cc
        src/ir/regexp/display.cc
        src/ir/regexp/encoding/enc.cc
        src/ir/regexp/encoding/range_suffix.cc
        src/ir/regexp/encoding/utf8/utf8_regexp.cc
        src/ir/regexp/encoding/utf8/utf8_range.cc
        src/ir/regexp/encoding/utf8/utf8.cc
        src/ir/regexp/encoding/utf16/utf16_regexp.cc
        src/ir/regexp/encoding/utf16/utf16.cc
        src/ir/regexp/encoding/utf16/utf16_range.cc
        src/ir/regexp/fixed_length.cc
        src/ir/regexp/regexp.cc
        src/ir/compile.cc
        src/ir/rule_rank.cc
        src/ir/skeleton/control_flow.cc
        src/ir/skeleton/generate_code.cc
        src/ir/skeleton/generate_data.cc
        src/ir/skeleton/match_empty.cc
        src/ir/skeleton/maxlen.cc
        src/ir/skeleton/skeleton.cc
        src/ir/skeleton/unreachable.cc
        src/ir/skeleton/way.cc
        src/main.cc
        src/parse/code.cc
        src/parse/input.cc
	src/parse/lex.cc
	src/parse/lex_conf.cc
	src/parse/parser.cc
        src/parse/scanner.cc
        src/parse/unescape.cc
        src/util/s_to_n32_unsafe.cc
        src/util/range.cc )

set( CROSS_EXPORTS ${CROSS_EXPORTS} re2c PARENT_SCOPE )

endif()
