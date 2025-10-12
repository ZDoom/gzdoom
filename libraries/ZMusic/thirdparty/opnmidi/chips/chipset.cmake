set(CHIPS_SOURCES
    "src/opl/chips/gens/Ym2612.cpp"
    "src/opl/chips/mamefm/fm.cpp"
    "src/opl/chips/mamefm/resampler.cpp"
    "src/opl/chips/mamefm/ymdeltat.cpp"
    "src/opl/chips/np2/fmgen_file.cpp"
    "src/opl/chips/np2/fmgen_fmgen.cpp"
    "src/opl/chips/np2/fmgen_fmtimer.cpp"
    "src/opl/chips/np2/fmgen_opna.cpp"
    "src/opl/chips/np2/fmgen_psg.cpp"
    "src/opl/chips/gens_opn2.cpp"
    "src/opl/chips/gx_opn2.cpp"
    "src/opl/chips/mame_opn2.cpp"
    "src/opl/chips/mame_opna.cpp"
    "src/opl/chips/np2_opna.cpp"
    "src/opl/chips/nuked_opn2.cpp"
    "src/opl/chips/gx/gx_ym2612.c"
    "src/opl/chips/mame/mame_ym2612fm.c"
    "src/opl/chips/mamefm/emu2149.c"
    "src/opl/chips/nuked/ym3438.c"
    "src/opl/chips/pmdwin_opna.cpp"
    "src/opl/chips/pmdwin/opna.c"
    "src/opl/chips/pmdwin/psg.c"
    "src/opl/chips/pmdwin/rhythmdata.c")

if(COMPILER_SUPPORTS_CXX14) # YMFM can be built in only condition when C++14 and newer were available
  set(YMFM_SOURCES
      "src/opl/chips/ymfm_opn2.cpp"
      "src/opl/chips/ymfm_opna.cpp"
      "src/opl/chips/ymfm/ymfm_opn.cpp"
      "src/opl/chips/ymfm/ymfm_misc.cpp"
      "src/opl/chips/ymfm/ymfm_pcm.cpp"
      "src/opl/chips/ymfm/ymfm_adpcm.cpp"
      "src/opl/chips/ymfm/ymfm_ssg.cpp"
  )
  if(DEFINED FLAG_CPP14)
    set_source_files_properties(${YMFM_SOURCES} COMPILE_FLAGS ${FLAG_CPP14})
  endif()
  list(APPEND CHIPS_SOURCES ${YMFM_SOURCES})
endif()
