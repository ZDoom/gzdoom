// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: m_random.h,v 1.9 1998/05/01 14:20:31 killough Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//
//		[RH] We now use BOOM's random number generator
//
//-----------------------------------------------------------------------------


#ifndef __M_RANDOM__
#define __M_RANDOM__

#include "doomtype.h"

// killough 1/19/98: rewritten to use to use a better random number generator
// in the new engine, although the old one is available for compatibility.

// killough 2/16/98:
//
// Make every random number generator local to each control-equivalent block.
// Critical for demo sync. Changing the order of this list breaks all previous
// versions' demos. The random number generators are made local to reduce the
// chances of sync problems. In Doom, if a single random number generator call
// was off, it would mess up all random number generators. This reduces the
// chances of it happening by making each RNG local to a control flow block.
//
// Notes to developers: if you want to reduce your demo sync hassles, follow
// this rule: for each call to P_Random you add, add a new class to the enum
// type below for each block of code which calls P_Random. If two calls to
// P_Random are not in "control-equivalent blocks", i.e. there are any cases
// where one is executed, and the other is not, put them in separate classes.
//
// Keep all current entries in this list the same, and in the order
// indicated by the #'s, because they're critical for preserving demo
// sync. Do not remove entries simply because they become unused later.

typedef enum {
	pr_misc,					// 0
	pr_all_in_one,				// 1
	pr_dmspawn,					// 2
	pr_checkmissilerange,		// 3
	pr_trywalk,					// 4
	pr_newchasedir,				// 5
	pr_look,					// 6
	pr_chase,					// 7
	pr_facetarget,				// 8
	pr_posattack,				// 9
	pr_sposattack,				// 10
	pr_cposattack,				// 11
	pr_cposrefire,				// 12
	pr_spidrefire,				// 13
	pr_troopattack,				// 14
	pr_sargattack,				// 15
	pr_headattack,				// 16
	pr_bruisattack,				// 17
	pr_tracer,					// 18
	pr_skelfist,				// 19
	pr_scream,					// 20
	pr_brainscream,				// 21
	pr_brainexplode,			// 22
	pr_spawnfly,				// 23
	pr_killmobj,				// 24
	pr_damagemobj,				// 25
	pr_checkthing,				// 26
	pr_changesector,			// 27
	pr_explodemissile,			// 28
	pr_mobjthinker,				// 29
	pr_spawnmobj,				// 30
	pr_spawnmapthing,			// 31
	pr_spawnpuff,				// 32
	pr_spawnblood,				// 33
	pr_checkmissilespawn,		// 34
	pr_spawnmissile,			// 35
	pr_punch,					// 36
	pr_saw,						// 37
	pr_fireplasma,				// 38
	pr_gunshot,					// 39
	pr_fireshotgun2,			// 40
	pr_bfgspray,				// 41
	pr_checksight,				// 42
	pr_playerinspecialsector,	// 43
	pr_fireflicker,				// 44
	pr_lightflash,				// 45
	pr_spawnlightflash,			// 46
	pr_spawnstrobeflash,		// 47
	pr_doplat,					// 48
	pr_throwgib,				// 49
	pr_vel4dmg,					// 50
	pr_gengib,					// 51
	pr_acs,						// 52
	pr_animatepictures,			// 53
	pr_obituary,				// 54
	pr_quake,					// 55
	pr_playerscream,			// 56
	pr_playerpain,				// 57
	pr_bounce,					// 58
	pr_opendoor,				// 59
	pr_botmove,					// 60
	pr_botdofire,				// 61
	pr_botspawn,				// 62
	pr_botrespawn,				// 63
	pr_bottrywalk,				// 64
	pr_botnewchasedir,			// 65
	pr_botspawnmobj,			// 66
	pr_botopendoor,				// 67
	pr_botchecksight,			// 68
	pr_bobbing,					// 69
	pr_wpnreadysnd,				// 70
	pr_spark,					// 71
	pr_torch,					// 72
	pr_ssdelay,					// 73
	pr_afx,						// 74
	pr_chainwiggle,				// 75
	pr_switchanim,				// 76
	pr_scaredycat,				// 77
	pr_randsound,				// 78
	pr_decal,					// 79
	pr_lightning,				// 80
	pr_decalchoice,				// 81
	// Start new entries -- add new entries below

	// End of new entries
	NUMPRCLASS							// MUST be last item in list
} pr_class_t;

// The random number generator's state.
typedef struct {
	DWORD seed[NUMPRCLASS];		// Each block's random seed
	int rndindex, prndindex;			// For compatibility support
} rng_t;

extern rng_t rng;						// The rng's state

extern DWORD rngseed;			// The starting seed (not part of state)

// Returns a number from 0 to 255,
#define M_Random() P_Random(pr_misc)

// As M_Random, but used by the play simulation.
int P_Random(pr_class_t = pr_all_in_one);

inline int PS_Random(pr_class_t cls = pr_all_in_one)
{
	int t = P_Random (cls);
	return t - P_Random (cls);
}

// Fix randoms for demos.
void M_ClearRandom(void);

#endif
