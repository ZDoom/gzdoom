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
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//
//-----------------------------------------------------------------------------


#include "m_alloc.h"

#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"

#include "i_system.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_random.h"
#include "w_wad.h"

#include "r_local.h"
#include "p_local.h"

#include "g_game.h"

#include "s_sound.h"

// State.
#include "r_state.h"

// Data.
#include "sounds.h"

#include "c_consol.h"

// [RH] Needed for sky scrolling
#include "r_sky.h"

//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//
typedef struct
{
	BOOL 	istexture;
	int 	picnum;
	int 	basepic;
	int 	numpics;
	int 	speed;
} anim_t;

//
// source animation definition
//
// [RH] Note that in BOOM's ANIMATED lump, this is an array packed to
//		byte boundaries. Total size: 23 bytes per entry.
//
typedef struct
{
	byte 	istexture;		// if false, it is a flat
	char	endname[9];
	char	startname[9];
	int 	speed;
} animdef_t;



static anim_t*  lastanim;
static anim_t*  anims;

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnScrollers(void);

//
//		Animating line specials
//
//#define MAXLINEANIMS			64

//extern	short	numlinespecials;
//extern	line_t* linespeciallist[MAXLINEANIMS];


//
// P_InitPicAnims
//
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
//		no assumptions about how the compiler packs the animdefs array.
//
void P_InitPicAnims (void)
{
	byte *animdefs = W_CacheLumpName ("ANIMATED", PU_STATIC);
	byte *anim_p;

	// Init animation

	// [RH] Figure out maximum size of anims array
	{
		int i;

		for (i = 0, anim_p = animdefs; *anim_p != 255; anim_p += 23, i++)
			;

		if (i == 0) {
			// No animdefs
			anims = lastanim = NULL;
			Z_Free (animdefs);
			return;
		}

		lastanim = anims = Z_Malloc (i*sizeof(*anims), PU_STATIC, 0);
	}

	for (anim_p = animdefs; *anim_p != 255; anim_p += 23)
	{
		if (*anim_p /* .istexture */)
		{
			// different episode ?
			if (R_CheckTextureNumForName (anim_p + 10 /* .startname */) == -1)
				continue;		

			lastanim->picnum = R_TextureNumForName (anim_p + 1 /* .endname */);
			lastanim->basepic = R_TextureNumForName (anim_p + 10 /* .startname */);
		}
		else
		{
			if ((W_CheckNumForName)(anim_p + 10 /* .startname */, ns_flats) == -1)
				continue;

			lastanim->picnum = R_FlatNumForName (anim_p + 1 /* .endname */);
			lastanim->basepic = R_FlatNumForName (anim_p + 10 /* .startname */);
		}

		lastanim->istexture = *anim_p /* .istexture */;
		lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

		if (lastanim->numpics < 2)
			I_Error ("P_InitPicAnims: bad cycle from %s to %s",
					 anim_p + 10 /* .startname */,
					 anim_p + 1 /* .endname */);
		
		lastanim->speed = /* .speed */
						  (anim_p[19] << 0) |
						  (anim_p[20] << 8) |
						  (anim_p[21] << 16) |
						  (anim_p[22] << 24);
		lastanim++;
	}
	Z_Free (animdefs);
}


// [RH] Check dmflags for noexit and respond accordingly
BOOL CheckIfExitIsGood (mobj_t *self)
{
	if (deathmatch->value && dmflags & DF_NO_EXIT && self) {
		while (self->health > 0)
			P_DamageMobj (self, self, self, 10000, MOD_EXIT);
		return false;
	}
	Printf ("%s exited the level.\n", self->player->userinfo->netname);
	return true;
}


//
// UTILITIES
//



//
// getSide()
// Will return a side_t*
//	given the number of the current sector,
//	the line number, and the side (0/1) that you want.
//
side_t *getSide (int currentSector, int line, int side)
{
	return &sides[ (sectors[currentSector].lines[line])->sidenum[side] ];
}


//
// getSector()
// Will return a sector_t*
//	given the number of the current sector,
//	the line number and the side (0/1) that you want.
//
sector_t *getSector (int currentSector, int line, int side)
{
	return sides[ (sectors[currentSector].lines[line])->sidenum[side] ].sector;
}


//
// twoSided()
// Given the sector number and the line number,
//	it will tell you whether the line is two-sided or not.
//
int twoSided (int sector, int line)
{
	return (sectors[sector].lines[line])->flags & ML_TWOSIDED;
}




//
// getNextSector()
// Return sector_t * of sector next to current.
// NULL if not two-sided line
//
sector_t *getNextSector (line_t *line, sector_t *sec)
{
	if (!(line->flags & ML_TWOSIDED))
		return NULL;
				
	if (line->frontsector == sec)
		return line->backsector;
		
	return line->frontsector;
}

//
// P_SectorActive()
//
// Passed a linedef special class (floor, ceiling, lighting) and a sector
// returns whether the sector is already busy with a linedef special of the
// same class. If old demo compatibility true, all linedef special classes
// are the same.
//
// jff 2/23/98 added to prevent old demos from
//  succeeding in starting multiple specials on one sector
//
int P_SectorActive(special_e t,sector_t *sec)
{
	if (olddemo) {	// return whether any thinker is active
		return sec->floordata || sec->ceilingdata || sec->lightingdata;
	} else {
		switch (t)	// return whether thinker of same type is active
		{
			case floor_special:
				return (int)sec->floordata;
			case ceiling_special:
				return (int)sec->ceilingdata;
			case lighting_special:
				return (int)sec->lightingdata;
		}
		return 1;	// don't know which special, must be active, shouldn't be here
	}
}


//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t P_FindLowestFloorSurrounding(sector_t* sec)
{
	int 				i;
	line_t* 			check;
	sector_t*			other;
	fixed_t 			floor = sec->floorheight;
		
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
		other = getNextSector(check,sec);

		if (!other)
			continue;
		
		if (other->floorheight < floor)
			floor = other->floorheight;
	}
	return floor;
}



//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t P_FindHighestFloorSurrounding(sector_t *sec)
{
	int 				i;
	line_t* 			check;
	sector_t*			other;
	fixed_t 			floor = MININT;
		
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
		other = getNextSector(check,sec);
		
		if (!other)
			continue;
		
		if (other->floorheight > floor)
			floor = other->floorheight;
	}
	return floor;
}



//
// P_FindNextHighestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the smallest floor height in a surrounding sector larger than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// Rewritten by Lee Killough to avoid fixed array and to be faster
//
fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
	sector_t *other;
	int i;

	for (i = 0; i < sec->linecount; i++) {
		if ((other = getNextSector(sec->lines[i],sec)) &&
			 other->floorheight > currentheight) {
			int height = other->floorheight;

			while (++i < sec->linecount) {
				if ((other = getNextSector(sec->lines[i],sec)) &&
					 other->floorheight < height &&
					 other->floorheight > currentheight) {
					height = other->floorheight;
				}
			}
			return height;
		}
	}
	return currentheight;
}


//
// P_FindNextLowestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the largest floor height in a surrounding sector smaller than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextLowestFloor(sector_t *sec, int currentheight)
{
	sector_t *other;
	int i;

	for (i = 0; i < sec->linecount; i++)
	if ((other = getNextSector(sec->lines[i],sec)) &&
		 other->floorheight < currentheight) {
		int height = other->floorheight;

		while (++i < sec->linecount) {
			if ((other = getNextSector(sec->lines[i],sec)) &&
				 other->floorheight > height &&
				 other->floorheight < currentheight) {
				height = other->floorheight;
			}
		}
		return height;
	}
	return currentheight;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t P_FindLowestCeilingSurrounding (sector_t *sec)
{
	int 				i;
	line_t* 			check;
	sector_t*			other;
	fixed_t 			height = MAXINT;
		
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
		other = getNextSector(check,sec);

		if (!other)
			continue;

		if (other->ceilingheight < height)
			height = other->ceilingheight;
	}
	return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t P_FindHighestCeilingSurrounding (sector_t *sec)
{
	int 		i;
	line_t* 	check;
	sector_t*	other;
	fixed_t 	height = MININT;
		
	for (i=0 ;i < sec->linecount ; i++)
	{
		check = sec->lines[i];
		other = getNextSector(check,sec);

		if (!other)
			continue;

		if (other->ceilingheight > height)
			height = other->ceilingheight;
	}
	return height;
}



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
int P_FindSectorFromLineTag (line_t *line, int start)
{
	int i;
		
	for (i=start+1;i<numsectors;i++)
		if (sectors[i].tag == line->tag)
			return i;
	
	return -1;
}

int P_FindLineFromLineTag (line_t *line, int start)
{
	int i;

	for (i = start+1; i < numlines; i++)
		if (lines[i].tag == line->tag)
			return i;

	return -1;
}


//
// Find minimum light from an adjacent sector
//
int P_FindMinSurroundingLight (sector_t *sector, int max)
{
	int 		i;
	int 		min;
	line_t* 	line;
	sector_t*	check;
		
	min = max;
	for (i=0 ; i < sector->linecount ; i++)
	{
		line = sector->lines[i];
		check = getNextSector(line,sector);

		if (!check)
			continue;

		if (check->lightlevel < min)
			min = check->lightlevel;
	}
	return min;
}



//
// EVENTS
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_CrossSpecialLine - TRIGGER
// Called every time a thing origin is about
//	to cross a line with a non 0 special.
//
void P_CrossSpecialLine (int linenum, int side, mobj_t *thing)
{
	line_t* 	line;
	int 		ok;

	line = &lines[linenum];
	
	//	Triggers that other things can activate
	if (!thing->player)
	{
		// Things that should NOT trigger specials...
		switch(thing->type)
		{
		  case MT_ROCKET:
		  case MT_PLASMA:
		  case MT_BFG:
		  case MT_TROOPSHOT:
		  case MT_HEADSHOT:
		  case MT_BRUISERSHOT:
			return;
			break;
			
		  default: break;
		}
				
		ok = 0;
		switch(line->special)
		{
		  case 39:		// TELEPORT TRIGGER
		  case 97:		// TELEPORT RETRIGGER
		  case 125: 	// TELEPORT MONSTERONLY TRIGGER
		  case 126: 	// TELEPORT MONSTERONLY RETRIGGER
		  case 4:		// RAISE DOOR
		  case 10:		// PLAT DOWN-WAIT-UP-STAY TRIGGER
		  case 88:		// PLAT DOWN-WAIT-UP-STAY RETRIGGER
			ok = 1;
			break;
		}
		if (!ok)
			return;
	}

	
	// Note: could use some const's here.
	switch (line->special)
	{
		// TRIGGERS.
		// All from here to RETRIGGERS.
	  case 2:
		// Open Door
		EV_DoDoor(line,open);
		line->special = 0;
		break;

	  case 3:
		// Close Door
		EV_DoDoor(line,close);
		line->special = 0;
		break;

	  case 4:
		// Raise Door
		EV_DoDoor(line,normal);
		line->special = 0;
		break;
		
	  case 5:
		// Raise Floor
		EV_DoFloor(line,raiseFloor);
		line->special = 0;
		break;
		
	  case 6:
		// Fast Ceiling Crush & Raise
		EV_DoCeiling(line,fastCrushAndRaise);
		line->special = 0;
		break;
		
	  case 8:
		// Build Stairs
		EV_BuildStairs(line,build8);
		line->special = 0;
		break;
		
	  case 10:
		// PlatDownWaitUp
		EV_DoPlat(line,downWaitUpStay,0);
		line->special = 0;
		break;
		
	  case 12:
		// Light Turn On - brightest near
		EV_LightTurnOn(line,0);
		line->special = 0;
		break;
		
	  case 13:
		// Light Turn On 255
		EV_LightTurnOn(line,255);
		line->special = 0;
		break;
		
	  case 16:
		// Close Door 30
		EV_DoDoor(line,close30ThenOpen);
		line->special = 0;
		break;
		
	  case 17:
		// Start Light Strobing
		EV_StartLightStrobing(line);
		line->special = 0;
		break;
		
	  case 19:
		// Lower Floor
		EV_DoFloor(line,lowerFloor);
		line->special = 0;
		break;
		
	  case 22:
		// Raise floor to nearest height and change texture
		EV_DoPlat(line,raiseToNearestAndChange,0);
		line->special = 0;
		break;
		
	  case 25:
		// Ceiling Crush and Raise
		EV_DoCeiling(line,crushAndRaise);
		line->special = 0;
		break;
		
	  case 30:
		// Raise floor to shortest texture height
		//	on either side of lines.
		EV_DoFloor(line,raiseToTexture);
		line->special = 0;
		break;
		
	  case 35:
		// Lights Very Dark
		EV_LightTurnOn(line,35);
		line->special = 0;
		break;
		
	  case 36:
		// Lower Floor (TURBO)
		EV_DoFloor(line,turboLower);
		line->special = 0;
		break;
		
	  case 37:
		// LowerAndChange
		EV_DoFloor(line,lowerAndChange);
		line->special = 0;
		break;
		
	  case 38:
		// Lower Floor To Lowest
		EV_DoFloor( line, lowerFloorToLowest );
		line->special = 0;
		break;
		
	  case 39:
		// TELEPORT!
		EV_Teleport( line, side, thing );
		line->special = 0;
		break;

	  case 40:
		// RaiseCeilingLowerFloor
		EV_DoCeiling( line, raiseToHighest );
		EV_DoFloor( line, lowerFloorToLowest );
		line->special = 0;
		break;
		
	  case 44:
		// Ceiling Crush
		EV_DoCeiling( line, lowerAndCrush );
		line->special = 0;
		break;
		
	  case 52:
		// EXIT!
		if (CheckIfExitIsGood (thing))
			G_ExitLevel ();
		break;
		
	  case 53:
		// Perpetual Platform Raise
		EV_DoPlat(line,perpetualRaise,0);
		line->special = 0;
		break;
		
	  case 54:
		// Platform Stop
		EV_StopPlat(line);
		line->special = 0;
		break;

	  case 56:
		// Raise Floor Crush
		EV_DoFloor(line,raiseFloorCrush);
		line->special = 0;
		break;

	  case 57:
		// Ceiling Crush Stop
		EV_CeilingCrushStop(line);
		line->special = 0;
		break;
		
	  case 58:
		// Raise Floor 24
		EV_DoFloor(line,raiseFloor24);
		line->special = 0;
		break;

	  case 59:
		// Raise Floor 24 And Change
		EV_DoFloor(line,raiseFloor24AndChange);
		line->special = 0;
		break;
		
	  case 104:
		// Turn lights off in sector(tag)
		EV_TurnTagLightsOff(line);
		line->special = 0;
		break;
		
	  case 108:
		// Blazing Door Raise (faster than TURBO!)
		EV_DoDoor (line,blazeRaise);
		line->special = 0;
		break;
		
	  case 109:
		// Blazing Door Open (faster than TURBO!)
		EV_DoDoor (line,blazeOpen);
		line->special = 0;
		break;
		
	  case 100:
		// Build Stairs Turbo 16
		EV_BuildStairs(line,turbo16);
		line->special = 0;
		break;
		
	  case 110:
		// Blazing Door Close (faster than TURBO!)
		EV_DoDoor (line,blazeClose);
		line->special = 0;
		break;

	  case 119:
		// Raise floor to nearest surr. floor
		EV_DoFloor(line,raiseFloorToNearest);
		line->special = 0;
		break;
		
	  case 121:
		// Blazing PlatDownWaitUpStay
		EV_DoPlat(line,blazeDWUS,0);
		line->special = 0;
		break;
		
	  case 124:
		// Secret EXIT
		if (CheckIfExitIsGood (thing))
			G_SecretExitLevel ();
		break;
				
	  case 125:
		// TELEPORT MonsterONLY
		if (!thing->player)
		{
			EV_Teleport( line, side, thing );
			line->special = 0;
		}
		break;
		
	  case 130:
		// Raise Floor Turbo
		EV_DoFloor(line,raiseFloorTurbo);
		line->special = 0;
		break;
		
	  case 141:
		// Silent Ceiling Crush & Raise
		EV_DoCeiling(line,silentCrushAndRaise);
		line->special = 0;
		break;
		
		// RETRIGGERS.	All from here till end.
	  case 72:
		// Ceiling Crush
		EV_DoCeiling( line, lowerAndCrush );
		break;

	  case 73:
		// Ceiling Crush and Raise
		EV_DoCeiling(line,crushAndRaise);
		break;

	  case 74:
		// Ceiling Crush Stop
		EV_CeilingCrushStop(line);
		break;
		
	  case 75:
		// Close Door
		EV_DoDoor(line,close);
		break;
		
	  case 76:
		// Close Door 30
		EV_DoDoor(line,close30ThenOpen);
		break;
		
	  case 77:
		// Fast Ceiling Crush & Raise
		EV_DoCeiling(line,fastCrushAndRaise);
		break;
		
	  case 79:
		// Lights Very Dark
		EV_LightTurnOn(line,35);
		break;
		
	  case 80:
		// Light Turn On - brightest near
		EV_LightTurnOn(line,0);
		break;
		
	  case 81:
		// Light Turn On 255
		EV_LightTurnOn(line,255);
		break;
		
	  case 82:
		// Lower Floor To Lowest
		EV_DoFloor( line, lowerFloorToLowest );
		break;
		
	  case 83:
		// Lower Floor
		EV_DoFloor(line,lowerFloor);
		break;

	  case 84:
		// LowerAndChange
		EV_DoFloor(line,lowerAndChange);
		break;

	  case 86:
		// Open Door
		EV_DoDoor(line,open);
		break;
		
	  case 87:
		// Perpetual Platform Raise
		EV_DoPlat(line,perpetualRaise,0);
		break;
		
	  case 88:
		// PlatDownWaitUp
		EV_DoPlat(line,downWaitUpStay,0);
		break;
		
	  case 89:
		// Platform Stop
		EV_StopPlat(line);
		break;
		
	  case 90:
		// Raise Door
		EV_DoDoor(line,normal);
		break;
		
	  case 91:
		// Raise Floor
		EV_DoFloor(line,raiseFloor);
		break;
		
	  case 92:
		// Raise Floor 24
		EV_DoFloor(line,raiseFloor24);
		break;
		
	  case 93:
		// Raise Floor 24 And Change
		EV_DoFloor(line,raiseFloor24AndChange);
		break;
		
	  case 94:
		// Raise Floor Crush
		EV_DoFloor(line,raiseFloorCrush);
		break;
		
	  case 95:
		// Raise floor to nearest height
		// and change texture.
		EV_DoPlat(line,raiseToNearestAndChange,0);
		break;
		
	  case 96:
		// Raise floor to shortest texture height
		// on either side of lines.
		EV_DoFloor(line,raiseToTexture);
		break;
		
	  case 97:
		// TELEPORT!
		EV_Teleport( line, side, thing );
		break;
		
	  case 98:
		// Lower Floor (TURBO)
		EV_DoFloor(line,turboLower);
		break;

	  case 105:
		// Blazing Door Raise (faster than TURBO!)
		EV_DoDoor (line,blazeRaise);
		break;
		
	  case 106:
		// Blazing Door Open (faster than TURBO!)
		EV_DoDoor (line,blazeOpen);
		break;

	  case 107:
		// Blazing Door Close (faster than TURBO!)
		EV_DoDoor (line,blazeClose);
		break;

	  case 120:
		// Blazing PlatDownWaitUpStay.
		EV_DoPlat(line,blazeDWUS,0);
		break;
		
	  case 126:
		// TELEPORT MonsterONLY.
		if (!thing->player)
			EV_Teleport( line, side, thing );
		break;
		
	  case 128:
		// Raise To Nearest Floor
		EV_DoFloor(line,raiseFloorToNearest);
		break;
		
	  case 129:
		// Raise Floor Turbo
		EV_DoFloor(line,raiseFloorTurbo);
		break;
	}
}



//
// P_ShootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void P_ShootSpecialLine (mobj_t *thing, line_t *line)
{
	int ok;
	
	//	Impacts that other things can activate.
	if (!thing->player)
	{
		ok = 0;
		switch(line->special)
		{
		  case 46:
			// OPEN DOOR IMPACT
			ok = 1;
			break;
		}
		if (!ok)
			return;
	}

	switch(line->special)
	{
	  case 24:
		// RAISE FLOOR
		EV_DoFloor(line,raiseFloor);
		P_ChangeSwitchTexture(line,0);
		break;
		
	  case 46:
		// OPEN DOOR
		EV_DoDoor(line,open);
		P_ChangeSwitchTexture(line,1);
		break;
		
	  case 47:
		// RAISE FLOOR NEAR AND CHANGE
		EV_DoPlat(line,raiseToNearestAndChange,0);
		P_ChangeSwitchTexture(line,0);
		break;
	}
}



//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t* player)
{
	sector_t*	sector;
		
	sector = player->mo->subsector->sector;

	// Falling, not all the way down yet?
	if (player->mo->z != sector->floorheight)
		return; 

	// Has hitten ground.
	switch (sector->special)
	{
	  case 5:
		// HELLSLIME DAMAGE
		if (!player->powers[pw_ironfeet])
			if (!(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 10, MOD_SLIME);
		break;
		
	  case 7:
		// NUKAGE DAMAGE
		if (!player->powers[pw_ironfeet])
			if (!(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 5, MOD_LAVA);
		break;
		
	  case 16:
		// SUPER HELLSLIME DAMAGE
	  case 4:
		// STROBE HURT
		if (!player->powers[pw_ironfeet]
			|| (P_Random (pr_playerinspecialsector)<5) )
		{
			if (!(level.time&0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 20, MOD_SLIME);
		}
		break;
						
	  case 9:
		// SECRET SECTOR
		player->secretcount++;
		level.found_secrets++;
		sector->special = 0;
		if (player == &players[displayplayer]) {
			C_MidPrint ("You found a secret area!");
			S_StartSound (ORIGIN_AMBIENT2, sfx_secret);
		}
		break;
						
	  case 11:
		// EXIT SUPER DAMAGE! (for E1M8 finale)
		player->cheats &= ~CF_GODMODE;

		if (!(level.time & 0x1f))
			P_DamageMobj (player->mo, NULL, NULL, 20, MOD_UNKNOWN);

		if (player->health <= 10 && (!deathmatch->value || !(dmflags & DF_NO_EXIT)))
			G_ExitLevel();
		break;
						
	  default:
		DPrintf ("P_PlayerInSpecialSector: %i unknown\n", sector->special);
		break;
	};
}




//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
extern cvar_t  *timelimit;

void P_UpdateSpecials (void)
{
	anim_t* 	anim;
	int 		pic;
	int 		i;

	
	//	LEVEL TIMER
	if (timelimit->value)
	{
		if (level.time >= (int)(timelimit->value * TICRATE * 60)) {
			Printf ("Timelimit exceeded\n");
			G_ExitLevel();
		}
	}
	
	//	ANIMATE FLATS AND TEXTURES GLOBALLY
	for (anim = anims ; anim < lastanim ; anim++)
	{
		for (i=anim->basepic ; i<anim->basepic+anim->numpics ; i++)
		{
			pic = anim->basepic + ( (level.time/anim->speed + i)%anim->numpics );
			if (anim->istexture)
				texturetranslation[i] = pic;
			else
				flattranslation[i] = pic;
		}
	}

	//	[RH] Scroll the sky
	sky1pos = (sky1pos + level.skyspeed1) & 0xffffff;
	sky2pos = (sky2pos + level.skyspeed2) & 0xffffff;
	

	//	DO BUTTONS
	for (i = 0; i < MAXBUTTONS; i++)
		if (buttonlist[i].btimer)
		{
			buttonlist[i].btimer--;
			if (!buttonlist[i].btimer)
			{
				switch(buttonlist[i].where)
				{
				  case top:
					sides[buttonlist[i].line->sidenum[0]].toptexture =
						buttonlist[i].btexture;
					break;
					
				  case middle:
					sides[buttonlist[i].line->sidenum[0]].midtexture =
						buttonlist[i].btexture;
					break;
					
				  case bottom:
					sides[buttonlist[i].line->sidenum[0]].bottomtexture =
						buttonlist[i].btexture;
					break;
				}
				S_StartSound((mobj_t *)&buttonlist[i].soundorg,sfx_swtchn);
				memset(&buttonlist[i],0,sizeof(button_t));
			}
		}
		
}



//
// Special Stuff that can not be categorized
//
int EV_DoDonut(line_t*	line)
{
	sector_t*			s1;
	sector_t*			s2;
	sector_t*			s3;
	int 				secnum;
	int 				rtn;
	int 				i;
	floormove_t*		floor;
		
	secnum = -1;
	rtn = 0;
	while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
	{
		s1 = &sectors[secnum];					// s1 is pillar's sector
				
		// ALREADY MOVING?	IF SO, KEEP GOING...
		if (P_SectorActive (floor_special, s1))	//jff 2/22/98
			continue;
						
		rtn = 1;
		s2 = getNextSector(s1->lines[0],s1);	// s2 is pool's sector
		if (!s2)								// note lowest numbered line around
			continue;							// pillar must be two-sided

		for (i = 0;i < s2->linecount;i++)
		{
			if ((!s2->lines[i]->flags & ML_TWOSIDED) ||
				(s2->lines[i]->backsector == s1))
				continue;
			s3 = s2->lines[i]->backsector;
			
			//	Spawn rising slime
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
			P_AddThinker (&floor->thinker);
			s2->floordata = floor;	//jff 2/22/98
			floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
			floor->type = donutRaise;
			floor->crush = false;
			floor->direction = 1;
			floor->sector = s2;
			floor->speed = FLOORSPEED / 2;
			floor->texture = s3->floorpic;
			floor->newspecial = 0;
			floor->floordestheight = s3->floorheight;
			
			//	Spawn lowering donut-hole
			floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
			P_AddThinker (&floor->thinker);
			s1->floordata = floor;	//jff 2/22/98
			floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
			floor->type = lowerFloor;
			floor->crush = false;
			floor->direction = -1;
			floor->sector = s1;
			floor->speed = FLOORSPEED / 2;
			floor->floordestheight = s3->floorheight;
			break;
		}
	}
	return rtn;
}



//
// SPECIAL SPAWNING
//

//
// P_SpawnSpecials
// After the map has been loaded, scan for specials
//	that spawn thinkers
//

void P_SpawnSpecials (void)
{
	sector_t*	sector;
	int 		i;
	int 		episode;

	episode = 1;
	if (W_CheckNumForName("texture2") >= 0)
		episode = 2;

	
	//	Init special SECTORs.
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		if (!sector->special)
			continue;
		
		switch (sector->special)
		{
		  case 1:
			// FLICKERING LIGHTS
			P_SpawnLightFlash (sector);
			break;

		  case 2:
			// STROBE FAST
			P_SpawnStrobeFlash(sector,FASTDARK,0);
			break;
			
		  case 3:
			// STROBE SLOW
			P_SpawnStrobeFlash(sector,SLOWDARK,0);
			break;
			
		  case 4:
			// STROBE FAST/DEATH SLIME
			P_SpawnStrobeFlash(sector,FASTDARK,0);
			sector->special = 4;
			break;
			
		  case 8:
			// GLOWING LIGHT
			P_SpawnGlowingLight(sector);
			break;
		  case 9:
			// SECRET SECTOR
			level.total_secrets++;
			break;
			
		  case 10:
			// DOOR CLOSE IN 30 SECONDS
			P_SpawnDoorCloseIn30 (sector);
			break;
			
		  case 12:
			// SYNC STROBE SLOW
			P_SpawnStrobeFlash (sector, SLOWDARK, 1);
			break;

		  case 13:
			// SYNC STROBE FAST
			P_SpawnStrobeFlash (sector, FASTDARK, 1);
			break;

		  case 14:
			// DOOR RAISE IN 5 MINUTES
			P_SpawnDoorRaiseIn5Mins (sector, i);
			break;
			
		  case 17:
			// fire flickering
			P_SpawnFireFlicker(sector);
			break;
		}
	}

	
	//	Init other misc stuff
	if (!activeceilings) {
		MaxCeilings = 30;		// [RH] Default. Increased as needed.
		activeceilings = Malloc (MaxCeilings * sizeof(ceiling_t *));
	}
	for (i = 0;i < MaxCeilings;i++)
		activeceilings[i] = NULL;

	if (!activeplats) {
		MaxPlats = 30;			// [RH] Default. Increased as needed.
		activeplats = Malloc (MaxPlats * sizeof(plat_t *));
	}
	for (i = 0;i < MaxPlats;i++)
		activeplats[i] = NULL;
	
	for (i = 0;i < MAXBUTTONS;i++)
		memset(&buttonlist[i],0,sizeof(button_t));

	// UNUSED: no horizonal sliders.
	//	P_InitSlidingDoorFrames();

	P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers

	for (i=0; i<numlines; i++)
		switch (lines[i].special)
		{
			int s, sec;

			// killough 3/7/98:
			// support for drawn heights coming from different sector
			case 242:
				sec = sides[*lines[i].sidenum].sector-sectors;
				for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
					sectors[s].heightsec = sec;
				break;

			// killough 3/16/98: Add support for setting
			// floor lighting independently (e.g. lava)
			case 213:
				sec = sides[*lines[i].sidenum].sector-sectors;
				for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
					sectors[s].floorlightsec = sec;
				break;

			// killough 4/11/98: Add support for setting
			// ceiling lighting independently
			case 261:
				sec = sides[*lines[i].sidenum].sector-sectors;
				for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
					sectors[s].ceilinglightsec = sec;
				break;
		}
}

// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98

void T_Scroll(scroll_t *s)
{
	fixed_t dx = s->dx, dy = s->dy;

	if (s->control != -1)
	{	// compute scroll amounts based on a sector's height changes
		fixed_t height = sectors[s->control].floorheight +
		sectors[s->control].ceilingheight;
		fixed_t delta = height - s->last_height;
		s->last_height = height;
		dx = FixedMul(dx, delta);
		dy = FixedMul(dy, delta);
	}

	// killough 3/14/98: Add acceleration
	if (s->accel)
	{
		s->vdx = dx += s->vdx;
		s->vdy = dy += s->vdy;
	}

	if (!(dx | dy))			// no-op if both (x,y) offsets 0
		return;

	switch (s->type)
	{
		side_t *side;
		sector_t *sec;
		fixed_t height, waterheight;	// killough 4/4/98: add waterheight
		msecnode_t *node;
		mobj_t *thing;

		case sc_side:					// killough 3/7/98: Scroll wall texture
			side = sides + s->affectee;
			side->textureoffset += dx;
			side->rowoffset += dy;
			break;

		case sc_floor:						// killough 3/7/98: Scroll floor texture
			sec = sectors + s->affectee;
			sec->floor_xoffs += dx;
			sec->floor_yoffs += dy;
			break;

		case sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			sec = sectors + s->affectee;
			sec->ceiling_xoffs += dx;
			sec->ceiling_yoffs += dy;
			break;

		case sc_carry:

			// killough 3/7/98: Carry things on floor
			// killough 3/20/98: use new sector list which reflects true members
			// killough 3/27/98: fix carrier bug
			// killough 4/4/98: Underwater, carry things even w/o gravity

			sec = sectors + s->affectee;
			height = sec->floorheight;
			waterheight = sec->heightsec != -1 &&
			sectors[sec->heightsec].floorheight > height ?
			sectors[sec->heightsec].floorheight : MININT;

			for (node = sec->touching_thinglist; node; node = node->m_snext)
				if (!((thing = node->m_thing)->flags & MF_NOCLIP) &&
					(!(thing->flags & MF_NOGRAVITY || thing->z > height) ||
					 thing->z < waterheight))
				  {
					// Move objects only if on floor or underwater,
					// non-floating, and clipped.
					thing->momx += dx;
					thing->momy += dy;
				  }
			break;

		case sc_carry_ceiling:       // to be added later
			break;
	}
}

//
// Add_Scroller()
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//

static void Add_Scroller(int type, fixed_t dx, fixed_t dy,
						 int control, int affectee, int accel)
{
	scroll_t *s = Z_Malloc(sizeof *s, PU_LEVSPEC, 0);
	s->thinker.function.acp1 = (actionf_p1) T_Scroll;
	s->type = type;
	s->dx = dx;
	s->dy = dy;
	s->accel = accel;
	s->vdx = s->vdy = 0;
	if ((s->control = control) != -1)
		s->last_height =
			sectors[control].floorheight + sectors[control].ceilingheight;
	s->affectee = affectee;
	P_AddThinker(&s->thinker);
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff

static void Add_WallScroller(fixed_t dx, fixed_t dy, const line_t *l,
							 int control, int accel)
{
	fixed_t x = abs(l->dx), y = abs(l->dy), d;
	if (y > x)
	d = x, x = y, y = d;
	d = FixedDiv(x, finesine[(tantoangle[FixedDiv(y,x) >> DBITS] + ANG90)
						  >> ANGLETOFINESHIFT]);
	x = -FixedDiv(FixedMul(dy, l->dy) + FixedMul(dx, l->dx), d);
	y = -FixedDiv(FixedMul(dx, l->dy) - FixedMul(dy, l->dx), d);
	Add_Scroller(sc_side, x, y, control, *l->sidenum, accel);
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
#define CARRYFACTOR ((fixed_t)(FRACUNIT*.09375))

// Initialize the scrollers
static void P_SpawnScrollers(void)
{
  int i;
  line_t *l = lines;

  for (i=0;i<numlines;i++,l++)
	{
	  fixed_t dx = l->dx >> SCROLL_SHIFT;  // direction and speed of scrolling
	  fixed_t dy = l->dy >> SCROLL_SHIFT;
	  int control = -1, accel = 0;         // no control sector or acceleration
	  int special = l->special;

	  // killough 3/7/98: Types 245-249 are same as 250-254 except that the
	  // first side's sector's heights cause scrolling when they change, and
	  // this linedef controls the direction and speed of the scrolling. The
	  // most complicated linedef since donuts, but powerful :)
	  //
	  // killough 3/15/98: Add acceleration. Types 214-218 are the same but
	  // are accelerative.

	  if (special >= 245 && special <= 249)         // displacement scrollers
		{
		  special += 250-245;
		  control = sides[*l->sidenum].sector - sectors;
		}
	  else
		if (special >= 214 && special <= 218)       // accelerative scrollers
		  {
			accel = 1;
			special += 250-214;
			control = sides[*l->sidenum].sector - sectors;
		  }

	  switch (special)
		{
		  register int s;

		case 250:   // scroll effect ceiling
		  for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
			Add_Scroller(sc_ceiling, -dx, dy, control, s, accel);
		  break;

		case 251:   // scroll effect floor
		case 253:   // scroll and carry objects on floor
		  for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
			Add_Scroller(sc_floor, -dx, dy, control, s, accel);
		  if (special != 253)
			break;

		case 252: // carry objects on floor
		  dx = FixedMul(dx,CARRYFACTOR);
		  dy = FixedMul(dy,CARRYFACTOR);
		  for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
			Add_Scroller(sc_carry, dx, dy, control, s, accel);
		  break;

		  // killough 3/1/98: scroll wall according to linedef
		  // (same direction and speed as scrolling floors)
		case 254:
		  for (s=-1; (s = P_FindLineFromLineTag(l,s)) >= 0;)
			if (s != i)
			  Add_WallScroller(dx, dy, lines+s, control, accel);
		  break;

		case 255:    // killough 3/2/98: scroll according to sidedef offsets
		  s = lines[i].sidenum[0];
		  Add_Scroller(sc_side, -sides[s].textureoffset,
					   sides[s].rowoffset, -1, s, accel);
		  break;

		case 48:                  // scroll first side
		  Add_Scroller(sc_side,  FRACUNIT, 0, -1, lines[i].sidenum[0], accel);
		  break;

		case 85:                  // jff 1/30/98 2-way scroll
		  Add_Scroller(sc_side, -FRACUNIT, 0, -1, lines[i].sidenum[0], accel);
		  break;
		}
	}
}

// killough 3/7/98 -- end generalized scroll effects
