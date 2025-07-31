cmake_minimum_required(VERSION 2.8.7...3.19)
project(list_midi_devices)

find_package(ZMusic REQUIRED)

add_executable(list_midi_devices list_midi_devices.cpp)
target_link_libraries(list_midi_devices PRIVATE ZMusic::zmusic)
