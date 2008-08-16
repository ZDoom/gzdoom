cmake_minimum_required( VERSION 2.4 )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/dehsupp.lmp
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
	COMMAND ${CMAKE_BINARY_DIR}/tools/dehsupp/dehsupp ${CMAKE_CURRENT_SOURCE_DIR}/sources/dehsupp.txt ${CMAKE_CURRENT_BINARY_DIR}/generated/dehsupp.lmp
	DEPENDS dehsupp ${CMAKE_CURRENT_SOURCE_DIR}/sources/dehsupp.txt )
	
add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/zdoom.pk3
   COMMAND ${CMAKE_BINARY_DIR}/tools/zipdir/zipdir ${ZDOOM_OUTPUT_DIR}/zdoom.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static ${CMAKE_CURRENT_BINARY_DIR}/generated
   DEPENDS zipdir ${CMAKE_CURRENT_BINARY_DIR}/generated/dehsupp.lmp ${CMAKE_CURRENT_SOURCE_DIR}/static )
	
add_custom_target( pk3 ALL
	DEPENDS ${ZDOOM_OUTPUT_DIR}/zdoom.pk3 )
