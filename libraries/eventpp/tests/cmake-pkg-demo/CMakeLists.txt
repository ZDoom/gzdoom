project(eventpppkgdemo)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 11)

set(TARGET pkgdemo)

set(SRC
	main.cpp
)

add_executable(
	${TARGET}
	${SRC}
)

find_package(eventpp CONFIG REQUIRED)
target_link_libraries(${TARGET} PRIVATE eventpp::eventpp)

# need this because https://github.com/Microsoft/vcpkg/issues/798
find_path(EVENTPP_INCLUDE_DIR eventpp/eventqueue.h)
include_directories(${EVENTPP_INCLUDE_DIR})
