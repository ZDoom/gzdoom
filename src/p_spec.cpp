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
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
//
//-----------------------------------------------------------------------------


#include "m_alloc.h"

#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"

#include "i_system.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_random.h"
#include "m_bbox.h"
#include "w_wad.h"

#include "r_local.h"
#include "p_local.h"
#include "p_lnspec.h"

#include "g_game.h"

#include "s_sound.h"
#include "sc_man.h"

// State.
#include "r_state.h"

#include "c_console.h"

// [RH] Needed for sky scrolling
#include "r_sky.h"

IMPLEMENT_SERIAL (DScroller, DThinker)

IMPLEMENT_POINTY_SERIAL (DPusher, DThinker)
 DECLARE_POINTER (m_Source)
END_POINTERS

DScroller::DScroller ()
{
}

void DScroller::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Type
			<< m_dx << m_dy
			<< m_Affectee
			<< m_Control
			<< m_LastHeight
			<< m_vdx << m_vdy
			<< m_Accel;
	}
	else
	{
		arc >> m_Type
			>> m_dx >> m_dy
			>> m_Affectee
			>> m_Control
			>> m_LastHeight
			>> m_vdx >> m_vdy
			>> m_Accel;
	}
}

DPusher::DPusher ()
{
}

void DPusher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << m_Type
			<< m_Source
			<< m_Xmag
			<< m_Ymag
			<< m_Magnitude
			<< m_Radius
			<< m_X
			<< m_Y
			<< m_Affectee;
	}
	else
	{
		arc >> m_Type
			>> m_Source
			>> m_Xmag
			>> m_Ymag
			>> m_Magnitude
			>> m_Radius
			>> m_X
			>> m_Y
			>> m_Affectee;
	}
}

//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//
#define MAX_ANIM_FRAMES	32

typedef struct
{
	short 	basepic;
	short	numframes;
	byte 	istexture;
	byte	uniqueframes;
	byte	countdown;
	byte	curframe;
	byte 	speedmin[MAX_ANIM_FRAMES];
	byte	speedmax[MAX_ANIM_FRAMES];
	short	framepic[MAX_ANIM_FRAMES];
} anim_t;



#define MAXANIMS	32		// Really just a starting point

static anim_t*  lastanim;
static anim_t*  anims;
static size_t	maxanims;


// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
#define CARRYFACTOR ((fixed_t)(FRACUNIT*.09375))

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnScrollers(void);

static void P_SpawnFriction(void);		// phares 3/16/98
static void P_SpawnPushers(void);		// phares 3/20/98

//
//		Animating line specials
//
//#define MAXLINEANIMS			64

//extern	short	numlinespecials;
//extern	line_t* linespeciallist[MAXLINEANIMS];

//
// [RH] P_InitAnimDefs
//
// This uses a Hexen ANIMDEFS lump to define the animation sequences
//
static void P_InitAnimDefs (void)
{
	int lump = W_CheckNumForName ("ANIMDEFS");
	enum {
		limbo,
		newflat,
		readingflat,
		newtexture,
		readingtexture,
		warp
	} state = limbo, newstate = limbo;
	int frame, min, max;
	char name[9];

	if (lump >= 0)
	{
		SC_OpenLumpNum (lump, "ANIMDEFS");

		while (SC_GetString ())
		{
			if (SC_Compare ("flat"))
			{
				newstate = newflat;
			}
			else if (SC_Compare ("texture"))
			{
				newstate = newtexture;
			}
			else if (SC_Compare ("warp"))
			{
				newstate = warp;
				SC_MustGetString ();
				if (SC_Compare ("flat"))
				{
					SC_MustGetString ();
					flatwarp[R_FlatNumForName (sc_String)] = true;
				}
				else if (SC_Compare ("texture"))
				{
					// TODO: Make texture warping work with wall textures
					SC_MustGetString ();
					R_TextureNumForName (sc_String);
				}
				else
				{
					SC_ScriptError (NULL, NULL);
				}
			}
			else if (SC_Compare ("pic"))
			{
				SC_MustGetNumber ();
				frame = sc_Number;
				SC_MustGetString ();
				if (SC_Compare ("tics"))
				{
					SC_MustGetNumber ();
					min = max = sc_Number;
				}
				else if (SC_Compare ("rand"))
				{
					SC_MustGetNumber ();
					min = sc_Number;
					SC_MustGetNumber ();
					max = sc_Number;
				}
				else
				{
					SC_ScriptError (NULL);
				}
			}

			if (newstate == newtexture || newstate == newflat || newstate == warp)
			{
				if (state != limbo)
				{
					if (lastanim->numframes < 2)
						I_FatalError ("P_InitAnimDefs: %s needs at least 2 frames", name);

					lastanim->countdown = lastanim->speedmin[0];
					lastanim++;
				}

				if (newstate != warp)
				{
					// 1/11/98 killough -- removed limit by array-doubling
					if (lastanim >= anims + maxanims)
					{
						size_t newmax = maxanims ? maxanims*2 : MAXANIMS;
						anims = (anim_t *)Realloc(anims, newmax*sizeof(*anims));   // killough
						lastanim = anims + maxanims;
						maxanims = newmax;
					}

					lastanim->uniqueframes = 1;
					lastanim->curframe = 0;
					lastanim->numframes = 0;
					lastanim->istexture = (newstate == newtexture);
					memset (lastanim->speedmin, 1, MAX_ANIM_FRAMES * sizeof(*lastanim->speedmin));
					memset (lastanim->speedmax, 1, MAX_ANIM_FRAMES * sizeof(*lastanim->speedmax));

					SC_MustGetString ();

					if (lastanim->istexture)
						lastanim->basepic = R_TextureNumForName (sc_String);
					else
						lastanim->basepic = R_FlatNumForName (sc_String);

					strncpy (name, sc_String, 8);
					name[8] = 0;
				}

				state = (newstate == newflat) ? readingflat : 
						(newstate == newtexture) ? readingtexture : limbo;
				newstate = limbo;
			}
			else if (state == readingflat || state == readingtexture)
			{
				if (lastanim->numframes < MAX_ANIM_FRAMES)
				{
					lastanim->speedmin[lastanim->numframes] = min;
					lastanim->speedmax[lastanim->numframes] = max;
					lastanim->framepic[lastanim->numframes] = frame + lastanim->basepic - 1;
					lastanim->numframes++;
				}
			}
		}
		if (state == readingflat || state == readingtexture)
		{
			lastanim->countdown = lastanim->speedmin[0];
			lastanim++;
		}
		SC_Close ();
	}
}


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
	byte *animdefs, *anim_p;

	// [RH] Load an ANIMDEFS lump first
	P_InitAnimDefs ();
	
	if (W_CheckNumForName ("ANIMATED") == -1)
		return;

	animdefs = (byte *)W_CacheLumpName ("ANIMATED", PU_STATIC);

	// Init animation

	for (anim_p = animdefs; *anim_p != 255; anim_p += 23)
	{
		// 1/11/98 killough -- removed limit by array-doubling
		if (lastanim >= anims + maxanims)
		{
			size_t newmax = maxanims ? maxanims*2 : MAXANIMS;
			anims = (anim_t *)Realloc(anims, newmax*sizeof(*anims));   // killough
			lastanim = anims + maxanims;
			maxanims = newmax;
		}

		if (*anim_p /* .istexture */)
		{
			// different episode ?
			if (R_CheckTextureNumForName (anim_p + 10 /* .startname */) == -1)
				continue;		

			lastanim->basepic = R_TextureNumForName (anim_p + 10 /* .startname */);
			lastanim->numframes = R_TextureNumForName (anim_p + 1 /* .endname */)
								  - lastanim->basepic + 1;
		}
		else
		{
			if (W_CheckNumForName (anim_p + 10 /* .startname */, ns_flats) == -1)
				continue;

			lastanim->basepic = R_FlatNumForName (anim_p + 10 /* .startname */);
			lastanim->numframes = R_FlatNumForName (anim_p + 1 /* .endname */)
								  - lastanim->basepic + 1;
		}

		lastanim->istexture = *anim_p /* .istexture */;
		lastanim->uniqueframes = 0;
		lastanim->curframe = 0;

		if (lastanim->numframes < 2)
			I_FatalError ("P_InitPicAnims: bad cycle from %s to %s",
					 anim_p + 10 /* .startname */,
					 anim_p + 1 /* .endname */);
		
		lastanim->speedmin[0] = lastanim->speedmax[0] = lastanim->countdown =
					/* .speed */
					(anim_p[19] << 0) |
					(anim_p[20] << 8) |
					(anim_p[21] << 16) |
					(anim_p[22] << 24);

		lastanim->countdown--;

		lastanim++;
	}
	Z_Free (animdefs);
}


// [RH] Check dmflags for noexit and respond accordingly
BOOL CheckIfExitIsGood (AActor *self)
{
	if (self == NULL)
		return false;

	if ((deathmatch.value || alwaysapplydmflags.value) && dmflags & DF_NO_EXIT && self)
	{
		while (self->health > 0)
			P_DamageMobj (self, self, self, 10000, MOD_EXIT);
		return false;
	}
	if (deathmatch.value)
		Printf (PRINT_HIGH, "%s exited the level.\n", self->player->userinfo.netname);
	return true;
}


//
// UTILITIES
//



// [RH]
// P_NextSpecialSector()
//
// Returns the next special sector attached to this sector
// with a certain special.
sector_t *P_NextSpecialSector (sector_t *sec, int type, sector_t *nogood)
{
	sector_t *tsec;
	int i;

	for (i = 0; i < sec->linecount; i++)
	{
		line_t *ln = sec->lines[i];

		if (!(ln->flags & ML_TWOSIDED) ||
			!(tsec = ln->frontsector))
			continue;

		if (sec == tsec)
		{
			tsec = ln->backsector;
			if (sec == tsec)
				continue;
		}

		if (tsec == nogood)
			continue;

		if ((tsec->special & 0x00ff) == type)
		{
			return tsec;
		}
	}
	return NULL;
}

//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t P_FindLowestFloorSurrounding (sector_t* sec)
{
	int i;
	line_t *check;
	sector_t *other;
	fixed_t floor = sec->floorheight;
		
	for (i = 0; i < sec->linecount; i++)
	{
		check = sec->lines[i];
		other = getNextSector (check,sec);

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
fixed_t P_FindHighestFloorSurrounding (sector_t *sec)
{
	int i;
	line_t *check;
	sector_t *other;
	fixed_t floor = MININT;
		
	for (i = 0; i < sec->linecount; i++)
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
fixed_t P_FindNextHighestFloor (sector_t *sec, int currentheight)
{
	sector_t *other;
	int i;

	for (i = 0; i < sec->linecount; i++)
	{
		if ((other = getNextSector(sec->lines[i],sec)) &&
			 other->floorheight > currentheight)
		{
			int height = other->floorheight;

			while (++i < sec->linecount)
			{
				if ((other = getNextSector(sec->lines[i],sec)) &&
					 other->floorheight < height &&
					 other->floorheight > currentheight)
				{
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
		 other->floorheight < currentheight)
	{
		int height = other->floorheight;

		while (++i < sec->linecount)
		{
			if ((other = getNextSector(sec->lines[i],sec)) &&
				 other->floorheight > height &&
				 other->floorheight < currentheight)
			{
				height = other->floorheight;
			}
		}
		return height;
	}
	return currentheight;
}

//
// P_FindNextLowestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the largest ceiling height in a surrounding sector smaller than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextLowestCeiling (sector_t *sec, int currentheight)
{
	sector_t *other;
	int i;

	for (i = 0; i < sec->linecount; i++)
		if ((other = getNextSector(sec->lines[i],sec)) &&
			other->ceilingheight < currentheight)
		{
			int height = other->ceilingheight;
			while (++i < sec->linecount)
				if ((other = getNextSector(sec->lines[i],sec)) &&
					 other->ceilingheight > height &&
					 other->ceilingheight < currentheight)
					height = other->ceilingheight;
			return height;
		}
	return currentheight;
}


//
// P_FindNextHighestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the smallest ceiling height in a surrounding sector larger than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t P_FindNextHighestCeiling (sector_t *sec, int currentheight)
{
	sector_t *other;
	int i;

	for (i = 0; i < sec->linecount; i++)
		if ((other = getNextSector(sec->lines[i],sec)) &&
			 other->ceilingheight > currentheight)
		{
			int height = other->ceilingheight;
			while (++i < sec->linecount)
				if ((other = getNextSector(sec->lines[i],sec)) &&
					 other->ceilingheight < height &&
					 other->ceilingheight > currentheight)
					height = other->ceilingheight;
			return height;
		}
	return currentheight;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t P_FindLowestCeilingSurrounding (sector_t *sec)
{
	int i;
	line_t *check;
	sector_t *other;
	fixed_t height = MAXINT;
		
	for (i = 0; i < sec->linecount; i++)
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
	int i;
	line_t *check;
	sector_t *other;
	fixed_t height = MININT;
		
	for (i = 0; i < sec->linecount; i++)
	{
		check = sec->lines[i];
		other = getNextSector (check,sec);

		if (!other)
			continue;

		if (other->ceilingheight > height)
			height = other->ceilingheight;
	}
	return height;
}


//
// P_FindShortestTextureAround()
//
// Passed a sector number, returns the shortest lower texture on a
// linedef bounding the sector.
//
// jff 02/03/98 Add routine to find shortest lower texture
//
fixed_t P_FindShortestTextureAround (int secnum)
{
	int minsize = MAXINT;
	side_t *side;
	int i;
	sector_t *sec = &sectors[secnum];

	for (i = 0; i < sec->linecount; i++)
	{
		if (twoSided (secnum, i))
		{
			side = getSide (secnum, i, 0);
			if (side->bottomtexture >= 0)
				if (textureheight[side->bottomtexture] < minsize)
					minsize = textureheight[side->bottomtexture];
			side = getSide (secnum, i, 1);
			if (side->bottomtexture >= 0)
				if (textureheight[side->bottomtexture] < minsize)
					minsize = textureheight[side->bottomtexture];
		}
	}
	return minsize;
}


//
// P_FindShortestUpperAround()
//
// Passed a sector number, returns the shortest upper texture on a
// linedef bounding the sector.
//
// Note: If no upper texture exists 32000*FRACUNIT is returned.
//       but if compatibility then MAXINT is returned
//
// jff 03/20/98 Add routine to find shortest upper texture
//
fixed_t P_FindShortestUpperAround (int secnum)
{
	int minsize = MAXINT;
	side_t *side;
	int i;
	sector_t *sec = &sectors[secnum];

	for (i = 0; i < sec->linecount; i++)
	{
		if (twoSided (secnum, i))
		{
			side = getSide (secnum,i,0);
			if (side->toptexture >= 0)
				if (textureheight[side->toptexture] < minsize)
					minsize = textureheight[side->toptexture];
			side = getSide (secnum,i,1);
			if (side->toptexture >= 0)
				if (textureheight[side->toptexture] < minsize)
					minsize = textureheight[side->toptexture];
		}
	}
	return minsize;
}


//
// P_FindModelFloorSector()
//
// Passed a floor height and a sector number, return a pointer to a
// a sector with that floor height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model floor
//  around a sector specified by sector number
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using floormove_t
//
sector_t *P_FindModelFloorSector (fixed_t floordestheight, int secnum)
{
	int i;
	sector_t *sec = &sectors[secnum];
	int linecount;

	//jff 5/23/98 don't disturb sec->linecount while searching
	// but allow early exit in old demos
	linecount = sec->linecount;
	for (i = 0; i < linecount; i++)
	{
		if (twoSided (secnum, i))
		{
			if (getSide (secnum,i,0)->sector-sectors == secnum)
				sec = getSector (secnum, i, 1);
			else
				sec = getSector (secnum, i, 0);

			if (sec->floorheight == floordestheight)
				return sec;
		}
	}
	return NULL;
}


//
// P_FindModelCeilingSector()
//
// Passed a ceiling height and a sector number, return a pointer to a
// a sector with that ceiling height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model ceiling
//  around a sector specified by sector number
//  used only from generalized ceiling types
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using ceiling_t
//
sector_t *P_FindModelCeilingSector (fixed_t ceildestheight, int secnum)
{
	int i;
	sector_t *sec = &sectors[secnum];
	int linecount;

	//jff 5/23/98 don't disturb sec->linecount while searching
	linecount = sec->linecount;
	for (i = 0; i < linecount; i++)
	{
		if (twoSided (secnum, i))
		{
			if (getSide (secnum,i,0)->sector-sectors == secnum)
				sec = getSector (secnum,i,1);
			else
				sec = getSector (secnum,i,0);

			if (sec->ceilingheight == ceildestheight)
				return sec;
		}
	}
	return NULL;
}


//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with a specified tag.
// Rewritten by Lee Killough to use chained hashing to improve speed

int P_FindSectorFromTag (int tag, int start)
{
	start = start >= 0 ? sectors[start].nexttag :
		sectors[(unsigned) tag % (unsigned) numsectors].firsttag;
	while (start >= 0 && sectors[start].tag != tag)
		start = sectors[start].nexttag;
	return start;
}

// killough 4/16/98: Same thing, only for linedefs

int P_FindLineFromID (int id, int start)
{
	start = start >= 0 ? lines[start].nextid :
		lines[(unsigned) id % (unsigned) numlines].firstid;
	while (start >= 0 && lines[start].id != id)
		start = lines[start].nextid;
	return start;
}

// Hash the sector tags across the sectors and linedefs.
static void P_InitTagLists (void)
{
	register int i;

	for (i=numsectors; --i>=0; )		// Initially make all slots empty.
		sectors[i].firsttag = -1;
	for (i=numsectors; --i>=0; )		// Proceed from last to first sector
	{									// so that lower sectors appear first
		int j = (unsigned) sectors[i].tag % (unsigned) numsectors;	// Hash func
		sectors[i].nexttag = sectors[j].firsttag;	// Prepend sector to chain
		sectors[j].firsttag = i;
	}

	// killough 4/17/98: same thing, only for linedefs

	for (i=numlines; --i>=0; )			// Initially make all slots empty.
		lines[i].firstid = -1;
	for (i=numlines; --i>=0; )        // Proceed from last to first linedef
	{									// so that lower linedefs appear first
		int j = (unsigned) lines[i].id % (unsigned) numlines;	// Hash func
		lines[i].nextid = lines[j].firstid;	// Prepend linedef to chain
		lines[j].firstid = i;
	}
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

// [RH] P_CheckKeys
//
//	Returns true if the player has the desired key,
//	false otherwise.

BOOL P_CheckKeys (player_t *p, keytype_t lock, BOOL remote)
{
	if ((lock & 0x7f) == NoKey)
		return true;

	if (!p)
		return false;

	char *msg;
	BOOL bc, rc, yc, bs, rs, ys;
	BOOL equiv = lock & 0x80;

	lock = (keytype_t)(lock & 0x7f);

	bc = p->cards[it_bluecard];
	rc = p->cards[it_redcard];
	yc = p->cards[it_yellowcard];
	bs = p->cards[it_blueskull];
	rs = p->cards[it_redskull];
	ys = p->cards[it_yellowskull];

	if (equiv) {
		bc = bs = (bc || bs);
		rc = rs = (rc || rs);
		yc = yc = (yc || ys);
	}

	switch (lock) {
		default:		// Unknown key; assume we have it
			return true;

		case AnyKey:
			if (bc || bs || rc || rs || yc || ys)
				return true;
			msg = PD_ANY;
			break;

		case AllKeys:
			if (bc && bs && rc && rs && yc && ys)
				return true;
			msg = equiv ? PD_ALL3 : PD_ALL6;
			break;

		case RCard:
			if (rc)
				return true;
			msg = equiv ? (remote ? PD_REDO : PD_REDK) : PD_REDC;
			break;

		case BCard:
			if (bc)
				return true;
			msg = equiv ? (remote ? PD_BLUEO : PD_BLUEK) : PD_BLUEC;
			break;

		case YCard:
			if (yc)
				return true;
			msg = equiv ? (remote ? PD_YELLOWO : PD_YELLOWK) : PD_YELLOWC;
			break;

		case RSkull:
			if (rs)
				return true;
			msg = equiv ? (remote ? PD_REDO : PD_REDK) : PD_REDS;
			break;

		case BSkull:
			if (bs)
				return true;
			msg = equiv ? (remote ? PD_BLUEO : PD_BLUEK) : PD_BLUES;
			break;

		case YSkull:
			if (ys)
				return true;
			msg = equiv ? (remote ? PD_YELLOWO : PD_YELLOWK) : PD_YELLOWS;
			break;
	}

	// If we get here, we don't have the right key,
	// so print an appropriate message and grunt.
	if (p->mo == players[consoleplayer].camera)
	{
		int keytrysound = S_FindSound ("misc/keytry");
		if (keytrysound > -1)
			S_Sound (p->mo, CHAN_VOICE, "misc/keytry", 1, ATTN_NORM);
		else
			S_Sound (p->mo, CHAN_VOICE, "*grunt1", 1, ATTN_NORM);
		C_MidPrint (msg);
	}
	return false;
}


//============================================================================
//
// P_ActivateLine
//
//============================================================================

BOOL P_ActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;
	BOOL repeat;
	BOOL buttonSuccess;

	lineActivation = GET_SPAC(line->flags);
	if (lineActivation == SPAC_USETHROUGH)
		lineActivation = SPAC_USE;
	if (lineActivation != activationType &&
		!(activationType == SPAC_MCROSS && lineActivation == SPAC_CROSS))
	{
		return false;
	}
	if (!mo->player && !(mo->flags & MF_MISSILE))
	{
		if ((activationType == SPAC_USE || activationType == SPAC_PUSH)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors
		if (!(line->flags & ML_MONSTERSCANACTIVATE))
		{ // [RH] monsters' ability to activate this line depends on its type
		  // (in Hexen, only MCROSS lines could be activated by monsters)
			BOOL noway = true;

			switch (lineActivation) {
				case SPAC_IMPACT:
				case SPAC_PCROSS:
					// shouldn't really be here if not a missile
				case SPAC_MCROSS:
					noway = false;
					break;
				case SPAC_CROSS:
					switch (line->special) {
						case Teleport:
						case Teleport_NoFog:
						case Teleport_Line:
						case Door_Raise:
						case Plat_DownWaitUpStayLip:
						case Plat_DownWaitUpStay:
							noway = false;
					}
					break;
				case SPAC_USE:
				case SPAC_PUSH:
					switch (line->special) {
						case Door_Raise:
							if (line->args[0] == 0)
								noway = false;
							break;
						case Teleport:
						case Teleport_NoFog:
							noway = false;
					}
					break;
			}
			if (noway)
				return false;
		}
	}
	repeat = line->flags & ML_REPEAT_SPECIAL;
	buttonSuccess = false;
	TeleportSide = side;
	buttonSuccess = LineSpecials[line->special]
					(line, mo, line->args[0],
					line->args[1], line->args[2],
					line->args[3], line->args[4]);

	if (!repeat && buttonSuccess)
	{ // clear the special on non-retriggerable lines
		line->special = 0;
	}
	if ((lineActivation == SPAC_USE || lineActivation == SPAC_IMPACT) 
		&& buttonSuccess)
	{
		P_ChangeSwitchTexture (line, repeat);
	}
	return true;
}

BOOL P_TestActivateLine (line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;

	lineActivation = GET_SPAC(line->flags);
	if (lineActivation == SPAC_USETHROUGH)
		lineActivation = SPAC_USE;
	if (lineActivation != activationType &&
		!(activationType == SPAC_MCROSS && lineActivation == SPAC_CROSS))
	{
		return false;
	}
	if (!mo->player && !(mo->flags & MF_MISSILE))
	{
		if ((activationType == SPAC_USE || activationType == SPAC_PUSH)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors
		if (!(line->flags & ML_MONSTERSCANACTIVATE))
		{ // [RH] monsters' ability to activate this line depends on its type
		  // (in Hexen, only MCROSS lines could be activated by monsters)
			BOOL noway = true;

			switch (lineActivation) {
				case SPAC_IMPACT:
				case SPAC_PCROSS:
					// shouldn't really be here if not a missile
				case SPAC_MCROSS:
					noway = false;
					break;
				case SPAC_CROSS:
					switch (line->special) {
						case Teleport:
						case Teleport_NoFog:
						case Teleport_Line:
						case Door_Raise:
						case Plat_DownWaitUpStayLip:
						case Plat_DownWaitUpStay:
							noway = false;
					}
					break;
				case SPAC_USE:
				case SPAC_PUSH:
					switch (line->special) {
						case Door_Raise:
							if (line->args[0] == 0)
								noway = false;
							break;
						case Teleport:
						case Teleport_NoFog:
							noway = false;
					}
					break;
			}
			if (noway)
				return false;
		}
	}
	return true;
}

//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t *player)
{
	sector_t *sector = player->mo->subsector->sector;
	int special = sector->special & ~SECRET_MASK;

	// Falling, not all the way down yet?
	if (player->mo->z != sector->floorheight && !player->mo->waterlevel)
		return; 

	// Has hitten ground.
	// [RH] Normal DOOM special or BOOM specialized?
	if (special >= dLight_Flicker && special <= dLight_FireFlicker)
	{
		switch (special)
		{
		  case dDamage_Hellslime:
			// HELLSLIME DAMAGE
			if (!player->powers[pw_ironfeet])
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 10, MOD_SLIME);
			break;
			
		  case dDamage_Nukage:
			// NUKAGE DAMAGE
			if (!player->powers[pw_ironfeet])
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 5, MOD_LAVA);
			break;
			
		  case dDamage_SuperHellslime:
			// SUPER HELLSLIME DAMAGE
		  case dLight_Strobe_Hurt:
			// STROBE HURT
			if (!player->powers[pw_ironfeet]
				|| (P_Random (pr_playerinspecialsector)<5) )
			{
				if (!(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, 20, MOD_SLIME);
			}
			break;
							
		  case dDamage_End:
			// EXIT SUPER DAMAGE! (for E1M8 finale)
			player->cheats &= ~CF_GODMODE;

			if (!(level.time & 0x1f))
				P_DamageMobj (player->mo, NULL, NULL, 20, MOD_UNKNOWN);

			if (player->health <= 10 && (!deathmatch.value || !(dmflags & DF_NO_EXIT)))
				G_ExitLevel(0);
			break;
							
		  default:
			// [RH] Ignore unknown specials
			break;
		}
	}
	else
	{
		//jff 3/14/98 handle extended sector types for secrets and damage
		switch (special & DAMAGE_MASK) {
			case 0x000: // no damage
				break;
			case 0x100: // 2/5 damage per 31 ticks
				if (!player->powers[pw_ironfeet])
					if (!(level.time&0x1f))
						P_DamageMobj (player->mo, NULL, NULL, 5, MOD_LAVA);
				break;
			case 0x200: // 5/10 damage per 31 ticks
				if (!player->powers[pw_ironfeet])
					if (!(level.time&0x1f))
						P_DamageMobj (player->mo, NULL, NULL, 10, MOD_SLIME);
				break;
			case 0x300: // 10/20 damage per 31 ticks
				if (!player->powers[pw_ironfeet]
					|| (P_Random(pr_playerinspecialsector)<5))	// take damage even with suit
				{
					if (!(level.time&0x1f))
						P_DamageMobj (player->mo, NULL, NULL, 20, MOD_SLIME);
				}
				break;
		}

		// [RH] Apply any customizable damage
		if (sector->damage) {
			if (sector->damage < 20) {
				if (!player->powers[pw_ironfeet] && !(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, sector->damage, sector->mod);
			} else if (sector->damage < 50) {
				if ((!player->powers[pw_ironfeet] || (P_Random(pr_playerinspecialsector)<5))
					 && !(level.time&0x1f))
					P_DamageMobj (player->mo, NULL, NULL, sector->damage, sector->mod);
			} else {
				P_DamageMobj (player->mo, NULL, NULL, sector->damage, sector->mod);
			}
		}

		if (sector->special & SECRET_MASK) {
			player->secretcount++;
			level.found_secrets++;
			sector->special &= ~SECRET_MASK;
			if (player->mo == players[consoleplayer].camera) {
				C_MidPrint ("A secret is revealed!");
				S_Sound (player->mo, CHAN_AUTO, "misc/secret", 1, ATTN_NORM);
			}
		}
	}
}




//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
EXTERN_CVAR (timelimit)

void P_UpdateSpecials (void)
{
	anim_t *anim;
	int i;
	
	// LEVEL TIMER
	if (deathmatch.value && timelimit.value)
	{
		if (level.time >= (int)(timelimit.value * TICRATE * 60))
		{
			Printf (PRINT_HIGH, "Timelimit hit.\n");
			G_ExitLevel(0);
		}
	}
	
	// ANIMATE FLATS AND TEXTURES GLOBALLY
	// [RH] Changed significantly to work with ANIMDEFS lumps
	for (anim = anims; anim < lastanim; anim++)
	{
		if (--anim->countdown == 0)
		{
			int speedframe;

			anim->curframe = (anim->curframe + 1) % anim->numframes;
			
			speedframe = (anim->uniqueframes) ? anim->curframe : 0;

			if (anim->speedmin[speedframe] == anim->speedmax[speedframe])
				anim->countdown = anim->speedmin[speedframe];
			else
				anim->countdown = P_Random(pr_animatepictures) %
					(anim->speedmax[speedframe] - anim->speedmin[speedframe]) +
					anim->speedmin[speedframe];
		}

		if (anim->uniqueframes)
		{
			int pic = anim->framepic[anim->curframe];

			if (anim->istexture)
				for (i = 0; i < anim->numframes; i++)
					texturetranslation[anim->framepic[i]] = pic;
			else
				for (i = 0; i < anim->numframes; i++)
					flattranslation[anim->framepic[i]] = pic;
		}
		else
		{
			for (i = anim->basepic; i < anim->basepic + anim->numframes; i++)
			{
				int pic = anim->basepic + (anim->curframe + i) % anim->numframes;

				if (anim->istexture)
					texturetranslation[i] = pic;
				else
					flattranslation[i] = pic;
			}
		}
	}

	// [RH] Scroll the sky
	sky1pos = (sky1pos + level.skyspeed1) & 0xffffff;
	sky2pos = (sky2pos + level.skyspeed2) & 0xffffff;
}



//
// SPECIAL SPAWNING
//

BEGIN_CUSTOM_CVAR (forcewater, "0", CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL)
	{
		int i;
		byte set = var.value ? 2 : 0;

		for (i = 0; i < numsectors; i++)
		{
			if (sectors[i].heightsec && sectors[i].heightsec->waterzone != 1)
				sectors[i].heightsec->waterzone = set;
		}
	}
}
END_CUSTOM_CVAR (forcewater)

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
	for (i = 0; i < numsectors; i++, sector++)
	{
		if (!sector->special)
			continue;

		// [RH] All secret sectors are marked with a BOOM-ish bitfield
		if (sector->special & SECRET_MASK)
			level.total_secrets++;

		switch (sector->special & 0xff)
		{
			// [RH] Normal DOOM/Hexen specials. We clear off the special for lights
			//	  here instead of inside the spawners.

		case dLight_Flicker:
			// FLICKERING LIGHTS
			new DLightFlash (sector);
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFast:
			// STROBE FAST
			new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			sector->special &= 0xff00;
			break;
			
		case dLight_StrobeSlow:
			// STROBE SLOW
			new DStrobe (sector, STROBEBRIGHT, SLOWDARK, false);
			sector->special &= 0xff00;
			break;

		case dLight_Strobe_Hurt:
			// STROBE FAST/DEATH SLIME
			new DStrobe (sector, STROBEBRIGHT, FASTDARK, false);
			break;

		case dLight_Glow:
			// GLOWING LIGHT
			new DGlow (sector);
			sector->special &= 0xff00;
			break;
			
		case dSector_DoorCloseIn30:
			// DOOR CLOSE IN 30 SECONDS
			P_SpawnDoorCloseIn30 (sector);
			break;
			
		case dLight_StrobeSlowSync:
			// SYNC STROBE SLOW
			new DStrobe (sector, STROBEBRIGHT, SLOWDARK, true);
			sector->special &= 0xff00;
			break;

		case dLight_StrobeFastSync:
			// SYNC STROBE FAST
			new DStrobe (sector, STROBEBRIGHT, FASTDARK, true);
			sector->special &= 0xff00;
			break;

		case dSector_DoorRaiseIn5Mins:
			// DOOR RAISE IN 5 MINUTES
			P_SpawnDoorRaiseIn5Mins (sector);
			break;
			
		case dLight_FireFlicker:
			// fire flickering
			new DFireFlicker (sector);
			sector->special &= 0xff00;
			break;

		  // [RH] Hexen-like phased lighting
		case LightSequenceStart:
			new DPhased (sector);
			break;

		case Light_Phased:
			new DPhased (sector, 48, 63 - (sector->lightlevel & 63));
			break;

		case Sky2:
			sector->sky = PL_SKYFLAT;
			break;

		default:
			  // [RH] Try for normal Hexen scroller
			if ((sector->special & 0xff) >= Scroll_North_Slow &&
				(sector->special & 0xff) <= Scroll_SouthWest_Fast)
			{
				static char hexenScrollies[24][2] =
				{
					{  0,  1 }, {  0,  2 }, {  0,  4 },
					{ -1,  0 }, { -2,  0 }, { -4,  0 },
					{  0, -1 }, {  0, -2 }, {  0, -4 },
					{  1,  0 }, {  2,  0 }, {  4,  0 },
					{  1,  1 }, {  2,  2 }, {  4,  4 },
					{ -1,  1 }, { -2,  2 }, { -4,  4 },
					{ -1, -1 }, { -2, -2 }, { -4, -4 },
					{  1, -1 }, {  2, -2 }, {  4, -4 }
				};
				int i = (sector->special & 0xff) - Scroll_North_Slow;
				int dx = hexenScrollies[i][0] * (FRACUNIT/2);
				int dy = hexenScrollies[i][1] * (FRACUNIT/2);

				new DScroller (DScroller::sc_floor, dx, dy, -1, sector-sectors, 0);
				// Hexen scrolling floors cause the player to move
				// faster than they scroll. I do this just for compatibility
				// with Hexen and recommend using Killough's more-versatile
				// scrollers instead.
				dx = FixedMul (-dx, CARRYFACTOR*2);
				dy = FixedMul (dy, CARRYFACTOR*2);
				new DScroller (DScroller::sc_carry, dx, dy, -1, sector-sectors, 0);
				sector->special &= 0xff00;
			}
			break;
		}
	}
	
	// Init other misc stuff

	// P_InitTagLists() must be called before P_FindSectorFromTag()
	// or P_FindLineFromID() can be called.

	P_InitTagLists();   // killough 1/30/98: Create xref tables for tags
	P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
	P_SpawnFriction();	// phares 3/12/98: New friction model using linedefs
	P_SpawnPushers();	// phares 3/20/98: New pusher model using linedefs

	for (i=0; i<numlines; i++)
		switch (lines[i].special)
		{
			int s;
			sector_t *sec;

		// killough 3/7/98:
		// support for drawn heights coming from different sector
		case Transfer_Heights:
			sec = sides[*lines[i].sidenum].sector;
			for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
			{
				sectors[s].heightsec = sec;
				sectors[s].alwaysfake = !!lines[i].args[1];
				if (forcewater.value)
					sec->waterzone = 2;
			}
			break;

		// killough 3/16/98: Add support for setting
		// floor lighting independently (e.g. lava)
		case Transfer_FloorLight:
			sec = sides[*lines[i].sidenum].sector;
			for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
				sectors[s].floorlightsec = sec;
			break;

		// killough 4/11/98: Add support for setting
		// ceiling lighting independently
		case Transfer_CeilingLight:
			sec = sides[*lines[i].sidenum].sector;
			for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
				sectors[s].ceilinglightsec = sec;
			break;

		// [RH] ZDoom Static_Init settings
		case Static_Init:
			switch (lines[i].args[1])
			{
			case Init_Gravity:
				{
				float grav = ((float)P_AproxDistance (lines[i].dx, lines[i].dy)) / (FRACUNIT * 100.0f);
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].gravity = grav;
				}
				break;

			//case Init_Color:
			// handled in P_LoadSideDefs2()

			case Init_Damage:
				{
					int damage = P_AproxDistance (lines[i].dx, lines[i].dy) >> FRACBITS;
					for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					{
						sectors[s].damage = damage;
						sectors[s].mod = MOD_UNKNOWN;
					}
				}
				break;

			// killough 10/98:
			//
			// Support for sky textures being transferred from sidedefs.
			// Allows scrolling and other effects (but if scrolling is
			// used, then the same sector tag needs to be used for the
			// sky sector, the sky-transfer linedef, and the scroll-effect
			// linedef). Still requires user to use F_SKY1 for the floor
			// or ceiling texture, to distinguish floor and ceiling sky.

			case Init_TransferSky:
				for (s = -1; (s = P_FindSectorFromTag(lines[i].args[0],s)) >= 0;)
					sectors[s].sky = (i+1) | PL_SKYFLAT;
				break;
			}
			break;
		}

	// [RH] Start running any open scripts on this map
	P_StartOpenScripts ();
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

void DScroller::RunThink ()
{
	fixed_t dx = m_dx, dy = m_dy;

	if (m_Control != -1)
	{	// compute scroll amounts based on a sector's height changes
		fixed_t height = sectors[m_Control].floorheight +
						 sectors[m_Control].ceilingheight;
		fixed_t delta = height - m_LastHeight;
		m_LastHeight = height;
		dx = FixedMul(dx, delta);
		dy = FixedMul(dy, delta);
	}

	// killough 3/14/98: Add acceleration
	if (m_Accel)
	{
		m_vdx = dx += m_vdx;
		m_vdy = dy += m_vdy;
	}

	if (!(dx | dy))			// no-op if both (x,y) offsets 0
		return;

	switch (m_Type)
	{
		side_t *side;
		sector_t *sec;
		fixed_t height, waterheight;	// killough 4/4/98: add waterheight
		msecnode_t *node;
		AActor *thing;

		case sc_side:					// killough 3/7/98: Scroll wall texture
			side = sides + m_Affectee;
			side->textureoffset += dx;
			side->rowoffset += dy;
			break;

		case sc_floor:						// killough 3/7/98: Scroll floor texture
			sec = sectors + m_Affectee;
			sec->floor_xoffs += dx;
			sec->floor_yoffs += dy;
			break;

		case sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			sec = sectors + m_Affectee;
			sec->ceiling_xoffs += dx;
			sec->ceiling_yoffs += dy;
			break;

		case sc_carry:

			// killough 3/7/98: Carry things on floor
			// killough 3/20/98: use new sector list which reflects true members
			// killough 3/27/98: fix carrier bug
			// killough 4/4/98: Underwater, carry things even w/o gravity

			sec = sectors + m_Affectee;
			height = sec->floorheight;
			waterheight = sec->heightsec &&
				sec->heightsec->floorheight > height ?
				sec->heightsec->floorheight : MININT;

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

DScroller::DScroller (EScrollType type, fixed_t dx, fixed_t dy,
					  int control, int affectee, int accel)
{
	m_Type = type;
	m_dx = dx;
	m_dy = dy;
	m_Accel = accel;
	m_vdx = m_vdy = 0;
	if ((m_Control = control) != -1)
		m_LastHeight =
			sectors[control].floorheight + sectors[control].ceilingheight;
	m_Affectee = affectee;
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff

DScroller::DScroller (fixed_t dx, fixed_t dy, const line_t *l,
					 int control, int accel)
{
	fixed_t x = abs(l->dx), y = abs(l->dy), d;
	if (y > x)
		d = x, x = y, y = d;
	d = FixedDiv (x, finesine[(tantoangle[FixedDiv(y,x) >> DBITS] + ANG90)
						  >> ANGLETOFINESHIFT]);
	x = -FixedDiv (FixedMul(dy, l->dy) + FixedMul(dx, l->dx), d);
	y = -FixedDiv (FixedMul(dx, l->dy) - FixedMul(dy, l->dx), d);

	m_Type = sc_side;
	m_dx = x;
	m_dy = y;
	m_Accel = accel;
	if ((m_Control = control) != -1)
		m_LastHeight = sectors[control].floorheight + sectors[control].ceilingheight;
	m_Affectee = *l->sidenum;
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5

// Initialize the scrollers
static void P_SpawnScrollers(void)
{
	int i;
	line_t *l = lines;

	for (i = 0; i < numlines; i++, l++)
	{
		fixed_t dx;	// direction and speed of scrolling
		fixed_t dy;
		int control = -1, accel = 0;		// no control sector or acceleration
		int special = l->special;

		// killough 3/7/98: Types 245-249 are same as 250-254 except that the
		// first side's sector's heights cause scrolling when they change, and
		// this linedef controls the direction and speed of the scrolling. The
		// most complicated linedef since donuts, but powerful :)
		//
		// killough 3/15/98: Add acceleration. Types 214-218 are the same but
		// are accelerative.

		// [RH] Assume that it's a scroller and zero the line's special.
		l->special = 0;

		if (special == Scroll_Ceiling ||
			special == Scroll_Floor ||
			special == Scroll_Texture_Model) {
			if (l->args[1] & 3) {
				// if 1, then displacement
				// if 2, then accelerative (also if 3)
				control = sides[*l->sidenum].sector - sectors;
				if (l->args[1] & 2)
					accel = 1;
			}
			if (special == Scroll_Texture_Model ||
				l->args[1] & 4) {
				// The line housing the special controls the
				// direction and speed of scrolling.
				dx = l->dx >> SCROLL_SHIFT;
				dy = l->dy >> SCROLL_SHIFT;
			} else {
				// The speed and direction are parameters to the special.
				dx = (l->args[3] - 128) * (FRACUNIT / 32);
				dy = (l->args[4] - 128) * (FRACUNIT / 32);
			}
		}

		switch (special)
		{
			register int s;

			case Scroll_Ceiling:
				for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
					new DScroller (DScroller::sc_ceiling, -dx, dy, control, s, accel);
				break;

			case Scroll_Floor:
				if (l->args[2] != 1)
					// scroll the floor
					for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
						new DScroller (DScroller::sc_floor, -dx, dy, control, s, accel);

				if (l->args[2] > 0) {
					// carry objects on the floor
					dx = FixedMul(dx,CARRYFACTOR);
					dy = FixedMul(dy,CARRYFACTOR);
					for (s=-1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0;)
						new DScroller (DScroller::sc_carry, dx, dy, control, s, accel);
				}
				break;

			// killough 3/1/98: scroll wall according to linedef
			// (same direction and speed as scrolling floors)
			case Scroll_Texture_Model:
				for (s=-1; (s = P_FindLineFromID (l->args[0],s)) >= 0;)
					if (s != i)
						new DScroller (dx, dy, lines+s, control, accel);
				break;

			case Scroll_Texture_Offsets:
				// killough 3/2/98: scroll according to sidedef offsets
				s = lines[i].sidenum[0];
				new DScroller (DScroller::sc_side, -sides[s].textureoffset,
							   sides[s].rowoffset, -1, s, accel);
				break;

			case Scroll_Texture_Left:
				new DScroller (DScroller::sc_side, l->args[0] * (FRACUNIT/64), 0,
							   -1, lines[i].sidenum[0], accel);
				break;

			case Scroll_Texture_Right:
				new DScroller (DScroller::sc_side, l->args[0] * (-FRACUNIT/64), 0,
							   -1, lines[i].sidenum[0], accel);
				break;

			case Scroll_Texture_Up:
				new DScroller (DScroller::sc_side, 0, l->args[0] * (FRACUNIT/64),
							   -1, lines[i].sidenum[0], accel);
				break;

			case Scroll_Texture_Down:
				new DScroller (DScroller::sc_side, 0, l->args[0] * (-FRACUNIT/64),
							   -1, lines[i].sidenum[0], accel);
				break;

			case Scroll_Texture_Both:
				if (l->args[0] == 0) {
					dx = (l->args[1] - l->args[2]) * (FRACUNIT/64);
					dy = (l->args[4] - l->args[3]) * (FRACUNIT/64);
					new DScroller (DScroller::sc_side, dx, dy, -1, lines[i].sidenum[0], accel);
				}
				break;

			default:
				// [RH] It wasn't a scroller after all, so restore the special.
				l->special = special;
				break;
		}
	}
}

// killough 3/7/98 -- end generalized scroll effects

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects

// As the player moves, friction is applied by decreasing the x and y
// momentum values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.

//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
// [RH] On the other hand, since I've given up on trying to maintain demo
//		sync between versions, these considerations aren't a big deal to me.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

static void P_SpawnFriction(void)
{
	int i;
	line_t *l = lines;

	for (i = 0 ; i < numlines ; i++,l++)
	{
		if (l->special == Sector_SetFriction)
		{
			int length, s;
			fixed_t friction, movefactor;

			if (l->args[1])
			{	// [RH] Allow setting friction amount from parameter
				length = l->args[1] <= 200 ? l->args[1] : 200;
			}
			else
			{
				length = P_AproxDistance(l->dx,l->dy)>>FRACBITS;
			}
			friction = (0x1EB8*length)/0x80 + 0xD000;

			// killough 8/28/98: prevent odd situations
			if (friction > FRACUNIT)
				friction = FRACUNIT;
			if (friction < 0)
				friction = 0;

			// The following check might seem odd. At the time of movement,
			// the move distance is multiplied by 'friction/0x10000', so a
			// higher friction value actually means 'less friction'.

			// [RH] Twiddled these values so that momentum on ice (with
			//		friction 0xf900) is the same as in Heretic/Hexen.
			if (friction > ORIG_FRICTION)	// ice
//				movefactor = ((0x10092 - friction)*(0x70))/0x158;
				movefactor = ((0x10092 - friction) * 1024) / 4352 + 568;
			else
				movefactor = ((friction - 0xDB34)*(0xA))/0x80;

			// killough 8/28/98: prevent odd situations
			if (movefactor < 32)
				movefactor = 32;

			for (s = -1; (s = P_FindSectorFromTag(l->args[0],s)) >= 0 ; )
			{
				// killough 8/28/98:
				//
				// Instead of spawning thinkers, which are slow and expensive,
				// modify the sector's own friction values. Friction should be
				// a property of sectors, not objects which reside inside them.
				// Original code scanned every object in every friction sector
				// on every tic, adjusting its friction, putting unnecessary
				// drag on CPU. New code adjusts friction of sector only once
				// at level startup, and then uses this friction value.

				sectors[s].friction = friction;
				sectors[s].movefactor = movefactor;
			}
		}
	}
}

//
// phares 3/12/98: End of friction effects
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    MT_PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PUSH. The force is felt only if the point
//    MT_PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an MT_PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PULL. The force is felt only if the point
//    MT_PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the MT_PUSH/MT_PULL Thing.


#define PUSH_FACTOR 7

/////////////////////////////
//
// Add a push thinker to the thinker list

DPusher::DPusher (DPusher::EPusher type, line_t *l, int magnitude, int angle,
				  AActor *source, int affectee)
{
	m_Source = source;
	m_Type = type;
	if (l)
	{
		m_Xmag = l->dx>>FRACBITS;
		m_Ymag = l->dy>>FRACBITS;
		m_Magnitude = P_AproxDistance (m_Xmag, m_Ymag);
	}
	else
	{ // [RH] Allow setting magnitude and angle with parameters
		ChangeValues (magnitude, angle);
	}
	if (source) // point source exist?
	{
		m_Radius = (m_Magnitude) << (FRACBITS+1); // where force goes to zero
		m_X = m_Source->x;
		m_Y = m_Source->y;
	}
	m_Affectee = affectee;
}

/////////////////////////////
//
// PIT_PushThing determines the angle and magnitude of the effect.
// The object's x and y momentum values are changed.
//
// tmpusher belongs to the point source (MT_PUSH/MT_PULL).
//

DPusher *tmpusher; // pusher structure for blockmap searches

BOOL PIT_PushThing (AActor *thing)
{
	if (thing->player &&
		!(thing->flags & (MF_NOGRAVITY | MF_NOCLIP)))
	{
		int sx = tmpusher->m_X;
		int sy = tmpusher->m_Y;
		int dist = P_AproxDistance (thing->x - sx,thing->y - sy);
		int speed = (tmpusher->m_Magnitude -
					((dist>>FRACBITS)>>1))<<(FRACBITS-PUSH_FACTOR-1);

		// If speed <= 0, you're outside the effective radius. You also have
		// to be able to see the push/pull source point.

		if ((speed > 0) && (P_CheckSight (thing, tmpusher->m_Source, true)))
		{
			angle_t pushangle = R_PointToAngle2 (thing->x, thing->y, sx, sy);
			if (tmpusher->m_Source->type == MT_PUSH)
				pushangle += ANG180;    // away
			pushangle >>= ANGLETOFINESHIFT;
			thing->momx += FixedMul (speed, finecosine[pushangle]);
			thing->momy += FixedMul (speed, finesine[pushangle]);
		}
	}
	return true;
}

/////////////////////////////
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//
extern fixed_t tmbbox[4];

void DPusher::RunThink ()
{
	sector_t *sec;
	AActor *thing;
	msecnode_t *node;
	int xspeed,yspeed;
	int xl,xh,yl,yh,bx,by;
	int radius;
	int ht = 0;

	if (!var_pushers.value)
		return;

	sec = sectors + m_Affectee;

	// Be sure the special sector type is still turned on. If so, proceed.
	// Else, bail out; the sector type has been changed on us.

	if (!(sec->special & PUSH_MASK))
		return;

	// For constant pushers (wind/current) there are 3 situations:
	//
	// 1) Affected Thing is above the floor.
	//
	//    Apply the full force if wind, no force if current.
	//
	// 2) Affected Thing is on the ground.
	//
	//    Apply half force if wind, full force if current.
	//
	// 3) Affected Thing is below the ground (underwater effect).
	//
	//    Apply no force if wind, full force if current.
	//
	// Apply the effect to clipped players only for now.
	//
	// In Phase II, you can apply these effects to Things other than players.

	if (m_Type == p_push)
	{
		// Seek out all pushable things within the force radius of this
		// point pusher. Crosses sectors, so use blockmap.

		tmpusher = this; // MT_PUSH/MT_PULL point source
		radius = m_Radius; // where force goes to zero
		tmbbox[BOXTOP]    = m_Y + radius;
		tmbbox[BOXBOTTOM] = m_Y - radius;
		tmbbox[BOXRIGHT]  = m_X + radius;
		tmbbox[BOXLEFT]   = m_X - radius;

		xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
		xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
		yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
		yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;
		for (bx=xl ; bx<=xh ; bx++)
			for (by=yl ; by<=yh ; by++)
				P_BlockThingsIterator (bx, by, PIT_PushThing);
		return;
	}

	// constant pushers p_wind and p_current

	if (sec->heightsec) // special water sector?
		ht = sec->heightsec->floorheight;
	node = sec->touching_thinglist; // things touching this sector
	for ( ; node ; node = node->m_snext)
	{
		thing = node->m_thing;
		if (!thing->player || (thing->flags & (MF_NOGRAVITY | MF_NOCLIP)))
			continue;
		if (m_Type == p_wind)
		{
			if (sec->heightsec == NULL) // NOT special water sector
			{
				if (thing->z > thing->floorz) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else // on ground
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
			else // special water sector
			{
				if (thing->z > ht) // above ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
				else if (thing->player->viewz < ht) // underwater
					xspeed = yspeed = 0; // no force
				else // wading in water
				{
					xspeed = (m_Xmag)>>1; // half force
					yspeed = (m_Ymag)>>1;
				}
			}
		}
		else // p_current
		{
			if (sec->heightsec == NULL)
			{ // NOT special water sector
				if (thing->z > sec->floorheight) // above ground
					xspeed = yspeed = 0; // no force
				else // on ground
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
			}
			else
			{ // special water sector
				if (thing->z > ht) // above ground
					xspeed = yspeed = 0; // no force
				else // underwater
				{
					xspeed = m_Xmag; // full force
					yspeed = m_Ymag;
				}
			}
		}
		thing->momx += xspeed<<(FRACBITS-PUSH_FACTOR);
		thing->momy += yspeed<<(FRACBITS-PUSH_FACTOR);
	}
}

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

AActor *P_GetPushThing (int s)
{
	AActor* thing;
	sector_t* sec;

	sec = sectors + s;
	thing = sec->thinglist;
	while (thing)
	{
		switch (thing->type)
		{
		  case MT_PUSH:
		  case MT_PULL:
			return thing;
		  default:
			break;
		}
		thing = thing->snext;
	}
	return NULL;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

static void P_SpawnPushers(void)
{
	int i;
	line_t *l = lines;
	register int s;

	for (i = 0; i < numlines; i++, l++)
		switch (l->special)
		{
		  case Sector_SetWind: // wind
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_wind, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			break;
		  case Sector_SetCurrent: // current
			for (s = -1; (s = P_FindSectorFromTag (l->args[0],s)) >= 0 ; )
				new DPusher (DPusher::p_current, l->args[3] ? l : NULL, l->args[1], l->args[2], NULL, s);
			break;
		  case PointPush_SetForce: // push/pull
			if (l->args[0]) {	// [RH] Find thing by sector
				for (s = -1; (s = P_FindSectorFromTag (l->args[0], s)) >= 0 ; )
				{
					AActor *thing = P_GetPushThing (s);
					if (thing) {	// No MT_P* means no effect
						// [RH] Allow narrowing it down by tid
						if (!l->args[1] || l->args[1] == thing->tid)
							new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
										 0, thing, s);
					}
				}
			} else {	// [RH] Find thing by tid
				AActor *thing = NULL;

				while ( (thing = AActor::FindByTID (thing, l->args[1])) )
				{
					if (thing->type == MT_PUSH || thing->type == MT_PULL)
						new DPusher (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
									 0, thing, thing->subsector->sector - sectors);
				}
			}
			break;
		}
}

//
// phares 3/20/98: End of Pusher effects
//
////////////////////////////////////////////////////////////////////////////
