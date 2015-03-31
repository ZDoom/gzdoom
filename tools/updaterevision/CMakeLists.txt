cmake_minimum_required( VERSION 2.4 )

if( WIN32 )
	if( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE OR ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/trustinfo.o
			COMMAND windres -o ${CMAKE_CURRENT_BINARY_DIR}/trustinfo.o -i ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.rc
			DEPENDS trustinfo.rc )
		set( TRUSTINFO trustinfo.o )
	else( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE OR ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		if( MSVC_VERSION GREATER 1399 )
			# VC 8+ adds a manifest automatically to the executable. We need to
			# merge ours with it.
			set( MT_MERGE ON )
		else( MSVC_VERSION GREATER 1399 )
			set( TRUSTINFO trustinfo.rc )
		endif( MSVC_VERSION GREATER 1399 )
	endif( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE OR ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
else( WIN32 )
	set( TRUSTINFO "" )
endif( WIN32 )

if( NOT CMAKE_CROSSCOMPILING )
	add_executable( updaterevision updaterevision.c ${TRUSTINFO} )
	set( CROSS_EXPORTS ${CROSS_EXPORTS} updaterevision PARENT_SCOPE )
endif( NOT CMAKE_CROSSCOMPILING )

if( MT_MERGE )
	add_custom_command(TARGET updaterevision POST_BUILD
		COMMAND mt -inputresource:$<TARGET_FILE:updaterevision> -manifest ${CMAKE_CURRENT_SOURCE_DIR}/trustinfo.txt -outputresource:$<TARGET_FILE:updaterevision> -nologo
		COMMENT "Embedding trustinfo into updaterevision" )
endif( MT_MERGE )
