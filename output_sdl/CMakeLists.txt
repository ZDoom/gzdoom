cmake_minimum_required( VERSION 2.8.7 )
if( NOT NO_FMOD AND FMOD_INCLUDE_DIR )
	add_library( output_sdl MODULE output_sdl.c )
	include_directories( ${FMOD_INCLUDE_DIR} ${SDL2_INCLUDE_DIR} )
	target_link_libraries( output_sdl ${SDL2_LIBRARY} )

	FILE( WRITE ${CMAKE_CURRENT_BINARY_DIR}/link-make "if [ ! -e ${ZDOOM_OUTPUT_DIR}/liboutput_sdl.so ]; then ln -sf output_sdl/liboutput_sdl.so ${ZDOOM_OUTPUT_DIR}/liboutput_sdl.so; fi" )
	add_custom_command( TARGET output_sdl POST_BUILD
		COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/link-make
		COMMAND /bin/sh -c ${CMAKE_CURRENT_BINARY_DIR}/link-make )
endif()
