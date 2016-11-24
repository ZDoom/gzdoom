cmake_minimum_required( VERSION 2.8.7 )

if( WIN32 )
	if( MSVC_VERSION GREATER 1399 )
		# VC 8+ adds a manifest automatically to the executable. We need to
		# merge ours with it.
		set( MT_MERGE ON )
	else()
		set( TRUSTINFO trustinfo.rc )
	endif()
else( WIN32 )
	set( TRUSTINFO "" )
endif()

if( NOT CMAKE_CROSSCOMPILING )
	add_executable( updaterevision updaterevision.c ${TRUSTINFO} )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} updaterevision PARENT_SCOPE )
endif()

if( MT_MERGE )
	add_custom_command(TARGET updaterevision POST_BUILD
		COMMAND mt -inputresource:$<TARGET_FILE:updaterevision> -manifest ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.txt -outputresource:$<TARGET_FILE:updaterevision> -nologo
		COMMENT "Embedding trustinfo into updaterevision" )
endif()
