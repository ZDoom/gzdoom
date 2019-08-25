cmake_minimum_required(VERSION 2.8.7)
#make_release_only()

project(asmjit CXX)

set(ASMJITNAME asmjit)
add_definitions(-DASMJIT_STATIC)

if(CMAKE_VERSION VERSION_LESS "3.1")
	if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	endif()
else()
	set(CMAKE_CXX_STANDARD 11)
endif()

if(MSVC)
	set(CMAKE_DEBUG_POSTFIX "d")
	add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

if(APPLE)
	# Suppress stdlib.h:334:6: warning: pointer is missing a nullability type specifier (_Nonnull, _Nullable, or _Null_unspecified)
	add_definitions(-Wno-nullability-completeness)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(ASMJIT_SRCS
	asmjit/asmjit.h

	asmjit/core.h
	asmjit/core/build.h
	asmjit/core/arch.cpp
	asmjit/core/arch.h
	asmjit/core/assembler.cpp
	asmjit/core/assembler.h
	asmjit/core/builder.cpp
	asmjit/core/builder.h
	asmjit/core/callconv.cpp
	asmjit/core/callconv.h
	asmjit/core/codebufferwriter_p.h
	asmjit/core/codeholder.cpp
	asmjit/core/codeholder.h
	asmjit/core/compiler.cpp
	asmjit/core/compiler.h
	asmjit/core/constpool.cpp
	asmjit/core/constpool.h
	asmjit/core/cpuinfo.cpp
	asmjit/core/cpuinfo.h
	asmjit/core/datatypes.h
	asmjit/core/emitter.cpp
	asmjit/core/emitter.h
	asmjit/core/features.h
	asmjit/core/func.cpp
	asmjit/core/func.h
	asmjit/core/globals.cpp
	asmjit/core/globals.h
	asmjit/core/inst.cpp
	asmjit/core/inst.h
	asmjit/core/jitallocator.cpp
	asmjit/core/jitallocator.h
	asmjit/core/jitruntime.cpp
	asmjit/core/jitruntime.h
	asmjit/core/logging.cpp
	asmjit/core/logging.h
	asmjit/core/misc_p.h
	asmjit/core/operand.cpp
	asmjit/core/operand.h
	asmjit/core/osutils.cpp
	asmjit/core/osutils.h
	asmjit/core/raassignment_p.h
	asmjit/core/rabuilders_p.h
	asmjit/core/radefs_p.h
	asmjit/core/ralocal.cpp
	asmjit/core/ralocal_p.h
	asmjit/core/rapass.cpp
	asmjit/core/rapass_p.h
	asmjit/core/rastack.cpp
	asmjit/core/rastack_p.h
	asmjit/core/string.cpp
	asmjit/core/string.h
	asmjit/core/support.cpp
	asmjit/core/support.h
	asmjit/core/target.cpp
	asmjit/core/target.h
	asmjit/core/type.cpp
	asmjit/core/type.h
	asmjit/core/virtmem.cpp
	asmjit/core/virtmem.h
	asmjit/core/zone.cpp
	asmjit/core/zone.h
	asmjit/core/zonehash.cpp
	asmjit/core/zonehash.h
	asmjit/core/zonelist.cpp
	asmjit/core/zonelist.h
	asmjit/core/zonestack.cpp
	asmjit/core/zonestack.h
	asmjit/core/zonestring.h
	asmjit/core/zonetree.cpp
	asmjit/core/zonetree.h
	asmjit/core/zonevector.cpp
	asmjit/core/zonevector.h

	asmjit/x86.h
	asmjit/x86/x86assembler.cpp
	asmjit/x86/x86assembler.h
	asmjit/x86/x86builder.cpp
	asmjit/x86/x86builder.h
	asmjit/x86/x86callconv.cpp
	asmjit/x86/x86callconv_p.h
	asmjit/x86/x86compiler.cpp
	asmjit/x86/x86compiler.h
	asmjit/x86/x86emitter.h
	asmjit/x86/x86features.cpp
	asmjit/x86/x86features.h
	asmjit/x86/x86globals.h
	asmjit/x86/x86instapi.cpp
	asmjit/x86/x86instapi_p.h
	asmjit/x86/x86instdb.cpp
	asmjit/x86/x86instdb.h
	asmjit/x86/x86instdb_p.h
	asmjit/x86/x86internal.cpp
	asmjit/x86/x86internal_p.h
	asmjit/x86/x86logging.cpp
	asmjit/x86/x86logging_p.h
	asmjit/x86/x86opcode_p.h
	asmjit/x86/x86operand.cpp
	asmjit/x86/x86operand.h
	asmjit/x86/x86rapass.cpp
	asmjit/x86/x86rapass_p.h
)

set(ASMJIT_PUBLIC_HDRS "")
foreach(_src_file ${ASMJIT_SRCS})
	if ("${_src_file}" MATCHES "\\.h$" AND NOT "${_src_file}" MATCHES "_p\\.h$")
		list(APPEND ASMJIT_PUBLIC_HDRS ${_src_file})
	endif()
endforeach()

add_library(${ASMJITNAME} STATIC ${ASMJIT_SRCS})
set_target_properties(${ASMJITNAME} PROPERTIES OUTPUT_NAME asmjit)

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
	install(TARGETS ${ASMJITNAME}
		RUNTIME DESTINATION bin
		ARCHIVE DESTINATION lib
		LIBRARY DESTINATION lib)
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
	install(FILES ${ASMJIT_PUBLIC_HDRS} DESTINATION include)
endif()
