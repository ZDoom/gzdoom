add_subdirectory(miniz)
# GME is not currently released in a way that's conducive to using as a system
# library. Nevertheless at least one person tried, and so the ability to use a
# system copy exists soley to placate people following distro guidelines to the
# letter without regard as to why the vendored copy is forced.
# [Blzut3] Commented out per request from Graf Zahl.
#option(FORCE_INTERNAL_GME "Use internal gme (it is highly unlikely this should be turned off)" ON)
#mark_as_advanced(FORCE_INTERNAL_GME GME_INCLUDE_DIR GME_LIBRARY)
#find_package(GME QUIET)
#if(GME_FOUND AND NOT FORCE_INTERNAL_GME)
#	message(STATUS "Using system gme library, includes found at ${GME_INCLUDE_DIRS}")
#	set_property(TARGET gme PROPERTY IMPORTED_GLOBAL TRUE)
#	determine_package_config_dependency(ZMUSIC_PACKAGE_DEPENDENCIES TARGET gme MODULE GME)
#else()
#	message(STATUS "Using internal gme library")
	# Use MAME as it's a balanced emulator: well-accurate, but doesn't eats lot of CPU
	# Nuked OPN2 is very accurate emulator, but it eats too much CPU for the workflow
	set(GME_YM2612_EMU "Nuked" CACHE STRING "Which YM2612 emulator to use: \"Nuked\" (LGPLv2.1+), \"MAME\" (GPLv2+), or \"GENS\" (LGPLv2.1+)")
	mark_as_advanced(GME_YM2612_EMU)
	add_subdirectory(game-music-emu)
#endif()

add_subdirectory(dumb)
add_subdirectory(adlmidi)
add_subdirectory(opnmidi)
add_subdirectory(timidity)
add_subdirectory(timidityplus)
add_subdirectory(wildmidi)
add_subdirectory(oplsynth)
add_subdirectory(libxmp)
add_subdirectory(fluidsynth/src)
