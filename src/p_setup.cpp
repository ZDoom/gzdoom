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
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "templates.h"
#include "m_alloc.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "g_game.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "nodebuild.h"
#include "s_sound.h"
#include "doomstat.h"
#include "p_lnspec.h"
#include "v_palette.h"
#include "c_console.h"
#include "p_acs.h"
#include "vectors.h"
#include "announcer.h"
#include "wi_stuff.h"
#include "stats.h"

extern void P_SpawnMapThing (mapthing2_t *mthing, int position);
extern bool P_LoadBuildMap (BYTE *mapdata, size_t len, mapthing2_t *start);

extern void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
extern void P_TranslateTeleportThings (void);
extern int	P_TranslateSectorSpecial (int);

extern unsigned int R_OldBlend;

CVAR (Bool, genblockmap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, gennodes, false, CVAR_GLOBALCONFIG);
CVAR (Bool, genglnodes, false, 0);

static void P_InitTagLists ();

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

FExtraLight*	ExtraLights;
FLightStack*	LightStacks;

int sidecount;
struct sidei_t	// [RH] Only keep BOOM sidedef init stuff around for init
{
	union
	{
		// Used when unpacking sidedefs and assigning
		// properties based on linedefs.
		struct
		{
			short tag, special;
			WORD map;
		} a;

		// Used when grouping sidedefs into loops.
		struct
		{
			WORD first, next;
			char lineside;
		} b;
	};
}				*sidetemp;
static WORD		*linemap;

// [RH] Set true if the map contains a BEHAVIOR lump
BOOL			HasBehavior;

EXTERN_CVAR(Bool, r_experimental)
bool			UsingGLNodes;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size 256x256.
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
static bool		rejectmapped;

static bool		ForceNodeBuild;

// Maintain single and multi player starting spots.
TArray<mapthing2_t> deathmatchstarts (16);
mapthing2_t		playerstarts[MAXPLAYERS];

static void P_AllocateSideDefs (int count);
static void P_SetSideNum (WORD *sidenum_p, WORD sidenum);

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
	const byte *data;
	int i;

	// Determine number of vertices:
	//	total lump length / vertex record length.
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

	// Allocate memory for buffer.
	vertexes = new vertex_t[numvertexes];		

	// Load data into cache.
	data = (byte *)W_MapLumpNum (lump);
		
	// Copy and convert vertex coordinates,
	// internal representation as fixed.
	for (i = 0; i < numvertexes; i++)
	{
		vertexes[i].x = SHORT(((mapvertex_t *)data)[i].x)<<FRACBITS;
		vertexes[i].y = SHORT(((mapvertex_t *)data)[i].y)<<FRACBITS;
	}

	// Free buffer memory.
	W_UnMapLump (data);
}



//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadSegs (int lump)
{
	int  i;
	const byte *data;
	byte *vertchanged = new byte[numvertexes];	// phares 10/4/98
	DWORD segangle;
	line_t* line;		// phares 10/4/98
	int ptp_angle;		// phares 10/4/98
	int delta_angle;	// phares 10/4/98
	int dis;			// phares 10/4/98
	int dx,dy;			// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);

	if (numsegs == 0)
	{
		Printf ("This map has no segs.\n");
		delete[] subsectors;
		delete[] nodes;
		ForceNodeBuild = true;
		return;
	}

	segs = new seg_t[numsegs];
	memset (segs, 0, numsegs*sizeof(seg_t));
	data = (byte *)W_MapLumpNum (lump);

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
		li->PartnerSeg = NULL;

		segangle = (WORD)SHORT(ml->angle);

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

		ptp_angle = R_PointToAngle2 (li->v1->x, li->v1->y, li->v2->x, li->v2->y);
		dis = 0;
		delta_angle = (abs(ptp_angle-(segangle<<16))>>ANGLETOFINESHIFT)*360/FINEANGLES;

		vnum1 = li->v1 - vertexes;
		vnum2 = li->v2 - vertexes;

		if (vnum1 >= numvertexes || vnum2 >= numvertexes)
		{
			Printf ("Seg %d references a nonexistant vertex.\n"
					"The BSP will be rebuilt.\n", i);
			delete[] vertchanged;
			W_UnMapLump (data);
			delete[] segs;
			delete[] subsectors;
			delete[] nodes;
			ForceNodeBuild = true;
			return;
		}

		if (delta_angle != 0)
		{
			segangle >>= (ANGLETOFINESHIFT-16);
			dx = (li->v1->x - li->v2->x)>>FRACBITS;
			dy = (li->v1->y - li->v2->y)>>FRACBITS;
			dis = ((int) sqrt(dx*dx + dy*dy))<<FRACBITS;
			dx = finecosine[segangle];
			dy = finesine[segangle];
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
		if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != NO_INDEX)
		{
			li->backsector = sides[ldef->sidenum[side^1]].sector;
		}
		else
		{
			li->backsector = 0;
			ldef->flags &= ~ML_TWOSIDED;
		}
	}

	W_UnMapLump (data);
	delete[] vertchanged; // phares 10/4/98
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
	DWORD maxseg;
	const byte *data;
	int i;

	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	maxseg = W_LumpLength (lump - ML_SSECTORS + ML_SEGS) / sizeof(mapseg_t);

	if (numsubsectors == 0 || maxseg == 0)
	{
		Printf ("This map has an incomplete BSP tree.\n");
		delete[] nodes;
		ForceNodeBuild = true;
		return;
	}

	subsectors = new subsector_t[numsubsectors];		
	data = (byte *)W_MapLumpNum (lump);
		
	memset (subsectors, 0, numsubsectors*sizeof(subsector_t));
	
	for (i = 0; i < numsubsectors; i++)
	{
		subsectors[i].numlines = SHORT(((mapsubsector_t *)data)[i].numsegs);
		subsectors[i].firstline = SHORT(((mapsubsector_t *)data)[i].firstseg);

		if (subsectors[i].firstline >= maxseg)
		{
			Printf ("Subsector %d contains invalid segs %lu-%lu\n"
				"The BSP will be rebuilt.\n", i, subsectors[i].firstline,
				subsectors[i].firstline + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			delete[] nodes;
			delete[] subsectors;
			break;
		}
		else if (subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %lu-%lu\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				subsectors[i].firstline + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			delete[] nodes;
			delete[] subsectors;
			break;
		}
	}

	W_UnMapLump (data);
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
	const byte*			data;
	int 				i;
	mapsector_t*		ms;
	sector_t*			ss;
	int					defSeqType;
	FDynamicColormap	*fogMap, *normMap;

	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = new sector_t[numsectors];		
	memset (sectors, 0, numsectors*sizeof(sector_t));
	data = (byte *)W_MapLumpNum (lump);

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
		ss->floor_xscale = FRACUNIT;	// [RH] floor and ceiling scaling
		ss->floor_yscale = FRACUNIT;
		ss->ceiling_xscale = FRACUNIT;
		ss->ceiling_yscale = FRACUNIT;

		ss->heightsec = NULL;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		ss->gravity = 1.f;	// [RH] Default sector gravity of 1.0

		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		if (level.outsidefog != 0xff000000 && ss->ceilingpic == skyflatnum)
		{
			if (fogMap == NULL)
				fogMap = GetSpecialLights (PalEntry (255,255,255), level.outsidefog);
			ss->ColorMap = fogMap;
		}
		else
		{
			if (normMap == NULL)
				normMap = GetSpecialLights (PalEntry (255,255,255), level.fadeto);
			ss->ColorMap = normMap;
		}

		// killough 8/28/98: initialize all sectors to normal friction
		ss->friction = ORIG_FRICTION;
		ss->movefactor = ORIG_FRICTION_FACTOR;
	}
		
	W_UnMapLump (data);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
	const byte*	data;
	int 		i;
	int 		j;
	int 		k;
	mapnode_t*	mn;
	node_t* 	no;
	int			maxss;
	WORD*		used;
		
	numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
	maxss = W_LumpLength (lump - ML_NODES + ML_SSECTORS) / sizeof(mapsubsector_t);

	if (numnodes == 0 || maxss == 0)
	{
		ForceNodeBuild = true;
		return;
	}

	
	nodes = new node_t[numnodes];		
	data = (byte *)W_MapLumpNum (lump);
	used = (WORD *)alloca (sizeof(WORD)*numnodes);
	memset (used, 0, sizeof(WORD)*numnodes);

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
			WORD child = SHORT(mn->children[j]);
			if (child & NF_SUBSECTOR)
			{
				child &= ~NF_SUBSECTOR;
				if (child >= maxss)
				{
					Printf ("BSP node %d references invalid subsector %d.\n"
						"The BSP will be rebuilt.\n", i, child);
					ForceNodeBuild = true;
					delete[] nodes;
					W_UnMapLump (data);
					return;
				}
				no->children[j] = (BYTE *)&subsectors[child] + 1;
			}
			else if (child >= numnodes)
			{
				Printf ("BSP node %d references invalid node %d.\n"
					"The BSP will be rebuilt.\n", i, (node_t *)no->children[j] - nodes);
				ForceNodeBuild = true;
				delete[] nodes;
				W_UnMapLump (data);
				return;
			}
			else if (used[child])
			{
				Printf ("BSP node %d references node %d,\n"
					"which is already used by node %d.\n"
					"The BSP will be rebuilt.\n", i, child, used[child]-1);
				ForceNodeBuild = true;
				delete[] nodes;
				W_UnMapLump (data);
				return;
			}
			else
			{
				no->children[j] = &nodes[child];
				used[child] = j + 1;
			}
			for (k = 0; k < 4; k++)
			{
				no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
			}
		}
	}
		
	W_UnMapLump (data);
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
	mapthing2_t mt2;		// [RH] for translation
	const byte *data = (byte *)W_MapLumpNum (lump, true);
	const mapthing_t *mt = (mapthing_t *)data;
	const mapthing_t *lastmt = (mapthing_t *)(data + W_LumpLength (lump));

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
		
	W_UnMapLump (data);
}

//
// P_SpawnSlopeMakers
//

static void P_SlopeLineToPoint (int lineid, fixed_t x, fixed_t y, fixed_t z, BOOL slopeCeil)
{
	int linenum = -1;

	while ((linenum = P_FindLineFromID (lineid, linenum)) != -1)
	{
		const line_t *line = &lines[linenum];
		sector_t *sec;
		secplane_t *plane;
		
		if (P_PointOnLineSide (x, y, line) == 0)
		{
			sec = line->frontsector;
		}
		else
		{
			sec = line->backsector;
		}
		if (sec == NULL)
		{
			continue;
		}
		if (slopeCeil)
		{
			plane = &sec->ceilingplane;
		}
		else
		{
			plane = &sec->floorplane;
		}

		vec3_t p, v1, v2, cross;

		p[0] = FIXED2FLOAT (line->v1->x);
		p[1] = FIXED2FLOAT (line->v1->y);
		p[2] = FIXED2FLOAT (plane->ZatPoint (line->v1->x, line->v1->y));
		v1[0] = FIXED2FLOAT (line->dx);
		v1[1] = FIXED2FLOAT (line->dy);
		v1[2] = FIXED2FLOAT (plane->ZatPoint (line->v2->x, line->v2->y)) - p[2];
		v2[0] = FIXED2FLOAT (x - line->v1->x);
		v2[1] = FIXED2FLOAT (y - line->v1->y);
		v2[2] = FIXED2FLOAT (z) - p[2];

		CrossProduct (v1, v2, cross);
		VectorNormalize (cross);

		// Fix backward normals
		if ((cross[2] < 0 && !slopeCeil) || (cross[2] > 0 && slopeCeil))
		{
			cross[0] = -cross[0];
			cross[1] = -cross[1];
			cross[2] = -cross[2];
		}

		plane->a = FLOAT2FIXED (cross[0]);
		plane->b = FLOAT2FIXED (cross[1]);
		plane->c = FLOAT2FIXED (cross[2]);
		plane->ic = FLOAT2FIXED (1.f/cross[2]);
		plane->d = -TMulScale16 (plane->a, x,
								 plane->b, y,
								 plane->c, z);
	}
}

static void P_CopyPlane (int tag, fixed_t x, fixed_t y, BOOL copyCeil)
{
	sector_t *dest = R_PointInSubsector (x, y)->sector;
	sector_t *source;
	int secnum;
	size_t planeofs;

	secnum = P_FindSectorFromTag (tag, -1);
	if (secnum == -1)
	{
		return;
	}

	source = &sectors[secnum];

	if (copyCeil)
	{
		planeofs = myoffsetof(sector_t, ceilingplane);
	}
	else
	{
		planeofs = myoffsetof(sector_t, floorplane);
	}
	*(secplane_t *)((BYTE *)dest + planeofs) = *(secplane_t *)((BYTE *)source + planeofs);
}

void P_SetSlope (secplane_t *plane, BOOL setCeil, int xyangi, int zangi,
	fixed_t x, fixed_t y, fixed_t z)
{
	angle_t xyang;
	angle_t zang;

	if (zangi >= 180)
	{
		zang = ANGLE_180-ANGLE_1;
	}
	else if (zangi <= 0)
	{
		zang = ANGLE_1;
	}
	else
	{
		zang = Scale (zangi, ANGLE_180, 180);
	}
	if (!setCeil)
	{
		zang += ANGLE_180;
	}
	zang >>= ANGLETOFINESHIFT;

	xyang = (angle_t)Scale (xyangi, ANGLE_180, 180) >> ANGLETOFINESHIFT;

	vec3_t norm;

	norm[0] = (float)(finecosine[zang] * finecosine[xyang]);
	norm[1] = (float)(finecosine[zang] * finesine[xyang]);
	norm[2] = (float)(finesine[zang]) * 65536.f;
	VectorNormalize (norm);
	plane->a = (int)(norm[0] * 65536.f);
	plane->b = (int)(norm[1] * 65536.f);
	plane->c = (int)(norm[2] * 65536.f);
	plane->ic = (int)(65536.f / norm[2]);
	plane->d = -TMulScale16 (plane->a, x,
							 plane->b, y,
							 plane->c, z);
}

enum
{
	THING_SlopeFloorPointLine = 9500,
	THING_SlopeCeilingPointLine = 9501,
	THING_SetFloorSlope = 9502,
	THING_SetCeilingSlope = 9503,
	THING_CopyFloorPlane = 9510,
	THING_CopyCeilingPlane = 9511,
};

static void P_SpawnSlopeMakers (mapthing2_t *mt, mapthing2_t *lastmt)
{
	mapthing2_t *firstmt = mt;

	for (; mt < lastmt; ++mt)
	{
		if (mt->type >= THING_SlopeFloorPointLine &&
			mt->type <= THING_SetCeilingSlope)
		{
			fixed_t x, y, z;
			secplane_t *refplane;
			sector_t *sec;

			x = mt->x << FRACBITS;
			y = mt->y << FRACBITS;
			sec = R_PointInSubsector (x, y)->sector;
			if (mt->type & 1)
			{
				refplane = &sec->ceilingplane;
			}
			else
			{
				refplane = &sec->floorplane;
			}
			z = refplane->ZatPoint (x, y) + (mt->z << FRACBITS);
			if (mt->type <= THING_SlopeCeilingPointLine)
			{
				P_SlopeLineToPoint (mt->args[0], x, y, z, mt->type & 1);
			}
			else
			{
				P_SetSlope (refplane, mt->type & 1, mt->angle, mt->args[0], x, y, z);
			}
			mt->type = 0;
		}
	}

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type == THING_CopyFloorPlane ||
			mt->type == THING_CopyCeilingPlane)
		{
			P_CopyPlane (mt->args[0], mt->x << FRACBITS, mt->y << FRACBITS, mt->type & 1);
			mt->type = 0;
		}
	}
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
	byte *data = (byte *)W_MapLumpNum (lump, true);
	mapthing2_t *mt = (mapthing2_t *)data;
	mapthing2_t *lastmt = (mapthing2_t *)(data + W_LumpLength (lump));

#ifdef __BIG_ENDIAN__
	for (; mt < lastmt; ++mt)
	{
		mt->thingid = SHORT(mt->thingid);
		mt->x = SHORT(mt->x);
		mt->y = SHORT(mt->y);
		mt->z = SHORT(mt->z);
		mt->angle = SHORT(mt->angle);
		mt->type = SHORT(mt->type);
		mt->flags = SHORT(mt->flags);
	}
#endif

	// [RH] Spawn slope creating things first.
	P_SpawnSlopeMakers (mt, lastmt);

	for (; mt < lastmt; mt++)
	{
		P_SpawnMapThing (mt, position);
	}

	W_UnMapLump (data);
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
		ld->slopetype = ((ld->dy ^ ld->dx) >= 0) ? ST_POSITIVE : ST_NEGATIVE;
			
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
}

void P_SaveLineSpecial (line_t *ld)
{
	// killough 4/4/98: support special sidedef interpretation below
	if ((ld->sidenum[0] != NO_INDEX) &&
		// [RH] Save Static_Init only if it's interested in the textures
		(ld->special != Static_Init || ld->args[1] == Init_Color))
	{
		sidetemp[*ld->sidenum].a.special = ld->special;
		sidetemp[*ld->sidenum].a.tag = ld->args[0];
	}
	else
	{
		sidetemp[*ld->sidenum].a.special = 0;
	}
}

// killough 4/4/98: delay using sidedefs until they are loaded
void P_FinishLoadingLineDefs ()
{
	WORD len;
	int i, linenum;
	line_t *ld = lines;

	for (i = numlines, linenum = 0; i--; ld++, linenum++)
	{
		ld->frontsector = ld->sidenum[0]!=NO_INDEX ? sides[ld->sidenum[0]].sector : 0;
		ld->backsector  = ld->sidenum[1]!=NO_INDEX ? sides[ld->sidenum[1]].sector : 0;
		float dx = FIXED2FLOAT(ld->v2->x - ld->v1->x);
		float dy = FIXED2FLOAT(ld->v2->y - ld->v1->y);
		SBYTE light;

		if (ld->frontsector == NULL)
		{
			Printf ("Line %d has no front sector\n", linemap[linenum]);
		}

		// [RH] Set some new sidedef properties
		len = (int)sqrtf (dx*dx + dy*dy);
		light = dy == 0 ? level.WallHorizLight :
				dx == 0 ? level.WallVertLight : 0;

		if (ld->sidenum[0] != NO_INDEX)
		{
			sides[ld->sidenum[0]].linenum = linenum;
			sides[ld->sidenum[0]].TexelLength = len;
			sides[ld->sidenum[0]].Light = light;
		}
		if (ld->sidenum[1] != NO_INDEX)
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
			{
				ld->alpha = (byte)ld->args[1];
				if (ld->args[2] == 1)
				{
					sides[ld->sidenum[0]].Flags |= WALLF_ADDTRANS;
					if (ld->sidenum[1] != NO_INDEX)
					{
						sides[ld->sidenum[1]].Flags |= WALLF_ADDTRANS;
					}
				}
			}
			else
			{
				for (j = 0; j < numlines; j++)
				{
					if (lines[j].id == ld->args[0])
					{
						lines[j].alpha = (byte)ld->args[1];
						if (lines[j].args[2] == 1)
						{
							sides[lines[j].sidenum[0]].Flags |= WALLF_ADDTRANS;
							if (lines[j].sidenum[1] != NO_INDEX)
							{
								sides[lines[j].sidenum[1]].Flags |= WALLF_ADDTRANS;
							}
						}
					}
				}
			}
			ld->special = 0;
			break;
		}
	}
}

void P_LoadLineDefs (int lump)
{
	byte *data;
	int i, skipped;
	line_t *ld;
		
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = new line_t[numlines];
	linemap = new WORD[numlines];
	memset (lines, 0, numlines*sizeof(line_t));
	data = (byte *)W_MapLumpNum (lump, true);

	// [RH] Count the number of sidedef references. This is the number of
	// sidedefs we need. The actual number in the SIDEDEFS lump might be less.
	// Lines with 0 length are also removed.

	for (skipped = sidecount = i = 0; i < numlines; )
	{
		maplinedef_t *mld = ((maplinedef_t *)data) + i;

		if (mld->v1 == mld->v2 ||
			(vertexes[SHORT(mld->v1)].x == vertexes[SHORT(mld->v2)].x &&
			 vertexes[SHORT(mld->v1)].y == vertexes[SHORT(mld->v2)].y))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			skipped++;
			numlines--;
		}
		else
		{
			if (SHORT(mld->sidenum[0]) != NO_INDEX)
				sidecount++;
			if (SHORT(mld->sidenum[1]) != NO_INDEX)
				sidecount++;
			linemap[i] = i+skipped;
			i++;
		}
	}
	if (skipped > 0)
	{
		ForceNodeBuild = true;
	}

	P_AllocateSideDefs (sidecount);

	maplinedef_t *mld = (maplinedef_t *)data;
	ld = lines;
	for (i = numlines; i > 0; i--, mld++, ld++)
	{
		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.
		P_TranslateLineDef (ld, mld);

		ld->v1 = &vertexes[SHORT(mld->v1)];
		ld->v2 = &vertexes[SHORT(mld->v2)];

		P_SetSideNum (&ld->sidenum[0], SHORT(mld->sidenum[0]));
		P_SetSideNum (&ld->sidenum[1], SHORT(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SaveLineSpecial (ld);
	}
		
	W_UnMapLump (data);
}

// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
void P_LoadLineDefs2 (int lump)
{
	byte*				data;
	int 				i, skipped;
	maplinedef2_t*		mld;
	line_t* 			ld;

	numlines = W_LumpLength (lump) / sizeof(maplinedef2_t);
	lines = new line_t[numlines];
	linemap = new WORD[numlines];
	memset (lines, 0, numlines*sizeof(line_t));
	data = (byte *)W_MapLumpNum (lump, true);

	// [RH] Remove any lines that have 0 length and count sidedefs used
	for (skipped = sidecount = i = 0; i < numlines; )
	{
		maplinedef2_t *mld = ((maplinedef2_t *)data) + i;

		if (mld->v1 == mld->v2 ||
			(vertexes[SHORT(mld->v1)].x == vertexes[SHORT(mld->v2)].x &&
			 vertexes[SHORT(mld->v1)].y == vertexes[SHORT(mld->v2)].y))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			skipped++;
			numlines--;
		}
		else
		{
			if (SHORT(mld->sidenum[0]) != NO_INDEX)
				sidecount++;
			if (SHORT(mld->sidenum[1]) != NO_INDEX)
				sidecount++;
			linemap[i] = i+skipped;
			i++;
		}
	}
	if (skipped > 0)
	{
		ForceNodeBuild = true;
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

		P_SetSideNum (&ld->sidenum[0], SHORT(mld->sidenum[0]));
		P_SetSideNum (&ld->sidenum[1], SHORT(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SaveLineSpecial (ld);
	}
		
	W_UnMapLump (data);
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

	sides = new side_t[count];
	memset (sides, 0, count*sizeof(side_t));

	sidetemp = new sidei_t[MAX(count,numvertexes)];
	for (i = 0; i < count; i++)
	{
		sidetemp[i].a.special = sidetemp[i].a.tag = 0;
		sidetemp[i].a.map = NO_INDEX;
	}
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}

static void P_SetSideNum (WORD *sidenum_p, WORD sidenum)
{
	sidenum = SHORT(sidenum);
	if (sidenum == NO_INDEX)
	{
		*sidenum_p = sidenum;
	}
	else if (sidecount < numsides)
	{
		sidetemp[sidecount].a.map = sidenum;
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
		sidetemp[i].b.first = NO_INDEX;
		sidetemp[i].b.next = NO_INDEX;
	}
	for (; i < numsides; ++i)
	{
		sidetemp[i].b.next = NO_INDEX;
	}

	for (i = 0; i < numsides; ++i)
	{
		// For each vertex, build a list of sidedefs that use that vertex
		// as their left edge.
		line_t *line = &lines[sides[i].linenum];
		int lineside = (line->sidenum[0] != i);
		int vert = (lineside ? line->v2 : line->v1) - vertexes;
		
		sidetemp[i].b.lineside = lineside;
		sidetemp[i].b.next = sidetemp[vert].b.first;
		sidetemp[vert].b.first = i;

		// Set each side so that it is the only member of its loop
		sides[i].LeftSide = NO_INDEX;
		sides[i].RightSide = NO_INDEX;
	}

	// For each side, find the side that is to its right and set the
	// loop pointers accordingly. If two sides share a left vertex, the
	// one that forms the smallest angle is assumed to be the right one.
	for (i = 0; i < numsides; ++i)
	{
		WORD right;
		line_t *line = &lines[sides[i].linenum];

		// If the side's line only exists in a single sector,
		// then consider that line to be a self-contained loop
		// instead of as part of another loop
		if (line->frontsector == line->backsector)
		{
			right = line->sidenum[!sidetemp[i].b.lineside];
		}
		else
		{
			if (sidetemp[i].b.lineside)
			{
				right = line->v1 - vertexes;
			}
			else
			{
				right = line->v2 - vertexes;
			}

			right = sidetemp[right].b.first;

			if (right == NO_INDEX)
			{ // There is no right side!
				Printf ("Line %d's right edge is unconnected\n", linemap[line-lines]);
				continue;
			}

			if (sidetemp[right].b.next != NO_INDEX)
			{
				int bestright = right;	// Shut up, GCC
				angle_t bestang = ANGLE_MAX;
				line_t *leftline, *rightline;
				angle_t ang1, ang2, ang;

				leftline = &lines[sides[i].linenum];
				ang1 = R_PointToAngle2 (0, 0, leftline->dx, leftline->dy);
				if (!sidetemp[i].b.lineside)
				{
					ang1 += ANGLE_180;
				}

				while (right != NO_INDEX)
				{
					if (sides[right].LeftSide == NO_INDEX)
					{
						rightline = &lines[sides[right].linenum];
						if (rightline->frontsector != rightline->backsector)
						{
							ang2 = R_PointToAngle (rightline->dx, rightline->dy);
							if (sidetemp[right].b.lineside)
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
					right = sidetemp[right].b.next;
				}
				right = bestright;
			}
		}
		sides[i].RightSide = right;
		sides[right].LeftSide = i;
	}

	// Throw away sidedef init info now that we're done with it
	delete[] sidetemp;
	sidetemp = NULL;
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2 (int lump)
{
	const byte *data = (byte *)W_MapLumpNum (lump);
	int  i;

	for (i = 0; i < numsides; i++)
	{
		mapsidedef_t *msd = (mapsidedef_t *)data + sidetemp[i].a.map;
		side_t *sd = sides + i;
		sector_t *sec;

		sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
		sd->linenum = NO_INDEX;

		// killough 4/4/98: allow sidedef texture names to be overloaded
		// killough 4/11/98: refined to allow colormaps to work as wall
		// textures if invalid as colormaps but valid as textures.

		if ((unsigned)SHORT(msd->sector)>=(unsigned)numsectors)
		{
			Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
			sd->sector = sec = NULL;
		}
		else
		{
			sd->sector = sec = &sectors[SHORT(msd->sector)];
		}
		switch (sidetemp[i].a.special)
		{
		case Transfer_Heights:	// variable colormap via 242 linedef
			  // [RH] The colormap num we get here isn't really a colormap,
			  //	  but a packed ARGB word for blending, so we also allow
			  //	  the blend to be specified directly by the texture names
			  //	  instead of figuring something out from the colormap.
			if (sec != NULL)
			{
				SetTexture (&sd->bottomtexture, &sec->bottommap, msd->bottomtexture);
				SetTexture (&sd->midtexture, &sec->midmap, msd->midtexture);
				SetTexture (&sd->toptexture, &sec->topmap, msd->toptexture);
			}
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
						if (sectors[s].tag == sidetemp[i].a.tag)
						{
							sectors[s].ColorMap = colormap;
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
	W_UnMapLump (data);
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
	vertex_t *refvert = (*sec->lines)->v1;	// Shut up, GCC
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
			lines[i].id = lines[i].args[2];
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
// [RH] My own blockmap builder, not Killough's or TeamTNT's.
//
// Killough's turned out not to be correct enough, and I had
// written this for ZDBSP before I discovered that, so
// replacing the one he wrote for MBF seemed like the easiest
// thing to do. (Doom E3M6, near vertex 0--the one furthest east
// on the map--had problems.)
//
// Using a hash table to get the minimum possible blockmap size
// seems like overkill, but I wanted to change the code as little
// as possible from its ZDBSP incarnation.
//

static unsigned int BlockHash (TArray<WORD> *block)
{
	int hash = 0;
	WORD *ar = &(*block)[0];
	for (size_t i = 0; i < block->Size(); ++i)
	{
		hash = hash * 12235 + ar[i];
	}
	return hash & 0x7fffffff;
}

static bool BlockCompare (TArray<WORD> *block1, TArray<WORD> *block2)
{
	size_t size = block1->Size();

	if (size != block2->Size())
	{
		return false;
	}
	if (size == 0)
	{
		return true;
	}
	WORD *ar1 = &(*block1)[0];
	WORD *ar2 = &(*block2)[0];
	for (size_t i = 0; i < size; ++i)
	{
		if (ar1[i] != ar2[i])
		{
			return false;
		}
	}
	return true;
}

static void CreatePackedBlockmap (TArray<int> &BlockMap, TArray<WORD> *blocks, int bmapwidth, int bmapheight)
{
	int buckets[4096];
	int *hashes, hashblock;
	TArray<WORD> *block;
	int zero = 0;
	int terminator = -1;
	WORD *array;
	int i, hash;
	int hashed = 0, nothashed = 0;

	hashes = new int[bmapwidth * bmapheight];

	memset (hashes, 0xff, sizeof(WORD)*bmapwidth*bmapheight);
	memset (buckets, 0xff, sizeof(buckets));

	for (i = 0; i < bmapwidth * bmapheight; ++i)
	{
		block = &blocks[i];
		hash = BlockHash (block) % 4096;
		hashblock = buckets[hash];
		while (hashblock != -1)
		{
			if (BlockCompare (block, &blocks[hashblock]))
			{
				break;
			}
			hashblock = hashes[hashblock];
		}
		if (hashblock != -1)
		{
			BlockMap[4+i] = BlockMap[4+hashblock];
			hashed++;
		}
		else
		{
			hashes[i] = buckets[hash];
			buckets[hash] = i;
			BlockMap[4+i] = BlockMap.Size ();
			BlockMap.Push (zero);
			array = &(*block)[0];
			for (size_t j = 0; j < block->Size(); ++j)
			{
				BlockMap.Push (array[j]);
			}
			BlockMap.Push (terminator);
			nothashed++;
		}
	}

	delete[] hashes;

//	printf ("%d blocks written, %d blocks saved\n", nothashed, hashed);
}

#define BLOCKBITS 7
#define BLOCKSIZE 128

static void P_CreateBlockMap ()
{
	TArray<WORD> *BlockLists, *block, *endblock;
	WORD adder;
	int bmapwidth, bmapheight;
	int minx, maxx, miny, maxy;
	int i;
	WORD line;

	if (numvertexes <= 0)
		return;

	// Find map extents for the blockmap
	minx = maxx = vertexes[0].x;
	miny = maxy = vertexes[0].y;

	for (i = 1; i < numvertexes; ++i)
	{
			 if (vertexes[i].x < minx) minx = vertexes[i].x;
		else if (vertexes[i].x > maxx) maxx = vertexes[i].x;
			 if (vertexes[i].y < miny) miny = vertexes[i].y;
		else if (vertexes[i].y > maxy) maxy = vertexes[i].y;
	}

	maxx >>= FRACBITS;
	minx >>= FRACBITS;
	maxy >>= FRACBITS;
	miny >>= FRACBITS;

	bmapwidth =	 ((maxx - minx) >> BLOCKBITS) + 1;
	bmapheight = ((maxy - miny) >> BLOCKBITS) + 1;

	TArray<int> BlockMap (bmapwidth * bmapheight * 3 + 4);

	adder = minx;			BlockMap.Push (adder);
	adder = miny;			BlockMap.Push (adder);
	adder = bmapwidth;		BlockMap.Push (adder);
	adder = bmapheight;		BlockMap.Push (adder);

	BlockLists = new TArray<WORD>[bmapwidth * bmapheight];

	for (line = 0; line < numlines; ++line)
	{
		int x1 = lines[line].v1->x >> FRACBITS;
		int y1 = lines[line].v1->y >> FRACBITS;
		int x2 = lines[line].v2->x >> FRACBITS;
		int y2 = lines[line].v2->y >> FRACBITS;
		int dx = x2 - x1;
		int dy = y2 - y1;
		int bx = (x1 - minx) >> BLOCKBITS;
		int by = (y1 - miny) >> BLOCKBITS;
		int bx2 = (x2 - minx) >> BLOCKBITS;
		int by2 = (y2 - miny) >> BLOCKBITS;

		block = &BlockLists[bx + by * bmapwidth];
		endblock = &BlockLists[bx2 + by2 * bmapwidth];

		if (block == endblock)	// Single block
		{
			block->Push (line);
		}
		else if (by == by2)		// Horizontal line
		{
			if (bx > bx2)
			{
				swap (block, endblock);
			}
			do
			{
				block->Push (line);
				block += 1;
			} while (block <= endblock);
		}
		else if (bx == bx2)	// Vertical line
		{
			if (by > by2)
			{
				swap (block, endblock);
			}
			do
			{
				block->Push (line);
				block += bmapwidth;
			} while (block <= endblock);
		}
		else				// Diagonal line
		{
			int xchange = (dx < 0) ? -1 : 1;
			int ychange = (dy < 0) ? -1 : 1;
			int ymove = ychange * bmapwidth;
			int adx = abs (dx);
			int ady = abs (dy);

			if (adx == ady)		// 45 degrees
			{
				int xb = (x1 - minx) & (BLOCKSIZE-1);
				int yb = (y1 - miny) & (BLOCKSIZE-1);
				if (dx < 0)
				{
					xb = BLOCKSIZE-xb;
				}
				if (dy < 0)
				{
					yb = BLOCKSIZE-yb;
				}
				if (xb < yb)
					adx--;
			}
			if (adx >= ady)		// X-major
			{
				int yadd = dy < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((by << BLOCKBITS) + yadd - (y1 - miny), dx, dy) + (x1 - minx)) >> BLOCKBITS;
					while (bx != stop)
					{
						block->Push (line);
						block += xchange;
						bx += xchange;
					}
					block->Push (line);
					block += ymove;
					by += ychange;
				} while (by != by2);
				while (block != endblock)
				{
					block->Push (line);
					block += xchange;
				}
				block->Push (line);
			}
			else					// Y-major
			{
				int xadd = dx < 0 ? -1 : BLOCKSIZE;
				do
				{
					int stop = (Scale ((bx << BLOCKBITS) + xadd - (x1 - minx), dy, dx) + (y1 - miny)) >> BLOCKBITS;
					while (by != stop)
					{
						block->Push (line);
						block += ymove;
						by += ychange;
					}
					block->Push (line);
					block += xchange;
					bx += xchange;
				} while (bx != bx2);
				while (block != endblock)
				{
					block->Push (line);
					block += ymove;
				}
				block->Push (line);
			}
		}
	}

	BlockMap.Reserve (bmapwidth * bmapheight);
	CreatePackedBlockmap (BlockMap, BlockLists, bmapwidth, bmapheight);
	delete[] BlockLists;

	blockmaplump = new int[BlockMap.Size()];
	for (size_t ii = 0; ii < BlockMap.Size(); ++ii)
	{
		blockmaplump[ii] = BlockMap[ii];
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

void P_LoadBlockMap (int lump)
{
	int count;

	if (ForceNodeBuild || genblockmap ||
		(count = W_LumpLength(lump)/2) >= 0x10000 ||
		Args.CheckParm("-blockmap") ||
		W_LumpLength (lump) == 0)
	{
		DPrintf ("Generating BLOCKMAP\n");
		P_CreateBlockMap ();
	}
	else
	{
		const short *wadblockmaplump = (short *)W_MapLumpNum (lump);
		int i;
		blockmaplump = new int[count];

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

		W_UnMapLump (wadblockmaplump);
	}

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

	// clear out mobj chains
	count = bmapwidth*bmapheight;
	blocklinks = new AActor *[count];
	memset (blocklinks, 0, count*sizeof(*blocklinks));
	blockmap = blockmaplump+4;
}



//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
// [RH] Handles extra lights
//
struct linf { short tag; WORD count; };
line_t**				linebuffer;

static void P_GroupLines (bool buildmap)
{
	TArray<linf>		exLightTags;
	int 				i;
	int 				j;
	int 				total;
	int					totallights;
	line_t* 			li;
	sector_t*			sector;
	DBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	size_t				ii, jj;
		
	// look up sector number for each subsector
	for (i = 0; i < numsubsectors; i++)
	{
		subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;
		subsectors[i].validcount = validcount;

		double accumx = 0.0, accumy = 0.0;

		for (jj = 0; jj < subsectors[i].numlines; ++jj)
		{
			seg_t *seg = &segs[subsectors[i].firstline + jj];
			seg->Subsector = &subsectors[i];
			accumx += seg->v1->x + seg->v2->x;
			accumy += seg->v1->y + seg->v2->y;
		}
		subsectors[i].CenterX = fixed_t(accumx * 0.5 / subsectors[i].numlines);
		subsectors[i].CenterY = fixed_t(accumy * 0.5 / subsectors[i].numlines);
	}

	// count number of lines in each sector
	total = 0;
	totallights = 0;
	for (i = 0, li = lines; i < numlines; i++, li++)
	{
		if (li->frontsector == NULL)
		{
			if (!flaggedNoFronts)
			{
				flaggedNoFronts = true;
				Printf ("The following lines do not have a front sidedef:\n");
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

		// [RH] Count extra lights
		if (li->special == ExtraFloor_LightOnly)
		{
			int adder = li->args[1] == 1 ? 2 : 1;

			for (ii = 0; ii < exLightTags.Size(); ++ii)
			{
				if (exLightTags[ii].tag == li->args[0])
					break;
			}
			if (ii == exLightTags.Size())
			{
				linf info = { li->args[0], adder };
				exLightTags.Push (info);
				totallights += adder;
			}
			else
			{
				totallights += adder;
				exLightTags[ii].count += adder;
			}
		}
	}
	if (flaggedNoFronts)
	{
		I_Error ("You need to fix these lines to play this map.\n");
	}

	// collect extra light info
	LightStacks = new FLightStack[totallights];
	ExtraLights = new FExtraLight[exLightTags.Size()];
	memset (ExtraLights, 0, exLightTags.Size()*sizeof(FExtraLight));

	for (ii = 0, jj = 0; ii < exLightTags.Size(); ++ii)
	{
		ExtraLights[ii].Tag = exLightTags[ii].tag;
		ExtraLights[ii].NumLights = exLightTags[ii].count;
		ExtraLights[ii].Lights = &LightStacks[jj];
		jj += ExtraLights[ii].NumLights;
	}

	// build line tables for each sector		
	linebuffer = new line_t *[total];
	line_t **lineb_p = linebuffer;
	sector = sectors;
	for (i = 0; i < numsectors; i++, sector++)
	{
		bbox.ClearBox ();
		if (sector->linecount == 0)
		{
			Printf ("Sector %i (tag %i) has no lines\n", i, sector->tag);
			// 0 the sector's tag so that no specials can use it
			sector->tag = 0;
		}
		else
		{
			sector->lines = lineb_p;
			li = lines;
			for (j = 0; j < numlines; j++, li++)
			{
				if (li->frontsector == sector || li->backsector == sector)
				{
					*lineb_p++ = li;
					bbox.AddToBox (li->v1->x, li->v1->y);
					bbox.AddToBox (li->v2->x, li->v2->y);
				}
			}
			if (lineb_p - sector->lines != sector->linecount)
			{
				I_Error ("P_GroupLines: miscounted");
			}
		}

		// set the soundorg to the middle of the bounding box
		sector->soundorg[0] = (bbox.Right()+bbox.Left())/2;
		sector->soundorg[1] = (bbox.Top()+bbox.Bottom())/2;

		// Find a triangle in the sector for sorting extra lights
		// The points must be in the sector, because intersecting
		// planes are okay so long as they intersect beyond all
		// sectors that use them.
		if (sector->linecount == 0)
		{ // If the sector has no lines, its tag is guaranteed to be 0, which
		  // means it cannot be used for extralights. So just use some dummy
		  // vertices for the triangle.
			sector->Triangle[0] = vertexes;
			sector->Triangle[1] = vertexes;
			sector->Triangle[2] = vertexes;
		}
		else
		{
			sector->Triangle[0] = sector->lines[0]->v1;
			sector->Triangle[1] = sector->lines[0]->v2;
			sector->Triangle[2] = sector->Triangle[0];	// failsafe
			if (sector->linecount > 1)
			{
				fixed_t dx = sector->Triangle[1]->x - sector->Triangle[0]->x;
				fixed_t dy = sector->Triangle[1]->y - sector->Triangle[1]->y;
				// Find another point in the sector that does not lie
				// on the same line as the first two points.
				for (j = 2; j < sector->linecount*2; ++j)
				{
					vertex_t *v;

					v = (j & 1) ? sector->lines[j>>1]->v1 : sector->lines[j>>1]->v2;
					if (DMulScale32 (v->y - sector->Triangle[0]->y, dx,
									sector->Triangle[0]->x - v->x, dy) != 0)
					{
						sector->Triangle[2] = v;
					}
				}
			}
		}
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

	// [RH] Moved this here
	P_InitTagLists();   // killough 1/30/98: Create xref tables for tags

	if (!buildmap)
	{
		P_SetSlopes ();
	}

	for (i = 0, li = lines; i < numlines; ++i, ++li)
	{
		if (li->special == ExtraFloor_LightOnly)
		{
			for (ii = 0; ii < exLightTags.Size(); ++ii)
			{
				if (ExtraLights[ii].Tag == li->args[0])
					break;
			}
			if (ii < exLightTags.Size())
			{
				ExtraLights[ii].InsertLight (li->frontsector->ceilingplane, li, li->args[1] == 2);
				if (li->args[1] == 1)
				{
					ExtraLights[ii].InsertLight (li->frontsector->floorplane, li, 2);
				}
				j = -1;
				while ((j = P_FindSectorFromTag (li->args[0], j)) >= 0)
				{
					sectors[j].ExtraLights = &ExtraLights[ii];
				}
			}
		}
	}
}

void FExtraLight::InsertLight (const secplane_t &inplane, line_t *line, int type)
{
	// type 0 : !bottom, !flooder
	// type 1 : !bottom, flooder
	// type 2 : bottom, !flooder

	vertex_t **triangle = line->frontsector->Triangle;
	int i, j;
	fixed_t diff = 0;
	secplane_t plane = inplane;

	if (type != 2)
	{
		plane.FlipVert ();
	}

	// Find the first plane this light is above and insert it there
	for (i = 0; i < NumUsedLights; ++i)
	{
		for (j = 0; j < 3; ++j)
		{
			diff = plane.ZatPoint (triangle[j]) - Lights[i].Plane.ZatPoint (triangle[j]);
			if (diff != 0)
			{
				break;
			}
		}
		if (diff >= 0)
		{
			break;
		}
	}
	if (i < NumLights)
	{
		for (j = MIN<int>(NumUsedLights, NumLights-1); j > i; --j)
		{
			Lights[j] = Lights[j-1];
		}
		Lights[i].Plane = plane;
		Lights[i].Master = type == 2 ? NULL : line->frontsector;
		Lights[i].bBottom = type == 2;
		Lights[i].bFlooder = type == 1;
		Lights[i].bOverlaps = diff == 0;
		if (NumUsedLights < NumLights)
		{
			++NumUsedLights;
		}
	}
}

//
// P_LoadReject
//
void P_LoadReject (int lump, bool junk)
{
	const int neededsize = (numsectors * numsectors + 7) >> 3;
	int rejectsize = junk ? 0 : W_LumpLength (lump);

	if (rejectsize < neededsize)
	{
		if (rejectsize > 0)
		{
			Printf ("REJECT is %d byte%s too small.\n", neededsize - rejectsize,
				neededsize-rejectsize==1?"":"s");
			rejectmatrix = new byte[neededsize];
			rejectmapped = false;
			W_ReadLump (lump, rejectmatrix);
			memset (rejectmatrix+rejectsize, 0, neededsize-rejectsize);
		}
		else
		{
			rejectmatrix = NULL;
		}
	}
	else
	{
		rejectmatrix = (byte *)W_MapLumpNum (lump);
		rejectmapped = true;
	}

	// Check if the reject has some actual content. If not, free it.
	rejectsize = MIN (rejectsize, neededsize);
	int qwords = rejectsize / 8;
	int i;

	if (qwords > 0)
	{
		QWORD *qreject = (QWORD *)rejectmatrix;

		for (i = 0; i < qwords; ++i)
		{
			if (qreject != 0)
				return;
		}
	}
	rejectsize &= 7;
	qwords *= 8;
	for (i = 0; i < rejectsize; ++i)
	{
		if (rejectmatrix[qwords+rejectsize] != 0)
			return;
	}

	// Reject has no data, so pretend it isn't there.
	if (rejectmapped)
	{
		W_UnMapLump (rejectmatrix);
	}
	else
	{
		delete[] rejectmatrix;
	}
	rejectmatrix = NULL;
}

//
// [RH] P_LoadBehavior
//
void P_LoadBehavior (int lumpnum)
{
	level.behavior = FBehavior::StaticLoadModule (lumpnum);
	if (!FBehavior::StaticCheckAllGood ())
	{
		Printf ("ACS scripts unloaded.\n");
		FBehavior::StaticUnloadModules ();
		level.behavior = NULL;
	}
}

// Hash the sector tags across the sectors and linedefs.
static void P_InitTagLists ()
{
	int i;

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

static void P_GetPolySpots (int lump, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors)
{
	if (HasBehavior)
	{
		int spot1, spot2, anchor;
		const mapthing2_t *mt = (mapthing2_t *)W_MapLumpNum (lump);
		int num = W_LumpLength (lump) / sizeof(*mt);

		if (HexenHack)
		{
			spot1 = SHORT(PO_HEX_SPAWN_TYPE);
			spot2 = SHORT(PO_HEX_SPAWNCRUSH_TYPE);
			anchor = SHORT(PO_HEX_ANCHOR_TYPE);
		}
		else
		{
			spot1 = SHORT(PO_SPAWN_TYPE);
			spot2 = SHORT(PO_SPAWNCRUSH_TYPE);
			anchor = SHORT(PO_ANCHOR_TYPE);
		}

		for (int i = 0; i < num; ++i)
		{
			if (mt[i].type == spot1 || mt[i].type == spot2 || mt[i].type == anchor)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = SHORT(mt[i].x) << FRACBITS;
				newvert.y = SHORT(mt[i].y) << FRACBITS;
				newvert.polynum = SHORT(mt[i].angle);
				if (mt[i].type == anchor)
				{
					anchors.Push (newvert);
				}
				else
				{
					spots.Push (newvert);
				}
			}
		}
	}
}

//
// P_SetupLevel
//
extern polyblock_t **PolyBlockMap;

// [RH] position indicates the start spot to spawn at
void P_SetupLevel (char *lumpname, int position)
{
	mapthing2_t buildstart;
	int i, lumpnum;
	bool buildmap;

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
	// [RH] Set default scripted translation colors
	for (i = 0; i < 256; ++i)
	{
		translationtables[TRANSLATION_LevelScripted][i] = i;
	}
	for (i = 1; i < MAX_ACS_TRANSLATIONS; ++i)
	{
		memcpy (&translationtables[TRANSLATION_LevelScripted][i*256],
				translationtables[TRANSLATION_LevelScripted], 256);
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

	// Free all level data from the previous map
	DThinker::DestroyAllThinkers ();
	if (vertexes != NULL)
	{
		delete[] vertexes;
		vertexes = NULL;
	}
	if (segs != NULL)
	{
		delete[] segs;
		segs = NULL;
	}
	if (sectors != NULL)
	{
		delete[] sectors;
		sectors = NULL;
	}
	if (subsectors != NULL)
	{
		delete[] subsectors;
		subsectors = NULL;
	}
	if (nodes != NULL)
	{
		delete[] nodes;
		nodes = NULL;
	}
	if (lines != NULL)
	{
		delete[] lines;
		lines = NULL;
	}
	if (sides != NULL)
	{
		delete[] sides;
		sides = NULL;
	}
	if (blockmaplump != NULL)
	{
		delete[] blockmaplump;
		blockmaplump = NULL;
	}
	if (blocklinks != NULL)
	{
		delete[] blocklinks;
		blocklinks = NULL;
	}
	if (PolyBlockMap != NULL)
	{
		for (i = bmapwidth*bmapheight-1; i >= 0; --i)
		{
			polyblock_t *link = PolyBlockMap[i];
			while (link != NULL)
			{
				polyblock_t *next = link->next;
				delete link;
				link = next;
			}
		}
		delete[] PolyBlockMap;
		PolyBlockMap = NULL;
	}
	if (rejectmatrix != NULL)
	{
		if (rejectmapped)
		{
			W_UnMapLump (rejectmatrix);
		}
		else
		{
			delete[] rejectmatrix;
		}
		rejectmatrix = NULL;
	}
	if (LightStacks != NULL)
	{
		delete[] LightStacks;
		LightStacks = NULL;
	}
	if (ExtraLights != NULL)
	{
		delete[] ExtraLights;
		ExtraLights = NULL;
	}
	if (linebuffer != NULL)
	{
		delete[] linebuffer;
		linebuffer = NULL;
	}
	if (polyobjs != NULL)
	{
		for (i = 0; i < po_NumPolyobjs; ++i)
		{
			if (polyobjs[i].segs != NULL)
			{
				delete[] polyobjs[i].segs;
			}
			if (polyobjs[i].originalPts != NULL)
			{
				delete[] polyobjs[i].originalPts;
			}
			if (polyobjs[i].prevPts != NULL)
			{
				delete[] polyobjs[i].prevPts;
			}
		}
		delete[] polyobjs;
		polyobjs = NULL;
	}

	// UNUSED W_Profile ();

	// find map num
	level.lumpnum = lumpnum = W_GetNumForName (lumpname);

	// [RH] Support loading Build maps (because I felt like it. :-)
	buildmap = false;
	if (W_LumpLength (lumpnum) > 0)
	{
		BYTE *mapdata = new BYTE[W_LumpLength (lumpnum)];
		W_ReadLump (lumpnum, mapdata);
		buildmap = P_LoadBuildMap (mapdata, W_LumpLength (lumpnum), &buildstart);
		delete[] mapdata;
	}

	if (!buildmap)
	{
		// [RH] Check if this map is Hexen-style.
		//		LINEDEFS and THINGS need to be handled accordingly.
		HasBehavior = W_CheckLumpName (lumpnum+ML_BEHAVIOR, "BEHAVIOR");

		// note: most of this ordering is important 

		ForceNodeBuild = gennodes;
		// [RH] Load in the BEHAVIOR lump
		FBehavior::StaticUnloadModules ();
		level.behavior = NULL;
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
		delete[] linemap;
		linemap = NULL;
	}
	else
	{
		ForceNodeBuild = true;
	}

	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);

	UsingGLNodes = false;
	if (!ForceNodeBuild) P_LoadSubsectors (lumpnum+ML_SSECTORS);
	if (!ForceNodeBuild) P_LoadNodes (lumpnum+ML_NODES);
	if (!ForceNodeBuild) P_LoadSegs (lumpnum+ML_SEGS);
	if (ForceNodeBuild)
	{
		QWORD startTime, endTime;

		startTime = I_MSTime ();
		TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
		P_GetPolySpots (lumpnum+ML_THINGS, polyspots, anchors);
		FNodeBuilder::FLevel leveldata =
		{
			vertexes, numvertexes,
			sides, numsides,
			lines, numlines
		};
		FNodeBuilder builder (leveldata, polyspots, anchors, r_experimental||genglnodes);
		UsingGLNodes = r_experimental;
		delete[] vertexes;
		builder.Extract (nodes, numnodes,
			segs, numsegs,
			subsectors, numsubsectors,
			vertexes, numvertexes);
		endTime = I_MSTime ();
		Printf ("BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, numsegs);
	}

	P_LoadReject (lumpnum+ML_REJECT, buildmap);

	P_GroupLines (buildmap);

	bodyqueslot = 0;
// phares 8/10/98: Clear body queue so the corpses from previous games are
// not assumed to be from this one.

	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	po_NumPolyobjs = 0;

	deathmatchstarts.Clear ();

	if (!buildmap)
	{
		if (!HasBehavior)
			P_LoadThings (lumpnum+ML_THINGS);
		else
			P_LoadThings2 (lumpnum+ML_THINGS, position);	// [RH] Load Hexen-style things

		if (!HasBehavior)
			P_TranslateTeleportThings ();	// [RH] Assign teleport destination TIDs
	}
	else
	{
		P_SpawnMapThing (&buildstart, 0);
	}

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

	R_OldBlend = 0xffffffff;

	// [RH] Remove all particles
	R_ClearParticles ();

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
