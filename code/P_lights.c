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
//		Handle Sector base lighting effects.
//
//-----------------------------------------------------------------------------



#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "p_lnspec.h"

// State.
#include "r_state.h"

// [RH] These functions only change the sector special in
//		demo compatibility mode.
extern BOOL olddemo;

// [RH] Make sure the light level is in bounds.
#define CLIPLIGHT(l)	(((l) < 0) ? 0 : (((l) > 255) ? 255 : (l)))

//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker (fireflicker_t *flick)
{
	int amount;
		
	if (--flick->count)
		return;
		
	amount = (P_Random (pr_fireflicker) & 3) << 4;
	
	if (flick->sector->lightlevel - amount < flick->minlight)
		flick->sector->lightlevel = (short)flick->minlight;
	else
		flick->sector->lightlevel = (short)(flick->maxlight - amount);

	flick->count = 4;
}



//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker (sector_t *sector)
{
	fireflicker_t *flick;

	if (olddemo)
		sector->special &= 0xff00;

	flick = Z_Malloc ( sizeof(*flick), PU_LEVSPEC, 0);

	P_AddThinker (&flick->thinker);

	flick->thinker.function.acp1 = (actionf_p1) T_FireFlicker;
	flick->sector = sector;
	flick->maxlight = sector->lightlevel;
	flick->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel)+16;
	flick->count = 4;
}



//
// BROKEN LIGHT FLASHING
//


//
// T_LightFlash
// Do flashing lights.
//
void T_LightFlash (lightflash_t* flash)
{
	if (--flash->count)
		return;
		
	if (flash->sector->lightlevel == flash->maxlight)
	{
		flash-> sector->lightlevel = flash->minlight;
		flash->count = (P_Random (pr_lightflash) & flash->mintime) + 1;
	}
	else
	{
		flash-> sector->lightlevel = flash->maxlight;
		flash->count = (P_Random (pr_lightflash) & flash->maxtime) + 1;
	}

}




//
// P_SpawnLightFlash
// [RH] Added min and max parameters
//
void P_SpawnLightFlash (sector_t *sector, int min, int max)
{
	lightflash_t *flash;

	if (olddemo)
		sector->special &= 0xff00;
		
	flash = Z_Malloc ( sizeof(*flash), PU_LEVSPEC, 0);

	P_AddThinker (&flash->thinker);

	flash->thinker.function.acp1 = (actionf_p1) T_LightFlash;
	flash->sector = sector;
	if (min < 0) {
		// If min is negative, find light levels like Doom.
		flash->maxlight = sector->lightlevel;
		flash->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel);
	} else {
		// Otherwise, use the ones provided.
		flash->maxlight = CLIPLIGHT(max);
		flash->minlight = CLIPLIGHT(min);
	}
	flash->maxtime = 64;
	flash->mintime = 7;
	flash->count = (P_Random (pr_spawnlightflash) & flash->maxtime) + 1;
}

// [RH] New function to start light flashing on-demand.
void EV_StartLightFlashing (int tag, int upper, int lower)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (P_SectorActive (lighting_special, sec))
			continue;
		
		P_SpawnLightFlash (sec, lower, upper);
	}
}


//
// STROBE LIGHT FLASHING
//


//
// T_StrobeFlash
//
void T_StrobeFlash (strobe_t *flash)
{
	if (--flash->count)
		return;
		
	if (flash->sector->lightlevel == flash->minlight)
	{
		flash-> sector->lightlevel = (short)flash->maxlight;
		flash->count = flash->brighttime;
	}
	else
	{
		flash-> sector->lightlevel = (short)flash->minlight;
		flash->count = flash->darktime;
	}

}



//
// P_SpawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
// [RH] brighttime is also adjustable
//
void P_SpawnStrobeFlash (sector_t *sector, int upper, int lower,
						 int utics, int ltics, int inSync)
{
	strobe_t *flash;
		
	flash = Z_Malloc ( sizeof(*flash), PU_LEVSPEC, 0);

	P_AddThinker (&flash->thinker);

	flash->sector = sector;
	flash->darktime = ltics;
	flash->brighttime = utics;
	flash->thinker.function.acp1 = (actionf_p1) T_StrobeFlash;

	// [RH] If the upper light level is -1, we use Doom's mechanism
	//		for setting this up. Otherwise, we use Hexen's.
	if (upper < 0) {
		flash->maxlight = sector->lightlevel;
		flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);

		if (flash->minlight == flash->maxlight)
			flash->minlight = 0;

		flash->count = inSync ? 1 : (P_Random (pr_spawnstrobeflash) & 7) + 1;
	} else {
		flash->maxlight = CLIPLIGHT(upper);
		flash->minlight = CLIPLIGHT(lower);
		flash->count = 1;	// Hexen-style is always in sync.
	}
	if (olddemo)
		sector->special &= 0xff00;
}


//
// Start strobing lights (usually from a trigger)
// [RH] Made it more configurable.
//
void EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (P_SectorActive (lighting_special, sec))	//jff 2/22/98
			continue;
		
		P_SpawnStrobeFlash (sec, upper, lower, utics, ltics, 0);
	}
}



//
// TURN LINE'S TAG LIGHTS OFF
// [RH] Takes a tag instead of a line
//
void EV_TurnTagLightsOff (int tag)
{
	int i;
	int secnum;

	// [RH] Don't do a linear search
	for (secnum = -1; (secnum = P_FindSectorFromTag (tag, secnum)) >= 0; ) 
	{
		sector_t *sector = sectors + secnum;
		int min = sector->lightlevel;

		for (i = 0; i < sector->linecount; i++)
		{
			sector_t *tsec = getNextSector (sector->lines[i],sector);
			if (!tsec)
				continue;
			if (tsec->lightlevel < min)
				min = tsec->lightlevel;
		}
		sector->lightlevel = min;
	}
}


//
// TURN LINE'S TAG LIGHTS ON
// [RH] Takes a tag instead of a line
//
void EV_LightTurnOn (int tag, int bright)
{
	int secnum = -1;

	// [RH] Don't do a linear search
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) 
	{
		sector_t *sector = sectors + secnum;

		// bright = -1 means to search ([RH] Not 0)
		// for highest light level
		// surrounding sector
		if (bright < 0)
		{
			int j;

			bright = 0;
			for (j = 0; j < sector->linecount; j++)
			{
				sector_t *temp = getNextSector (sector->lines[j], sector);

				if (!temp)
					continue;

				if (temp->lightlevel > bright)
					bright = temp->lightlevel;
			}
		}
		sector->lightlevel = CLIPLIGHT(bright);
	}
}


//
// [RH] New function to adjust tagged sectors' light levels
//		by a relative amount. Light levels are clipped to
//		within the range 0-255 inclusive.
//
void EV_LightChange (int tag, int value)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) {
		int newlight = sectors[secnum].lightlevel + value;
		sectors[secnum].lightlevel = CLIPLIGHT(newlight);
	}
}

	
//
// Spawn glowing light
//

void T_Glow (glow_t *g)
{
	switch(g->direction)
	{
	  case -1:
		// DOWN
		g->sector->lightlevel -= GLOWSPEED;
		if (g->sector->lightlevel <= g->minlight)
		{
			g->sector->lightlevel += GLOWSPEED;
			g->direction = 1;
		}
		break;
		
	  case 1:
		// UP
		g->sector->lightlevel += GLOWSPEED;
		if (g->sector->lightlevel >= g->maxlight)
		{
			g->sector->lightlevel -= GLOWSPEED;
			g->direction = -1;
		}
		break;
	}
}


void P_SpawnGlowingLight (sector_t *sector)
{
	glow_t *g;
		
	g = Z_Malloc( sizeof(*g), PU_LEVSPEC, 0);

	P_AddThinker(&g->thinker);

	g->sector = sector;
	g->minlight = P_FindMinSurroundingLight(sector,sector->lightlevel);
	g->maxlight = sector->lightlevel;
	g->thinker.function.acp1 = (actionf_p1) T_Glow;
	g->direction = -1;

	if (olddemo)
		sector->special &= 0xff00;
}

//
// [RH] More glowing light, this time appropriate for Hexen-ish uses.
//
void T_Glow2 (glow2_t *g)
{
	if (g->tics++ >= g->maxtics) {
		if (g->oneshot) {
			g->sector->lightlevel = g->end;
			P_RemoveThinker (&g->thinker);
			return;
		} else {
			int temp = g->start;
			g->start = g->end;
			g->end = temp;
			g->tics -= g->maxtics;
		}
	}

	g->sector->lightlevel = ((g->end - g->start) * g->tics) / g->maxtics + g->start;
}


static void P_SpawnGlowingLight2 (sector_t *sector, int start, int end,
								  int tics, BOOL oneshot)
{
	glow2_t *g;
		
	g = Z_Malloc( sizeof(*g), PU_LEVSPEC, 0);

	P_AddThinker(&g->thinker);

	g->sector = sector;
	g->start = CLIPLIGHT(start);
	g->end = CLIPLIGHT(end);
	g->maxtics = tics;
	g->tics = -1;
	g->oneshot = oneshot;
	g->thinker.function.acp1 = (actionf_p1) T_Glow2;
}

void EV_StartLightGlowing (int tag, int upper, int lower, int tics)
{
	int secnum;

	if (upper < lower) {
		int temp = upper;
		upper = lower;
		lower = temp;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (P_SectorActive (lighting_special, sec))
			continue;
		
		P_SpawnGlowingLight2 (sec, upper, lower, tics, false);
	}
}

void EV_StartLightFading (int tag, int value, int tics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (P_SectorActive (lighting_special, sec))
			continue;

		// No need to fade if lightlevel is already at desired value.
		if (sec->lightlevel == value)
			continue;

		P_SpawnGlowingLight2 (sec, sec->lightlevel, value, tics, true);
	}
}


// [RH] Phased lighting ala Hexen
void T_PhasedLight (phased_t *l)
{
	const int steps = 12;

	if (l->phase < steps) {
		l->sector->lightlevel = ((255 - l->baselevel) * l->phase) / steps + l->baselevel;
	} else if (l->phase < 2*steps) {
		l->sector->lightlevel = ((255 - l->baselevel) * (2*steps - l->phase - 1) / steps
								+ l->baselevel);
	} else {
		l->sector->lightlevel = l->baselevel;
	}

	if (l->phase-- == 0)
		l->phase = 63;
}

static int phasehelper (sector_t *sector, int index, int light, sector_t *prev)
{
	if (!sector) {
		return index;
	} else {
		phased_t *l = Z_Malloc (sizeof(*l), PU_LEVSPEC, 0);
		int numsteps;

		l->sector = sector;
		l->baselevel = sector->lightlevel ? sector->lightlevel : light;
		numsteps = phasehelper (P_NextSpecialSector (sector,
				(sector->special & 0x00ff) == LightSequenceSpecial1 ?
					LightSequenceSpecial2 : LightSequenceSpecial1, prev),
				index + 1, l->baselevel, sector);
		l->phase = ((numsteps - index - 1) * 64) / numsteps;
		l->thinker.function.acp1 = (actionf_p1) T_PhasedLight;

		P_AddThinker (&l->thinker);

		sector->special &= 0xff00;

		return numsteps;
	}
}

void P_SpawnLightSequence (sector_t *sector)
{
	phasehelper (sector, 0, 0, NULL);
}

void P_SpawnLightPhased (sector_t *sector)
{
	phased_t *l = Z_Malloc (sizeof(*l), PU_LEVSPEC, 0);

	l->sector = sector;
	l->baselevel = 48;
	l->phase = 63 - (sector->lightlevel & 63);
	l->thinker.function.acp1 = (actionf_p1) T_PhasedLight;
	P_AddThinker (&l->thinker);

	sector->special &= 0xff00;
}