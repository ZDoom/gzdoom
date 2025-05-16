# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22)

project(cppdap VERSION 1.65.0 LANGUAGES CXX C)

set (CMAKE_CXX_STANDARD 11)

include(GNUInstallDirs)

###########################################################
# Options
###########################################################
function (option_if_not_defined name description default)
    if(NOT DEFINED ${name})
        option(${name} ${description} ${default})
    endif()
endfunction()

option_if_not_defined(CPPDAP_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option_if_not_defined(CPPDAP_BUILD_EXAMPLES "Build example applications" OFF)
option_if_not_defined(CPPDAP_BUILD_TESTS "Build tests" OFF)
option_if_not_defined(CPPDAP_BUILD_FUZZER "Build fuzzer" OFF)
option_if_not_defined(CPPDAP_ASAN "Build dap with address sanitizer" OFF)
option_if_not_defined(CPPDAP_MSAN "Build dap with memory sanitizer" OFF)
option_if_not_defined(CPPDAP_TSAN "Build dap with thread sanitizer" OFF)
option_if_not_defined(CPPDAP_INSTALL_VSCODE_EXAMPLES "Build and install dap examples into vscode extensions directory" OFF)
option_if_not_defined(CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE "Use nlohmann_json with find_package() instead of building internal submodule" OFF)
option_if_not_defined(CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE "Use RapidJSON with find_package()" OFF)
option_if_not_defined(CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE "Use JsonCpp with find_package()" OFF)
option_if_not_defined(CPPDAP_USE_EXTERNAL_GTEST_PACKAGE "Use googletest with find_package()" OFF)

###########################################################
# Directories
###########################################################
function (set_if_not_defined name value)
    if(NOT DEFINED ${name})
        set(${name} ${value} PARENT_SCOPE)
    endif()
endfunction()

set(CPPDAP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CPPDAP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_if_not_defined(CPPDAP_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set_if_not_defined(CPPDAP_JSON_DIR        ${CPPDAP_THIRD_PARTY_DIR}/json)
set_if_not_defined(CPPDAP_GOOGLETEST_DIR  ${CPPDAP_THIRD_PARTY_DIR}/googletest)

###########################################################
# Submodules
###########################################################
if(CPPDAP_BUILD_TESTS AND NOT CPPDAP_USE_EXTERNAL_GTEST_PACKAGE)
    if(NOT EXISTS ${CPPDAP_GOOGLETEST_DIR}/.git)
        message(WARNING "third_party/googletest submodule missing.")
        message(WARNING "Run: `git submodule update --init` to build tests.")
        set(CPPDAP_BUILD_TESTS OFF)
    endif()
endif()

###########################################################
# JSON library
###########################################################

if(CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE)
    list(APPEND CPPDAP_EXTERNAL_JSON_PACKAGES "CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE")
endif()
if(CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE)
    list(APPEND CPPDAP_EXTERNAL_JSON_PACKAGES "CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE")
endif()
if(CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE)
    list(APPEND CPPDAP_EXTERNAL_JSON_PACKAGES "CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE")
endif()
list(LENGTH CPPDAP_EXTERNAL_JSON_PACKAGES CPPDAP_EXTERNAL_JSON_PACKAGES_COUNT)

if(CPPDAP_EXTERNAL_JSON_PACKAGES_COUNT GREATER 1)
    message(FATAL_ERROR "At most one of CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE, \
CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE, and CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE can \
be set, but ${CPPDAP_EXTERNAL_JSON_PACKAGES} were all set.")
elseif(CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE)
    find_package(nlohmann_json CONFIG REQUIRED)
    set(CPPDAP_JSON_LIBRARY "nlohmann")
elseif(CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE)
    find_package(RapidJSON CONFIG REQUIRED)
    set(CPPDAP_JSON_LIBRARY "rapid")
elseif(CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE)
    find_package(jsoncpp CONFIG REQUIRED)
    set(CPPDAP_JSON_LIBRARY "jsoncpp")
endif()

if(NOT DEFINED CPPDAP_JSON_LIBRARY)
    # Attempt to detect JSON library from CPPDAP_JSON_DIR
    if(NOT EXISTS "${CPPDAP_JSON_DIR}")
        message(FATAL_ERROR "CPPDAP_JSON_DIR '${CPPDAP_JSON_DIR}' does not exist")
    endif()

    if(EXISTS "${CPPDAP_JSON_DIR}/include/nlohmann")
        set(CPPDAP_JSON_LIBRARY "nlohmann")
    elseif(EXISTS "${CPPDAP_JSON_DIR}/include/rapidjson")
        set(CPPDAP_JSON_LIBRARY "rapid")
    elseif(EXISTS "${CPPDAP_JSON_DIR}/include/json")
        set(CPPDAP_JSON_LIBRARY "jsoncpp")
    else()
        message(FATAL_ERROR "Could not determine JSON library from ${CPPDAP_JSON_DIR}")
    endif()
endif()
string(TOUPPER ${CPPDAP_JSON_LIBRARY} CPPDAP_JSON_LIBRARY_UPPER)

###########################################################
# File lists
###########################################################
set(CPPDAP_LIST
    ${CPPDAP_SRC_DIR}/content_stream.cpp
    ${CPPDAP_SRC_DIR}/io.cpp
    ${CPPDAP_SRC_DIR}/${CPPDAP_JSON_LIBRARY}_json_serializer.cpp
    ${CPPDAP_SRC_DIR}/network.cpp
    ${CPPDAP_SRC_DIR}/null_json_serializer.cpp
    ${CPPDAP_SRC_DIR}/protocol_events.cpp
    ${CPPDAP_SRC_DIR}/protocol_requests.cpp
    ${CPPDAP_SRC_DIR}/protocol_response.cpp
    ${CPPDAP_SRC_DIR}/protocol_types.cpp
    ${CPPDAP_SRC_DIR}/session.cpp
    ${CPPDAP_SRC_DIR}/socket.cpp
    ${CPPDAP_SRC_DIR}/typeinfo.cpp
    ${CPPDAP_SRC_DIR}/typeof.cpp
)

###########################################################
# OS libraries
###########################################################
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(CPPDAP_OS_LIBS WS2_32)
    set(CPPDAP_OS_EXE_EXT ".exe")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPPDAP_OS_LIBS pthread)
    set(CPPDAP_OS_EXE_EXT "")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPPDAP_OS_LIBS)
    set(CPPDAP_OS_EXE_EXT "")
endif()

###########################################################
# Functions
###########################################################

function(cppdap_set_json_links target)
    if (CPPDAP_USE_EXTERNAL_NLOHMANN_JSON_PACKAGE)
        target_link_libraries(${target} PRIVATE "$<BUILD_INTERFACE:nlohmann_json::nlohmann_json>")
    elseif(CPPDAP_USE_EXTERNAL_RAPIDJSON_PACKAGE)
        target_link_libraries(${target} PRIVATE rapidjson)
    elseif(CPPDAP_USE_EXTERNAL_JSONCPP_PACKAGE)
        target_link_libraries(${target} PRIVATE JsonCpp::JsonCpp)
    else()
        target_include_directories(${target} PRIVATE "${CPPDAP_JSON_DIR}/include/")
    endif()
endfunction(cppdap_set_json_links)

function(cppdap_set_target_options target)
    # Enable all warnings
    if(MSVC)
        target_compile_options(${target} PRIVATE "-W4")
    else()
        target_compile_options(${target} PRIVATE "-Wall")
    endif()

    # Disable specific, pedantic warnings
    if(MSVC)
        target_compile_options(${target} PRIVATE
            "-D_CRT_SECURE_NO_WARNINGS"

            # Warnings from nlohmann/json headers.
            "/wd4267" # 'argument': conversion from 'size_t' to 'int', possible loss of data
        )
    endif()

    # Add define for JSON library in use
    target_compile_definitions(${target}
        PRIVATE "CPPDAP_JSON_${CPPDAP_JSON_LIBRARY_UPPER}=1"
    )

    # Treat all warnings as errors
    if(CPPDAP_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE "/WX")
        else()
            target_compile_options(${target} PRIVATE "-Werror")
        endif()
    endif(CPPDAP_WARNINGS_AS_ERRORS)

    if(CPPDAP_ASAN)
        target_compile_options(${target} PUBLIC "-fsanitize=address")
        target_link_options(${target} PUBLIC "-fsanitize=address")
    elseif(CPPDAP_MSAN)
        target_compile_options(${target} PUBLIC "-fsanitize=memory")
        target_link_options(${target} PUBLIC "-fsanitize=memory")
    elseif(CPPDAP_TSAN)
        target_compile_options(${target} PUBLIC "-fsanitize=thread")
        target_link_options(${target} PUBLIC "-fsanitize=thread")
    endif()

    # Error on undefined symbols
    # if(NOT MSVC)
    #     target_compile_options(${target} PRIVATE "-Wl,--no-undefined")
    # endif()
    target_include_directories(
	"${target}"
	PUBLIC
		"$<BUILD_INTERFACE:${CPPDAP_INCLUDE_DIR}>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )
    cppdap_set_json_links(${target})
    target_link_libraries(${target} PRIVATE ${CPPDAP_OS_LIBS})
endfunction(cppdap_set_target_options)

###########################################################
# Targets
###########################################################

# dap
add_library(cppdap ${CPPDAP_LIST})
set_target_properties(cppdap PROPERTIES POSITION_INDEPENDENT_CODE 1)

cppdap_set_target_options(cppdap)

set(CPPDAP_TARGET_NAME               ${PROJECT_NAME})
set(CPPDAP_CONFIG_INSTALL_DIR        "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE INTERNAL "")
set(CPPDAP_INCLUDE_INSTALL_DIR       "${CMAKE_INSTALL_INCLUDEDIR}")
set(CPPDAP_TARGETS_EXPORT_NAME       "${PROJECT_NAME}-targets")
set(CPPDAP_CMAKE_CONFIG_TEMPLATE     "cmake/Config.cmake.in")
set(CPPDAP_CMAKE_CONFIG_DIR          "${CMAKE_CURRENT_BINARY_DIR}")
set(CPPDAP_CMAKE_VERSION_CONFIG_FILE "${CPPDAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(CPPDAP_CMAKE_PROJECT_CONFIG_FILE "${CPPDAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")
set(CPPDAP_CMAKE_PROJECT_TARGETS_FILE "${CPPDAP_CMAKE_CONFIG_DIR}/${PROJECT_NAME}-targets.cmake")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	${CPPDAP_CMAKE_VERSION_CONFIG_FILE} COMPATIBILITY SameMinorVersion
)
configure_package_config_file(
	${CPPDAP_CMAKE_CONFIG_TEMPLATE}
	"${CPPDAP_CMAKE_PROJECT_CONFIG_FILE}"
	INSTALL_DESTINATION ${CPPDAP_CONFIG_INSTALL_DIR}
)

install(
	TARGETS                  "${CPPDAP_TARGET_NAME}"
	EXPORT                   "${CPPDAP_TARGETS_EXPORT_NAME}"
	ARCHIVE DESTINATION      "${CMAKE_INSTALL_LIBDIR}"
	LIBRARY DESTINATION      "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME DESTINATION      "${CMAKE_INSTALL_BINDIR}"
	INCLUDES DESTINATION     "${CPPDAP_INCLUDE_INSTALL_DIR}"
)

install(
	EXPORT "${CPPDAP_TARGETS_EXPORT_NAME}"
	NAMESPACE "${CPPDAP_TARGET_NAME}::"
	DESTINATION "${CPPDAP_CONFIG_INSTALL_DIR}"
)

install(
	DIRECTORY
		"include/dap"
	DESTINATION "${CPPDAP_INCLUDE_INSTALL_DIR}"
)
install(FILES ${CPPDAP_CMAKE_VERSION_CONFIG_FILE} ${CPPDAP_CMAKE_PROJECT_CONFIG_FILE}
DESTINATION ${CPPDAP_CONFIG_INSTALL_DIR})

# tests
if(CPPDAP_BUILD_TESTS)
    enable_testing()

    set(DAP_TEST_LIST
        ${CPPDAP_SRC_DIR}/any_test.cpp
        ${CPPDAP_SRC_DIR}/chan_test.cpp
        ${CPPDAP_SRC_DIR}/content_stream_test.cpp
        ${CPPDAP_SRC_DIR}/dap_test.cpp
        ${CPPDAP_SRC_DIR}/json_serializer_test.cpp
        ${CPPDAP_SRC_DIR}/network_test.cpp
        ${CPPDAP_SRC_DIR}/optional_test.cpp
        ${CPPDAP_SRC_DIR}/rwmutex_test.cpp
        ${CPPDAP_SRC_DIR}/session_test.cpp
        ${CPPDAP_SRC_DIR}/socket_test.cpp
        ${CPPDAP_SRC_DIR}/traits_test.cpp
        ${CPPDAP_SRC_DIR}/typeinfo_test.cpp
        ${CPPDAP_SRC_DIR}/variant_test.cpp
    )

    if(CPPDAP_USE_EXTERNAL_GTEST_PACKAGE)
        find_package(GTest REQUIRED)
    else()
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        add_subdirectory(${CPPDAP_GOOGLETEST_DIR})
        # googletest has -Werror=maybe-uninitialized problems.
        # Disable all warnings in googletest code.
        target_compile_options(gtest PRIVATE -w)
        # gmock has -Werror=deprecated-copy problems.
        target_compile_options(gmock PRIVATE -w)
    endif()

    add_executable(cppdap-unittests ${DAP_TEST_LIST})
    add_test(NAME cppdap-unittests COMMAND cppdap-unittests)

    set_target_properties(cppdap-unittests PROPERTIES
        FOLDER "Tests"
    )

    if(MSVC)
        # googletest emits warning C4244: 'initializing': conversion from 'double' to 'testing::internal::BiggestInt', possible loss of data
        target_compile_options(cppdap-unittests PRIVATE "/wd4244")
    endif()

    cppdap_set_target_options(cppdap-unittests)
    if(CPPDAP_USE_EXTERNAL_GTEST_PACKAGE)
        target_link_libraries(cppdap-unittests PRIVATE cppdap GTest::gtest)
    else()
        target_link_libraries(cppdap-unittests PRIVATE cppdap gtest gmock)
    endif()
endif(CPPDAP_BUILD_TESTS)

# fuzzer
if(CPPDAP_BUILD_FUZZER)
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "CPPDAP_BUILD_FUZZER can currently only be used with the clang toolchain")
    endif()
    set(DAP_FUZZER_LIST
        ${CPPDAP_LIST}
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz.cpp
    )
    add_executable(cppdap-fuzzer ${DAP_FUZZER_LIST})
    if(CPPDAP_ASAN)
        target_compile_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,address")
        target_link_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,address")
    elseif(CPPDAP_MSAN)
        target_compile_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,memory")
        target_link_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,memory")
    elseif(CPPDAP_TSAN)
        target_compile_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,thread")
        target_link_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer,thread")
    else()
        target_compile_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer")
        target_link_options(cppdap-fuzzer PUBLIC "-fsanitize=fuzzer")
    endif()
    target_include_directories(cppdap-fuzzer PUBLIC
        ${CPPDAP_INCLUDE_DIR}
        ${CPPDAP_SRC_DIR}
    )
    cppdap_set_json_links(cppdap-fuzzer)
    target_link_libraries(cppdap-fuzzer PRIVATE cppdap "${CPPDAP_OS_LIBS}")

endif(CPPDAP_BUILD_FUZZER)

# examples
if(CPPDAP_BUILD_EXAMPLES)
    function(build_example target)
        add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/examples/${target}.cpp")
        set_target_properties(${target} PROPERTIES
            FOLDER "Examples"
        )
        cppdap_set_target_options(${target})
        target_link_libraries(${target} PRIVATE cppdap)

        if(CPPDAP_INSTALL_VSCODE_EXAMPLES)
            if(CMAKE_SYSTEM_NAME MATCHES "Windows")
                set(extroot "$ENV{USERPROFILE}\\.vscode\\extensions")
            else()
                set(extroot "$ENV{HOME}/.vscode/extensions")
            endif()
            if(EXISTS ${extroot})
                set(extdir "${extroot}/google.cppdap-example-${target}-1.0.0")
                configure_file(${CMAKE_CURRENT_SOURCE_DIR}/examples/vscode/package.json ${extdir}/package.json)
                add_custom_command(TARGET ${target}
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${extdir})
            else()
                message(WARNING "Could not install vscode example extension as '${extroot}' does not exist")
            endif()
        endif(CPPDAP_INSTALL_VSCODE_EXAMPLES)
    endfunction(build_example)

    build_example(hello_debugger)
    build_example(simple_net_client_server)

endif(CPPDAP_BUILD_EXAMPLES)
