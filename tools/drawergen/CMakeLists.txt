cmake_minimum_required( VERSION 2.8.7 )

include( CheckCXXCompilerFlag )

include(../../precompiled_headers.cmake)

# Path where it looks for the LLVM compiled files on Windows
set( LLVM_PRECOMPILED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../llvm" )

if( NOT DRAWERGEN_LIBS )
	set( DRAWERGEN_LIBS "" )
endif()

include_directories( . )

file( GLOB HEADER_FILES
	*.h
	ssa/*.h
	fixedfunction/*.h
)

if( NOT WIN32 )
	set( LLVM_COMPONENTS core support asmparser asmprinter bitreader bitwriter codegen ipo
		irreader transformutils instrumentation profiledata runtimedyld
		object instcombine linker analysis selectiondag scalaropts vectorize executionengine
		mc mcdisassembler mcparser mcjit target x86asmprinter x86info x86desc x86utils x86codegen )

	# Example LLVM_DIR folder: C:/Development/Environment/Src/llvm-3.9.0/build/lib/cmake/llvm
	find_package(LLVM REQUIRED CONFIG)
	message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
	llvm_map_components_to_libnames( llvm_libs ${LLVM_COMPONENTS} )
	include_directories( ${LLVM_INCLUDE_DIRS} )
	set( DRAWERGEN_LIBS ${DRAWERGEN_LIBS} ${llvm_libs} )
else()
	set( LLVM_COMPONENTS core support asmparser asmprinter bitreader bitwriter codegen passes ipo
		irreader transformutils instrumentation profiledata debuginfocodeview runtimedyld
		object instcombine linker analysis selectiondag scalaropts vectorize executionengine
		mc mcdisassembler mcparser mcjit target x86asmprinter x86info x86desc x86utils x86codegen )

	include_directories( "${LLVM_PRECOMPILED_DIR}/include" )
	if( X64 )
		include_directories( "${LLVM_PRECOMPILED_DIR}/64bit-include" )
		set( llvm_libs_base "${LLVM_PRECOMPILED_DIR}/64bit-" )
	else()
		include_directories( "${LLVM_PRECOMPILED_DIR}/32bit-include" )
		set( llvm_libs_base "${LLVM_PRECOMPILED_DIR}/32bit-" )
	endif()
	foreach(buildtype IN ITEMS RELEASE DEBUG)
		set( llvm_libs_${buildtype} "${llvm_libs_base}${buildtype}" )
		set( LLVM_${buildtype}_LIBS "" )
		foreach( llvm_module ${LLVM_COMPONENTS} )
			find_library( LLVM_${llvm_module}_LIBRARY_${buildtype} LLVM${llvm_module} PATHS ${llvm_libs_${buildtype}} )
			set( LLVM_${buildtype}_LIBS ${LLVM_${buildtype}_LIBS} ${LLVM_${llvm_module}_LIBRARY_${buildtype}} )
		endforeach( llvm_module )
	endforeach(buildtype)
endif()

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.5")
		set( CMAKE_C_FLAGS "-Wno-unused-result ${CMAKE_C_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-Wno-unused-result ${CMAKE_CXX_FLAGS}" )
	endif()
	if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
		if( APPLE OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.6" )
			set( CMAKE_CXX_FLAGS "-Wno-inconsistent-missing-override ${CMAKE_CXX_FLAGS}" )
		endif()
	endif()
	set( CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_C_FLAGS}" )
	set( CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_CXX_FLAGS}" )

	# Use the highest C++ standard available since VS2015 compiles with C++14
	# but we only require C++11.  The recommended way to do this in CMake is to
	# probably to use target_compile_features, but I don't feel like maintaining
	# a list of features we use.
	CHECK_CXX_COMPILER_FLAG( "-std=gnu++14" CAN_DO_CPP14 )
	if ( CAN_DO_CPP14 )
		set ( CMAKE_CXX_FLAGS "-std=gnu++14 ${CMAKE_CXX_FLAGS}" )
	else ()
		CHECK_CXX_COMPILER_FLAG( "-std=gnu++1y" CAN_DO_CPP1Y )
		if ( CAN_DO_CPP1Y )
			set ( CMAKE_CXX_FLAGS "-std=gnu++1y ${CMAKE_CXX_FLAGS}" )
		else ()
			CHECK_CXX_COMPILER_FLAG( "-std=gnu++11" CAN_DO_CPP11 )
			if ( CAN_DO_CPP11 )
				set ( CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}" )
			else ()
				CHECK_CXX_COMPILER_FLAG( "-std=gnu++0x" CAN_DO_CPP0X )
				if ( CAN_DO_CPP0X )
					set ( CMAKE_CXX_FLAGS "-std=gnu++0x ${CMAKE_CXX_FLAGS}" )
				endif ()
			endif ()
		endif ()
	endif ()
	
	if ( APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
		set( CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++ ${CMAKE_EXE_LINKER_FLAGS}" )
	endif ()
endif()

if( WIN32 )
	if( MSVC_VERSION GREATER 1399 )
		# VC 8+ adds a manifest automatically to the executable. We need to
		# merge ours with it.
		set( MT_MERGE ON )
	else()
		set( TRUSTINFO trustinfo.rc )
	endif()
else( WIN32 )
	set( TRUSTINFO "" )
endif()

set (SOURCES
	drawergen.cpp
	llvmprogram.cpp
	llvmdrawers.cpp
	ssa/ssa_bool.cpp
	ssa/ssa_float.cpp
	ssa/ssa_float_ptr.cpp
	ssa/ssa_for_block.cpp
	ssa/ssa_function.cpp
	ssa/ssa_if_block.cpp
	ssa/ssa_int.cpp
	ssa/ssa_int_ptr.cpp
	ssa/ssa_short.cpp
	ssa/ssa_scope.cpp
	ssa/ssa_struct_type.cpp
	ssa/ssa_ubyte.cpp
	ssa/ssa_ubyte_ptr.cpp
	ssa/ssa_value.cpp
	ssa/ssa_vec4f.cpp
	ssa/ssa_vec4f_ptr.cpp
	ssa/ssa_vec4i.cpp
	ssa/ssa_vec4i_ptr.cpp
	ssa/ssa_vec8s.cpp
	ssa/ssa_vec16ub.cpp
	fixedfunction/drawercodegen.cpp
	fixedfunction/drawspancodegen.cpp
	fixedfunction/drawwallcodegen.cpp
	fixedfunction/drawcolumncodegen.cpp
	fixedfunction/drawskycodegen.cpp
	fixedfunction/drawtrianglecodegen.cpp
	fixedfunction/setuptrianglecodegen.cpp
)
enable_precompiled_headers( precomp.h SOURCES )

if( NOT CMAKE_CROSSCOMPILING )
	add_executable( drawergen ${SOURCES} ${TRUSTINFO} ${HEADER_FILES} )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} drawergen PARENT_SCOPE )
endif()

if( MT_MERGE )
	add_custom_command(TARGET drawergen POST_BUILD
		COMMAND mt -inputresource:$<TARGET_FILE:drawergen> -manifest ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.txt -outputresource:$<TARGET_FILE:drawergen> -nologo
		COMMENT "Embedding trustinfo into drawergen" )
endif()

# Linux - add these flags for LLVM compatibility to prevent crashing
#if ( UNIX AND NOT APPLE )
#	set( CMAKE_EXE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_EXE_LINKER_FLAGS}" )
#endif()

target_link_libraries( drawergen ${DRAWERGEN_LIBS} )

if( WIN32 )
	foreach(debuglib ${LLVM_DEBUG_LIBS})
		target_link_libraries( drawergen debug ${debuglib} )
	endforeach(debuglib)
	foreach(releaselib ${LLVM_RELEASE_LIBS})
		target_link_libraries( drawergen optimized ${releaselib} )
	endforeach(releaselib)
endif()

#source_group("Render Compiler" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_compiler/.+")
#source_group("Render Compiler\\SSA" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_compiler/ssa/.+")
#source_group("Render Compiler\\Fixed Function" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_compiler/fixedfunction/.+")

source_group("Compiler" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/.+\\.(cpp|h)$")
source_group("Compiler\\SSA" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/ssa/.+")
source_group("Compiler\\Fixed Function" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/fixedfunction/.+")
