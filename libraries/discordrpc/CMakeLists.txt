project (DiscordRPC)

include(GNUInstallDirs)

# format
file(GLOB_RECURSE ALL_SOURCE_FILES
    include/*.h
    src/*.cpp src/*.h src/*.c
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/thirdparty)

# add subdirs

add_subdirectory(src)
