cmake_minimum_required( VERSION 2.4 )

if( WIN32 )
	if( CMAKE_COMPILER_IS_GNUC OR CMAKE_COMPILER_IS_GNUCXX )
		add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/trustinfo.o
			COMMAND windres -o ${CMAKE_CURRENT_BINARY_DIR}/trustinfo.o -i ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.rc
			DEPENDS trustinfo.rc )
		set( TRUSTINFO trustinfo.o )
	else( CMAKE_COMPILER_IS_GNUC OR CMAKE_COMPILER_IS_GNUCXX )
		if( MSVC_VERSION GREATER 1399 )
			# VC 8+ adds a manifest automatically to the executable. We need to
			# merge ours with it.
			set( MT_MERGE ON )
		else( MSVC_VERSION GREATER 1399 )
			set( TRUSTINFO trustinfo.rc )
		endif( MSVC_VERSION GREATER 1399 )
	endif( CMAKE_COMPILER_IS_GNUC OR CMAKE_COMPILER_IS_GNUCXX )
endif( WIN32 )

add_executable( updaterevision updaterevision.c ${TRUSTINFO} )

if( MT_MERGE )
	get_target_property( UPDATEREVISION_EXE updaterevision LOCATION )
	add_custom_command(TARGET updaterevision POST_BUILD
		COMMAND mt -inputresource:${UPDATEREVISION_EXE} -manifest ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.txt -outputresource:${UPDATEREVISION_EXE} -nologo
		COMMENT "Embedding trustinfo into updaterevision" )
endif( MT_MERGE )
