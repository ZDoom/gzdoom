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
//		Do all the WAD I/O, get map description,
//		set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------


#include <math.h>

#include "m_alloc.h"

#include "z_zone.h"

#include "m_swap.h"
#include "m_bbox.h"

#include "g_game.h"

#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "doomstat.h"


void	P_SpawnMapThing (mapthing2_t *mthing);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int 			numvertexes;
vertex_t*		vertexes;

int 			numsegs;
seg_t*			segs;

int 			numsectors;
sector_t*		sectors;

int 			numsubsectors;
subsector_t*	subsectors;

int 			numnodes;
node_t* 		nodes;

int 			numlines;
line_t* 		lines;

int 			numsides;
side_t* 		sides;

// [RH] Set true if the map contains a BEHAVIOR lump
static	BOOL	HasBehavior;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int 			bmapwidth;
int 			bmapheight; 	// size in mapblocks

int				*blockmap;		// int for larger maps ([RH] Made int because BOOM does)
int				*blockmaplump;	// offsets in blockmap are from here	

fixed_t 		bmaporgx;		// origin of block map
fixed_t 		bmaporgy;

mobj_t**		blocklinks;		// for thing chains
			


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//	LineOf Sight calculation.
// Without special effect, this could be
//	used as a PVS lookup as well.
//
byte*			rejectmatrix;


// Maintain single and multi player starting spots.
int				MaxDeathmatchStarts;
mapthing2_t		*deathmatchstarts;
mapthing2_t		*deathmatch_p;
mapthing2_t		playerstarts[MAXPLAYERS];





//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
	byte*				data;
	int 				i;
	mapvertex_t*		ml;
	vertex_t*			li;

	// Determine number of lumps:
	//	total lump length / vertex record length.
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

	// Allocate zone memory for buffer.
	vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);		

	// Load data into cache.
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	ml = (mapvertex_t *)data;
	li = vertexes;

	// Copy and convert vertex coordinates,
	// internal representation as fixed.
	for (i=0 ; i<numvertexes ; i++, li++, ml++)
	{
		li->x = SHORT(ml->x)<<FRACBITS;
		li->y = SHORT(ml->y)<<FRACBITS;
	}

	// Free buffer memory.
	Z_Free (data);
}



//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
	byte*				data;
	int 				i;
	mapseg_t*			ml;
	seg_t*				li;
	line_t* 			ldef;
	int 				linedef;
	int 				side;
		
	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
	segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0); 
	memset (segs, 0, numsegs*sizeof(seg_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	ml = (mapseg_t *)data;
	li = segs;
	for (i=0 ; i<numsegs ; i++, li++, ml++)
	{
		li->v1 = &vertexes[SHORT(ml->v1)];
		li->v2 = &vertexes[SHORT(ml->v2)];
										
		li->angle = (SHORT(ml->angle))<<16;
		li->offset = (SHORT(ml->offset))<<16;
		linedef = SHORT(ml->linedef);
		ldef = &lines[linedef];
		li->linedef = ldef;
		side = SHORT(ml->side);
		li->sidedef = &sides[ldef->sidenum[side]];
		li->frontsector = sides[ldef->sidenum[side]].sector;
		if (ldef-> flags & ML_TWOSIDED)
			li->backsector = sides[ldef->sidenum[side^1]].sector;
		else
			li->backsector = 0;
	}
		
	Z_Free (data);
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
	byte*				data;
	int 				i;
	mapsubsector_t* 	ms;
	subsector_t*		ss;
		
	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);		
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	ms = (mapsubsector_t *)data;
	memset (subsectors,0, numsubsectors*sizeof(subsector_t));
	ss = subsectors;
	
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		ss->numlines = SHORT(ms->numsegs);
		ss->firstline = SHORT(ms->firstseg);
	}
		
	Z_Free (data);
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
	byte*				data;
	int 				i;
	mapsector_t*		ms;
	sector_t*			ss;
		
	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);		
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	ms = (mapsector_t *)data;
	ss = sectors;
	for (i=0 ; i<numsectors ; i++, ss++, ms++)
	{
		ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = (short)R_FlatNumForName(ms->floorpic);
		ss->ceilingpic = (short)R_FlatNumForName(ms->ceilingpic);
		ss->lightlevel = SHORT(ms->lightlevel);
		ss->special = SHORT(ms->special);
		ss->tag = SHORT(ms->tag);
		ss->thinglist = NULL;
		ss->touching_thinglist = NULL;		// phares 3/14/98

		ss->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		ss->prevsec = -1;	// stair retriggering until build completes

		// killough 3/7/98:
		ss->floor_xoffs = 0;
		ss->floor_yoffs = 0;	// floor and ceiling flats offsets
		ss->ceiling_xoffs = 0;
		ss->ceiling_yoffs = 0;
		ss->heightsec = -1;		// sector used to get floor and ceiling height
		ss->floorlightsec = -1;	// sector used to get floor lighting
		// killough 3/7/98: end changes

		// killough 4/11/98 sector used to get ceiling lighting:
		ss->ceilinglightsec = -1;
	}
		
	Z_Free (data);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
	byte*		data;
	int 		i;
	int 		j;
	int 		k;
	mapnode_t*	mn;
	node_t* 	no;
		
	numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
	nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);		
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	mn = (mapnode_t *)data;
	no = nodes;
	
	for (i=0 ; i<numnodes ; i++, no++, mn++)
	{
		no->x = SHORT(mn->x)<<FRACBITS;
		no->y = SHORT(mn->y)<<FRACBITS;
		no->dx = SHORT(mn->dx)<<FRACBITS;
		no->dy = SHORT(mn->dy)<<FRACBITS;
		for (j=0 ; j<2 ; j++)
		{
			no->children[j] = SHORT(mn->children[j]);
			for (k=0 ; k<4 ; k++)
				no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
		}
	}
		
	Z_Free (data);
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
	mapthing2_t mt2;		// [RH] for translation
	byte *data = W_CacheLumpNum (lump,PU_STATIC);
	mapthing_t *mt = (mapthing_t *)data;
	mapthing_t *lastmt = (mapthing_t *)(data + W_LumpLength (lump));

	// [RH] I'm (considering) moving toward Hexen-style maps
	//		as the native format for ZDoom. This is the only
	//		place where Doom-style Things are ever referenced,
	//		and we translate them into a Hexen-style thing.
	//		Thanks to recent talks with Ty Halderman, I may
	//		not actually complete this, although the process
	//		has been started.
	memset (&mt2, 0, sizeof(mt2));

	for ( ; mt < lastmt; mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. R_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = SHORT(mt->options);
		mt2.flags = (short)((flags & 0xf) | 0x700);
		if (flags & BTF_NOTSINGLE)			mt2.flags &= ~MTF_SINGLE;
		if (flags & BTF_NOTDEATHMATCH)		mt2.flags &= ~MTF_DEATHMATCH;
		if (flags & BTF_NOTCOOPERATIVE)		mt2.flags &= ~MTF_COOPERATIVE;

		mt2.x = SHORT(mt->x);
		mt2.y = SHORT(mt->y);
		mt2.angle = SHORT(mt->angle);
		mt2.type = SHORT(mt->type);
		
		P_SpawnMapThing (&mt2);
	}
		
	Z_Free (data);
}

// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style.
//
void P_LoadThings2 (int lump)
{
	byte *data = W_CacheLumpNum (lump, PU_STATIC);
	mapthing2_t *mt = (mapthing2_t *)data;
	mapthing2_t *lastmt = (mapthing2_t *)(data + W_LumpLength (lump));

	for ( ; mt < lastmt; mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. R_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		mt->thingid = SHORT(mt->thingid);
		mt->x = SHORT(mt->x);
		mt->y = SHORT(mt->y);
		mt->z = SHORT(mt->z);
		mt->angle = SHORT(mt->angle);
		mt->type = SHORT(mt->type);
		mt->flags = SHORT(mt->flags);

		P_SpawnMapThing (mt);
	}

	Z_Free (data);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
static void P_AdjustLine (line_t *ld)
{
	vertex_t *v1, *v2;

	v1 = ld->v1;
	v2 = ld->v2;

	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (!ld->dx)
		ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
		ld->slopetype = ST_HORIZONTAL;
	else
	{
		if (FixedDiv (ld->dy , ld->dx) > 0)
			ld->slopetype = ST_POSITIVE;
		else
			ld->slopetype = ST_NEGATIVE;
	}
			
	if (v1->x < v2->x)
	{
		ld->bbox[BOXLEFT] = v1->x;
		ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
		ld->bbox[BOXLEFT] = v2->x;
		ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
		ld->bbox[BOXBOTTOM] = v1->y;
		ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
		ld->bbox[BOXBOTTOM] = v2->y;
		ld->bbox[BOXTOP] = v1->y;
	}

	if (ld->sidenum[0] != -1)
		ld->frontsector = sides[ld->sidenum[0]].sector;
	else
		ld->frontsector = 0;

	if (ld->sidenum[1] != -1)
		ld->backsector = sides[ld->sidenum[1]].sector;
	else
		ld->backsector = 0;
}

void P_LoadLineDefs (int lump)
{
	byte*				data;
	int 				i;
	maplinedef_t*		mld;
	line_t* 			ld;
		
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);		
	memset (lines, 0, numlines*sizeof(line_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	mld = (maplinedef_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		ld->flags = SHORT(mld->flags);
		// [RH] Remap ML_PASSUSE flag from BOOM.
		if (ld->flags & ML_PASSUSEORG)
			ld->flags = (ld->flags & ~ML_PASSUSEORG) | ML_PASSUSE;
		ld->special = SHORT(mld->special);
		ld->tag = SHORT(mld->tag);
		ld->v1 = &vertexes[SHORT(mld->v1)];
		ld->v2 = &vertexes[SHORT(mld->v2)];

		ld->sidenum[0] = SHORT(mld->sidenum[0]);
		ld->sidenum[1] = SHORT(mld->sidenum[1]);

		P_AdjustLine (ld);
	}
		
	Z_Free (data);
}

// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
void P_LoadLineDefs2 (int lump)
{
	byte*				data;
	int 				i;
	maplinedef2_t*		mld;
	line_t* 			ld;
		
	numlines = W_LumpLength (lump) / sizeof(maplinedef2_t);
	lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);		
	memset (lines, 0, numlines*sizeof(line_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	mld = (maplinedef2_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		ld->flags = SHORT(mld->flags);
		ld->special = mld->special;
		memcpy (ld->args, mld->args, 5);
		ld->tag = ld->args[0];
		ld->v1 = &vertexes[SHORT(mld->v1)];
		ld->v2 = &vertexes[SHORT(mld->v2)];

		ld->sidenum[0] = SHORT(mld->sidenum[0]);
		ld->sidenum[1] = SHORT(mld->sidenum[1]);

		P_AdjustLine (ld);
	}
		
	Z_Free (data);
}


//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
	byte*				data;
	int 				i;
	mapsidedef_t*		msd;
	side_t* 			sd;
		
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
	sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);		
	memset (sides, 0, numsides*sizeof(side_t));
	data = W_CacheLumpNum (lump,PU_STATIC);
		
	msd = (mapsidedef_t *)data;
	sd = sides;
	for (i=0 ; i<numsides ; i++, msd++, sd++)
	{
		sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
		sd->toptexture = (short)R_TextureNumForName(msd->toptexture);
		sd->bottomtexture = (short)R_TextureNumForName(msd->bottomtexture);
		sd->midtexture = (short)R_TextureNumForName(msd->midtexture);
		sd->sector = &sectors[SHORT(msd->sector)];
	}
		
	Z_Free (data);
}


//
// P_LoadBlockMap
//
// [RH] Took a look at BOOM's version and made some changes.
//
void P_LoadBlockMap (int lump)
{
	int i, count;
	short *wadblockmaplump;
	
	count = W_LumpLength (lump) >> 1;
	wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
	blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

	// killough 3/1/98: Expand wad blockmap into larger internal one,
	// by treating all offsets except -1 as unsigned and zero-extending
	// them. This potentially doubles the size of blockmaps allowed,
	// because Doom originally considered the offsets as always signed.

	blockmaplump[0] = SHORT(wadblockmaplump[0]);
	blockmaplump[1] = SHORT(wadblockmaplump[1]);
	blockmaplump[2] = (long)(SHORT(wadblockmaplump[2])) & 0xffff;
	blockmaplump[3] = (long)(SHORT(wadblockmaplump[3])) & 0xffff;

	for (i=4 ; i<count ; i++)
	{
		short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
		blockmaplump[i] = t == -1 ? -1l : (long) t & 0xffff;
	}

	Z_Free(wadblockmaplump);

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

	// clear out mobj chains
	count = sizeof(*blocklinks) * bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL, 0);
	memset (blocklinks, 0, count);
	blockmap = blockmaplump+4;
}



//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
	line_t**			linebuffer;
	int 				i;
	int 				j;
	int 				total;
	line_t* 			li;
	sector_t*			sector;
	subsector_t*		ss;
	seg_t*				seg;
	fixed_t 			bbox[4];
	int 				block;
		
	// look up sector number for each subsector
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		seg = &segs[ss->firstline];
		ss->sector = seg->sidedef->sector;
	}

	// count number of lines in each sector
	li = lines;
	total = 0;
	for (i=0 ; i<numlines ; i++, li++)
	{
		total++;
		li->frontsector->linecount++;

		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->linecount++;
			total++;
		}
	}
		
	// build line tables for each sector		
	linebuffer = Z_Malloc (total*4, PU_LEVEL, 0);
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		M_ClearBox (bbox);
		sector->lines = linebuffer;
		li = lines;
		for (j=0 ; j<numlines ; j++, li++)
		{
			if (li->frontsector == sector || li->backsector == sector)
			{
				*linebuffer++ = li;
				M_AddToBox (bbox, li->v1->x, li->v1->y);
				M_AddToBox (bbox, li->v2->x, li->v2->y);
			}
		}
		if (linebuffer - sector->lines != sector->linecount)
			I_Error ("P_GroupLines: miscounted");
						
		// set the degenmobj_t to the middle of the bounding box
		sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
		sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
				
		// adjust bounding box to map blocks
		block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapheight ? bmapheight-1 : block;
		sector->blockbox[BOXTOP]=block;

		block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXBOTTOM]=block;

		block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapwidth ? bmapwidth-1 : block;
		sector->blockbox[BOXRIGHT]=block;

		block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXLEFT]=block;
	}
		
}


//
// P_SetupLevel
//
void P_SetupLevel (char *lumpname)
{
	int		i;
	int 	lumpnum;
		
	level.total_monsters = level.total_items = level.total_secrets =
		level.killed_monsters = level.found_items = level.found_secrets =
		wminfo.maxfrags = 0;
	wminfo.partime = 180;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		players[i].killcount = players[i].secretcount 
			= players[i].itemcount = 0;
	}

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer].viewz = 1; 

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start (); 				
	
#if 0 // UNUSED
	if (debugfile)
	{
		Z_FreeTags (PU_LEVEL, MAXINT);
		Z_FileDumpHeap (debugfile);
	}
	else
#endif
	Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

	// UNUSED W_Profile ();
	P_InitThinkers ();

	// if working with a devlopment map, reload it
	// [RH] Not present
	// W_Reload ();
		   
	// find map num
	lumpnum = W_GetNumForName (lumpname);

	// [RH] Check if this map is Hexen-style.
	//		LINEDEFS and THINGS need to be handled accordingly.
	HasBehavior = W_CheckLumpName (lumpnum+ML_BEHAVIOR, "BEHAVIOR");

	// note: most of this ordering is important 
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);

	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSectors (lumpnum+ML_SECTORS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);

	if (!HasBehavior)
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	else
		P_LoadLineDefs2 (lumpnum+ML_LINEDEFS);	// [RH] Load Hexen-style linedefs
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadSegs (lumpnum+ML_SEGS);
		
	rejectmatrix = W_CacheLumpNum (lumpnum+ML_REJECT,PU_LEVEL);
	P_GroupLines ();

	bodyqueslot = 0;
	if (!deathmatchstarts) {
		MaxDeathmatchStarts = 10;	// [RH] Default. Increased as needed.
		deathmatchstarts = Malloc (MaxDeathmatchStarts * sizeof(mapthing2_t));
	}
	deathmatch_p = deathmatchstarts;

	if (!HasBehavior)
		P_LoadThings (lumpnum+ML_THINGS);
	else
		P_LoadThings2 (lumpnum+ML_THINGS);	// [RH] Load Hexen-style things
	
	// if deathmatch, randomly spawn the active players
	if (deathmatch->value)
	{
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (playeringame[i])
			{
				players[i].mo = NULL;
				G_DeathMatchSpawnPlayer (i);
			}
						
	}

	// clear special respawning que
	iquehead = iquetail = 0;			
		
	// set up world state
	P_SpawnSpecials ();
		
	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	// preload graphics
	if (precache)
		R_PrecacheLevel ();

	//Printf ("free memory: 0x%x\n", Z_FreeMemory());
		
	level.time = 0;
}



//
// P_Init
//
void P_Init (void)
{
	P_InitSwitchList ();
	P_InitPicAnims ();
	R_InitSprites (sprnames);
}



