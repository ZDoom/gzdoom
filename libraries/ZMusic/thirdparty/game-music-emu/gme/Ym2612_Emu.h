// YM2612 FM sound chip emulator interface

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#if !defined(VGM_YM2612_GENS) && !defined(VGM_YM2612_NUKED) && !defined(VGM_YM2612_MAME)
#define VGM_YM2612_NUKED
#endif

#ifdef VGM_YM2612_GENS // LGPL v2.1+ license
# if defined(VGM_YM2612_NUKED) || defined(VGM_YM2612_MAME)
#   error Only one of VGM_YM2612_GENS, VGM_YM2612_NUKED or VGM_YM2612_MAME can be defined
# endif
#include "Ym2612_GENS.h"
typedef Ym2612_GENS_Emu Ym2612_Emu;
#endif

#ifdef VGM_YM2612_NUKED // LGPL v2.1+ license
# if defined(VGM_YM2612_GENS) || defined(VGM_YM2612_MAME)
#   error Only one of VGM_YM2612_GENS, VGM_YM2612_NUKED or VGM_YM2612_MAME can be defined
# endif
#include "Ym2612_Nuked.h"
typedef Ym2612_Nuked_Emu Ym2612_Emu;
#endif

#ifdef VGM_YM2612_MAME // GPL v2+ license
# if defined(VGM_YM2612_GENS) || defined(VGM_YM2612_NUKED)
#   error Only one of VGM_YM2612_GENS, VGM_YM2612_NUKED or VGM_YM2612_MAME can be defined
# endif
#include "Ym2612_MAME.h"
typedef Ym2612_MAME_Emu Ym2612_Emu;
#endif

