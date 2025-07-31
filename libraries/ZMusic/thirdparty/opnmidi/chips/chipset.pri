SOURCES+= \
    $$PWD/gens/Ym2612.cpp \
    $$PWD/mamefm/fm.cpp \
    $$PWD/mamefm/resampler.cpp \
    $$PWD/mamefm/ymdeltat.cpp \
    $$PWD/np2/fmgen_file.cpp \
    $$PWD/np2/fmgen_fmgen.cpp \
    $$PWD/np2/fmgen_fmtimer.cpp \
    $$PWD/np2/fmgen_opna.cpp \
    $$PWD/np2/fmgen_psg.cpp \
    $$PWD/gens_opn2.cpp \
    $$PWD/gx_opn2.cpp \
    $$PWD/mame_opn2.cpp \
    $$PWD/mame_opna.cpp \
    $$PWD/np2_opna.cpp \
    $$PWD/nuked_opn2.cpp \
    $$PWD/pmdwin_opna.cpp \
    $$PWD/gx/gx_ym2612.c \
    $$PWD/mame/mame_ym2612fm.c \
    $$PWD/mamefm/emu2149.c \
    $$PWD/nuked/ym3438.c \
    $$PWD/pmdwin/opna.c \
    $$PWD/pmdwin/psg.c \
    $$PWD/pmdwin/rhythmdata.c

HEADERS+= \
    $$PWD/gens/Ym2612.hpp \
    $$PWD/gens/Ym2612_p.hpp \
    $$PWD/gx/gx_ym2612.h \
    $$PWD/mame/mame_ym2612fm.h \
    $$PWD/mame/mamedef.h \
    $$PWD/mamefm/2608intf.h \
    $$PWD/mamefm/emu.h \
    $$PWD/mamefm/emu2149.h \
    $$PWD/mamefm/emutypes.h \
    $$PWD/mamefm/fm.h \
    $$PWD/mamefm/fmopn_2608rom.h \
    $$PWD/mamefm/resampler.hpp \
    $$PWD/mamefm/ymdeltat.h \
    $$PWD/np2/compiler.h \
    $$PWD/np2/fmgen_diag.h \
    $$PWD/np2/fmgen_file.h \
    $$PWD/np2/fmgen_fmgen.h \
    $$PWD/np2/fmgen_fmgeninl.h \
    $$PWD/np2/fmgen_fmtimer.h \
    $$PWD/np2/fmgen_headers.h \
    $$PWD/np2/fmgen_misc.h \
    $$PWD/np2/fmgen_opna.h \
    $$PWD/np2/fmgen_psg.h \
    $$PWD/np2/fmgen_types.h \
    $$PWD/nuked/ym3438.h \
    $$PWD/pmdwin/op.h \
    $$PWD/pmdwin/opna.h \
    $$PWD/pmdwin/psg.h \
    $$PWD/pmdwin/rhythmdata.h \
    $$PWD/gens_opn2.h \
    $$PWD/gx_opn2.h \
    $$PWD/mame_opn2.h \
    $$PWD/mame_opna.h \
    $$PWD/np2_opna.h \
    $$PWD/nuked_opn2.h \
    $$PWD/pmdwin_opna.h \
    $$PWD/opn_chip_base.h \
    $$PWD/opn_chip_base.tcc \
    $$PWD/opn_chip_family.h

# Available when C++14 is supported
enable_ymfm: {
DEFINES += ENABLE_YMFM_EMULATOR

SOURCES+= \
    $$PWD/ymfm_opn2.cpp \
    $$PWD/ymfm_opna.cpp \
    $$PWD/ymfm/ymfm_adpcm.cpp \
    $$PWD/ymfm/ymfm_misc.cpp \
    $$PWD/ymfm/ymfm_opn.cpp \
    $$PWD/ymfm/ymfm_pcm.cpp \
    $$PWD/ymfm/ymfm_ssg.cpp

HEADERS+= \
    $$PWD/ymfm_opn2.h \
    $$PWD/ymfm_opna.h \
    $$PWD/ymfm/ymfm.h \
    $$PWD/ymfm/ymfm_adpcm.h \
    $$PWD/ymfm/ymfm_fm.h \
    $$PWD/ymfm/ymfm_fm.ipp \
    $$PWD/ymfm/ymfm_misc.h \
    $$PWD/ymfm/ymfm_opn.h \
    $$PWD/ymfm/ymfm_pcm.h \
    $$PWD/ymfm/ymfm_ssg.h
}
