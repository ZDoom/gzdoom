cmake_minimum_required(VERSION 3.15)
project(zvulkan)

set(ZVULKAN_SOURCES
	src/vulkanbuilders.cpp
	src/vulkandevice.cpp
	src/vulkaninstance.cpp
	src/vulkansurface.cpp
	src/vulkanswapchain.cpp
	src/vk_mem_alloc/vk_mem_alloc.cpp
	src/vk_mem_alloc/vk_mem_alloc.natvis
	src/volk/volk.c
	src/glslang/glslang/MachineIndependent/SymbolTable.h
	src/glslang/glslang/MachineIndependent/propagateNoContraction.cpp
	src/glslang/glslang/MachineIndependent/PoolAlloc.cpp
	src/glslang/glslang/MachineIndependent/Intermediate.cpp
	src/glslang/glslang/MachineIndependent/gl_types.h
	src/glslang/glslang/MachineIndependent/parseVersions.h
	src/glslang/glslang/MachineIndependent/attribute.cpp
	src/glslang/glslang/MachineIndependent/Scan.cpp
	src/glslang/glslang/MachineIndependent/iomapper.h
	src/glslang/glslang/MachineIndependent/ParseHelper.h
	src/glslang/glslang/MachineIndependent/glslang_tab.cpp.h
	src/glslang/glslang/MachineIndependent/SymbolTable.cpp
	src/glslang/glslang/MachineIndependent/RemoveTree.cpp
	src/glslang/glslang/MachineIndependent/Versions.h
	src/glslang/glslang/MachineIndependent/reflection.cpp
	src/glslang/glslang/MachineIndependent/LiveTraverser.h
	src/glslang/glslang/MachineIndependent/iomapper.cpp
	src/glslang/glslang/MachineIndependent/intermOut.cpp
	src/glslang/glslang/MachineIndependent/Versions.cpp
	src/glslang/glslang/MachineIndependent/Initialize.h
	src/glslang/glslang/MachineIndependent/linkValidate.cpp
	src/glslang/glslang/MachineIndependent/InfoSink.cpp
	src/glslang/glslang/MachineIndependent/Constant.cpp
	src/glslang/glslang/MachineIndependent/IntermTraverse.cpp
	src/glslang/glslang/MachineIndependent/propagateNoContraction.h
	src/glslang/glslang/MachineIndependent/glslang_tab.cpp
	src/glslang/glslang/MachineIndependent/ShaderLang.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/Pp.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/PpTokens.h
	src/glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp
	src/glslang/glslang/MachineIndependent/preprocessor/PpContext.h
	src/glslang/glslang/MachineIndependent/attribute.h
	src/glslang/glslang/MachineIndependent/localintermediate.h
	src/glslang/glslang/MachineIndependent/parseConst.cpp
	src/glslang/glslang/MachineIndependent/Initialize.cpp
	src/glslang/glslang/MachineIndependent/limits.cpp
	src/glslang/glslang/MachineIndependent/ParseContextBase.cpp
	src/glslang/glslang/MachineIndependent/RemoveTree.h
	src/glslang/glslang/MachineIndependent/ParseHelper.cpp
	src/glslang/glslang/MachineIndependent/Scan.h
	src/glslang/glslang/MachineIndependent/reflection.h
	src/glslang/glslang/MachineIndependent/ScanContext.h
	src/glslang/glslang/MachineIndependent/SpirvIntrinsics.cpp
	src/glslang/glslang/OSDependent/osinclude.h
	src/glslang/glslang/GenericCodeGen/Link.cpp
	src/glslang/glslang/GenericCodeGen/CodeGen.cpp
	src/glslang/glslang/Public/ShaderLang.h
	src/glslang/glslang/Include/ConstantUnion.h
	src/glslang/glslang/Include/InitializeGlobals.h
	src/glslang/glslang/Include/Common.h
	src/glslang/glslang/Include/PoolAlloc.h
	src/glslang/glslang/Include/arrays.h
	src/glslang/glslang/Include/ShHandle.h
	src/glslang/glslang/Include/InfoSink.h
	src/glslang/glslang/Include/ResourceLimits.h
	src/glslang/glslang/Include/Types.h
	src/glslang/glslang/Include/BaseTypes.h
	src/glslang/glslang/Include/intermediate.h
	src/glslang/glslang/Include/SpirvIntrinsics.h
	src/glslang/glslang/Include/build_info.h
	src/glslang/glslang/OSDependent/osinclude.h
	src/glslang/spirv/Logger.h
	src/glslang/spirv/GlslangToSpv.cpp
	src/glslang/spirv/SPVRemapper.h
	src/glslang/spirv/GLSL.ext.EXT.h
	src/glslang/spirv/hex_float.h
	src/glslang/spirv/doc.cpp
	src/glslang/spirv/disassemble.cpp
	src/glslang/spirv/SpvPostProcess.cpp
	src/glslang/spirv/bitutils.h
	src/glslang/spirv/InReadableOrder.cpp
	src/glslang/spirv/GLSL.ext.AMD.h
	src/glslang/spirv/GLSL.ext.NV.h
	src/glslang/spirv/SPVRemapper.cpp
	src/glslang/spirv/SpvBuilder.h
	src/glslang/spirv/GLSL.ext.KHR.h
	src/glslang/spirv/disassemble.h
	src/glslang/spirv/SpvBuilder.cpp
	src/glslang/spirv/GlslangToSpv.h
	src/glslang/spirv/doc.h
	src/glslang/spirv/SpvTools.cpp
	src/glslang/spirv/spvIR.h
	src/glslang/spirv/Logger.cpp
	src/glslang/spirv/SpvTools.h
	src/glslang/spirv/GLSL.std.450.h
	src/glslang/spirv/NonSemanticDebugPrintf.h
	src/glslang/OGLCompilersDLL/InitializeDll.cpp
	src/glslang/OGLCompilersDLL/InitializeDll.h
)

set(ZVULKAN_INCLUDES
	include/zvulkan/vulkanbuilders.h
	include/zvulkan/vulkancompatibledevice.h
	include/zvulkan/vulkandevice.h
	include/zvulkan/vulkaninstance.h
	include/zvulkan/vulkanobjects.h
	include/zvulkan/vulkansurface.h
	include/zvulkan/vulkanswapchain.h
	include/zvulkan/volk/volk.h
	include/zvulkan/vk_mem_alloc/vk_mem_alloc.h
)

set(ZVULKAN_WIN32_SOURCES
	src/glslang/glslang/OSDependent/Windows/ossource.cpp
)

set(ZVULKAN_UNIX_SOURCES
	src/glslang/glslang/OSDependent/Unix/ossource.cpp
)

set(VULKAN_INCLUDES
	include/vulkan/vk_enum_string_helper.h
	include/vulkan/vk_icd.h
	include/vulkan/vk_layer.h
	include/vulkan/vk_layer_dispatch_table.h
	include/vulkan/vk_platform.h
	include/vulkan/vk_sdk_platform.h
	include/vulkan/vulkan.h
	include/vulkan/vulkan_android.h
	include/vulkan/vulkan_beta.h
	include/vulkan/vulkan_core.h
	include/vulkan/vulkan_directfb.h
	include/vulkan/vulkan_fuchsia.h
	include/vulkan/vulkan_ggp.h
	include/vulkan/vulkan_ios.h
	include/vulkan/vulkan_macos.h
	include/vulkan/vulkan_metal.h
	include/vulkan/vulkan_mir.h
	include/vulkan/vulkan_screen.h
	include/vulkan/vulkan_vi.h
	include/vulkan/vulkan_wayland.h
	include/vulkan/vulkan_win32.h
	include/vulkan/vulkan_xcb.h
	include/vulkan/vulkan_xlib.h
	include/vulkan/vulkan_xlib_xrandr.h
)

source_group("src" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("src\\glslang" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/.+")
source_group("src\\glslang\\glslang" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/.+")
source_group("src\\glslang\\glslang\\GenericCodeGen" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/GenericCodeGen/.+")
source_group("src\\glslang\\glslang\\Include" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/Include/.+")
source_group("src\\glslang\\glslang\\MachineIndependent" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/MachineIndependent/.+")
source_group("src\\glslang\\glslang\\MachineIndependent\\preprocessor" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/MachineIndependent/preprocessor/.+")
source_group("src\\glslang\\glslang\\OSDependent" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/OSDependent/.+")
source_group("src\\glslang\\glslang\\OSDependent\\Unix" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/OSDependent/Unix/.+")
source_group("src\\glslang\\glslang\\OSDependent\\Web" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/OSDependent/Web/.+")
source_group("src\\glslang\\glslang\\OSDependent\\Windows" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/OSDependent/Windows/.+")
source_group("src\\glslang\\glslang\\Public" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/glslang/Public/.+")
source_group("src\\glslang\\OGLCompilersDLL" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/OGLCompilersDLL/.+")
source_group("src\\glslang\\spirv" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/glslang/spirv/.+")
source_group("src\\vk_mem_alloc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_mem_alloc/.+")
source_group("src\\volk" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/volk/.+")
source_group("include" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zvulkan/.+")
source_group("include\\vk_mem_alloc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zvulkan/vk_mem_alloc/.+")
source_group("include\\volk" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zvulkan/volk/.+")

include_directories(include include/zvulkan src)

if(WIN32)
	set(ZVULKAN_SOURCES ${ZVULKAN_SOURCES} ${ZVULKAN_WIN32_SOURCES})
	add_definitions(-DUNICODE -D_UNICODE)
else()
	set(ZVULKAN_SOURCES ${ZVULKAN_SOURCES} ${ZVULKAN_UNIX_SOURCES})
	set(ZVULKAN_LIBS ${CMAKE_DL_LIBS} -ldl)
	add_definitions(-DUNIX -D_UNIX)
	add_link_options(-pthread)
endif()

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# Ignore warnings in third party code
	#set_source_files_properties(${ZVULKAN_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 -D_CRT_SECURE_NO_WARNINGS")
endif()

add_library(zvulkan STATIC ${ZVULKAN_SOURCES} ${ZVULKAN_INCLUDES} ${VULKAN_INCLUDES})
target_link_libraries(zvulkan ${ZVULKAN_LIBS})
set_target_properties(zvulkan PROPERTIES CXX_STANDARD 17)

if(MSVC)
	set_property(TARGET zvulkan PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
