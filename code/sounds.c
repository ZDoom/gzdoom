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
// $Log:$
//
// DESCRIPTION:
//		Created by a sound utility.
//		Kept as a sample, DOOM2 sounds.
//
//-----------------------------------------------------------------------------


static const char 
rcsid[] = "$Id: sounds.c,v 1.3 1997/01/29 22:40:44 b1 Exp $";


#include "doomtype.h"
#include "sounds.h"

//
// Information about all the music
//

musicinfo_t S_music[] =
{
	{ "" },
	{ "d_e1m1" },
	{ "d_e1m2" },
	{ "d_e1m3" },
	{ "d_e1m4" },
	{ "d_e1m5" },
	{ "d_e1m6" },
	{ "d_e1m7" },
	{ "d_e1m8" },
	{ "d_e1m9" },
	{ "d_e2m1" },
	{ "d_e2m2" },
	{ "d_e2m3" },
	{ "d_e2m4" },
	{ "d_e2m5" },
	{ "d_e2m6" },
	{ "d_e2m7" },
	{ "d_e2m8" },
	{ "d_e2m9" },
	{ "d_e3m1" },
	{ "d_e3m2" },
	{ "d_e3m3" },
	{ "d_e3m4" },
	{ "d_e3m5" },
	{ "d_e3m6" },
	{ "d_e3m7" },
	{ "d_e3m8" },
	{ "d_e3m9" },
	{ "d_inter" },
	{ "d_intro" },
	{ "d_bunny" },
	{ "d_victor" },
	{ "d_introa" },
	{ "d_runnin" },
	{ "d_stalks" },
	{ "d_countd" },
	{ "d_betwee" },
	{ "d_doom" },
	{ "d_the_da" },
	{ "d_shawn" },
	{ "d_ddtblu" },
	{ "d_in_cit" },
	{ "d_dead" },
	{ "d_stlks2" },
	{ "d_theda2" },
	{ "d_doom2" },
	{ "d_ddtbl2" },
	{ "d_runni2" },
	{ "d_dead2" },
	{ "d_stlks3" },
	{ "d_romero" },
	{ "d_shawn2" },
	{ "d_messag" },
	{ "d_count2" },
	{ "d_ddtbl3" },
	{ "d_ampie" },
	{ "d_theda3" },
	{ "d_adrian" },
	{ "d_messg2" },
	{ "d_romer2" },
	{ "d_tense" },
	{ "d_shawn3" },
	{ "d_openin" },
	{ "d_evil" },
	{ "d_ultima" },
	{ "d_read_m" },
	{ "d_dm2ttl" },
	{ "d_dm2int" } 
};


//
// Information about all the sfx
//

sfxinfo_t S_sfx[] =
{
  // S_sfx[0] needs to be a dummy for odd reasons.
  { "none", false,	0, 0, -1, -1, 0 },

  { "pistol", false, 64, 0, -1, -1, 0 },
  { "shotgn", false, 64, 0, -1, -1, 0 },
  { "sgcock", false, 64, 0, -1, -1, 0 },
  { "dshtgn", false, 64, 0, -1, -1, 0 },
  { "dbopn", false, 64, 0, -1, -1, 0 },
  { "dbcls", false, 64, 0, -1, -1, 0 },
  { "dbload", false, 64, 0, -1, -1, 0 },
  { "plasma", false, 64, 0, -1, -1, 0 },
  { "bfg", false, 64, 0, -1, -1, 0 },
  { "sawup", false, 64, 0, -1, -1, 0 },
  { "sawidl", false, 118, 0, -1, -1, 0 },
  { "sawful", false, 64, 0, -1, -1, 0 },
  { "sawhit", false, 64, 0, -1, -1, 0 },
  { "rlaunc", false, 64, 0, -1, -1, 0 },
  { "rxplod", false, 70, 0, -1, -1, 0 },
  { "firsht", false, 70, 0, -1, -1, 0 },
  { "firxpl", false, 70, 0, -1, -1, 0 },
  { "pstart", false, 100, 0, -1, -1, 0 },
  { "pstop", false, 100, 0, -1, -1, 0 },
  { "doropn", false, 100, 0, -1, -1, 0 },
  { "dorcls", false, 100, 0, -1, -1, 0 },
  { "stnmov", false, 119, 0, -1, -1, 0 },
  { "swtchn", false, 78, 0, -1, -1, 0 },
  { "swtchx", false, 78, 0, -1, -1, 0 },
  { "plpain", false, 96, 0, -1, -1, 0 },
  { "dmpain", false, 96, 0, -1, -1, 0 },
  { "popain", false, 96, 0, -1, -1, 0 },
  { "vipain", false, 96, 0, -1, -1, 0 },
  { "mnpain", false, 96, 0, -1, -1, 0 },
  { "pepain", false, 96, 0, -1, -1, 0 },
  { "slop", false, 78, 0, -1, -1, 0 },
  { "itemup", true, 78, 0, -1, -1, 0 },
  { "wpnup", true, 78, 0, -1, -1, 0 },
  { "oof", false, 96, 0, -1, -1, 0 },
  { "telept", false, 32, 0, -1, -1, 0 },
  { "posit1", true, 98, 0, -1, -1, 0 },
  { "posit2", true, 98, 0, -1, -1, 0 },
  { "posit3", true, 98, 0, -1, -1, 0 },
  { "bgsit1", true, 98, 0, -1, -1, 0 },
  { "bgsit2", true, 98, 0, -1, -1, 0 },
  { "sgtsit", true, 98, 0, -1, -1, 0 },
  { "cacsit", true, 98, 0, -1, -1, 0 },
  { "brssit", true, 94, 0, -1, -1, 0 },
  { "cybsit", true, 92, 0, -1, -1, 0 },
  { "spisit", true, 90, 0, -1, -1, 0 },
  { "bspsit", true, 90, 0, -1, -1, 0 },
  { "kntsit", true, 90, 0, -1, -1, 0 },
  { "vilsit", true, 90, 0, -1, -1, 0 },
  { "mansit", true, 90, 0, -1, -1, 0 },
  { "pesit", true, 90, 0, -1, -1, 0 },
  { "sklatk", false, 70, 0, -1, -1, 0 },
  { "sgtatk", false, 70, 0, -1, -1, 0 },
  { "skepch", false, 70, 0, -1, -1, 0 },
  { "vilatk", false, 70, 0, -1, -1, 0 },
  { "claw", false, 70, 0, -1, -1, 0 },
  { "skeswg", false, 70, 0, -1, -1, 0 },
  { "pldeth", false, 32, 0, -1, -1, 0 },
  { "pdiehi", false, 32, 0, -1, -1, 0 },
  { "podth1", false, 70, 0, -1, -1, 0 },
  { "podth2", false, 70, 0, -1, -1, 0 },
  { "podth3", false, 70, 0, -1, -1, 0 },
  { "bgdth1", false, 70, 0, -1, -1, 0 },
  { "bgdth2", false, 70, 0, -1, -1, 0 },
  { "sgtdth", false, 70, 0, -1, -1, 0 },
  { "cacdth", false, 70, 0, -1, -1, 0 },
  { "skldth", false, 70, 0, -1, -1, 0 },
  { "brsdth", false, 32, 0, -1, -1, 0 },
  { "cybdth", false, 32, 0, -1, -1, 0 },
  { "spidth", false, 32, 0, -1, -1, 0 },
  { "bspdth", false, 32, 0, -1, -1, 0 },
  { "vildth", false, 32, 0, -1, -1, 0 },
  { "kntdth", false, 32, 0, -1, -1, 0 },
  { "pedth", false, 32, 0, -1, -1, 0 },
  { "skedth", false, 32, 0, -1, -1, 0 },
  { "posact", true, 120, 0, -1, -1, 0 },
  { "bgact", true, 120, 0, -1, -1, 0 },
  { "dmact", true, 120, 0, -1, -1, 0 },
  { "bspact", true, 100, 0, -1, -1, 0 },
  { "bspwlk", true, 100, 0, -1, -1, 0 },
  { "vilact", true, 100, 0, -1, -1, 0 },
  { "noway", false, 78, 0, -1, -1, 0 },
  { "barexp", false, 60, 0, -1, -1, 0 },
  { "punch", false, 64, 0, -1, -1, 0 },
  { "hoof", false, 70, 0, -1, -1, 0 },
  { "metal", false, 70, 0, -1, -1, 0 },
  { "chgun", false, 64, &S_sfx[sfx_pistol], 150, 0, 0 },
  { "tink", false, 60, 0, -1, -1, 0 },
  { "bdopn", false, 100, 0, -1, -1, 0 },
  { "bdcls", false, 100, 0, -1, -1, 0 },
  { "itmbk", false, 100, 0, -1, -1, 0 },
  { "flame", false, 32, 0, -1, -1, 0 },
  { "flamst", false, 32, 0, -1, -1, 0 },
  { "getpow", false, 60, 0, -1, -1, 0 },
  { "bospit", false, 70, 0, -1, -1, 0 },
  { "boscub", false, 70, 0, -1, -1, 0 },
  { "bossit", false, 70, 0, -1, -1, 0 },
  { "bospn", false, 70, 0, -1, -1, 0 },
  { "bosdth", false, 70, 0, -1, -1, 0 },
  { "manatk", false, 70, 0, -1, -1, 0 },
  { "mandth", false, 70, 0, -1, -1, 0 },
  { "sssit", false, 70, 0, -1, -1, 0 },
  { "ssdth", false, 70, 0, -1, -1, 0 },
  { "keenpn", false, 70, 0, -1, -1, 0 },
  { "keendt", false, 70, 0, -1, -1, 0 },
  { "skeact", false, 70, 0, -1, -1, 0 },
  { "skesit", false, 70, 0, -1, -1, 0 },
  { "skeatk", false, 70, 0, -1, -1, 0 },
  { "radio", false, 60, 0, -1, -1, 0 },
  { "secret", false, 70, 0, -1, -1, 0 }
};

