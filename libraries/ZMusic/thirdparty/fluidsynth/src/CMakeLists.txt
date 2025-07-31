# FluidSynth - A Software Synthesizer
#
# Copyright (C) 2003-2010 Peter Hanappe and others.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307, USA

# CMake based build system. Pedro Lopez-Cabanillas <plcl@users.sf.net>

# ************ library ************

if ( LIBINSTPATCH_SUPPORT )
  set ( fluid_libinstpatch_SOURCES sfloader/fluid_instpatch.c sfloader/fluid_instpatch.h )
endif ( LIBINSTPATCH_SUPPORT )


set ( fluidsynth_SOURCES
	config.h
    utils/fluid_conv.c
    utils/fluid_conv.h
    utils/fluid_hash.c
    utils/fluid_hash.h
    utils/fluid_list.c
    utils/fluid_list.h
    utils/fluid_ringbuffer.c
    utils/fluid_ringbuffer.h
    utils/fluid_settings.c
    utils/fluid_settings.h
    utils/fluidsynth_priv.h
    utils/fluid_sys.c
    utils/fluid_sys.h
    sfloader/fluid_defsfont.c
    sfloader/fluid_defsfont.h
    sfloader/fluid_sfont.h
    sfloader/fluid_sfont.c
    sfloader/fluid_sffile.c
    sfloader/fluid_sffile.h
    sfloader/fluid_samplecache.c
    sfloader/fluid_samplecache.h
    rvoice/fluid_adsr_env.c
    rvoice/fluid_adsr_env.h
    rvoice/fluid_chorus.c
    rvoice/fluid_chorus.h
    rvoice/fluid_iir_filter.c
    rvoice/fluid_iir_filter.h
    rvoice/fluid_lfo.c
    rvoice/fluid_lfo.h
    rvoice/fluid_rvoice.h
    rvoice/fluid_rvoice.c
    rvoice/fluid_rvoice_dsp.c
    rvoice/fluid_rvoice_event.h
    rvoice/fluid_rvoice_event.c
    rvoice/fluid_rvoice_mixer.h
    rvoice/fluid_rvoice_mixer.c
    rvoice/fluid_phase.h
    rvoice/fluid_rev.c
    rvoice/fluid_rev.h
    synth/fluid_chan.c
    synth/fluid_chan.h
    synth/fluid_event.c
    synth/fluid_event.h
    synth/fluid_gen.c
    synth/fluid_gen.h
    synth/fluid_mod.c
    synth/fluid_mod.h
    synth/fluid_synth.c
    synth/fluid_synth.h
    synth/fluid_synth_monopoly.c
    synth/fluid_tuning.c
    synth/fluid_tuning.h
    synth/fluid_voice.c
    synth/fluid_voice.h
    midi/fluid_midi.c
    midi/fluid_midi.h
    midi/fluid_midi_router.c
    midi/fluid_midi_router.h
    midi/fluid_seqbind.c
    midi/fluid_seqbind_notes.cpp
    midi/fluid_seq.c
    midi/fluid_seq_queue.cpp
    drivers/fluid_adriver.c
    drivers/fluid_adriver.h
    drivers/fluid_mdriver.c
    drivers/fluid_mdriver.h
    bindings/fluid_filerenderer.c
    bindings/fluid_ladspa.c
    bindings/fluid_ladspa.h
)

if ( WIN32 )
    set( fluidsynth_SOURCES
        ${fluidsynth_SOURCES}
        utils/win32_glibstubs.c
        utils/win32_glibstubs.h
    )
endif ( WIN32 )

set ( public_HEADERS
    ../include/fluidsynth/audio.h
    ../include/fluidsynth/event.h
    ../include/fluidsynth/gen.h
    ../include/fluidsynth/ladspa.h
    ../include/fluidsynth/log.h
    ../include/fluidsynth/midi.h
    ../include/fluidsynth/misc.h
    ../include/fluidsynth/mod.h
    ../include/fluidsynth/seq.h
    ../include/fluidsynth/seqbind.h
    ../include/fluidsynth/settings.h
    ../include/fluidsynth/sfont.h
    ../include/fluidsynth/shell.h
    ../include/fluidsynth/synth.h
    ../include/fluidsynth/types.h
    ../include/fluidsynth/voice.h
    ../include/fluidsynth/version.h
)

set ( public_main_HEADER
    ../include/fluidsynth.h
)

add_library ( fluidsynth OBJECT
    ${config_SOURCES}
    ${fluidsynth_SOURCES}
    ${public_HEADERS}
    ${public_main_HEADER}
)

target_include_directories ( fluidsynth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/synth
    ${CMAKE_CURRENT_SOURCE_DIR}/rvoice
    ${CMAKE_CURRENT_SOURCE_DIR}/midi
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/sfloader
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/decoder
)


if (NOT WIN32)
   find_package(PkgConfig REQUIRED)
   pkg_search_module(GLIB REQUIRED glib-2.0)
   target_include_directories ( fluidsynth PUBLIC ${GLIB_INCLUDE_DIRS} )
   target_link_libraries ( fluidsynth PUBLIC ${pkgcfg_lib_GLIB_glib-2.0} )

endif()
