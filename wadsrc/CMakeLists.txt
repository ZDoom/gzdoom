cmake_minimum_required( VERSION 2.4 )

get_target_property(ZIPDIR_EXE zipdir LOCATION)

add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/zdoom.pk3
	COMMAND ${ZIPDIR_EXE} -udf ${ZDOOM_OUTPUT_DIR}/zdoom.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static 
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZDOOM_OUTPUT_DIR}/zdoom.pk3 $<TARGET_FILE_DIR:zdoom>
	DEPENDS zipdir ${CMAKE_CURRENT_SOURCE_DIR}/static )

add_custom_target( pk3 ALL
	DEPENDS ${ZDOOM_OUTPUT_DIR}/zdoom.pk3 )
