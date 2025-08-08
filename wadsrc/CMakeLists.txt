add_pk3(gzdoom.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static)

# insert appstream data into gzdoom.pk3, for release notes
add_custom_target(
	copy_metadata
	COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/../src/posix/freedesktop/org.zdoom.GZDoom.metainfo.xml"
		"${CMAKE_CURRENT_SOURCE_DIR}/static/meta.xml"
)
add_dependencies(gzdoom_pk3 copy_metadata)
add_custom_command(
	TARGET gzdoom_pk3
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove
		"${CMAKE_CURRENT_SOURCE_DIR}/static/meta.xml"
)
