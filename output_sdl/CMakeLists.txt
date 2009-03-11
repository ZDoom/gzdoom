cmake_minimum_required( VERSION 2.4 )
add_library( output_sdl MODULE output_sdl.c )
include_directories( ${FMOD_INCLUDE_DIR} ${SDL_INCLUDE_DIR} )

FILE( WRITE ${CMAKE_CURRENT_BINARY_DIR}/link-make "if [ ! -e ${ZDOOM_OUTPUT_DIR}/liboutput_sdl.so ]; then ln -sf output_sdl/liboutput_sdl.so ${ZDOOM_OUTPUT_DIR}/liboutput_sdl.so; fi" )
add_custom_command( TARGET output_sdl POST_BUILD
	COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/link-make
	COMMAND /bin/sh -c ${CMAKE_CURRENT_BINARY_DIR}/link-make )
