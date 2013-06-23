cmake_minimum_required( VERSION 2.4 )

add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/brightmaps.pk3
	COMMAND ${CMAKE_BINARY_DIR}/tools/zipdir/zipdir ${ZDOOM_OUTPUT_DIR}/brightmaps.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static ${CMAKE_CURRENT_BINARY_DIR}/generated
	DEPENDS zipdir ${CMAKE_CURRENT_SOURCE_DIR}/static )

add_custom_target( brightmaps_pk3 ALL
	DEPENDS ${ZDOOM_OUTPUT_DIR}/brightmaps.pk3 )

