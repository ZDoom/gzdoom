// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
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
//		Created by the sound utility written by Dave Taylor.
//		Kept as a sample, DOOM2  sounds. Frozen.
//
//-----------------------------------------------------------------------------

#ifndef __SOUNDS__
#define __SOUNDS__


//
// SoundFX struct.
//
typedef struct sfxinfo_struct	sfxinfo_t;

struct sfxinfo_struct
{
	// up to 6-character name
	char		*name;

	// Sfx singularity (only one at a time)
	int 		singularity;

	// Sfx priority
	int 		priority;

	// referenced sound if a link
	sfxinfo_t*	link;

	// pitch if a link
	int 		pitch;

	// volume if a link
	int 		volume;

	// sound data
	void*		data;

	// this is checked every second to see if sound
	// can be thrown out (if 0, then decrement, if -1,
	// then throw out, if > 0, then it is in use)
	int 		usefulness;

	// lump number of sfx
	int 		lumpnum;				
};




// the complete set of sound effects
extern sfxinfo_t		S_sfx[];

//
// Identifiers for all sfx in game.
//

typedef enum
{
	sfx_None,
	sfx_pistol,
	sfx_shotgn,
	sfx_sgcock,
	sfx_dshtgn,
	sfx_dbopn,
	sfx_dbcls,
	sfx_dbload,
	sfx_plasma,
	sfx_bfg,
	sfx_sawup,
	sfx_sawidl,
	sfx_sawful,
	sfx_sawhit,
	sfx_rlaunc,
	sfx_rxplod,
	sfx_firsht,
	sfx_firxpl,
	sfx_pstart,
	sfx_pstop,
	sfx_doropn,
	sfx_dorcls,
	sfx_stnmov,
	sfx_swtchn,
	sfx_swtchx,
	sfx_plpain,
	sfx_dmpain,
	sfx_popain,
	sfx_vipain,
	sfx_mnpain,
	sfx_pepain,
	sfx_slop,
	sfx_itemup,
	sfx_wpnup,
	sfx_oof,
	sfx_telept,
	sfx_posit1,
	sfx_posit2,
	sfx_posit3,
	sfx_bgsit1,
	sfx_bgsit2,
	sfx_sgtsit,
	sfx_cacsit,
	sfx_brssit,
	sfx_cybsit,
	sfx_spisit,
	sfx_bspsit,
	sfx_kntsit,
	sfx_vilsit,
	sfx_mansit,
	sfx_pesit,
	sfx_sklatk,
	sfx_sgtatk,
	sfx_skepch,
	sfx_vilatk,
	sfx_claw,
	sfx_skeswg,
	sfx_pldeth,
	sfx_pdiehi,
	sfx_podth1,
	sfx_podth2,
	sfx_podth3,
	sfx_bgdth1,
	sfx_bgdth2,
	sfx_sgtdth,
	sfx_cacdth,
	sfx_skldth,
	sfx_brsdth,
	sfx_cybdth,
	sfx_spidth,
	sfx_bspdth,
	sfx_vildth,
	sfx_kntdth,
	sfx_pedth,
	sfx_skedth,
	sfx_posact,
	sfx_bgact,
	sfx_dmact,
	sfx_bspact,
	sfx_bspwlk,
	sfx_vilact,
	sfx_noway,
	sfx_barexp,
	sfx_punch,
	sfx_hoof,
	sfx_metal,
	sfx_chgun,
	sfx_tink,
	sfx_bdopn,
	sfx_bdcls,
	sfx_itmbk,
	sfx_flame,
	sfx_flamst,
	sfx_getpow,
	sfx_bospit,
	sfx_boscub,
	sfx_bossit,
	sfx_bospn,
	sfx_bosdth,
	sfx_manatk,
	sfx_mandth,
	sfx_sssit,
	sfx_ssdth,
	sfx_keenpn,
	sfx_keendt,
	sfx_skeact,
	sfx_skesit,
	sfx_skeatk,
	sfx_radio,
	sfx_secret,
	NUMSFX
} sfxenum_t;

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------

