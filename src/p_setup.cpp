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

#include "templates.h"
#include "m_alloc.h"
#include "m_argv.h"
#include "z_zone.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "g_game.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "s_sound.h"
#include "doomstat.h"
#include "p_lnspec.h"
#include "v_palette.h"
#include "c_console.h"
#include "p_acs.h"
#include "vectors.h"
#include "announcer.h"

extern void P_SpawnMapThing (mapthing2_t *mthing, int position);

extern void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
extern void P_TranslateTeleportThings (void);
extern int	P_TranslateSectorSpecial (int);

extern unsigned int R_OldBlend;

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

int sidecount;
struct sidei_t	// [RH] Only keep BOOM sidedef init stuff around for init
{
	union
	{
		// Used when unpacking sidedefs and assigning
		// properties based on linedefs.
		struct
		{
			short tag, special, map;
		};

		// Used when grouping sidedefs into loops.
		struct
		{
			short first, next;
			char lineside;
		};
	};
}				*sidetemp;

// [RH] Set true if the map contains a BEHAVIOR lump
BOOL			HasBehavior;


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

AActor**		blocklinks;		// for thing chains
			


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//	LineOf Sight calculation.
// Without special effect, this could be
//	used as a PVS lookup as well.
//
byte*			rejectmatrix;
BOOL			rejectempty;


// Maintain single and multi player starting spots.
TArray<mapthing2_t> deathmatchstarts (16);
mapthing2_t		playerstarts[MAXPLAYERS];

static void P_AllocateSideDefs (int count);
static void P_SetSideNum (short *sidenum_p, short sidenum);

// [RH] Figure out blends for deep water sectors
static void SetTexture (short *texture, DWORD *blend, char *name)
{
	if ((*blend = R_ColormapNumForName (name)) == 0)
	{
		if ((*texture = R_CheckTextureNumForName (name)) == -1)
		{
			char name2[9];
			char *stop;
			strncpy (name2, name, 8);
			name2[8] = 0;
			*blend = strtoul (name2, &stop, 16);
			*texture = 0;
		}
		else
		{
			*blend = 0;
		}
	}
	else
	{
		*texture = 0;
	}
}

static void SetTextureNoErr (short *texture, DWORD *color, char *name)
{
	if ((*texture = R_CheckTextureNumForName (name)) == -1)
	{
		char name2[9];
		char *stop;
		strncpy (name2, name, 8);
		name2[8] = 0;
		*color = strtoul (name2, &stop, 16);
		*texture = 0;
	}
}

//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
	byte *data;
	int i;

	// Determine number of vertices:
	//	total lump length / vertex record length.
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

	// Allocate zone memory for buffer.
	vertexes = (vertex_t *)Z_Malloc (numvertexes*sizeof(vertex_t), PU_LEVEL, 0);		

	// Load data into cache.
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);
		
	// Copy and convert vertex coordinates,
	// internal representation as fixed.
	for (i = 0; i < numvertexes; i++)
	{
		vertexes[i].x = SHORT(((mapvertex_t *)data)[i].x)<<FRACBITS;
		vertexes[i].y = SHORT(((mapvertex_t *)data)[i].y)<<FRACBITS;
	}

	// Free buffer memory.
	Z_Free (data);
}



//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadSegs (int lump)
{
	int  i;
	byte *data;
	byte *vertchanged = (byte *)Z_Malloc (numvertexes,PU_LEVEL,0);	// phares 10/4/98
	line_t* line;		// phares 10/4/98
	int ptp_angle;		// phares 10/4/98
	int delta_angle;	// phares 10/4/98
	int dis;			// phares 10/4/98
	int dx,dy;			// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
	segs = (seg_t *)Z_Malloc (numsegs*sizeof(seg_t), PU_LEVEL, 0);
	memset (segs, 0, numsegs*sizeof(seg_t));
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);

	// phares: 10/4/98: Vertchanged is an array that represents the vertices.
	// Mark those used by linedefs. A marked vertex is one that is not a
	// candidate for movement further down.

	line = lines;
	for (i = 0; i < numlines ; i++, line++)
	{
		vertchanged[line->v1 - vertexes] = vertchanged[line->v2 - vertexes] = 1;
	}

	for (i = 0; i < numsegs; i++)
	{
		seg_t *li = segs+i;
		mapseg_t *ml = (mapseg_t *) data + i;

		int side, linedef;
		line_t *ldef;

		li->v1 = &vertexes[SHORT(ml->v1)];
		li->v2 = &vertexes[SHORT(ml->v2)];

		li->angle = (SHORT(ml->angle))<<16;

// phares 10/4/98: In the case of a lineseg that was created by splitting
// another line, it appears that the line angle is inherited from the
// father line. Due to roundoff, the new vertex may have been placed 'off
// the line'. When you get close to such a line, and it is very short,
// it's possible that the roundoff error causes 'firelines', the thin
// lines that can draw from screen top to screen bottom occasionally. This
// is due to all the angle calculations that are done based on the line
// angle, the angles from the viewer to the vertices, and the viewer's
// angle in the world. In the case of firelines, the rounded-off position
// of one of the vertices determines one of these angles, and introduces
// an error in the scaling factor for mapping textures and determining
// where on the screen the ceiling and floor spans should be shown. For a
// fireline, the engine thinks the ceiling bottom and floor top are at the
// midpoint of the screen. So you get ceilings drawn all the way down to the
// screen midpoint, and floors drawn all the way up. Thus 'firelines'. The
// name comes from the original sighting, which involved a fire texture.
//
// To correct this, reset the vertex that was added so that it sits ON the
// split line.
//
// To know which of the two vertices was added, its number is greater than
// that of the last of the author-created vertices. If both vertices of the
// line were added by splitting, pick the higher-numbered one. Once you've
// changed a vertex, don't change it again if it shows up in another seg.
//
// To determine if there's an error in the first place, find the
// angle of the line between the two seg vertices. If it's one degree or more
// off, then move one vertex. This may seem insignificant, but one degree
// errors _can_ cause firelines.

		ptp_angle = R_PointToAngle2(li->v1->x,li->v1->y,li->v2->x,li->v2->y);
		dis = 0;
		delta_angle = (abs(ptp_angle-li->angle)>>ANGLETOFINESHIFT)*360/8192;

		if (delta_angle != 0)
		{
			dx = (li->v1->x - li->v2->x)>>FRACBITS;
			dy = (li->v1->y - li->v2->y)>>FRACBITS;
			dis = ((int) sqrt(dx*dx + dy*dy))<<FRACBITS;
			dx = finecosine[li->angle>>ANGLETOFINESHIFT];
			dy = finesine[li->angle>>ANGLETOFINESHIFT];
			vnum1 = li->v1 - vertexes; 
			vnum2 = li->v2 - vertexes; 
			if ((vnum2 > vnum1) && (vertchanged[vnum2] == 0))
			{
				li->v2->x = li->v1->x + FixedMul(dis,dx);
				li->v2->y = li->v1->y + FixedMul(dis,dy);
				vertchanged[vnum2] = 1; // this was changed
			}
			else if (vertchanged[vnum1] == 0)
			{
				li->v1->x = li->v2->x - FixedMul(dis,dx);
				li->v1->y = li->v2->y - FixedMul(dis,dy);
				vertchanged[vnum1] = 1; // this was changed
			}
		}

		linedef = SHORT(ml->linedef);
		ldef = &lines[linedef];
		li->linedef = ldef;
		side = SHORT(ml->side);
		li->sidedef = &sides[ldef->sidenum[side]];
		li->frontsector = sides[ldef->sidenum[side]].sector;

		// killough 5/3/98: ignore 2s flag if second sidedef missing:
		if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=-1)
		{
			li->backsector = sides[ldef->sidenum[side^1]].sector;
		}
		else
		{
			li->backsector = 0;
			ldef->flags &= ~ML_TWOSIDED;
		}
	}

	Z_Free (data);
	Z_Free(vertchanged); // phares 10/4/98
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
	byte *data;
	int i;
		
	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = (subsector_t *)Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);		
	data = (byte *)W_CacheLumpNum (lump,PU_STATIC);
		
	memset (subsectors, 0, numsubsectors*sizeof(subsector_t));
	
	for (i = 0; i < numsubsectors; i++)
	{
		subsectors[i].numlines = SHORT(((mapsubsector_t *)data)[i].numsegs);
		subsectors[i].firstline = SHORT(((mapsubsector_t *)data)[i].firstseg);
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
	int					defSeqType;
	FDynamicColormap	*fogMap, *normMap;

	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = (sector_t *)Z_Malloc (numsectors*sizeof(sector_t), PU_LEVEL, 0);		
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);

	if (level.flags & LEVEL_SNDSEQTOTALCTRL)
		defSeqType = 0;
	else
		defSeqType = -1;

	fogMap = normMap = NULL;

	ms = (mapsector_t *)data;
	ss = sectors;
	for (i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->floortexz = SHORT(ms->floorheight)<<FRACBITS;
		ss->floorplane.d = -ss->floortexz;
		ss->floorplane.c = FRACUNIT;
		ss->floorplane.ic = FRACUNIT;
		ss->ceilingtexz = SHORT(ms->ceilingheight)<<FRACBITS;
		ss->ceilingplane.d = ss->ceilingtexz;
		ss->ceilingplane.c = -FRACUNIT;
		ss->ceilingplane.ic = -FRACUNIT;
		ss->floorpic = (short)R_FlatNumForName(ms->floorpic);
		ss->ceilingpic = (short)R_FlatNumForName(ms->ceilingpic);
		ss->lightlevel = clamp (SHORT(ms->lightlevel), (short)0, (short)255);
		if (HasBehavior)
			ss->special = SHORT(ms->special);
		else	// [RH] Translate to new sector special
			ss->special = P_TranslateSectorSpecial (SHORT(ms->special));
		ss->tag = SHORT(ms->tag);
		ss->thinglist = NULL;
		ss->touching_thinglist = NULL;		// phares 3/14/98
		ss->seqType = defSeqType;
		ss->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		ss->prevsec = -1;	// stair retriggering until build completes

		// killough 3/7/98:
		ss->floor_xoffs = 0;
		ss->floor_yoffs = 0;	// floor and ceiling flats offsets
		ss->ceiling_xoffs = 0;
		ss->ceiling_yoffs = 0;

		ss->floor_xscale = FRACUNIT;	// [RH] floor and ceiling scaling
		ss->floor_yscale = FRACUNIT;
		ss->ceiling_xscale = FRACUNIT;
		ss->ceiling_yscale = FRACUNIT;

		ss->floor_angle = 0;	// [RH] floor and ceiling rotation
		ss->ceiling_angle = 0;

		ss->base_ceiling_angle = ss->base_ceiling_yoffs =
			ss->base_floor_angle = ss->base_floor_yoffs = 0;

		ss->heightsec = NULL;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		ss->FloorFlags = ss->CeilingFlags = 0;
		ss->FloorLight = ss->CeilingLight = 0;

		ss->gravity = 1.0f;	// [RH] Default sector gravity of 1.0

		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		if (level.outsidefog != 0xff000000 && ss->ceilingpic == skyflatnum)
		{
			if (fogMap == NULL)
				fogMap = GetSpecialLights (PalEntry (255,255,255), level.outsidefog);
			ss->ceilingcolormap = ss->floorcolormap = fogMap;
		}
		else
		{
			if (normMap == NULL)
				normMap = GetSpecialLights (PalEntry (255,255,255), level.fadeto);
			ss->ceilingcolormap = ss->floorcolormap = normMap;
		}

		ss->sky = 0;

		// killough 8/28/98: initialize all sectors to normal friction
		ss->friction = ORIG_FRICTION;
		ss->movefactor = ORIG_FRICTION_FACTOR;
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
	nodes = (node_t *)Z_Malloc (numnodes*sizeof(node_t), PU_LEVEL, 0);		
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);
		
	mn = (mapnode_t *)data;
	no = nodes;
	
	for (i = 0; i < numnodes; i++, no++, mn++)
	{
		no->x = SHORT(mn->x)<<FRACBITS;
		no->y = SHORT(mn->y)<<FRACBITS;
		no->dx = SHORT(mn->dx)<<FRACBITS;
		no->dy = SHORT(mn->dy)<<FRACBITS;
		for (j = 0; j < 2; j++)
		{
			no->children[j] = SHORT(mn->children[j]);
			for (k = 0; k < 4; k++)
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
	byte *data = (byte *)W_CacheLumpNum (lump, PU_STATIC);
	mapthing_t *mt = (mapthing_t *)data;
	mapthing_t *lastmt = (mapthing_t *)(data + W_LumpLength (lump));

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	memset (&mt2, 0, sizeof(mt2));

	for ( ; mt < lastmt; mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. R_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = SHORT(mt->options);
		mt2.flags = (short)((flags & 0xf) | 0x7e0);
		if (flags & BTF_NOTSINGLE)			mt2.flags &= ~MTF_SINGLE;
		if (flags & BTF_NOTDEATHMATCH)		mt2.flags &= ~MTF_DEATHMATCH;
		if (flags & BTF_NOTCOOPERATIVE)		mt2.flags &= ~MTF_COOPERATIVE;

		mt2.x = SHORT(mt->x);
		mt2.y = SHORT(mt->y);
		mt2.angle = SHORT(mt->angle);
		mt2.type = SHORT(mt->type);

		P_SpawnMapThing (&mt2, 0);
	}
		
	Z_Free (data);
}

// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style. Position also controls which single-
// player start spots are spawned by filtering out those
// whose first parameter don't match position.
//
void P_LoadThings2 (int lump, int position)
{
	byte *data = (byte *)W_CacheLumpNum (lump, PU_STATIC);
	mapthing2_t *mt = (mapthing2_t *)data;
	mapthing2_t *lastmt = (mapthing2_t *)(data + W_LumpLength (lump));

	for ( ; mt < lastmt; mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. P_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		mt->thingid = SHORT(mt->thingid);
		mt->x = SHORT(mt->x);
		mt->y = SHORT(mt->y);
		mt->z = SHORT(mt->z);
		mt->angle = SHORT(mt->angle);
		mt->type = SHORT(mt->type);
		mt->flags = SHORT(mt->flags);

		P_SpawnMapThing (mt, position);
	}

	Z_Free (data);
}


//
// P_LoadLineDefs
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// [RH] Actually split into four functions to allow for Hexen and Doom
//		linedefs.
void P_AdjustLine (line_t *ld)
{
	vertex_t *v1, *v2;

	ld->alpha = 255;	// [RH] Opaque by default

	v1 = ld->v1;
	v2 = ld->v2;

	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (ld->dx == 0)
		ld->slopetype = ST_VERTICAL;
	else if (ld->dy == 0)
		ld->slopetype = ST_HORIZONTAL;
	else
		ld->slopetype = (FixedDiv (ld->dy , ld->dx) > 0) ? ST_POSITIVE : ST_NEGATIVE;
			
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

	// [RH] Set line id (as appropriate) here
	if (ld->special == Line_SetIdentification ||
		ld->special == Teleport_Line ||
		ld->special == TranslucentLine ||
		ld->special == Scroll_Texture_Model)
	{
		ld->id = ld->args[0];
	}
	// killough 4/4/98: support special sidedef interpretation below
	if ((ld->sidenum[0] != -1) &&
		// [RH] Save Static_Init only if it's interested in the textures
		(ld->special != Static_Init || ld->args[1] == Init_Color))
	{
		sidetemp[*ld->sidenum].special = ld->special;
		sidetemp[*ld->sidenum].tag = ld->args[0];
	}
	else
	{
		sidetemp[*ld->sidenum].special = 0;
	}
}

// killough 4/4/98: delay using sidedefs until they are loaded
void P_FinishLoadingLineDefs ()
{
	WORD len;
	int i, linenum;
	register line_t *ld = lines;

	for (i = numlines, linenum = 0; i--; ld++, linenum++)
	{
		ld->frontsector = ld->sidenum[0]!=-1 ? sides[ld->sidenum[0]].sector : 0;
		ld->backsector  = ld->sidenum[1]!=-1 ? sides[ld->sidenum[1]].sector : 0;
		float dx = FIXED2FLOAT(ld->v2->x - ld->v1->x);
		float dy = FIXED2FLOAT(ld->v2->y - ld->v1->y);
		SBYTE light;

		if (ld->frontsector == NULL)
		{
			Printf (PRINT_HIGH, "Line %d has no front sector\n", linenum);
		}

		// [RH] Set some new sidedef properties
		len = (int)sqrtf (dx*dx + dy*dy);
		light = dy == 0 ? level.WallHorizLight :
				dx == 0 ? level.WallVertLight : 0;

		if (ld->sidenum[0] != -1)
		{
			sides[ld->sidenum[0]].linenum = linenum;
			sides[ld->sidenum[0]].TexelLength = len;
			sides[ld->sidenum[0]].Light = light;
		}
		if (ld->sidenum[1] != -1)
		{
			sides[ld->sidenum[1]].linenum = linenum;
			sides[ld->sidenum[1]].TexelLength = len;
			sides[ld->sidenum[1]].Light = light;
		}

		switch (ld->special)
		{						// killough 4/11/98: handle special types
			int j;

		case TranslucentLine:			// killough 4/11/98: translucent 2s textures
			// [RH] Second arg controls how opaque it is.
			if (!ld->args[0])
				ld->alpha = (byte)ld->args[1];
			else
				for (j = 0; j < numlines; j++)
					if (lines[j].id == ld->args[0])
						lines[j].alpha = (byte)ld->args[1];
			break;
		}
	}
}

void P_LoadLineDefs (int lump)
{
	byte *data;
	int i;
	line_t *ld;
		
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = (line_t *)Z_Malloc (numlines*sizeof(line_t), PU_LEVEL, 0);		
	memset (lines, 0, numlines*sizeof(line_t));
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);

	// [RH] Count the number of sidedef references. This is the number of
	// sidedefs we need. The actual number in the SIDEDEFS lump might be less.
	for (i = sidecount = 0; i < numlines; i++)
	{
		maplinedef_t *mld = ((maplinedef_t *)data) + i;
		if (SHORT(mld->sidenum[0]) != -1)
			sidecount++;
		if (SHORT(mld->sidenum[1]) != -1)
			sidecount++;
	}
	P_AllocateSideDefs (sidecount);

	ld = lines;
	for (i = 0; i < numlines; i++, ld++)
	{
		maplinedef_t *mld = ((maplinedef_t *)data) + i;

		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.
		P_TranslateLineDef (ld, mld);

		ld->v1 = &vertexes[SHORT(mld->v1)];
		ld->v2 = &vertexes[SHORT(mld->v2)];

		P_SetSideNum (&ld->sidenum[0], mld->sidenum[0]);
		P_SetSideNum (&ld->sidenum[1], mld->sidenum[1]);

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
	lines = (line_t *)Z_Malloc (numlines*sizeof(line_t), PU_LEVEL,0 );
	memset (lines, 0, numlines*sizeof(line_t));
	data = (byte *)W_CacheLumpNum (lump, PU_STATIC);

	for (i = sidecount = 0; i < numlines; i++)
	{
		maplinedef2_t *mld = ((maplinedef2_t *)data) + i;
		if (SHORT(mld->sidenum[0]) != -1)
			sidecount++;
		if (SHORT(mld->sidenum[1]) != -1)
			sidecount++;
	}
	P_AllocateSideDefs (sidecount);

	mld = (maplinedef2_t *)data;
	ld = lines;
	for (i = numlines; i > 0; i--, mld++, ld++)
	{
		int j;

		for (j = 0; j < 5; j++)
			ld->args[j] = mld->args[j];

		ld->flags = SHORT(mld->flags);
		ld->special = mld->special;

		ld->v1 = &vertexes[SHORT(mld->v1)];
		ld->v2 = &vertexes[SHORT(mld->v2)];

		P_SetSideNum (&ld->sidenum[0], mld->sidenum[0]);
		P_SetSideNum (&ld->sidenum[1], SHORT(mld->sidenum[1]));

		P_AdjustLine (ld);
	}
		
	Z_Free (data);
}


//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions
void P_LoadSideDefs (int lump)
{
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
}

static void P_AllocateSideDefs (int count)
{
	int i;

	sides = (side_t *)Z_Malloc (count*sizeof(side_t), PU_LEVEL, 0);
	memset (sides, 0, count*sizeof(side_t));

	sidetemp = (sidei_t *)Z_Malloc (MAX(count,numvertexes)
		*sizeof(sidei_t), PU_LEVEL, 0);
	for (i = 0; i < count; i++)
	{
		sidetemp[i].special = sidetemp[i].tag = 0;
		sidetemp[i].map = -1;
	}
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}

static void P_SetSideNum (short *sidenum_p, short sidenum)
{
	sidenum = SHORT(sidenum);
	if (sidenum == -1)
	{
		*sidenum_p = sidenum;
	}
	else if (sidecount < numsides)
	{
		sidetemp[sidecount].map = sidenum;
		*sidenum_p = sidecount++;
	}
	else
	{
		I_Error ("%d sidedefs is not enough\n", sidecount);
	}
}

// [RH] Group sidedefs into loops so that we can easily determine
// what walls any particular wall neighbors.

static void P_LoopSidedefs ()
{
	int i;

	for (i = 0; i < numvertexes; ++i)
	{
		sidetemp[i].first = -1;
		sidetemp[i].next = -1;
	}
	for (; i < numsides; ++i)
	{
		sidetemp[i].next = -1;
	}

	for (i = 0; i < numsides; ++i)
	{
		// For each vertex, build a list of sidedefs that use that vertex
		// as their left edge.
		line_t *line = &lines[sides[i].linenum];
		int lineside = (line->sidenum[0] != i);
		int vert = (lineside ? line->v2 : line->v1) - vertexes;
		
		sidetemp[i].lineside = lineside;
		sidetemp[i].next = sidetemp[vert].first;
		sidetemp[vert].first = i;

		// Set each side so that it is the only member of its loop
		sides[i].LeftSide = -1;
		sides[i].RightSide = -1;
	}

	// For each side, find the side that is to its right and set the
	// loop pointers accordingly. If two sides share a left vertex, the
	// one that forms the smallest angle is assumed to be the right one.
	for (i = 0; i < numsides; ++i)
	{
		int right;
		line_t *line = &lines[sides[i].linenum];

		// If the side's line only exists in a single sector,
		// then consider that line to be a self-contained loop
		// instead of as part of another loop
		if (line->frontsector == line->backsector)
		{
			right = line->sidenum[!sidetemp[i].lineside];
		}
		else
		{
			if (sidetemp[i].lineside)
			{
				right = line->v1 - vertexes;
			}
			else
			{
				right = line->v2 - vertexes;
			}

			right = sidetemp[right].first;

			if (sidetemp[right].next != -1)
			{
				int bestright;
				angle_t bestang = ANGLE_MAX;
				line_t *leftline, *rightline;
				angle_t ang1, ang2, ang;

				leftline = &lines[sides[i].linenum];
				ang1 = R_PointToAngle (leftline->dx, leftline->dy);
				if (!sidetemp[i].lineside)
				{
					ang1 += ANGLE_180;
				}

				while (right != -1)
				{
					if (sides[right].LeftSide == -1)
					{
						rightline = &lines[sides[right].linenum];
						if (rightline->frontsector != rightline->backsector)
						{
							ang2 = R_PointToAngle (rightline->dx, rightline->dy);
							if (sidetemp[right].lineside)
							{
								ang2 += ANGLE_180;
							}

							ang = ang2 - ang1;

							if (ang != 0 && ang <= bestang)
							{
								bestright = right;
								bestang = ang;
							}
						}
					}
					right = sidetemp[right].next;
				}
				right = bestright;
			}
		}
		sides[i].RightSide = right;
		sides[right].LeftSide = i;
	}

	// Throw away sidedef init info now that we're done with it
	Z_Free (sidetemp);
	sidetemp = NULL;
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2 (int lump)
{
	byte *data = (byte *)W_CacheLumpNum (lump, PU_STATIC);
	int  i;

	for (i = 0; i < numsides; i++)
	{
		register mapsidedef_t *msd = (mapsidedef_t *)data + sidetemp[i].map;
		register side_t *sd = sides + i;
		register sector_t *sec;

		sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
		sd->linenum = -1;

		// killough 4/4/98: allow sidedef texture names to be overloaded
		// killough 4/11/98: refined to allow colormaps to work as wall
		// textures if invalid as colormaps but valid as textures.

		if ((unsigned)SHORT(msd->sector)>=numsectors)
		{
			Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
			sd->sector = NULL;
		}
		else
		{
			sd->sector = sec = &sectors[SHORT(msd->sector)];
		}
		switch (sidetemp[i].special)
		{
		case Transfer_Heights:	// variable colormap via 242 linedef
			  // [RH] The colormap num we get here isn't really a colormap,
			  //	  but a packed ARGB word for blending, so we also allow
			  //	  the blend to be specified directly by the texture names
			  //	  instead of figuring something out from the colormap.
			SetTexture (&sd->bottomtexture, &sec->bottommap, msd->bottomtexture);
			SetTexture (&sd->midtexture, &sec->midmap, msd->midtexture);
			SetTexture (&sd->toptexture, &sec->topmap, msd->toptexture);
			break;

		case Static_Init:
			// [RH] Set sector color and fog
			// upper "texture" is light color
			// lower "texture" is fog color
			{
				DWORD color = 0xffffff, fog = 0x000000;

				SetTextureNoErr (&sd->bottomtexture, &fog, msd->bottomtexture);
				SetTextureNoErr (&sd->toptexture, &color, msd->toptexture);
				sd->midtexture = R_TextureNumForName (msd->midtexture);

				if (fog != 0x000000 || color != 0xffffff)
				{
					int s;
					FDynamicColormap *colormap = GetSpecialLights (color, fog);

					for (s = 0; s < numsectors; s++)
					{
						if (sectors[s].tag == sidetemp[i].tag)
						{
							sectors[s].ceilingcolormap =
								sectors[s].floorcolormap = colormap;
						}
					}
				}
			}
			break;

/*
		  case TranslucentLine:	// killough 4/11/98: apply translucency to 2s normal texture
			sd->midtexture = strncasecmp("TRANMAP", msd->midtexture, 8) ?
				(sd->special = W_CheckNumForName(msd->midtexture)) < 0 ||
				W_LumpLength(sd->special) != 65536 ?
				sd->special=0, R_TextureNumForName(msd->midtexture) :
					(sd->special++, 0) : (sd->special=0);
			sd->toptexture = R_TextureNumForName(msd->toptexture);
			sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
			break;
*/
		default:			// normal cases
			sd->midtexture = R_TextureNumForName(msd->midtexture);
			sd->toptexture = R_TextureNumForName(msd->toptexture);
			sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
			break;
		}
	}
	Z_Free (data);
}

// [RH] Set slopes for sectors, based on line specials

//
// P_AlignPlane
//
// Aligns the floor or ceiling of a sector to the corresponding plane
// on the other side of the reference line. (By definition, line must be
// two-sided.)
//
// If (which & 1), sets floor.
// If (which & 2), sets ceiling.
//

static void P_AlignPlane (sector_t *sec, line_t *line, int which)
{
	sector_t *refsec;
	int bestdist;
	vertex_t *refvert;
	int i;
	line_t **probe;

	if (line->backsector == NULL)
		return;

	// Find furthest vertex from the reference line. It, along with the two ends
	// of the line will define the plane.
	bestdist = 0;
	for (i = sec->linecount*2, probe = sec->lines; i > 0; i--)
	{
		int dist;
		vertex_t *vert;

		// Do calculations with only the upper bits, because the lower ones
		// are all zero, and we would overflow for a lot of distances if we
		// kept them around.

		if (i & 1)
			vert = (*probe++)->v2;
		else
			vert = (*probe)->v1;
		dist = abs (((line->v1->y - vert->y) >> FRACBITS) * (line->dx >> FRACBITS) -
					((line->v1->x - vert->x) >> FRACBITS) * (line->dy >> FRACBITS));

		if (dist > bestdist)
		{
			bestdist = dist;
			refvert = vert;
		}
	}

	refsec = line->frontsector == sec ? line->backsector : line->frontsector;

	vec3_t p, v1, v2, cross;

	p[0] = FIXED2FLOAT (line->v1->x);
	p[1] = FIXED2FLOAT (line->v1->y);
	v1[0] = FIXED2FLOAT (line->dx);
	v1[1] = FIXED2FLOAT (line->dy);
	v2[0] = FIXED2FLOAT (refvert->x - line->v1->x);
	v2[1] = FIXED2FLOAT (refvert->y - line->v1->y);

	const secplane_t *refplane;
	secplane_t *srcplane;
	fixed_t srcheight, destheight;

	refplane = (which == 0) ? &refsec->floorplane : &refsec->ceilingplane;
	srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
	srcheight = (which == 0) ? sec->floortexz : sec->ceilingtexz;
	destheight = (which == 0) ? refsec->floortexz : refsec->ceilingtexz;

	p[2] = FIXED2FLOAT (destheight);
	v1[2] = 0;
	v2[2] = FIXED2FLOAT (srcheight - destheight);

	CrossProduct (v1, v2, cross);
	VectorNormalize (cross);

	// Fix backward normals
	if ((cross[2] < 0 && which == 0) || (cross[2] > 0 && which == 1))
	{
		cross[0] = -cross[0];
		cross[1] = -cross[1];
		cross[2] = -cross[2];
	}

	srcplane->a = FLOAT2FIXED (cross[0]);
	srcplane->b = FLOAT2FIXED (cross[1]);
	srcplane->c = FLOAT2FIXED (cross[2]);
	srcplane->ic = FLOAT2FIXED (1.f/cross[2]);
	srcplane->d = -TMulScale16 (srcplane->a, line->v1->x,
								srcplane->b, line->v1->y,
								srcplane->c, destheight);
}

void P_SetSlopes ()
{
	int i, s;

	for (i = 0; i < numlines; i++)
	{
		if (lines[i].special == Plane_Align)
		{
			lines[i].special = 0;
			if (lines[i].backsector != NULL)
			{
				// args[0] is for floor, args[1] is for ceiling
				//
				// As a special case, if args[1] is 0,
				// then args[0], bits 2-3 are for ceiling.
				for (s = 0; s < 2; s++)
				{
					int bits = lines[i].args[s] & 3;

					if (s == 1 && bits == 0)
						bits = (lines[i].args[0] >> 2) & 3;

					if (bits == 1)			// align front side to back
						P_AlignPlane (lines[i].frontsector, lines + i, s);
					else if (bits == 2)		// align back side to front
						P_AlignPlane (lines[i].backsector, lines + i, s);
				}
			}
		}
	}
}


//
// killough 10/98:
//
// Rewritten to use faster algorithm.
//
// New procedure uses Bresenham-like algorithm on the linedefs, adding the
// linedef to each block visited from the beginning to the end of the linedef.
//
// The algorithm's complexity is on the order of nlines*total_linedef_length.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.

static void P_CreateBlockMap ()
{
	register int i;
	fixed_t minx = FIXED_MAX, miny = FIXED_MAX,
			maxx = FIXED_MIN, maxy = FIXED_MIN;

	// First find limits of map

	for (i = 0; i < numvertexes; i++)
	{
		if (vertexes[i].x >> FRACBITS < minx)
			minx = vertexes[i].x >> FRACBITS;
		else if (vertexes[i].x >> FRACBITS > maxx)
			maxx = vertexes[i].x >> FRACBITS;
		if (vertexes[i].y >> FRACBITS < miny)
			miny = vertexes[i].y >> FRACBITS;
		else if (vertexes[i].y >> FRACBITS > maxy)
			maxy = vertexes[i].y >> FRACBITS;
	}

	// Save blockmap parameters

	bmaporgx = minx << FRACBITS;
	bmaporgy = miny << FRACBITS;
	bmapwidth  = ((maxx-minx) >> MAPBTOFRAC) + 1;
	bmapheight = ((maxy-miny) >> MAPBTOFRAC) + 1;

	// Compute blockmap, which is stored as a 2d array of variable-sized lists.
	//
	// Pseudocode:
	//
	// For each linedef:
	//
	//   Map the starting and ending vertices to blocks.
	//
	//   Starting in the starting vertex's block, do:
	//
	//     Add linedef to current block's list, dynamically resizing it.
	//
	//     If current block is the same as the ending vertex's block, exit loop.
	//
	//     Move to an adjacent block by moving towards the ending block in 
	//     either the x or y direction, to the block which contains the linedef.

	struct bmap_t { int n, nalloc, *list; };			// blocklist structure
	unsigned tot = bmapwidth * bmapheight;				// size of blockmap
	bmap_t *bmap = (bmap_t *)calloc(sizeof *bmap, tot);	// array of blocklists

	for (i = 0; i < numlines; i++)
	{
		// starting coordinates
		int x = (lines[i].v1->x >> FRACBITS) - minx;
		int y = (lines[i].v1->y >> FRACBITS) - miny;

		// x-y deltas
		int adx = lines[i].dx >> FRACBITS, dx = adx < 0 ? -1 : 1;
		int ady = lines[i].dy >> FRACBITS, dy = ady < 0 ? -1 : 1; 

		// difference in preferring to move across y (>0) instead of x (<0)
		int diff = !adx ? 1 : !ady ? -1 :
		  (((x >> MAPBTOFRAC) << MAPBTOFRAC) + 
		   (dx > 0 ? MAPBLOCKUNITS-1 : 0) - x) * (ady = abs(ady)) * dx -
		  (((y >> MAPBTOFRAC) << MAPBTOFRAC) + 
		   (dy > 0 ? MAPBLOCKUNITS-1 : 0) - y) * (adx = abs(adx)) * dy;

		// starting block, and pointer to its blocklist structure
		int b = (y >> MAPBTOFRAC)*bmapwidth + (x >> MAPBTOFRAC);

		// ending block
		int bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC) *
		  bmapwidth + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

		// delta for pointer when moving across y
		dy *= bmapwidth;

		// deltas for diff inside the loop
		adx <<= MAPBTOFRAC;
		ady <<= MAPBTOFRAC;

		// Now we simply iterate block-by-block until we reach the end block.
		while ((unsigned) b < tot)		// failsafe -- should ALWAYS be true
		{
			// Increase size of allocated list if necessary
			if (bmap[b].n >= bmap[b].nalloc)
				bmap[b].list = (int *)realloc(bmap[b].list, 
					(bmap[b].nalloc = bmap[b].nalloc ? 
					 bmap[b].nalloc*2 : 8)*sizeof*bmap->list);

			// Add linedef to end of list
			bmap[b].list[bmap[b].n++] = i;

			// If we have reached the last block, exit
			if (b == bend)
				break;

			// Move in either the x or y direction to the next block
			if (diff < 0) 
				diff += ady, b += dx;
			else
				diff -= adx, b += dy;
		}
	}

	// Compute the total size of the blockmap.
	//
	// Compression of empty blocks is performed by reserving two offset words
	// at tot and tot+1.
	//
	// 4 words, unused if this routine is called, are reserved at the start.

	{
		int count = tot+6;  // we need at least 1 word per block, plus reserved's

		for (i = 0; (unsigned)i < tot; i++)
			if (bmap[i].n)
				count += bmap[i].n + 2; // 1 header word + 1 trailer word + blocklist

		// Allocate blockmap lump with computed count
		blockmaplump = (int *)Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);
	}

	// Now compress the blockmap.
	{
		int ndx = tot += 4;			// Advance index to start of linedef lists
		bmap_t *bp = bmap;			// Start of uncompressed blockmap

		blockmaplump[ndx++] = 0;	// Store an empty blockmap list at start
		blockmaplump[ndx++] = -1;	// (Used for compression)

		for (i = 4; (unsigned)i < tot; i++, bp++)
		if (bp->n)											// Non-empty blocklist
		{
			blockmaplump[blockmaplump[i] = ndx++] = 0;		// Store index & header
			do
				blockmaplump[ndx++] = bp->list[--bp->n];	// Copy linedef list
			while (bp->n);
			blockmaplump[ndx++] = -1;						// Store trailer
			free(bp->list);									// Free linedef list
		}
		else			// Empty blocklist: point to reserved empty blocklist
			blockmaplump[i] = tot;

		free (bmap);	// Free uncompressed blockmap
	}
}

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
CVAR (Bool, genblockmap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

void P_LoadBlockMap (int lump)
{
	int count;
	
	if (genblockmap ||
		(count = W_LumpLength(lump)/2) >= 0x10000 ||
		Args.CheckParm("-blockmap") ||
		W_LumpLength (lump) == 0)
	{
		DPrintf ("Generating BLOCKMAP lump\n");
		P_CreateBlockMap ();
	}
	else
	{
		short *wadblockmaplump = (short *)W_CacheLumpNum (lump, PU_LEVEL);
		int i;
		blockmaplump = (int *)Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

		// killough 3/1/98: Expand wad blockmap into larger internal one,
		// by treating all offsets except -1 as unsigned and zero-extending
		// them. This potentially doubles the size of blockmaps allowed,
		// because Doom originally considered the offsets as always signed.

		blockmaplump[0] = SHORT(wadblockmaplump[0]);
		blockmaplump[1] = SHORT(wadblockmaplump[1]);
		blockmaplump[2] = (DWORD)(SHORT(wadblockmaplump[2])) & 0xffff;
		blockmaplump[3] = (DWORD)(SHORT(wadblockmaplump[3])) & 0xffff;

		for (i = 4; i < count; i++)
		{
			short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
			blockmaplump[i] = t == -1 ? (DWORD)0xffffffff : (DWORD) t & 0xffff;
		}

		Z_Free (wadblockmaplump);

		bmaporgx = blockmaplump[0]<<FRACBITS;
		bmaporgy = blockmaplump[1]<<FRACBITS;
		bmapwidth = blockmaplump[2];
		bmapheight = blockmaplump[3];
	}

	// clear out mobj chains
	count = sizeof(*blocklinks) * bmapwidth*bmapheight;
	blocklinks = (AActor **)Z_Malloc (count, PU_LEVEL, 0);
	memset (blocklinks, 0, count);
	blockmap = blockmaplump+4;
}



//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines ()
{
	line_t**			linebuffer;
	int 				i;
	int 				j;
	int 				total;
	line_t* 			li;
	sector_t*			sector;
	DBoundingBox		bbox;
	bool				flaggedNoFronts = false;
		
	// look up sector number for each subsector
	for (i = 0; i < numsubsectors; i++)
		subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;

	// count number of lines in each sector
	li = lines;
	total = 0;
	for (i = 0; i < numlines; i++, li++)
	{
		if (li->frontsector == NULL)
		{
			if (!flaggedNoFronts)
			{
				flaggedNoFronts = true;
				Printf ("The following lines do not have a frontsector:\n");
			}
			Printf (" %d\n", i);
		}
		else
		{
			li->frontsector->linecount++;
			total++;
		}

		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->linecount++;
			total++;
		}
	}
	if (flaggedNoFronts)
	{
		I_Error ("This map contains errors that must be fixed.\n");
	}
		
	// build line tables for each sector		
	linebuffer = (line_t **)Z_Malloc (total*sizeof(line_t *), PU_LEVEL, 0);
	sector = sectors;
	for (i = 0; i < numsectors; i++, sector++)
	{
		bbox.ClearBox ();
		sector->lines = linebuffer;
		li = lines;
		for (j = 0; j < numlines; j++, li++)
		{
			if (li->frontsector == sector || li->backsector == sector)
			{
				*linebuffer++ = li;
				bbox.AddToBox (li->v1->x, li->v1->y);
				bbox.AddToBox (li->v2->x, li->v2->y);
			}
		}
		if (linebuffer - sector->lines != sector->linecount)
		{
			I_Error ("P_GroupLines: miscounted");
		}
						
		// set the soundorg to the middle of the bounding box
		sector->soundorg[0] = (bbox.Right()+bbox.Left())/2;
		sector->soundorg[1] = (bbox.Top()+bbox.Bottom())/2;
		
#if 0
		int block;

		// adjust bounding box to map blocks
		block = (bbox.Top()-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapheight ? bmapheight-1 : block;
		//sector->blockbox.Top()=block;

		block = (bbox.Bottom()-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		//sector->blockbox.Bottom()=block;

		block = (bbox.Right()-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapwidth ? bmapwidth-1 : block;
		//sector->blockbox.Right()=block;

		block = (bbox.Left()-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		//sector->blockbox.Left()=block;
#endif
	}
}

//
// [RH] P_LoadBehavior
//
void P_LoadBehavior (int lumpnum)
{
	byte *behavior = (byte *)W_CacheLumpNum (lumpnum, PU_LEVEL);

	level.behavior = new FBehavior (behavior, lumpinfo[lumpnum].size);
	if (!level.behavior->IsGood ())
	{
		delete level.behavior;
		level.behavior = NULL;
	}
}

//
// P_SetupLevel
//
extern AActor *bodyquesize[];
extern polyblock_t **PolyBlockMap;

// [RH] position indicates the start spot to spawn at
void P_SetupLevel (char *lumpname, int position)
{
	int i, lumpnum;
		
	level.total_monsters = level.total_items = level.total_secrets =
		level.killed_monsters = level.found_items = level.found_secrets =
		wminfo.maxfrags = 0;
	wminfo.partime = 180;

	if (!savegamerestore)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].killcount = players[i].secretcount 
				= players[i].itemcount = 0;
		}
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		players[i].mo = NULL;
	}

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer].viewz = 1; 

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start ();

	// [RH] Clear all ThingID hash chains.
	AActor::ClearTIDHashes ();

	// [RH] clear out the mid-screen message
	C_MidPrint (NULL);

	PolyBlockMap = NULL;

	DThinker::DestroyAllThinkers ();
	Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);
	NormalLight.Next = NULL;	// [RH] Z_FreeTags frees all the custom colormaps

	// UNUSED W_Profile ();

	// find map num
	level.lumpnum = lumpnum = W_GetNumForName (lumpname);

	// [RH] Check if this map is Hexen-style.
	//		LINEDEFS and THINGS need to be handled accordingly.
	HasBehavior = W_CheckLumpName (lumpnum+ML_BEHAVIOR, "BEHAVIOR");

	// note: most of this ordering is important 
	
	// [RH] Load in the BEHAVIOR lump
	if (level.behavior != NULL)
	{
		delete level.behavior;
		level.behavior = NULL;
	}
	if (HasBehavior)
	{
		P_LoadBehavior (lumpnum+ML_BEHAVIOR);
	}
	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSectors (lumpnum+ML_SECTORS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
	if (!HasBehavior)
		P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	else
		P_LoadLineDefs2 (lumpnum+ML_LINEDEFS);	// [RH] Load Hexen-style linedefs
	P_LoadSideDefs2 (lumpnum+ML_SIDEDEFS);
	P_FinishLoadingLineDefs ();
	P_LoopSidedefs ();
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadSegs (lumpnum+ML_SEGS);
		
	rejectmatrix = (byte *)W_CacheLumpNum (lumpnum+ML_REJECT, PU_LEVEL);
	{
		// [RH] Scan the rejectmatrix and see if it actually contains anything
		int i, end = (W_LumpLength (lumpnum+ML_REJECT)-(sizeof(int)-1)) / sizeof(int);

		for (i = 0; i < end; i++)
		{
			if (((int *)rejectmatrix)[i])
			{
				rejectempty = false;
				break;
			}
		}
		if (i >= end)
		{
			DPrintf ("Reject matrix is empty\n");
			rejectempty = true;
		}
	}
	P_GroupLines ();

	bodyqueslot = 0;
// phares 8/10/98: Clear body queue so the corpses from previous games are
// not assumed to be from this one. The actors belonging to these corpses
// are cleared in the normal freeing of zoned memory between maps, so all
// we have to do here is clear the pointers to them.

	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	po_NumPolyobjs = 0;

	deathmatchstarts.Clear ();

	P_SetSlopes ();

	if (!HasBehavior)
		P_LoadThings (lumpnum+ML_THINGS);
	else
		P_LoadThings2 (lumpnum+ML_THINGS, position);	// [RH] Load Hexen-style things

	if (!HasBehavior)
		P_TranslateTeleportThings ();	// [RH] Assign teleport destination TIDs

	PO_Init ();	// Initialize the polyobjs

	// if deathmatch, randomly spawn the active players
	if (deathmatch)
	{
		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (playeringame[i])
			{
				players[i].mo = NULL;
				G_DeathMatchSpawnPlayer (i);
			}
		}
	}

	// set up world state
	P_SpawnSpecials ();

	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	R_OldBlend = ~0;
	// preload graphics
	if (precache)
		R_PrecacheLevel ();

	if (deathmatch)
	{
		AnnounceGameStart ();
	}

	P_ResetSightCounters (true);

	//Printf ("free memory: 0x%x\n", Z_FreeMemory());
}



//
// P_Init
//
void P_Init ()
{
	P_InitEffects ();		// [RH]
	P_InitPicAnims ();
	P_InitSwitchList ();
	P_InitTerrainTypes ();
	R_InitSprites ();
}
