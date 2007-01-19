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
#include "doomerrors.h"
#include "gi.h"
#include "p_conversation.h"
#include "a_keys.h"
#include "s_sndseq.h"
#include "sbar.h"
#include "p_setup.h"

extern void P_SpawnMapThing (mapthing2_t *mthing, int position);
extern bool P_LoadBuildMap (BYTE *mapdata, size_t len, mapthing2_t **things, int *numthings);

extern void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
extern void P_TranslateTeleportThings (void);
extern int	P_TranslateSectorSpecial (int);

extern int numinterpolations;
extern unsigned int R_OldBlend;

CVAR (Bool, genblockmap, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, gennodes, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, genglnodes, false, CVAR_SERVERINFO);
CVAR (Bool, showloadtimes, false, 0);

static void P_InitTagLists ();
static void P_Shutdown ();



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

int				numzones;
zone_t*			zones;

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
			short alpha;
			DWORD map;
		} a;

		// Used when grouping sidedefs into loops.
		struct
		{
			DWORD first, next;
			char lineside;
		} b;
	};
}				*sidetemp;
static WORD		*linemap;

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

FBlockNode**	blocklinks;		// for thing chains
			


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//	LineOf Sight calculation.
// Without special effect, this could be
//	used as a PVS lookup as well.
//
BYTE*			rejectmatrix;

static bool		ForceNodeBuild;

// Maintain single and multi player starting spots.
TArray<mapthing2_t> deathmatchstarts (16);
mapthing2_t		playerstarts[MAXPLAYERS];

static void P_AllocateSideDefs (int count);
static void P_SetSideNum (DWORD *sidenum_p, WORD sidenum);




//===========================================================================
//
// GetMapIndex
//
// Gets the type of map lump or -1 if invalid or -2 if required and not found.
//
//===========================================================================

struct checkstruct
{
	const char lumpname[9];
	bool  required;
};

static int GetMapIndex(const char *mapname, int lastindex, const char *lumpname, bool needrequired)
{
	static const checkstruct check[] = 
	{
		{"",		 true},
		{"THINGS",	 true},
		{"LINEDEFS", true},
		{"SIDEDEFS", true},
		{"VERTEXES", true},
		{"SEGS",	 false},
		{"SSECTORS", false},
		{"NODES",	 false},
		{"SECTORS",	 true},
		{"REJECT",	 false},
		{"BLOCKMAP", false},
		{"BEHAVIOR", false},
		//{"SCRIPTS",	 false},
	};

	if (lumpname==NULL) lumpname="";

	for(size_t i=lastindex+1;i<countof(check);i++)
	{
		if (!strnicmp(lumpname, check[i].lumpname, 8))
			return (int)i;

		if (check[i].required)
		{
			if (needrequired)
			{
				I_Error("'%s' not found in %s\n", check[i].lumpname, mapname);
			}
			return -2;
		}
	}

	return -1;	// End of map reached
}

//===========================================================================
//
// Opens a map for reading
//
//===========================================================================

MapData *P_OpenMapData(const char * mapname)
{
	MapData * map = new MapData;
	bool externalfile = !strnicmp(mapname, "file:", 5);
	
	
	if (externalfile)
	{
		mapname += 5;
		if (!FileExists(mapname))
		{
			delete map;
			return NULL;
		}
		map->file = new FileReader(mapname);
		map->CloseOnDestruct = true;
	}
	else
	{
		FString fmt;
		int lump_wad;
		int lump_map;
		int lump_name;
		
		// Check for both *.wad and *.map in order to load Build maps
		// as well. The higher one will take precedence.
		lump_name = Wads.CheckNumForName(mapname);
		fmt.Format("maps/%s.wad", mapname);
		lump_wad = Wads.CheckNumForFullName(fmt);
		fmt.Format("maps/%s.map", mapname);
		lump_map = Wads.CheckNumForFullName(fmt);
		
		if (lump_name > lump_wad && lump_name > lump_map && lump_name != -1)
		{
			int lumpfile=Wads.GetLumpFile(lump_name);
			int nextfile=Wads.GetLumpFile(lump_name+1);

			if (lumpfile != nextfile)
			{
				// The following lump is from a different file so whatever this is,
				// it is not a multi-lump Doom level.
				return map;
			}

			// This case can only happen if the lump is inside a real WAD file.
			// As such any special handling for other types of lumps is skipped.
			map->file = Wads.GetFileReader(lumpfile);
			map->CloseOnDestruct = false;
			map->lumpnum = lump_name;

			map->MapLumps[0].FilePos = Wads.GetLumpOffset(lump_name);
			map->MapLumps[0].Size = Wads.LumpLength(lump_name);
			map->Encrypted = Wads.IsEncryptedFile(lump_name);

			if (map->Encrypted)
			{ // If it's encrypted, then it's a Blood file, presumably a map.
				return map;
			}

			int index = 0;
			for(int i = 1;; i++)
			{
				// Since levels must be stored in WADs they can't really have full
				// names and for any valid level lump this always returns the short name.
				const char * lumpname = Wads.GetLumpFullName(lump_name + i);
				index = GetMapIndex(mapname, index, lumpname, i != 1 || map->MapLumps[0].Size == 0);
				if (index == ML_BEHAVIOR) map->HasBehavior = true;

				// The next lump is not part of this map anymore
				if (index < 0) break;

				map->MapLumps[index].FilePos = Wads.GetLumpOffset(lump_name + i);
				map->MapLumps[index].Size = Wads.LumpLength(lump_name + i);
				strncpy(map->MapLumps[index].Name, lumpname, 8);
			}
			return map;
		}
		else
		{
			if (lump_map > lump_wad)
			{
				lump_wad = lump_map;
			}
			if (lump_wad == -1)
			{
				delete map;
				return NULL;
			}
			map->lumpnum = lump_wad;
			map->file = Wads.ReopenLumpNum(lump_wad);
			map->CloseOnDestruct = true;
		}
	}
	DWORD id;
	(*map->file) >> id;
	
	if (id == IWAD_ID || id == PWAD_ID)
	{
		char maplabel[9]="";
		int index=0;
		DWORD dirofs, numentries;

		(*map->file) >> numentries >> dirofs;

		map->file->Seek(dirofs, SEEK_SET);
		for(DWORD i = 0; i < numentries; i++)
		{
			DWORD offset, size;
			char lumpname[8];

			(*map->file) >> offset >> size;
			map->file->Read(lumpname, 8);

			if (i>0)
			{
				index = GetMapIndex(maplabel, index, lumpname, true);
				if (index == ML_BEHAVIOR) map->HasBehavior = true;

				// The next lump is not part of this map anymore
				if (index < 0) break;
			}
			else
			{
				strncpy(maplabel, lumpname, 8);
				maplabel[8]=0;
			}

			map->MapLumps[index].FilePos = offset;
			map->MapLumps[index].Size = size;
			strncpy(map->MapLumps[index].Name, lumpname, 8);
		}
	}
	else
	{
		// This is a Build map and not subject to WAD consistency checks.
		map->MapLumps[0].Size = map->file->GetLength();
	}
	return map;		
}


//===========================================================================
//
// [RH] Figure out blends for deep water sectors
//
//===========================================================================

static void SetTexture (short *texture, DWORD *blend, char *name8)
{
	char name[9];
	strncpy (name, name8, 8);
	name[8] = 0;
	if ((*blend = R_ColormapNumForName (name)) == 0)
	{
		if ((*texture = TexMan.CheckForTexture (name, FTexture::TEX_Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny)
			) == -1)
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

static void SetTextureNoErr (short *texture, DWORD *color, char *name8, bool *validcolor)
{
	char name[9];
	strncpy (name, name8, 8);
	name[8] = 0;
	if ((*texture = TexMan.CheckForTexture (name, FTexture::TEX_Wall,
		FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny)
		) == -1)
	{
		char name2[9];
		char *stop;
		strncpy (name2, name, 8);
		name2[8] = 0;
		*color = strtoul (name2, &stop, 16);
		*texture = 0;
		*validcolor = (*stop == 0) && (stop >= name2 + 2) && (stop <= name2 + 6);
	}
	else
	{
		*validcolor = false;
	}
}

//===========================================================================
//
// Sound enviroment handling
//
//===========================================================================

void P_FloodZone (sector_t *sec, int zonenum)
{
	int i;

	if (sec->ZoneNumber == zonenum)
		return;

	sec->ZoneNumber = zonenum;

	for (i = 0; i < sec->linecount; ++i)
	{
		line_t *check = sec->lines[i];
		sector_t *other;

		if (check->sidenum[1] == NO_SIDE || (check->flags & ML_ZONEBOUNDARY))
			continue;

		if (check->frontsector == sec)
			other = check->backsector;
		else
			other = check->frontsector;

		if (other->ZoneNumber != zonenum)
			P_FloodZone (other, zonenum);
	}
}

void P_FloodZones ()
{
	int z = 0, i;

	for (i = 0; i < numsectors; ++i)
	{
		if (sectors[i].ZoneNumber == 0xFFFF)
		{
			P_FloodZone (&sectors[i], z++);
		}
	}
	numzones = z;
	zones = new zone_t[z];
	for (i = 0; i < z; ++i)
	{
		zones[i].Environment = DefaultEnvironments[0];
	}
}

//===========================================================================
//
// P_LoadVertexes
//
//===========================================================================

void P_LoadVertexes (MapData * map)
{
	int i;

	// Determine number of vertices:
	//	total lump length / vertex record length.
	numvertexes = map->MapLumps[ML_VERTEXES].Size / sizeof(mapvertex_t);

	if (numvertexes == 0)
	{
		I_Error ("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	vertexes = new vertex_t[numvertexes];		

	map->Seek(ML_VERTEXES);
		
	// Copy and convert vertex coordinates, internal representation as fixed.
	for (i = 0; i < numvertexes; i++)
	{
		SWORD x, y;

		(*map->file) >> x >> y;
		vertexes[i].x = x << FRACBITS;
		vertexes[i].y = y << FRACBITS;
	}
}

//===========================================================================
//
// P_LoadZSegs
//
//===========================================================================

void P_LoadZSegs (FileReaderZ &data)
{
	for (int i = 0; i < numsegs; ++i)
	{
		line_t *ldef;
		DWORD v1, v2;
		WORD line;
		BYTE side;

		data >> v1 >> v2 >> line >> side;

		segs[i].v1 = &vertexes[v1];
		segs[i].v2 = &vertexes[v2];
		segs[i].linedef = ldef = &lines[line];
		segs[i].sidedef = &sides[ldef->sidenum[side]];
		segs[i].PartnerSeg = NULL;
		segs[i].frontsector = sides[ldef->sidenum[side]].sector;
		if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != NO_SIDE)
		{
			segs[i].backsector = sides[ldef->sidenum[side^1]].sector;
		}
		else
		{
			segs[i].backsector = 0;
			ldef->flags &= ~ML_TWOSIDED;
		}
	}
}

//===========================================================================
//
// P_LoadGLZSegs
//
// This is the GL nodes version of the above function.
//
//===========================================================================

void P_LoadGLZSegs (FileReaderZ &data)
{
	for (int i = 0; i < numsubsectors; ++i)
	{
		for (size_t j = 0; j < subsectors[i].numlines; ++j)
		{
			seg_t *seg;
			DWORD v1, partner;
			WORD line;
			BYTE side;

			data >> v1 >> partner >> line >> side;

			seg = &segs[subsectors[i].firstline + j];
			seg->v1 = &vertexes[v1];
			if (j == 0)
			{
				seg[subsectors[i].numlines - 1].v2 = seg->v1;
			}
			else
			{
				seg[-1].v2 = seg->v1;
			}
			if (partner == 0xFFFFFFFF)
			{
				seg->PartnerSeg = NULL;
			}
			else
			{
				seg->PartnerSeg = &segs[partner];
			}
			if (line != 0xFFFF)
			{
				line_t *ldef;

				seg->linedef = ldef = &lines[line];
				seg->sidedef = &sides[ldef->sidenum[side]];
				seg->frontsector = sides[ldef->sidenum[side]].sector;
				if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != NO_SIDE)
				{
					seg->backsector = sides[ldef->sidenum[side^1]].sector;
				}
				else
				{
					seg->backsector = 0;
					ldef->flags &= ~ML_TWOSIDED;
				}
			}
			else
			{
				seg->linedef = NULL;
				seg->sidedef = NULL;
				seg->frontsector = seg->backsector = segs[subsectors[i].firstline].frontsector;
			}
		}
	}
}

//===========================================================================
//
// P_LoadZNodes
//
//===========================================================================

static void P_LoadZNodes (FileReader &dalump, DWORD id)
{
	FileReaderZ data (dalump);
	DWORD i;

	// Read extra vertices added during node building
	DWORD orgVerts, newVerts;
	vertex_t *newvertarray;

	data >> orgVerts >> newVerts;
	if (orgVerts + newVerts == (DWORD)numvertexes)
	{
		newvertarray = vertexes;
	}
	else
	{
		newvertarray = new vertex_t[orgVerts + newVerts];
		memcpy (newvertarray, vertexes, orgVerts * sizeof(vertex_t));
	}
	for (i = 0; i < newVerts; ++i)
	{
		data >> newvertarray[i + orgVerts].x >> newvertarray[i + orgVerts].y;
	}
	if (vertexes != newvertarray)
	{
		for (i = 0; i < (DWORD)numlines; ++i)
		{
			lines[i].v1 = lines[i].v1 - vertexes + newvertarray;
			lines[i].v2 = lines[i].v2 - vertexes + newvertarray;
		}
		delete[] vertexes;
		vertexes = newvertarray;
		numvertexes = orgVerts + newVerts;
	}

	// Read the subsectors
	DWORD numSubs, currSeg;

	data >> numSubs;
	numsubsectors = numSubs;
	subsectors = new subsector_t[numSubs];
	memset (subsectors, 0, numsubsectors*sizeof(subsector_t));

	for (i = currSeg = 0; i < numSubs; ++i)
	{
		DWORD numsegs;

		data >> numsegs;
		subsectors[i].firstline = currSeg;
		subsectors[i].numlines = numsegs;
		currSeg += numsegs;
	}

	// Read the segs
	DWORD numSegs;

	data >> numSegs;

	// The number of segs stored should match the number of
	// segs used by subsectors.
	if (numSegs != currSeg)
	{
		Printf ("Incorrect number of segs in nodes.\n");
		delete[] subsectors;
		ForceNodeBuild = true;
		return;
	}

	numsegs = numSegs;
	segs = new seg_t[numsegs];
	memset (segs, 0, numsegs*sizeof(seg_t));

	if (id == MAKE_ID('Z','N','O','D'))
	{
		P_LoadZSegs (data);
	}
	else
	{
		P_LoadGLZSegs (data);
	}

	// Read nodes
	DWORD numNodes;

	data >> numNodes;
	numnodes = numNodes;
	nodes = new node_t[numNodes];
	memset (nodes, 0, sizeof(node_t)*numNodes);

	for (i = 0; i < numNodes; ++i)
	{
		SWORD x, y, dx, dy;

		data >> x >> y >> dx >> dy;
		nodes[i].x = x << FRACBITS;
		nodes[i].y = y << FRACBITS;
		nodes[i].dx = dx << FRACBITS;
		nodes[i].dy = dy << FRACBITS;
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				SWORD coord;
				data >> coord;
				nodes[i].bbox[j][k] = coord << FRACBITS;
			}
		}
		for (int m = 0; m < 2; ++m)
		{
			DWORD child;
			data >> child;
			if (child & 0x80000000)
			{
				nodes[i].children[m] = (BYTE *)&subsectors[child & 0x7FFFFFFF] + 1;
			}
			else
			{
				nodes[i].children[m] = &nodes[child];
			}
		}
	}
}


//===========================================================================
//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

void P_LoadSegs (MapData * map)
{
	int  i;
	BYTE *data;
	BYTE *vertchanged = new BYTE[numvertexes];	// phares 10/4/98
	DWORD segangle;
	line_t* line;		// phares 10/4/98
	int ptp_angle;		// phares 10/4/98
	int delta_angle;	// phares 10/4/98
	int dis;			// phares 10/4/98
	int dx,dy;			// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98
	int lumplen = map->MapLumps[ML_SEGS].Size;

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	numsegs = lumplen / sizeof(mapseg_t);

	if (numsegs == 0)
	{
		Printf ("This map has no segs.\n");
		delete[] subsectors;
		delete[] nodes;
		delete[] vertchanged;
		ForceNodeBuild = true;
		return;
	}

	segs = new seg_t[numsegs];
	memset (segs, 0, numsegs*sizeof(seg_t));

	data = new BYTE[lumplen];
	map->Read(ML_SEGS, data);

	// phares: 10/4/98: Vertchanged is an array that represents the vertices.
	// Mark those used by linedefs. A marked vertex is one that is not a
	// candidate for movement further down.

	line = lines;
	for (i = 0; i < numlines ; i++, line++)
	{
		vertchanged[line->v1 - vertexes] = vertchanged[line->v2 - vertexes] = 1;
	}

	try
	{
		for (i = 0; i < numsegs; i++)
		{
			seg_t *li = segs+i;
			mapseg_t *ml = (mapseg_t *) data + i;

			int side, linedef;
			line_t *ldef;

			vnum1 = LittleShort(ml->v1);
			vnum2 = LittleShort(ml->v2);

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw i * 4;
			}

			li->v1 = &vertexes[vnum1];
			li->v2 = &vertexes[vnum2];
			li->PartnerSeg = NULL;

			segangle = (WORD)LittleShort(ml->angle);

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

			if (delta_angle != 0)
			{
				segangle >>= (ANGLETOFINESHIFT-16);
				dx = (li->v1->x - li->v2->x)>>FRACBITS;
				dy = (li->v1->y - li->v2->y)>>FRACBITS;
				dis = ((int) sqrt((double)(dx*dx + dy*dy)))<<FRACBITS;
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

			linedef = LittleShort(ml->linedef);
			if ((unsigned)linedef >= (unsigned)numlines)
			{
				throw i * 4 + 1;
			}
			ldef = &lines[linedef];
			li->linedef = ldef;
			side = LittleShort(ml->side);
			if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
			{
				throw i * 4 + 2;
			}
			li->sidedef = &sides[ldef->sidenum[side]];
			li->frontsector = sides[ldef->sidenum[side]].sector;

			// killough 5/3/98: ignore 2s flag if second sidedef missing:
			if (ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1] != NO_SIDE)
			{
				li->backsector = sides[ldef->sidenum[side^1]].sector;
			}
			else
			{
				li->backsector = 0;
				ldef->flags &= ~ML_TWOSIDED;
			}
		}
	}
	catch (int foo)
	{
		switch (foo & 3)
		{
		case 0:
			Printf ("Seg %d references a nonexistant vertex.\n", foo >> 2);
			break;

		case 1:
			Printf ("Seg %d references a nonexistant linedef.\n", foo >> 2);
			break;

		case 2:
			Printf ("The linedef for seg %d references a nonexistant sidedef.\n", foo >> 2);
			break;
		}
		Printf ("The BSP will be rebuilt.\n");
		delete[] segs;
		delete[] subsectors;
		delete[] nodes;
		ForceNodeBuild = true;
	}

	delete[] vertchanged; // phares 10/4/98
	delete[] data;
}


//===========================================================================
//
// P_LoadSubsectors
//
//===========================================================================

void P_LoadSubsectors (MapData * map)
{
	int i;
	DWORD maxseg = map->Size(ML_SEGS) / sizeof(mapseg_t);

	numsubsectors = map->MapLumps[ML_SSECTORS].Size / sizeof(mapsubsector_t);

	if (numsubsectors == 0 || maxseg == 0 )
	{
		Printf ("This map has an incomplete BSP tree.\n");
		delete[] nodes;
		ForceNodeBuild = true;
		return;
	}

	subsectors = new subsector_t[numsubsectors];		
	map->Seek(ML_SSECTORS);
		
	memset (subsectors, 0, numsubsectors*sizeof(subsector_t));
	
	for (i = 0; i < numsubsectors; i++)
	{
		WORD numsegs, firstseg;

		(*map->file) >> numsegs >> firstseg;

		if (numsegs == 0)
		{
			Printf ("Subsector %i is empty.\n", i);
			delete[] subsectors;
			delete[] nodes;
			ForceNodeBuild = true;
			return;
		}

		subsectors[i].numlines = numsegs;
		subsectors[i].firstline = firstseg;

		if (subsectors[i].firstline >= maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, subsectors[i].firstline,
				subsectors[i].firstline + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			delete[] nodes;
			delete[] subsectors;
			break;
		}
		else if (subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				subsectors[i].firstline + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			delete[] nodes;
			delete[] subsectors;
			break;
		}
	}
}


//===========================================================================
//
// P_LoadSectors
//
//===========================================================================

void P_LoadSectors (MapData * map)
{
	char				fname[9];
	int 				i;
	char				*msp;
	mapsector_t			*ms;
	sector_t*			ss;
	int					defSeqType;
	FDynamicColormap	*fogMap, *normMap;
	int					lumplen = map->Size(ML_SECTORS);

	numsectors = lumplen / sizeof(mapsector_t);
	sectors = new sector_t[numsectors];		
	memset (sectors, 0, numsectors*sizeof(sector_t));

	if (level.flags & LEVEL_SNDSEQTOTALCTRL)
		defSeqType = 0;
	else
		defSeqType = -1;

	fogMap = normMap = NULL;
	fname[8] = 0;

	msp = new char[lumplen];
	map->Read(ML_SECTORS, msp);
	ms = (mapsector_t*)msp;
	ss = sectors;
	for (i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->floortexz = LittleShort(ms->floorheight)<<FRACBITS;
		ss->floorplane.d = -ss->floortexz;
		ss->floorplane.c = FRACUNIT;
		ss->floorplane.ic = FRACUNIT;
		ss->ceilingtexz = LittleShort(ms->ceilingheight)<<FRACBITS;
		ss->ceilingplane.d = ss->ceilingtexz;
		ss->ceilingplane.c = -FRACUNIT;
		ss->ceilingplane.ic = -FRACUNIT;
		strncpy (fname, ms->floorpic, 8);
		ss->floorpic = TexMan.GetTexture (fname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
		strncpy (fname, ms->ceilingpic, 8);
		ss->ceilingpic = TexMan.GetTexture (fname, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
		ss->lightlevel = clamp (LittleShort(ms->lightlevel), (short)0, (short)255);
		if (map->HasBehavior)
			ss->special = LittleShort(ms->special);
		else	// [RH] Translate to new sector special
			ss->special = P_TranslateSectorSpecial (LittleShort(ms->special));
		ss->oldspecial = !!(ss->special&SECRET_MASK);
		ss->tag = LittleShort(ms->tag);
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
		ss->ZoneNumber = 0xFFFF;

		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		if (level.outsidefog != 0xff000000 && ss->ceilingpic == skyflatnum)
		{
			if (fogMap == NULL)
				fogMap = GetSpecialLights (PalEntry (255,255,255), level.outsidefog, 0);
			ss->ColorMap = fogMap;
		}
		else
		{
			if (normMap == NULL)
				normMap = GetSpecialLights (PalEntry (255,255,255), level.fadeto, NormalLight.Desaturate);
			ss->ColorMap = normMap;
		}

		// killough 8/28/98: initialize all sectors to normal friction
		ss->friction = ORIG_FRICTION;
		ss->movefactor = ORIG_FRICTION_FACTOR;
	}
	delete[] msp;
}


//===========================================================================
//
// P_LoadNodes
//
//===========================================================================

void P_LoadNodes (MapData * map)
{
	FMemLump	data;
	int 		i;
	int 		j;
	int 		k;
	char		*mnp;
	mapnode_t	*mn;
	node_t* 	no;
	WORD*		used;
	int			lumplen = map->Size(ML_NODES);
	int			maxss = map->Size(ML_SSECTORS) / sizeof(mapsubsector_t);

	numnodes = lumplen / sizeof(mapnode_t);

	if ((numnodes == 0 && maxss != 1) || maxss == 0)
	{
		ForceNodeBuild = true;
		return;
	}
	
	nodes = new node_t[numnodes];		
	used = (WORD *)alloca (sizeof(WORD)*numnodes);
	memset (used, 0, sizeof(WORD)*numnodes);

	mnp = new char[lumplen];
	mn = (mapnode_t*)mnp;
	map->Read(ML_NODES, mn);
	no = nodes;
	
	for (i = 0; i < numnodes; i++, no++, mn++)
	{
		no->x = LittleShort(mn->x)<<FRACBITS;
		no->y = LittleShort(mn->y)<<FRACBITS;
		no->dx = LittleShort(mn->dx)<<FRACBITS;
		no->dy = LittleShort(mn->dy)<<FRACBITS;
		for (j = 0; j < 2; j++)
		{
			WORD child = LittleShort(mn->children[j]);
			if (child & NF_SUBSECTOR)
			{
				child &= ~NF_SUBSECTOR;
				if (child >= maxss)
				{
					Printf ("BSP node %d references invalid subsector %d.\n"
						"The BSP will be rebuilt.\n", i, child);
					ForceNodeBuild = true;
					delete[] nodes;
					delete[] mnp;
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
				delete[] mnp;
				return;
			}
			else if (used[child])
			{
				Printf ("BSP node %d references node %d,\n"
					"which is already used by node %d.\n"
					"The BSP will be rebuilt.\n", i, child, used[child]-1);
				ForceNodeBuild = true;
				delete[] nodes;
				delete[] mnp;
				return;
			}
			else
			{
				no->children[j] = &nodes[child];
				used[child] = j + 1;
			}
			for (k = 0; k < 4; k++)
			{
				no->bbox[j][k] = LittleShort(mn->bbox[j][k])<<FRACBITS;
			}
		}
	}
	delete[] mnp;
}


//===========================================================================
//
// P_LoadThings
//
//===========================================================================

void P_LoadThings (MapData * map, int position)
{
	mapthing2_t mt2;		// [RH] for translation
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthing_t);

	char *mtp;
	mapthing_t *mt;

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mt = (mapthing_t*)mtp;

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	for (int i=0 ; i < numthings; i++, mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. R_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = LittleShort(mt->options);

		memset (&mt2, 0, sizeof(mt2));
		mt2.flags = (short)((flags & 0xf) | 0x7e0);
		if (gameinfo.gametype == GAME_Strife)
		{
			mt2.flags &= ~MTF_AMBUSH;
			if (flags & STF_SHADOW)			mt2.flags |= MTF_SHADOW;
			if (flags & STF_ALTSHADOW)		mt2.flags |= MTF_ALTSHADOW;
			if (flags & STF_STANDSTILL)		mt2.flags |= MTF_STANDSTILL;
			if (flags & STF_AMBUSH)			mt2.flags |= MTF_AMBUSH;
			if (flags & STF_FRIENDLY)		mt2.flags |= MTF_FRIENDLY;
		}
		else
		{
			if (flags & BTF_BADEDITORCHECK)
			{
				flags &= 0x1F;
			}
			if (flags & BTF_NOTDEATHMATCH)	mt2.flags &= ~MTF_DEATHMATCH;
			if (flags & BTF_NOTCOOPERATIVE)	mt2.flags &= ~MTF_COOPERATIVE;
			if (flags & BTF_FRIENDLY)		mt2.flags |= MTF_FRIENDLY;
		}
		if (flags & BTF_NOTSINGLE)			mt2.flags &= ~MTF_SINGLE;

		mt2.x = LittleShort(mt->x);
		mt2.y = LittleShort(mt->y);
		mt2.angle = LittleShort(mt->angle);
		mt2.type = LittleShort(mt->type);

		P_SpawnMapThing (&mt2, position);
	}
	delete [] mtp;
}

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

static void P_SlopeLineToPoint (int lineid, fixed_t x, fixed_t y, fixed_t z, bool slopeCeil)
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

		FVector3 p, v1, v2, cross;

		p[0] = FIXED2FLOAT (line->v1->x);
		p[1] = FIXED2FLOAT (line->v1->y);
		p[2] = FIXED2FLOAT (plane->ZatPoint (line->v1->x, line->v1->y));
		v1[0] = FIXED2FLOAT (line->dx);
		v1[1] = FIXED2FLOAT (line->dy);
		v1[2] = FIXED2FLOAT (plane->ZatPoint (line->v2->x, line->v2->y)) - p[2];
		v2[0] = FIXED2FLOAT (x - line->v1->x);
		v2[1] = FIXED2FLOAT (y - line->v1->y);
		v2[2] = FIXED2FLOAT (z) - p[2];

		cross = v1 ^ v2;
		double len = cross.Length();
		if (len == 0)
		{
			Printf ("Slope thing at (%d,%d) lies directly on its target line.\n", int(x>>16), int(y>>16));
			return;
		}
		cross /= len;
		// Fix backward normals
		if ((cross.Z < 0 && !slopeCeil) || (cross.Z > 0 && slopeCeil))
		{
			cross = -cross;
		}

		plane->a = FLOAT2FIXED (cross[0]);
		plane->b = FLOAT2FIXED (cross[1]);
		plane->c = FLOAT2FIXED (cross[2]);
		//plane->ic = FLOAT2FIXED (1.f/cross[2]);
		plane->ic = DivScale32 (1, plane->c);
		plane->d = -TMulScale16 (plane->a, x,
								 plane->b, y,
								 plane->c, z);
	}
}

//===========================================================================
//
// P_CopyPlane
//
//===========================================================================

static void P_CopyPlane (int tag, fixed_t x, fixed_t y, bool copyCeil)
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

//===========================================================================
//
// P_SetSlope
//
//===========================================================================

void P_SetSlope (secplane_t *plane, bool setCeil, int xyangi, int zangi,
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
		zang = Scale (zangi, ANGLE_90, 90);
	}
	if (setCeil)
	{
		zang += ANGLE_180;
	}
	zang >>= ANGLETOFINESHIFT;

	xyang = (angle_t)Scale (xyangi, ANGLE_90, 90 << ANGLETOFINESHIFT);

	FVector3 norm;

	norm[0] = float(finecosine[zang]) * float(finecosine[xyang]);
	norm[1] = float(finecosine[zang]) * float(finesine[xyang]);
	norm[2] = float(finesine[zang]) * 65536.f;
	norm.MakeUnit();
	plane->a = (int)(norm[0] * 65536.f);
	plane->b = (int)(norm[1] * 65536.f);
	plane->c = (int)(norm[2] * 65536.f);
	//plane->ic = (int)(65536.f / norm[2]);
	plane->ic = DivScale32 (1, plane->c);
	plane->d = -TMulScale16 (plane->a, x,
							 plane->b, y,
							 plane->c, z);
}


//===========================================================================
//
// P_VavoomSlope
//
//===========================================================================

void P_VavoomSlope(sector_t * sec, int id, fixed_t x, fixed_t y, fixed_t z, int which)
{
	for (int i=0;i<sec->linecount;i++)
	{
		line_t * l=sec->lines[i];

		if (l->args[0]==id)
		{
			FVector3 v1, v2, cross;
			secplane_t *srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
			fixed_t srcheight = (which == 0) ? sec->floortexz : sec->ceilingtexz;

			v1[0] = FIXED2FLOAT (x - l->v2->x);
			v1[1] = FIXED2FLOAT (y - l->v2->y);
			v1[2] = FIXED2FLOAT (z - srcheight);
			
			v2[0] = FIXED2FLOAT (x - l->v1->x);
			v2[1] = FIXED2FLOAT (y - l->v1->y);
			v2[2] = FIXED2FLOAT (z - srcheight);

			cross = v1 ^ v2;
			double len = cross.Length();
			if (len == 0)
			{
				Printf ("Slope thing at (%d,%d) lies directly on its target line.\n", int(x>>16), int(y>>16));
				return;
			}
			cross /= len;

			// Fix backward normals
			if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
			{
				cross = -cross;
			}


			srcplane->a = FLOAT2FIXED (cross[0]);
			srcplane->b = FLOAT2FIXED (cross[1]);
			srcplane->c = FLOAT2FIXED (cross[2]);
			//plane->ic = FLOAT2FIXED (1.f/cross[2]);
			srcplane->ic = DivScale32 (1, srcplane->c);
			srcplane->d = -TMulScale16 (srcplane->a, x,
										srcplane->b, y,
										srcplane->c, z);
			return;
		}
	}
}
				   
enum
{
	THING_SlopeFloorPointLine = 9500,
	THING_SlopeCeilingPointLine = 9501,
	THING_SetFloorSlope = 9502,
	THING_SetCeilingSlope = 9503,
	THING_CopyFloorPlane = 9510,
	THING_CopyCeilingPlane = 9511,
	THING_VavoomFloor=1500,
	THING_VavoomCeiling=1501,
};

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

static void P_SpawnSlopeMakers (mapthing2_t *firstmt, mapthing2_t *lastmt)
{
	mapthing2_t *mt;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if ((mt->type >= THING_SlopeFloorPointLine &&
			 mt->type <= THING_SetCeilingSlope) ||
			mt->type==THING_VavoomFloor || mt->type==THING_VavoomCeiling)
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
			if (mt->type==THING_VavoomFloor || mt->type==THING_VavoomCeiling)
			{
				P_VavoomSlope(sec, mt->thingid, x, y, mt->z<<FRACBITS, mt->type & 1); 
			}
			else if (mt->type <= THING_SlopeCeilingPointLine)
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

//===========================================================================
//
// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style. Position also controls which single-
// player start spots are spawned by filtering out those
// whose first parameter don't match position.
//
//===========================================================================

void P_LoadThings2 (MapData * map, int position)
{
	int	lumplen = map->MapLumps[ML_THINGS].Size;
	int numthings = lumplen / sizeof(mapthing2_t);

	int i;
	char *mtp;
	mapthing2_t *mt;

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);

#ifdef WORDS_BIGENDIAN
	for (i=0, mt = (mapthing2_t*)mtp; i < numthings; i++,mt++)
	{
		mt->thingid = LittleShort(mt->thingid);
		mt->x = LittleShort(mt->x);
		mt->y = LittleShort(mt->y);
		mt->z = LittleShort(mt->z);
		mt->angle = LittleShort(mt->angle);
		mt->type = LittleShort(mt->type);
		mt->flags = LittleShort(mt->flags);
	}
#endif

	// [RH] Spawn slope creating things first.
	P_SpawnSlopeMakers ((mapthing2_t*)mtp, ((mapthing2_t*)mtp)+numthings);

	for (i=0, mt = (mapthing2_t*)mtp; i < numthings; i++,mt++)
	{
		P_SpawnMapThing (mt, position);
	}
	delete[] mtp;
}


//===========================================================================
//
// P_LoadLineDefs
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// [RH] Actually split into four functions to allow for Hexen and Doom
//		linedefs.
//
//===========================================================================

void P_AdjustLine (line_t *ld)
{
	vertex_t *v1, *v2;

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
	// for Doom format maps this must be done in P_TranslateLineDef because
	// the tag doesn't always go into the first arg.
	if (level.flags & LEVEL_HEXENFORMAT)	
	{
		switch (ld->special)
		{
		case Line_SetIdentification:
			ld->id = ld->args[0] + 256 * ld->args[4];
			ld->flags |= ld->args[1]<<16;
			break;

		case TranslucentLine:
			ld->id = ld->args[0];
			ld->flags |= ld->args[3]<<16;
			break;

		case Teleport_Line:
		case Scroll_Texture_Model:
			ld->id = ld->args[0];
			break;

		case Polyobj_StartLine:
			ld->id = ld->args[3];
			break;

		case Polyobj_ExplicitLine:
			ld->id = ld->args[4];
			break;
		}
	}
}

void P_SaveLineSpecial (line_t *ld)
{
	if (*ld->sidenum == NO_SIDE)
		return;

	// killough 4/4/98: support special sidedef interpretation below
	if ((ld->sidenum[0] != NO_SIDE) &&
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
		ld->frontsector = ld->sidenum[0]!=NO_SIDE ? sides[ld->sidenum[0]].sector : 0;
		ld->backsector  = ld->sidenum[1]!=NO_SIDE ? sides[ld->sidenum[1]].sector : 0;
		float dx = FIXED2FLOAT(ld->v2->x - ld->v1->x);
		float dy = FIXED2FLOAT(ld->v2->y - ld->v1->y);
		SBYTE light;

		if (ld->frontsector == NULL)
		{
			Printf ("Line %d has no front sector\n", linemap[linenum]);
		}

		// [RH] Set some new sidedef properties
		len = (int)(sqrtf (dx*dx + dy*dy) + 0.5f);
		light = dy == 0 ? level.WallHorizLight :
				dx == 0 ? level.WallVertLight : 0;

		if (ld->sidenum[0] != NO_SIDE)
		{
			sides[ld->sidenum[0]].linenum = linenum;
			sides[ld->sidenum[0]].TexelLength = len;
			sides[ld->sidenum[0]].Light = light;
		}
		if (ld->sidenum[1] != NO_SIDE)
		{
			sides[ld->sidenum[1]].linenum = linenum;
			sides[ld->sidenum[1]].TexelLength = len;
			sides[ld->sidenum[1]].Light = light;
		}

		switch (ld->special)
		{						// killough 4/11/98: handle special types
			int j;
			int alpha;

		case TranslucentLine:			// killough 4/11/98: translucent 2s textures
			// [RH] Second arg controls how opaque it is.
			alpha = sidetemp[ld->sidenum[0]].a.alpha;
			if (alpha < 0)
			{
				alpha = ld->args[1];
			}
			if (!ld->args[0])
			{
				ld->alpha = (BYTE)alpha;
				if (ld->args[2] == 1)
				{
					sides[ld->sidenum[0]].Flags |= WALLF_ADDTRANS;
					if (ld->sidenum[1] != NO_SIDE)
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
						lines[j].alpha = (BYTE)alpha;
						if (lines[j].args[2] == 1)
						{
							sides[lines[j].sidenum[0]].Flags |= WALLF_ADDTRANS;
							if (lines[j].sidenum[1] != NO_SIDE)
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

void P_LoadLineDefs (MapData * map)
{
	int i, skipped;
	line_t *ld;
	int lumplen = map->Size(ML_LINEDEFS);
	char * mldf;
	maplinedef_t *mld;
		
	numlines = lumplen / sizeof(maplinedef_t);
	lines = new line_t[numlines];
	linemap = new WORD[numlines];
	memset (lines, 0, numlines*sizeof(line_t));

	mldf = new char[lumplen];
	map->Read(ML_LINEDEFS, mldf);

	// [RH] Count the number of sidedef references. This is the number of
	// sidedefs we need. The actual number in the SIDEDEFS lump might be less.
	// Lines with 0 length are also removed.

	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef_t*)mldf) + i;
		int v1 = LittleShort(mld->v1);
		int v2 = LittleShort(mld->v2);

		if (v1 >= numvertexes || v2 >= numvertexes)
		{
			delete [] mldf;
			I_Error ("Line %d has invalid vertices: %d and/or %d.\nThe map only contains %d vertices.", i+skipped, v1, v2, numvertexes);
		}
		else if (v1 == v2 ||
			(vertexes[LittleShort(mld->v1)].x == vertexes[LittleShort(mld->v2)].x &&
			 vertexes[LittleShort(mld->v1)].y == vertexes[LittleShort(mld->v2)].y))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			ForceNodeBuild = true;
			skipped++;
			numlines--;
		}
		else
		{
			if (LittleShort(mld->sidenum[0]) != NO_INDEX)
				sidecount++;
			if (LittleShort(mld->sidenum[1]) != NO_INDEX)
				sidecount++;
			linemap[i] = i+skipped;
			i++;
		}
	}

	P_AllocateSideDefs (sidecount);

	mld = (maplinedef_t *)mldf;
	ld = lines;
	for (i = numlines; i > 0; i--, mld++, ld++)
	{
		ld->alpha = 255;	// [RH] Opaque by default

		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.
		P_TranslateLineDef (ld, mld);

		ld->v1 = &vertexes[LittleShort(mld->v1)];
		ld->v2 = &vertexes[LittleShort(mld->v2)];
		//ld->id = -1;		ID has been assigned in P_TranslateLineDef

		P_SetSideNum (&ld->sidenum[0], LittleShort(mld->sidenum[0]));
		P_SetSideNum (&ld->sidenum[1], LittleShort(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SaveLineSpecial (ld);
		if (level.flags & LEVEL_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags & LEVEL_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
	}
	delete[] mldf;
}

// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
void P_LoadLineDefs2 (MapData * map)
{
	int i, skipped;
	line_t *ld;
	int lumplen = map->Size(ML_LINEDEFS);
	char * mldf;
	maplinedef2_t *mld;
		
	numlines = lumplen / sizeof(maplinedef2_t);
	lines = new line_t[numlines];
	linemap = new WORD[numlines];
	memset (lines, 0, numlines*sizeof(line_t));

	mldf = new char[lumplen];
	map->Read(ML_LINEDEFS, mldf);

	// [RH] Remove any lines that have 0 length and count sidedefs used
	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef2_t*)mldf) + i;

		if (mld->v1 == mld->v2 ||
			(vertexes[LittleShort(mld->v1)].x == vertexes[LittleShort(mld->v2)].x &&
			 vertexes[LittleShort(mld->v1)].y == vertexes[LittleShort(mld->v2)].y))
		{
			Printf ("Removing 0-length line %d\n", i+skipped);
			memmove (mld, mld+1, sizeof(*mld)*(numlines-i-1));
			skipped++;
			numlines--;
		}
		else
		{
			if (LittleShort(mld->sidenum[0]) != NO_INDEX)
				sidecount++;
			if (LittleShort(mld->sidenum[1]) != NO_INDEX)
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

	mld = (maplinedef2_t *)mldf;
	ld = lines;
	for (i = numlines; i > 0; i--, mld++, ld++)
	{
		int j;

		for (j = 0; j < 5; j++)
			ld->args[j] = mld->args[j];

		ld->flags = LittleShort(mld->flags);
		ld->special = mld->special;

		ld->v1 = &vertexes[LittleShort(mld->v1)];
		ld->v2 = &vertexes[LittleShort(mld->v2)];
		ld->alpha = 255;	// [RH] Opaque by default
		ld->id = -1;

		P_SetSideNum (&ld->sidenum[0], LittleShort(mld->sidenum[0]));
		P_SetSideNum (&ld->sidenum[1], LittleShort(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SaveLineSpecial (ld);
		if (level.flags & LEVEL_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags & LEVEL_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
	}
	delete[] mldf;
}


//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions
void P_LoadSideDefs (MapData * map)
{
	numsides = map->Size(ML_SIDEDEFS) / sizeof(mapsidedef_t);
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
		sidetemp[i].a.alpha = -1;
		sidetemp[i].a.map = NO_SIDE;
	}
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}

static void P_SetSideNum (DWORD *sidenum_p, WORD sidenum)
{
	if (sidenum == NO_INDEX)
	{
		*sidenum_p = NO_SIDE;
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
		sidetemp[i].b.first = NO_SIDE;
		sidetemp[i].b.next = NO_SIDE;
	}
	for (; i < numsides; ++i)
	{
		sidetemp[i].b.next = NO_SIDE;
	}

	for (i = 0; i < numsides; ++i)
	{
		// For each vertex, build a list of sidedefs that use that vertex
		// as their left edge.
		line_t *line = &lines[sides[i].linenum];
		int lineside = (line->sidenum[0] != (DWORD)i);
		int vert = (lineside ? line->v2 : line->v1) - vertexes;
		
		sidetemp[i].b.lineside = lineside;
		sidetemp[i].b.next = sidetemp[vert].b.first;
		sidetemp[vert].b.first = i;

		// Set each side so that it is the only member of its loop
		sides[i].LeftSide = NO_SIDE;
		sides[i].RightSide = NO_SIDE;
	}

	// For each side, find the side that is to its right and set the
	// loop pointers accordingly. If two sides share a left vertex, the
	// one that forms the smallest angle is assumed to be the right one.
	for (i = 0; i < numsides; ++i)
	{
		DWORD right;
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

			if (right == NO_SIDE)
			{ // There is no right side!
				Printf ("Line %d's right edge is unconnected\n", linemap[line-lines]);
				continue;
			}

			if (sidetemp[right].b.next != NO_SIDE)
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

				while (right != NO_SIDE)
				{
					if (sides[right].LeftSide == NO_SIDE)
					{
						rightline = &lines[sides[right].linenum];
						if (rightline->frontsector != rightline->backsector)
						{
							ang2 = R_PointToAngle2 (0, 0, rightline->dx, rightline->dy);
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

int P_DetermineTranslucency (int lumpnum)
{
	FWadLump tranmap = Wads.OpenLumpNum (lumpnum);
	BYTE index;
	PalEntry newcolor;

	tranmap.Seek (GPalette.BlackIndex * 256 + GPalette.WhiteIndex, SEEK_SET);
	tranmap.Read (&index, 1);

	newcolor = GPalette.BaseColors[GPalette.Remap[index]];

	if (developer)
	{
		char lumpname[9];
		lumpname[8] = 0;
		Wads.GetLumpName (lumpname, lumpnum);
		Printf ("%s appears to be translucency %d (%d%%)\n", lumpname, newcolor.r,
			newcolor.r*100/255);
	}
	return newcolor.r;
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2 (MapData * map)
{
	int  i;
	char name[9];
	char * msdf = new char[map->Size(ML_SIDEDEFS)];
	map->Read(ML_SIDEDEFS, msdf);

	name[8] = 0;

	for (i = 0; i < numsides; i++)
	{
		mapsidedef_t *msd = ((mapsidedef_t*)msdf) + sidetemp[i].a.map;
		side_t *sd = sides + i;
		sector_t *sec;

		// [RH] The Doom renderer ignored the patch y locations when
		// drawing mid textures. ZDoom does not, so fix the laser beams in Strife.
		if (gameinfo.gametype == GAME_Strife &&
			strncmp (msd->midtexture, "LASERB01", 8) == 0)
		{
			msd->rowoffset += 102;
		}

		sd->textureoffset = LittleShort(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = LittleShort(msd->rowoffset)<<FRACBITS;
		sd->linenum = NO_INDEX;

		// killough 4/4/98: allow sidedef texture names to be overloaded
		// killough 4/11/98: refined to allow colormaps to work as wall
		// textures if invalid as colormaps but valid as textures.

		if ((unsigned)LittleShort(msd->sector)>=(unsigned)numsectors)
		{
			Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
			sd->sector = sec = NULL;
		}
		else
		{
			sd->sector = sec = &sectors[LittleShort(msd->sector)];
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
				DWORD color, fog;
				bool colorgood, foggood;

				SetTextureNoErr (&sd->bottomtexture, &fog, msd->bottomtexture, &foggood);
				SetTextureNoErr (&sd->toptexture, &color, msd->toptexture, &colorgood);
				strncpy (name, msd->midtexture, 8);
				sd->midtexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

				if (colorgood | foggood)
				{
					int s;
					FDynamicColormap *colormap = NULL;

					for (s = 0; s < numsectors; s++)
					{
						if (sectors[s].tag == sidetemp[i].a.tag)
						{
							if (!colorgood) color = sectors[s].ColorMap->Color;
							if (!foggood) fog = sectors[s].ColorMap->Fade;
							if (colormap == NULL ||
								colormap->Color != color ||
								colormap->Fade != fog)
							{
								colormap = GetSpecialLights (color, fog, 0);
							}
							sectors[s].ColorMap = colormap;
						}
					}
				}
			}
			break;

		case TranslucentLine:	// killough 4/11/98: apply translucency to 2s normal texture
			if (!map->HasBehavior)
			{
				int lumpnum;

				if (strnicmp ("TRANMAP", msd->midtexture, 8) == 0)
				{
					// The translator set the alpha argument already; no reason to do it again.
					sd->midtexture = 0;
				}
				else if ((lumpnum = Wads.CheckNumForName (msd->midtexture)) > 0 &&
					Wads.LumpLength (lumpnum) == 65536)
				{
					sidetemp[i].a.alpha = P_DetermineTranslucency (lumpnum);
					sd->midtexture = 0;
				}
				else
				{
					strncpy (name, msd->midtexture, 8);
					sd->midtexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
				}

				strncpy (name, msd->toptexture, 8);
				sd->toptexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

				strncpy (name, msd->bottomtexture, 8);
				sd->bottomtexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
				break;
			}
			// Fallthrough for Hexen maps is intentional
#if 0
			sd->midtexture = strncasecmp("TRANMAP", msd->midtexture, 8) ?
				(sd->special = W_CheckNumForName(msd->midtexture)) < 0 ||
				W_LumpLength(sd->special) != 65536 ?
				sd->special=0, R_TextureNumForName(msd->midtexture) :
					(sd->special++, 0) : (sd->special=0);
			sd->toptexture = R_TextureNumForName(msd->toptexture);
			sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
#endif

		default:			// normal cases
			strncpy (name, msd->midtexture, 8);
			sd->midtexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

			strncpy (name, msd->toptexture, 8);
			sd->toptexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

			strncpy (name, msd->bottomtexture, 8);
			sd->bottomtexture = TexMan.GetTexture (name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
			break;
		}
	}
	delete[] msdf;
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

	FVector3 p, v1, v2, cross;

	const secplane_t *refplane;
	secplane_t *srcplane;
	fixed_t srcheight, destheight;

	refplane = (which == 0) ? &refsec->floorplane : &refsec->ceilingplane;
	srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
	srcheight = (which == 0) ? sec->floortexz : sec->ceilingtexz;
	destheight = (which == 0) ? refsec->floortexz : refsec->ceilingtexz;

	p[0] = FIXED2FLOAT (line->v1->x);
	p[1] = FIXED2FLOAT (line->v1->y);
	p[2] = FIXED2FLOAT (destheight);
	v1[0] = FIXED2FLOAT (line->dx);
	v1[1] = FIXED2FLOAT (line->dy);
	v1[2] = 0;
	v2[0] = FIXED2FLOAT (refvert->x - line->v1->x);
	v2[1] = FIXED2FLOAT (refvert->y - line->v1->y);
	v2[2] = FIXED2FLOAT (srcheight - destheight);

	cross = (v1 ^ v2).Unit();

	// Fix backward normals
	if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
	{
		cross = -cross;
	}

	srcplane->a = FLOAT2FIXED (cross[0]);
	srcplane->b = FLOAT2FIXED (cross[1]);
	srcplane->c = FLOAT2FIXED (cross[2]);
	//srcplane->ic = FLOAT2FIXED (1.f/cross[2]);
	srcplane->ic = DivScale32 (1, srcplane->c);
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
	for (unsigned int ii = 0; ii < BlockMap.Size(); ++ii)
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

void P_LoadBlockMap (MapData * map)
{
	int count = map->Size(ML_BLOCKMAP);

	if (ForceNodeBuild || genblockmap ||
		count/2 >= 0x10000 || count == 0 ||
		Args.CheckParm("-blockmap")
		)
	{
		DPrintf ("Generating BLOCKMAP\n");
		P_CreateBlockMap ();
	}
	else
	{
		BYTE *data = new BYTE[count];
		map->Read(ML_BLOCKMAP, data);
		const short *wadblockmaplump = (short *)data;
		int i;

		count/=2;
		blockmaplump = new int[count];

		// killough 3/1/98: Expand wad blockmap into larger internal one,
		// by treating all offsets except -1 as unsigned and zero-extending
		// them. This potentially doubles the size of blockmaps allowed,
		// because Doom originally considered the offsets as always signed.

		blockmaplump[0] = LittleShort(wadblockmaplump[0]);
		blockmaplump[1] = LittleShort(wadblockmaplump[1]);
		blockmaplump[2] = (DWORD)(LittleShort(wadblockmaplump[2])) & 0xffff;
		blockmaplump[3] = (DWORD)(LittleShort(wadblockmaplump[3])) & 0xffff;

		for (i = 4; i < count; i++)
		{
			short t = LittleShort(wadblockmaplump[i]);          // killough 3/1/98
			blockmaplump[i] = t == -1 ? (DWORD)0xffffffff : (DWORD) t & 0xffff;
		}
		delete[] data;
	}

	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];

	// clear out mobj chains
	count = bmapwidth*bmapheight;
	blocklinks = new FBlockNode *[count];
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
	cycle_t times[16] = { 0 };
	TArray<linf>		exLightTags;
	int*				linesDoneInEachSector;
	int 				i;
	int 				j;
	int 				total;
	int					totallights;
	line_t* 			li;
	sector_t*			sector;
	DBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	unsigned int		ii, jj;
		
	// look up sector number for each subsector
	clock (times[0]);
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
	unclock (times[0]);

	// count number of lines in each sector
	clock (times[1]);
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
	unclock (times[1]);

	// collect extra light info
	clock (times[2]);
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
	unclock (times[2]);

	// build line tables for each sector
	clock (times[3]);
	linebuffer = new line_t *[total];
	line_t **lineb_p = linebuffer;
	linesDoneInEachSector = new int[numsectors];
	memset (linesDoneInEachSector, 0, sizeof(int)*numsectors);

	for (sector = sectors, i = 0; i < numsectors; i++, sector++)
	{
		if (sector->linecount == 0)
		{
			Printf ("Sector %i (tag %i) has no lines\n", i, sector->tag);
			// 0 the sector's tag so that no specials can use it
			sector->tag = 0;
		}
		else
		{
			sector->lines = lineb_p;
			lineb_p += sector->linecount;
		}
	}

	for (i = numlines, li = lines; i > 0; --i, ++li)
	{
		if (li->frontsector != NULL)
		{
			li->frontsector->lines[linesDoneInEachSector[li->frontsector - sectors]++] = li;
		}
		if (li->backsector != NULL && li->backsector != li->frontsector)
		{
			li->backsector->lines[linesDoneInEachSector[li->backsector - sectors]++] = li;
		}
	}

	for (i = 0, sector = sectors; i < numsectors; ++i, ++sector)
	{
		if (linesDoneInEachSector[i] != sector->linecount)
		{
			I_Error ("P_GroupLines: miscounted");
		}
		if (sector->linecount != 0)
		{
			bbox.ClearBox ();
			for (j = 0; j < sector->linecount; ++j)
			{
				li = sector->lines[j];
				bbox.AddToBox (li->v1->x, li->v1->y);
				bbox.AddToBox (li->v2->x, li->v2->y);
			}
		}

		// set the soundorg to the middle of the bounding box
		sector->soundorg[0] = bbox.Right()/2 + bbox.Left()/2;
		sector->soundorg[1] = bbox.Top()/2 + bbox.Bottom()/2;
		sector->soundorg[2] = sector->floorplane.ZatPoint (sector->soundorg[0], sector->soundorg[1]);

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
	delete[] linesDoneInEachSector;
	unclock (times[3]);

	// [RH] Moved this here
	clock (times[4]);
	P_InitTagLists();   // killough 1/30/98: Create xref tables for tags
	unclock (times[4]);

	clock (times[5]);
	if (!buildmap)
	{
		P_SetSlopes ();
	}
	unclock (times[5]);

	clock (times[6]);
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
	unclock (times[6]);

	if (showloadtimes)
	{
		Printf ("---Group Lines Times---\n");
		for (i = 0; i < 7; ++i)
		{
			Printf (" time %d:%10llu\n", i, times[i]);
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
void P_LoadReject (MapData * map, bool junk)
{
	const int neededsize = (numsectors * numsectors + 7) >> 3;
	int rejectsize;

	if (strnicmp (map->MapLumps[ML_REJECT].Name, "REJECT", 8) != 0)
	{
		rejectsize = 0;
	}
	else
	{
		rejectsize = junk ? 0 : map->Size(ML_REJECT);
	}

	if (rejectsize < neededsize)
	{
		if (rejectsize > 0)
		{
			Printf ("REJECT is %d byte%s too small.\n", neededsize - rejectsize,
				neededsize-rejectsize==1?"":"s");
		}
		rejectmatrix = NULL;
	}
	else
	{
		// Check if the reject has some actual content. If not, free it.
		rejectsize = MIN (rejectsize, neededsize);
		rejectmatrix = new BYTE[rejectsize];

		map->Seek(ML_REJECT);
		map->file->Read (rejectmatrix, rejectsize);

		int qwords = rejectsize / 8;
		int i;

		if (qwords > 0)
		{
			const QWORD *qreject = (const QWORD *)rejectmatrix;

			i = 0;
			do
			{
				if (qreject[i] != 0)
					return;
			} while (++i < qwords);
		}
		rejectsize &= 7;
		qwords *= 8;
		for (i = 0; i < rejectsize; ++i)
		{
			if (rejectmatrix[qwords+rejectsize] != 0)
				return;
		}

		// Reject has no data, so pretend it isn't there.
		delete[] rejectmatrix;
		rejectmatrix = NULL;
	}
}

//
// [RH] P_LoadBehavior
//
void P_LoadBehavior (MapData * map)
{
	map->Seek(ML_BEHAVIOR);
	FBehavior::StaticLoadModule (-1, map->file, map->Size(ML_BEHAVIOR));
	if (!FBehavior::StaticCheckAllGood ())
	{
		Printf ("ACS scripts unloaded.\n");
		FBehavior::StaticUnloadModules ();
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

void P_GetPolySpots (MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors)
{
	if (map->HasBehavior)
	{
		int spot1, spot2, spot3, anchor;

		int	lumplen = map->Size(ML_THINGS);
		int num = lumplen / sizeof(mapthing2_t);

		mapthing2_t *mt;

		map->Seek(ML_THINGS);
		mt = new mapthing2_t[num];
		map->file->Read(mt, num * sizeof(mapthing2_t));

		if (gameinfo.gametype == GAME_Hexen)
		{
			spot1 = LittleShort(PO_HEX_SPAWN_TYPE);
			spot2 = LittleShort(PO_HEX_SPAWNCRUSH_TYPE);
			anchor = LittleShort(PO_HEX_ANCHOR_TYPE);
		}
		else
		{
			spot1 = LittleShort(PO_SPAWN_TYPE);
			spot2 = LittleShort(PO_SPAWNCRUSH_TYPE);
			anchor = LittleShort(PO_ANCHOR_TYPE);
		}
		spot3 = LittleShort(PO_SPAWNHURT_TYPE);

		for (int i = 0; i < num; ++i)
		{
			if (mt[i].type == spot1 || mt[i].type == spot2 || mt[i].type == spot3 || mt[i].type == anchor)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = LittleShort(mt[i].x) << FRACBITS;
				newvert.y = LittleShort(mt[i].y) << FRACBITS;
				newvert.polynum = LittleShort(mt[i].angle);
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
		delete[] mt;
	}
}

extern polyblock_t **PolyBlockMap;

void P_FreeLevelData ()
{
	SN_StopAllSequences ();
	DThinker::DestroyAllThinkers ();
	level.total_monsters = level.total_items = level.total_secrets =
		level.killed_monsters = level.found_items = level.found_secrets =
		wminfo.maxfrags = 0;
	FBehavior::StaticUnloadModules ();
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
		numsectors = 0;	// needed for the pointer cleanup code
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
		for (int i = bmapwidth*bmapheight-1; i >= 0; --i)
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
		delete[] rejectmatrix;
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
		for (int i = 0; i < po_NumPolyobjs; ++i)
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
	po_NumPolyobjs = 0;
	if (zones != NULL)
	{
		delete[] zones;
		zones = NULL;
	}
	P_FreeStrifeConversations ();
	if (level.Scrolls != NULL)
	{
		delete[] level.Scrolls;
		level.Scrolls = NULL;
	}
}

extern msecnode_t *headsecnode;

void P_FreeExtraLevelData()
{
	// Free all blocknodes and msecnodes.
	// *NEVER* call this function without calling
	// P_FreeLevelData() first, or they might not all be freed.
	{
		FBlockNode *node = FBlockNode::FreeBlocks;
		while (node != NULL)
		{
			FBlockNode *next = node->NextBlock;
			delete node;
			node = next;
		}
	}
	{
		msecnode_t *node = headsecnode;

		while (node != NULL)
		{
			msecnode_t *next = node->m_snext;
			free (node);
			node = next;
		}
		headsecnode = NULL;
	}
}

//
// P_SetupLevel
//

// [RH] position indicates the start spot to spawn at
void P_SetupLevel (char *lumpname, int position)
{
	cycle_t times[20] = { 0 };
	mapthing2_t *buildthings;
	int numbuildthings;
	int i;
	bool buildmap;

	wminfo.partime = 180;

	clearinterpolations();	// [RH] Nothing to interpolate on a fresh level.
	FCanvasTextureInfo::EmptyList ();
	R_FreePastViewers ();

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

	// Free all level data from the previous map
	P_FreeLevelData ();

	MapData * map = P_OpenMapData(lumpname);
	if (map == NULL)
	{
		I_Error("Unable to open map '%s'\n", lumpname);
	}

	// find map num
	level.lumpnum = map->lumpnum;

	// [RH] Support loading Build maps (because I felt like it. :-)
	buildmap = false;
	if (map->MapLumps[0].Size > 0)
	{
		BYTE *mapdata = new BYTE[map->MapLumps[0].Size];
		map->Seek(0);
		map->file->Read(mapdata, map->MapLumps[0].Size);
		if (map->Encrypted)
		{
			BloodCrypt (mapdata, 0, MIN<int> (map->MapLumps[0].Size, 256));
		}
		buildmap = P_LoadBuildMap (mapdata, map->MapLumps[0].Size, &buildthings, &numbuildthings);
		delete[] mapdata;
	}

	if (!buildmap)
	{
		// note: most of this ordering is important 
		ForceNodeBuild = gennodes;
		// [RH] Load in the BEHAVIOR lump
		FBehavior::StaticUnloadModules ();
		if (map->HasBehavior)
		{
			P_LoadBehavior (map);
			level.flags |= LEVEL_HEXENFORMAT;
		}
		else
		{
			// Doom format maps get strict monster activation unless the mapinfo
			// specifies differently.
			if (!(level.flags & LEVEL_LAXACTIVATIONMAPINFO))
			{
				level.flags &= ~LEVEL_LAXMONSTERACTIVATION;
			}
		}
		FBehavior::StaticLoadDefaultModules ();

		P_LoadStrifeConversations (lumpname);

		clock (times[0]);
		P_LoadVertexes (map);
		unclock (times[0]);
		
		// Check for maps without any BSP data at all (e.g. SLIGE)
		clock (times[1]);
		P_LoadSectors (map);
		unclock (times[1]);

		clock (times[2]);
		P_LoadSideDefs (map);
		unclock (times[2]);

		clock (times[3]);
		if (!map->HasBehavior)
			P_LoadLineDefs (map);
		else
			P_LoadLineDefs2 (map);	// [RH] Load Hexen-style linedefs
		unclock (times[3]);

		clock (times[4]);
		P_LoadSideDefs2 (map);
		unclock (times[4]);

		clock (times[5]);
		P_FinishLoadingLineDefs ();
		unclock (times[5]);

		clock (times[6]);
		P_LoopSidedefs ();
		unclock (times[6]);

		delete[] linemap;
		linemap = NULL;
	}
	else
	{
		ForceNodeBuild = true;
	}

	UsingGLNodes = false;
	if (!ForceNodeBuild)
	{
		// Check for compressed nodes first, then uncompressed nodes
		FWadLump test;
		DWORD id = MAKE_ID('X','x','X','x'), idcheck=0;

		if (map->MapLumps[ML_ZNODES].Size != 0 && !UsingGLNodes)
		{
			map->Seek(ML_ZNODES);
			idcheck = MAKE_ID('Z','N','O','D');
		}
		else if (map->MapLumps[ML_GLZNODES].Size != 0)
		{
			// If normal nodes are not present but GL nodes are, use them.
			map->Seek(ML_GLZNODES);
			idcheck = MAKE_ID('Z','G','L','N');
		}

		map->file->Read (&id, 4);
		if (id == idcheck)
		{
			try
			{
				P_LoadZNodes (*map->file, id);
			}
			catch (CRecoverableError &error)
			{
				Printf ("Error loading nodes: %s\n", error.GetMessage());

				ForceNodeBuild = true;
				if (subsectors != NULL)
				{
					delete[] subsectors;
					subsectors = NULL;
				}
				if (segs != NULL)
				{
					delete[] segs;
					segs = NULL;
				}
				if (nodes != NULL)
				{
					delete[] nodes;
					nodes = NULL;
				}
			}
		}
		else
		{
			clock (times[7]);
			P_LoadSubsectors (map);
			unclock (times[7]);

			clock (times[8]);
			if (!ForceNodeBuild) P_LoadNodes (map);
			unclock (times[8]);

			clock (times[9]);
			if (!ForceNodeBuild) P_LoadSegs (map);
			unclock (times[9]);
		}

	}
	if (ForceNodeBuild)
	{
		unsigned int startTime, endTime;

		startTime = I_MSTime ();
		TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
		P_GetPolySpots (map, polyspots, anchors);
		FNodeBuilder::FLevel leveldata =
		{
			vertexes, numvertexes,
			sides, numsides,
			lines, numlines
		};
		leveldata.FindMapBounds ();
		UsingGLNodes |= genglnodes;
		FNodeBuilder builder (leveldata, polyspots, anchors, UsingGLNodes, CPU.bSSE2);
		delete[] vertexes;
		builder.Extract (nodes, numnodes,
			segs, numsegs,
			subsectors, numsubsectors,
			vertexes, numvertexes);
		endTime = I_MSTime ();
		Printf ("BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, numsegs);
	}

	clock (times[10]);
	P_LoadBlockMap (map);
	unclock (times[10]);

	clock (times[11]);
	P_LoadReject (map, buildmap);
	unclock (times[11]);

	clock (times[12]);
	P_GroupLines (buildmap);
	unclock (times[12]);

	clock (times[13]);
	P_FloodZones ();
	unclock (times[13]);

	bodyqueslot = 0;
// phares 8/10/98: Clear body queue so the corpses from previous games are
// not assumed to be from this one.

	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	deathmatchstarts.Clear ();

	if (!buildmap)
	{
		clock (times[14]);
		if (!map->HasBehavior)
			P_LoadThings (map, position);
		else
			P_LoadThings2 (map, position);	// [RH] Load Hexen-style things
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != NULL)
				players[i].health = players[i].mo->health;
		}
		unclock (times[14]);

		clock (times[15]);
		if (!map->HasBehavior)
			P_TranslateTeleportThings ();	// [RH] Assign teleport destination TIDs
		unclock (times[15]);
	}
	else
	{
		for (i = 0; i < numbuildthings; ++i)
		{
			P_SpawnMapThing (&buildthings[i], 0);
		}
		delete[] buildthings;
	}
	delete map;

	clock (times[16]);
	PO_Init ();	// Initialize the polyobjs
	unclock (times[16]);

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

	clock (times[17]);
	// preload graphics and sounds
	if (precache)
	{
		R_PrecacheLevel ();
		S_PrecacheLevel ();
	}
	unclock (times[17]);

	if (deathmatch)
	{
		AnnounceGameStart ();
	}

	P_ResetSightCounters (true);
	//Printf ("free memory: 0x%x\n", Z_FreeMemory());

	if (showloadtimes)
	{
		Printf ("---Total load times---\n");
		for (i = 0; i < 18; ++i)
		{
			static const char *timenames[] =
			{
				"load vertexes",
				"load sectors",
				"load sides",
				"load lines",
				"load sides 2",
				"load lines 2",
				"loop sides",
				"load subsectors",
				"load nodes",
				"load segs",
				"load blockmap",
				"load reject",
				"group lines",
				"flood zones",
				"load things",
				"translate teleports",
				"init polys",
				"precache"
			};
			Printf ("Time%3d:%10llu cycles (%s)\n", i, times[i], timenames[i]);
		}
	}
}



//
// P_Init
//
void P_Init ()
{
	atterm (P_Shutdown);

	P_InitEffects ();		// [RH]
	R_InitPicAnims ();
	P_InitSwitchList ();
	P_InitTerrainTypes ();
	P_InitKeyMessages ();
	R_InitSprites ();
}

static void P_Shutdown ()
{
	R_DeinitSprites ();
	P_DeinitKeyMessages ();
	P_FreeLevelData ();
	P_FreeExtraLevelData ();
	if (StatusBar != NULL)
	{
		delete StatusBar;
		StatusBar = NULL;
	}
}

#if 0
#include "c_dispatch.h"
CCMD (lineloc)
{
	if (argv.argc() != 2)
	{
		return;
	}
	int linenum = atoi (argv[1]);
	if (linenum < 0 || linenum >= numlines)
	{
		Printf ("No such line\n");
	}
	Printf ("(%d,%d) -> (%d,%d)\n", lines[linenum].v1->x >> FRACBITS,
		lines[linenum].v1->y >> FRACBITS,
		lines[linenum].v2->x >> FRACBITS,
		lines[linenum].v2->y >> FRACBITS);
}
#endif
