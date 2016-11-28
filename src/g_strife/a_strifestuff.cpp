#include "actor.h"
#include "g_level.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "vm.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_keys.h"
#include "a_sharedglobal.h"
#include "templates.h"
#include "d_event.h"
#include "v_font.h"
#include "serializer.h"
#include "p_spec.h"
#include "portal.h"
#include "vm.h"

// Include all the other Strife stuff here to reduce compile time
#include "a_strifeitems.cpp"
#include "a_strifeweapons.cpp"

// Notes so I don't forget them:
//
// When shooting missiles at something, if MF_SHADOW is set, the angle is adjusted with the formula:
//		angle += pr_spawnmissile.Random2() << 21
// When MF_STRIFEx4000000 is set, the angle is adjusted similarly:
//		angle += pr_spawnmissile.Random2() << 22
// Note that these numbers are different from those used by all the other Doom engine games.

