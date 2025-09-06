#!/usr/bin/cmake -P

# UpdateRevision.cmake
#
# Public domain. This program uses git commands command to get
# various bits of repository status for a particular directory
# and writes it into a header file so that it can be used for a
# project's versioning.

# Boilerplate to return a variable from a function.
macro(ret_var VAR)
	set(${VAR} "${${VAR}}" PARENT_SCOPE)
endmacro()

# Populate variables "Hash", "Tag", and "Timestamp" with relevant information
# from source repository.  If anything goes wrong return something in "Error."
function(query_repo_info)
	# are we git?
	execute_process(
		COMMAND git rev-parse --is-inside-work-tree
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		RESULT_VARIABLE is_git
		OUTPUT_QUIET
	)

	if(DEFINED ENV{GIT_DESCRIBE})
		# from env
		set(Tag "$ENV{GIT_DESCRIBE}")

		if (is_git EQUAL "0")
			message(STATUS "Version tag overridden by GIT_DESCRIBE env var")
		endif()

		# Extract hash from "...-gabcdef"
		string(REGEX MATCH "-g([0-9a-fA-F]+)" match_result "${Tag}")
		if(match_result)
			set(Hash "${CMAKE_MATCH_1}")
		else()
			set(Hash "0000000")
		endif()

		string(TIMESTAMP Timestamp "%Y-%m-%d %H:%M:%S %z")
	elseif(is_git EQUAL "0")
		# from git
		execute_process(
			COMMAND git describe --tags --dirty=-m
			RESULT_VARIABLE Error
			OUTPUT_VARIABLE Tag
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(NOT "${Error}" STREQUAL "0")
			message(STATUS "No git tags found! Set version tag by setting GIT_DESCRIBE env var")
			ret_var(Error)
			return()
		endif()

		execute_process(
			COMMAND git log -1 "--format=%ai;%H"
			RESULT_VARIABLE Error
			OUTPUT_VARIABLE CommitInfo
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(NOT "${Error}" STREQUAL "0")
			ret_var(Error)
			return()
		endif()

		string(REPLACE ";" ";" CommitInfoList "${CommitInfo}")
		list(GET CommitInfoList 0 Timestamp)
		list(GET CommitInfoList 1 Hash)
	else()
		# helpful message
		message(STATUS "Not a git repo! Set version tag by setting GIT_DESCRIBE env var")
		ret_var(Error)
		return()
	endif()

	ret_var(Tag)
	ret_var(Timestamp)
	ret_var(Hash)
endfunction()

# Although configure_file doesn't overwrite the file if the contents are the
# same we can't easily observe that to change the status message.  This
# function parses the existing file (if it exists) and puts the hash in
# variable "OldHash"
function(get_existing_hash File)
	if(EXISTS "${File}")
		file(STRINGS "${File}" OldHash LIMIT_COUNT 1)
		if(OldHash)
			string(SUBSTRING "${OldHash}" 3 -1 OldHash)
			ret_var(OldHash)
		endif()
	endif()
endfunction()

function(main)
	if(NOT CMAKE_ARGC EQUAL 4) # cmake -P UpdateRevision.cmake <OutputFile>
		message("Usage: ${CMAKE_ARGV2} <path to gitinfo.h>")
		return()
	endif()
	set(OutputFile "${CMAKE_ARGV3}")

	get_filename_component(ScriptDir "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)

	query_repo_info()
	if(NOT Hash)
		message("Failed to get commit info: ${Error}")
		set(Hash "0")
		set(Tag "<unknown version>")
		set(Timestamp "")
	endif()

	get_existing_hash("${OutputFile}")
	if(Hash STREQUAL OldHash)
		message("${OutputFile} is up to date at commit ${Tag}.")
		return()
	endif()

	configure_file("${ScriptDir}/gitinfo.h.in" "${OutputFile}")
	message("${OutputFile} updated to commit ${Tag}.")
endfunction()

main()
