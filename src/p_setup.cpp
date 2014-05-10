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
#include "m_argv.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "g_game.h"
#include "i_system.h"
#include "x86.h"
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
#include "c_cvars.h"
#include "p_acs.h"
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
#include "r_data/r_translate.h"
#include "r_data/r_interpolate.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "md5.h"
#include "compatibility.h"
#include "po_man.h"
#include "r_renderer.h"
#include "r_data/colormaps.h"

#include "fragglescript/t_fs.h"

#define MISSING_TEXTURE_WARN_LIMIT		20

void P_SpawnSlopeMakers (FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
void P_SetSlopes ();
void P_CopySlopes();
void BloodCrypt (void *data, int key, int len);
void P_ClearUDMFKeys();

extern AActor *P_SpawnMapThing (FMapThing *mthing, int position);
extern bool P_LoadBuildMap (BYTE *mapdata, size_t len, FMapThing **things, int *numthings);

extern void P_TranslateTeleportThings (void);

void P_ParseTextMap(MapData *map, FMissingTextureTracker &);

extern int numinterpolations;
extern unsigned int R_OldBlend;

EXTERN_CVAR(Bool, am_textured)

CVAR (Bool, genblockmap, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, gennodes, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, genglnodes, false, CVAR_SERVERINFO);
CVAR (Bool, showloadtimes, false, 0);

static void P_InitTagLists ();
static void P_Shutdown ();

bool P_IsBuildMap(MapData *map);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int 			numvertexes;
vertex_t*		vertexes;
int 			numvertexdatas;
vertexdata_t*		vertexdatas;

int 			numsegs;
seg_t*			segs;
glsegextra_t*	glsegextras;

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

node_t * 		gamenodes;
int 			numgamenodes;

subsector_t * 	gamesubsectors;
int 			numgamesubsectors;

bool			hasglnodes;

TArray<FMapThing> MapThingsConverted;
TMap<unsigned,unsigned>  MapThingsUserDataIndex;	// from mapthing idx -> user data idx
TArray<FMapThingUserData> MapThingsUserData;

int sidecount;
sidei_t *sidetemp;

TArray<int>		linemap;

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
int				bmapnegx;		// min negs of block map before wrapping
int				bmapnegy;

FBlockNode**	blocklinks;		// for thing chains


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//	LineOf Sight calculation.
// Without special effect, this could be
//	used as a PVS lookup as well.
//
BYTE*			rejectmatrix;

bool		ForceNodeBuild;

// Maintain single and multi player starting spots.
TArray<FPlayerStart> deathmatchstarts (16);
FPlayerStart		playerstarts[MAXPLAYERS];
TArray<FPlayerStart> AllPlayerStarts;

static void P_AllocateSideDefs (int count);


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

MapData *P_OpenMapData(const char * mapname, bool justcheck)
{
	MapData * map = new MapData;
	FileReader * wadReader = NULL;
	bool externalfile = !strnicmp(mapname, "file:", 5);
	
	if (externalfile)
	{
		mapname += 5;
		if (!FileExists(mapname))
		{
			delete map;
			return NULL;
		}
		map->resource = FResourceFile::OpenResourceFile(mapname, NULL, true);
		wadReader = map->resource->GetReader();
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
			int lumpfile = Wads.GetLumpFile(lump_name);
			int nextfile = Wads.GetLumpFile(lump_name+1);

			map->lumpnum = lump_name;

			if (lumpfile != nextfile)
			{
				// The following lump is from a different file so whatever this is,
				// it is not a multi-lump Doom level so let's assume it is a Build map.
				map->MapLumps[0].Reader = map->file = Wads.ReopenLumpNum(lump_name);
				if (!P_IsBuildMap(map))
				{
					delete map;
					return NULL;
				}
				return map;
			}

			// This case can only happen if the lump is inside a real WAD file.
			// As such any special handling for other types of lumps is skipped.
			map->MapLumps[0].Reader = map->file = Wads.ReopenLumpNum(lump_name);
			strncpy(map->MapLumps[0].Name, Wads.GetLumpFullName(lump_name), 8);
			map->Encrypted = Wads.IsEncryptedFile(lump_name);
			map->InWad = true;

			if (map->Encrypted)
			{ // If it's encrypted, then it's a Blood file, presumably a map.
				map->MapLumps[0].Reader = map->file = Wads.ReopenLumpNum(lump_name);
				if (!P_IsBuildMap(map))
				{
					delete map;
					return NULL;
				}
				return map;
			}

			int index = 0;

			if (stricmp(Wads.GetLumpFullName(lump_name + 1), "TEXTMAP") != 0)
			{
				for(int i = 1;; i++)
				{
					// Since levels must be stored in WADs they can't really have full
					// names and for any valid level lump this always returns the short name.
					const char * lumpname = Wads.GetLumpFullName(lump_name + i);
					try
					{
						index = GetMapIndex(mapname, index, lumpname, !justcheck);
					}
					catch(...)
					{
						delete map;
						throw;
					}
					if (index == -2)
					{
						delete map;
						return NULL;
					}
					if (index == ML_BEHAVIOR) map->HasBehavior = true;

					// The next lump is not part of this map anymore
					if (index < 0) break;

					map->MapLumps[index].Reader = Wads.ReopenLumpNum(lump_name + i);
					strncpy(map->MapLumps[index].Name, lumpname, 8);
				}
			}
			else
			{
				map->isText = true;
				map->MapLumps[1].Reader = Wads.ReopenLumpNum(lump_name + 1);
				for(int i = 2;; i++)
				{
					const char * lumpname = Wads.GetLumpFullName(lump_name + i);

					if (lumpname == NULL)
					{
						I_Error("Invalid map definition for %s", mapname);
					}
					else if (!stricmp(lumpname, "ZNODES"))
					{
						index = ML_GLZNODES;
					}
					else if (!stricmp(lumpname, "BLOCKMAP"))
					{
						// there is no real point in creating a blockmap but let's use it anyway
						index = ML_BLOCKMAP;
					}
					else if (!stricmp(lumpname, "REJECT"))
					{
						index = ML_REJECT;
					}
					else if (!stricmp(lumpname, "DIALOGUE"))
					{
						index = ML_CONVERSATION;
					}
					else if (!stricmp(lumpname, "BEHAVIOR"))
					{
						index = ML_BEHAVIOR;
						map->HasBehavior = true;
					}
					else if (!stricmp(lumpname, "ENDMAP"))
					{
						break;
					}
					else continue;
					map->MapLumps[index].Reader = Wads.ReopenLumpNum(lump_name + i);
					strncpy(map->MapLumps[index].Name, lumpname, 8);
				}
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
			map->resource = FResourceFile::OpenResourceFile(Wads.GetLumpFullName(lump_wad), Wads.ReopenLumpNum(lump_wad), true);
			wadReader = map->resource->GetReader();
		}
	}
	DWORD id;

	// Although we're using the resource system, we still want to be sure we're
	// reading from a wad file.
	wadReader->Seek(0, SEEK_SET);
	wadReader->Read(&id, sizeof(id));
	
	if (id == IWAD_ID || id == PWAD_ID)
	{
		char maplabel[9]="";
		int index=0;

		map->MapLumps[0].Reader = map->resource->GetLump(0)->NewReader();
		strncpy(map->MapLumps[0].Name, map->resource->GetLump(0)->Name, 8);

		for(DWORD i = 1; i < map->resource->LumpCount(); i++)
		{
			const char* lumpname = map->resource->GetLump(i)->Name;

			if (i == 1 && !strnicmp(lumpname, "TEXTMAP", 8))
			{
				map->isText = true;
				map->MapLumps[ML_TEXTMAP].Reader = map->resource->GetLump(i)->NewReader();
				strncpy(map->MapLumps[ML_TEXTMAP].Name, lumpname, 8);
				for(int i = 2;; i++)
				{
					lumpname = map->resource->GetLump(i)->Name;
					if (!strnicmp(lumpname, "ZNODES",8))
					{
						index = ML_GLZNODES;
					}
					else if (!strnicmp(lumpname, "BLOCKMAP",8))
					{
						// there is no real point in creating a blockmap but let's use it anyway
						index = ML_BLOCKMAP;
					}
					else if (!strnicmp(lumpname, "REJECT",8))
					{
						index = ML_REJECT;
					}
					else if (!strnicmp(lumpname, "DIALOGUE",8))
					{
						index = ML_CONVERSATION;
					}
					else if (!strnicmp(lumpname, "BEHAVIOR",8))
					{
						index = ML_BEHAVIOR;
						map->HasBehavior = true;
					}
					else if (!strnicmp(lumpname, "ENDMAP",8))
					{
						return map;
					}
					else continue;
					map->MapLumps[index].Reader = map->resource->GetLump(i)->NewReader();
					strncpy(map->MapLumps[index].Name, lumpname, 8);
				}
			}

			if (i>0)
			{
				try
				{
					index = GetMapIndex(maplabel, index, lumpname, !justcheck);
				}
				catch(...)
				{
					delete map;
					throw;
				}
				if (index == -2)
				{
					delete map;
					return NULL;
				}
				if (index == ML_BEHAVIOR) map->HasBehavior = true;

				// The next lump is not part of this map anymore
				if (index < 0) break;
			}
			else
			{
				strncpy(maplabel, lumpname, 8);
				maplabel[8]=0;
			}

			map->MapLumps[index].Reader = map->resource->GetLump(i)->NewReader();
			strncpy(map->MapLumps[index].Name, lumpname, 8);
		}
	}
	else
	{
		// This is a Build map and not subject to WAD consistency checks.
		//map->MapLumps[0].Size = wadReader->GetLength();
		if (!P_IsBuildMap(map))
		{
			delete map;
			return NULL;
		}
	}
	return map;		
}

bool P_CheckMapData(const char *mapname)
{
	MapData *mapd = P_OpenMapData(mapname, true);
	if (mapd == NULL) return false;
	delete mapd;
	return true;
}

//===========================================================================
//
// MapData :: GetChecksum
//
// Hashes a map based on its header, THINGS, LINEDEFS, SIDEDEFS, SECTORS,
// and BEHAVIOR lumps. Node-builder generated lumps are not included.
//
//===========================================================================

void MapData::GetChecksum(BYTE cksum[16])
{
	MD5Context md5;

	if (file != NULL)
	{
		if (isText)
		{
			Seek(ML_TEXTMAP);
			md5.Update(file, Size(ML_TEXTMAP));
		}
		else
		{
			if (Size(ML_LABEL) != 0)
			{
				Seek(ML_LABEL);
				md5.Update(file, Size(ML_LABEL));
			}
			Seek(ML_THINGS);
			md5.Update(file, Size(ML_THINGS));
			Seek(ML_LINEDEFS);
			md5.Update(file, Size(ML_LINEDEFS));
			Seek(ML_SIDEDEFS);
			md5.Update(file, Size(ML_SIDEDEFS));
			Seek(ML_SECTORS);
			md5.Update(file, Size(ML_SECTORS));
		}
		if (HasBehavior)
		{
			Seek(ML_BEHAVIOR);
			md5.Update(file, Size(ML_BEHAVIOR));
		}
	}
	md5.Final(cksum);
}


//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
//
//===========================================================================

static void SetTexture (side_t *side, int position, const char *name8, FMissingTextureTracker &track)
{
	static const char *positionnames[] = { "top", "middle", "bottom" };
	static const char *sidenames[] = { "first", "second" };
	char name[9];
	strncpy (name, name8, 8);
	name[8] = 0;
	FTextureID texture = TexMan.CheckForTexture (name, FTexture::TEX_Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!texture.Exists())
	{
		if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
		{
			// Print an error that lists all references to this sidedef.
			// We must scan the linedefs manually for all references to this sidedef.
			for(int i = 0; i < numlines; i++)
			{
				for(int j = 0; j < 2; j++)
				{
					if (lines[i].sidedef[j] == side)
					{
						Printf(TEXTCOLOR_RED"Unknown %s texture '"
							TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
							"' on %s side of linedef %d\n",
							positionnames[position], name, sidenames[j], i);
					}
				}
			}
		}
		texture = TexMan.GetDefaultTexture();
	}
	side->SetTexture(position, texture);
}

//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
// (Passing index separately is for UDMF which does not have sectors allocated yet)
//
//===========================================================================

void SetTexture (sector_t *sector, int index, int position, const char *name8, FMissingTextureTracker &track)
{
	static const char *positionnames[] = { "floor", "ceiling" };
	char name[9];
	strncpy (name, name8, 8);
	name[8] = 0;
	FTextureID texture = TexMan.CheckForTexture (name, FTexture::TEX_Flat,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!texture.Exists())
	{
		if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
		{
			Printf(TEXTCOLOR_RED"Unknown %s texture '"
				TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
				"' in sector %d\n",
				positionnames[position], name, index);
		}
		texture = TexMan.GetDefaultTexture();
	}
	sector->SetTexture(position, texture);
}

//===========================================================================
//
// SummarizeMissingTextures
//
// Lists textures that were missing more than MISSING_TEXTURE_WARN_LIMIT
// times.
//
//===========================================================================

static void SummarizeMissingTextures(const FMissingTextureTracker &missing)
{
	FMissingTextureTracker::ConstIterator it(missing);
	FMissingTextureTracker::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.Count > MISSING_TEXTURE_WARN_LIMIT)
		{
			Printf(TEXTCOLOR_RED "Missing texture '"
				TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
				"' is used %d more times\n",
				pair->Key.GetChars(), pair->Value.Count - MISSING_TEXTURE_WARN_LIMIT);
		}
	}
}

//===========================================================================
//
// [RH] Figure out blends for deep water sectors
//
//===========================================================================

static void SetTexture (side_t *side, int position, DWORD *blend, char *name8)
{
	char name[9];
	strncpy (name, name8, 8);
	name[8] = 0;
	FTextureID texture;
	if ((*blend = R_ColormapNumForName (name)) == 0)
	{
		texture = TexMan.CheckForTexture (name, FTexture::TEX_Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
		if (!texture.Exists())
		{
			char name2[9];
			char *stop;
			strncpy (name2, name, 8);
			name2[8] = 0;
			*blend = strtoul (name2, &stop, 16);
			texture = FNullTextureID();
		}
		else
		{
			*blend = 0;
		}
	}
	else
	{
		texture = FNullTextureID();
	}
	side->SetTexture(position, texture);
}

static void SetTextureNoErr (side_t *side, int position, DWORD *color, char *name8, bool *validcolor, bool isFog)
{
	char name[9];
	FTextureID texture;
	strncpy (name, name8, 8);
	name[8] = 0;
	*validcolor = false;
	texture = TexMan.CheckForTexture (name, FTexture::TEX_Wall,	
		FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);
	if (!texture.Exists())
	{
		char name2[9];
		char *stop;
		strncpy (name2, name+1, 7);
		name2[7] = 0;
		if (*name != '#')
		{
			*color = strtoul (name, &stop, 16);
			texture = FNullTextureID();
			*validcolor = (*stop == 0) && (stop >= name + 2) && (stop <= name + 6);
			return;
		}
		else	// Support for Legacy's color format!
		{
			int l=(int)strlen(name);
			texture = FNullTextureID();
			*validcolor = false;
			if (l>=7) 
			{
				for(stop=name2;stop<name2+6;stop++) if (!isxdigit(*stop)) *stop='0';

				int factor = l==7? 0 : clamp<int> ((name2[6]&223)-'A', 0, 25);

				name2[6]=0; int blue=strtol(name2+4,NULL,16);
				name2[4]=0; int green=strtol(name2+2,NULL,16);
				name2[2]=0; int red=strtol(name2,NULL,16);

				if (!isFog) 
				{
					if (factor==0) 
					{
						*validcolor=false;
						return;
					}
					factor = factor * 255 / 25;
				}
				else
				{
					factor=0;
				}

				*color=MAKEARGB(factor, red, green, blue);
				texture = FNullTextureID();
				*validcolor = true;
				return;
			}
		}
		texture = FNullTextureID();
	}
	side->SetTexture(position, texture);
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

		if (check->sidedef[1] == NULL || (check->flags & ML_ZONEBOUNDARY))
			continue;

		if (check->frontsector == sec)
		{
			assert(check->backsector != NULL);
			other = check->backsector;
		}
		else
		{
			assert(check->frontsector != NULL);
			other = check->frontsector;
		}

		if (other->ZoneNumber != zonenum)
			P_FloodZone (other, zonenum);
	}
}

void P_FloodZones ()
{
	int z = 0, i;
	ReverbContainer *reverb;

	for (i = 0; i < numsectors; ++i)
	{
		if (sectors[i].ZoneNumber == 0xFFFF)
		{
			P_FloodZone (&sectors[i], z++);
		}
	}
	numzones = z;
	zones = new zone_t[z];
	reverb = S_FindEnvironment(level.DefaultEnvironment);
	if (reverb == NULL)
	{
		Printf("Sound environment %d, %d not found\n", level.DefaultEnvironment >> 8, level.DefaultEnvironment & 255);
		reverb = DefaultEnvironments[0];
	}
	for (i = 0; i < z; ++i)
	{
		zones[i].Environment = reverb;
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
	numvertexes = map->Size(ML_VERTEXES) / sizeof(mapvertex_t);
	numvertexdatas = 0;

	if (numvertexes == 0)
	{
		I_Error ("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	vertexes = new vertex_t[numvertexes];		
	vertexdatas = NULL;

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

void P_LoadZSegs (FileReaderBase &data)
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
		segs[i].sidedef = ldef->sidedef[side];
		segs[i].frontsector = ldef->sidedef[side]->sector;
		if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
		{
			segs[i].backsector = ldef->sidedef[side^1]->sector;
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

void P_LoadGLZSegs (FileReaderBase &data, int type)
{
	for (int i = 0; i < numsubsectors; ++i)
	{
		for (size_t j = 0; j < subsectors[i].numlines; ++j)
		{
			seg_t *seg;
			DWORD v1, partner;
			DWORD line;
			WORD lineword;
			BYTE side;

			data >> v1 >> partner;
			if (type >= 2)
			{
				data >> line;
			}
			else
			{
				data >> lineword;
				line = lineword == 0xFFFF ? 0xFFFFFFFF : lineword;
			}
			data >> side;

			seg = subsectors[i].firstline + j;
			seg->v1 = &vertexes[v1];
			if (j == 0)
			{
				seg[subsectors[i].numlines - 1].v2 = seg->v1;
			}
			else
			{
				seg[-1].v2 = seg->v1;
			}
			glsegextras[seg - segs].PartnerSeg = partner;
			if (line != 0xFFFFFFFF)
			{
				line_t *ldef;

				seg->linedef = ldef = &lines[line];
				seg->sidedef = ldef->sidedef[side];
				seg->frontsector = ldef->sidedef[side]->sector;
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
				{
					seg->backsector = ldef->sidedef[side^1]->sector;
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
				seg->frontsector = seg->backsector = subsectors[i].firstline->frontsector;
			}
		}
	}
}

//===========================================================================
//
// P_LoadZNodes
//
//===========================================================================

void LoadZNodes(FileReaderBase &data, int glnodes)
{
	// Read extra vertices added during node building
	DWORD orgVerts, newVerts;
	vertex_t *newvertarray;
	unsigned int i;

	data >> orgVerts >> newVerts;
	if (orgVerts > (DWORD)numvertexes)
	{ // These nodes are based on a map with more vertex data than we have.
	  // We can't use them.
		throw CRecoverableError("Incorrect number of vertexes in nodes.\n");
	}
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
		subsectors[i].firstline = (seg_t *)(size_t)currSeg;		// Oh damn. I should have stored the seg count sooner.
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
		throw CRecoverableError("Incorrect number of segs in nodes.\n");
	}

	numsegs = numSegs;
	segs = new seg_t[numsegs];
	memset (segs, 0, numsegs*sizeof(seg_t));
	glsegextras = NULL;

	for (i = 0; i < numSubs; ++i)
	{
		subsectors[i].firstline = &segs[(size_t)subsectors[i].firstline];
	}

	if (glnodes == 0)
	{
		P_LoadZSegs (data);
	}
	else
	{
		glsegextras = new glsegextra_t[numsegs];
		P_LoadGLZSegs (data, glnodes);
	}

	// Read nodes
	DWORD numNodes;

	data >> numNodes;
	numnodes = numNodes;
	nodes = new node_t[numNodes];
	memset (nodes, 0, sizeof(node_t)*numNodes);

	for (i = 0; i < numNodes; ++i)
	{
		if (glnodes < 3)
		{
			SWORD x, y, dx, dy;

			data >> x >> y >> dx >> dy;
			nodes[i].x = x << FRACBITS;
			nodes[i].y = y << FRACBITS;
			nodes[i].dx = dx << FRACBITS;
			nodes[i].dy = dy << FRACBITS;
		}
		else
		{
			data >> nodes[i].x >> nodes[i].y >> nodes[i].dx >> nodes[i].dy;
		}
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


void P_LoadZNodes (FileReader &dalump, DWORD id)
{
	int type;
	bool compressed;

	switch (id)
	{
	case MAKE_ID('Z','N','O','D'):
		type = 0;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','N'):
		type = 1;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','2'):
		type = 2;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','3'):
		type = 3;
		compressed = true;
		break;

	case MAKE_ID('X','N','O','D'):
		type = 0;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','N'):
		type = 1;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','2'):
		type = 2;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','3'):
		type = 3;
		compressed = false;
		break;

	default:
		return;
	}
	
	if (compressed)
	{
		FileReaderZ data (dalump);
		LoadZNodes(data, type);
	}
	else
	{
		LoadZNodes(dalump, type);
	}
}



//===========================================================================
//
// P_CheckV4Nodes
// http://www.sbsoftware.com/files/DeePBSPV4specs.txt
//
//===========================================================================

static bool P_CheckV4Nodes(MapData *map)
{
	char header[8];

	map->Read(ML_NODES, header, 8);
	return !memcmp(header, "xNd4\0\0\0\0", 8);
}


//===========================================================================
//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

struct badseg
{
	badseg(int t, int s, int d) : badtype(t), badsegnum(s), baddata(d) {}
	int badtype;
	int badsegnum;
	int baddata;
};

template<class segtype>
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
	int lumplen = map->Size(ML_SEGS);

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	numsegs = lumplen / sizeof(segtype);

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

	for (i = 0; i < numsubsectors; ++i)
	{
		subsectors[i].firstline = &segs[(size_t)subsectors[i].firstline];
	}

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
			seg_t *li = segs + i;
			segtype *ml = ((segtype *) data) + i;

			int side, linedef;
			line_t *ldef;

			vnum1 = ml->V1();
			vnum2 = ml->V2();

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw badseg(0, i, MAX(vnum1, vnum2));
			}

			li->v1 = &vertexes[vnum1];
			li->v2 = &vertexes[vnum2];

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
				throw badseg(1, i, linedef);
			}
			ldef = &lines[linedef];
			li->linedef = ldef;
			side = LittleShort(ml->side);
			if ((unsigned)(ldef->sidedef[side] - sides) >= (unsigned)numsides)
			{
				throw badseg(2, i, int(ldef->sidedef[side] - sides));
			}
			li->sidedef = ldef->sidedef[side];
			li->frontsector = ldef->sidedef[side]->sector;

			// killough 5/3/98: ignore 2s flag if second sidedef missing:
			if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
			{
				li->backsector = ldef->sidedef[side^1]->sector;
			}
			else
			{
				li->backsector = 0;
				ldef->flags &= ~ML_TWOSIDED;
			}
		}
	}
	catch (badseg bad)
	{
		switch (bad.badtype)
		{
		case 0:
			Printf ("Seg %d references a nonexistant vertex %d (max %d).\n", bad.badsegnum, bad.baddata, numvertexes);
			break;

		case 1:
			Printf ("Seg %d references a nonexistant linedef %d (max %d).\n", bad.badsegnum, bad.baddata, numlines);
			break;

		case 2:
			Printf ("The linedef for seg %d references a nonexistant sidedef %d (max %d).\n", bad.badsegnum, bad.baddata, numsides);
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

template<class subsectortype, class segtype>
void P_LoadSubsectors (MapData * map)
{
	int i;
	DWORD maxseg = map->Size(ML_SEGS) / sizeof(segtype);

	numsubsectors = map->Size(ML_SSECTORS) / sizeof(subsectortype);

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
		subsectortype subd;

		(*map->file) >> subd.numsegs >> subd.firstseg;

		if (subd.numsegs == 0)
		{
			Printf ("Subsector %i is empty.\n", i);
			delete[] subsectors;
			delete[] nodes;
			ForceNodeBuild = true;
			return;
		}

		subsectors[i].numlines = subd.numsegs;
		subsectors[i].firstline = (seg_t *)(size_t)subd.firstseg;

		if ((size_t)subsectors[i].firstline >= maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, (unsigned)((size_t)subsectors[i].firstline),
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			delete[] nodes;
			delete[] subsectors;
			break;
		}
		else if ((size_t)subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
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

void P_LoadSectors (MapData *map, FMissingTextureTracker &missingtex)
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
	
	// Extended properties
	sectors[0].e = new extsector_t[numsectors];

	for (i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->e = &sectors[0].e[i];
		if (!map->HasBehavior) ss->Flags |= SECF_FLOORDROP;
		ss->SetPlaneTexZ(sector_t::floor, LittleShort(ms->floorheight)<<FRACBITS);
		ss->floorplane.d = -ss->GetPlaneTexZ(sector_t::floor);
		ss->floorplane.c = FRACUNIT;
		ss->floorplane.ic = FRACUNIT;
		ss->SetPlaneTexZ(sector_t::ceiling, LittleShort(ms->ceilingheight)<<FRACBITS);
		ss->ceilingplane.d = ss->GetPlaneTexZ(sector_t::ceiling);
		ss->ceilingplane.c = -FRACUNIT;
		ss->ceilingplane.ic = -FRACUNIT;
		SetTexture(ss, i, sector_t::floor, ms->floorpic, missingtex);
		SetTexture(ss, i, sector_t::ceiling, ms->ceilingpic, missingtex);
		ss->lightlevel = LittleShort(ms->lightlevel);
		if (map->HasBehavior)
			ss->special = LittleShort(ms->special);
		else	// [RH] Translate to new sector special
			ss->special = P_TranslateSectorSpecial (LittleShort(ms->special));
		ss->secretsector = !!(ss->special&SECRET_MASK);
		ss->tag = LittleShort(ms->tag);
		ss->thinglist = NULL;
		ss->touching_thinglist = NULL;		// phares 3/14/98
		ss->seqType = defSeqType;
		ss->SeqName = NAME_None;
		ss->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		ss->prevsec = -1;	// stair retriggering until build completes

		ss->SetAlpha(sector_t::floor, FRACUNIT);
		ss->SetAlpha(sector_t::ceiling, FRACUNIT);
		ss->SetXScale(sector_t::floor, FRACUNIT);	// [RH] floor and ceiling scaling
		ss->SetYScale(sector_t::floor, FRACUNIT);
		ss->SetXScale(sector_t::ceiling, FRACUNIT);
		ss->SetYScale(sector_t::ceiling, FRACUNIT);

		ss->heightsec = NULL;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		ss->gravity = 1.f;	// [RH] Default sector gravity of 1.0
		ss->ZoneNumber = 0xFFFF;

		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		if (level.outsidefog != 0xff000000 && (ss->GetTexture(sector_t::ceiling) == skyflatnum || (ss->special&0xff) == Sector_Outside))
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
		ss->sectornum = i;
	}
	delete[] msp;
}


//===========================================================================
//
// P_LoadNodes
//
//===========================================================================

template<class nodetype, class subsectortype>
void P_LoadNodes (MapData * map)
{
	FMemLump	data;
	int 		i;
	int 		j;
	int 		k;
	char		*mnp;
	nodetype	*mn;
	node_t* 	no;
	WORD*		used;
	int			lumplen = map->Size(ML_NODES);
	int			maxss = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	numnodes = (lumplen - nodetype::NF_LUMPOFFSET) / sizeof(nodetype);

	if ((numnodes == 0 && maxss != 1) || maxss == 0)
	{
		ForceNodeBuild = true;
		return;
	}
	
	nodes = new node_t[numnodes];		
	used = (WORD *)alloca (sizeof(WORD)*numnodes);
	memset (used, 0, sizeof(WORD)*numnodes);

	mnp = new char[lumplen];
	mn = (nodetype*)(mnp + nodetype::NF_LUMPOFFSET);
	map->Read(ML_NODES, mnp);
	no = nodes;
	
	for (i = 0; i < numnodes; i++, no++, mn++)
	{
		no->x = LittleShort(mn->x)<<FRACBITS;
		no->y = LittleShort(mn->y)<<FRACBITS;
		no->dx = LittleShort(mn->dx)<<FRACBITS;
		no->dy = LittleShort(mn->dy)<<FRACBITS;
		for (j = 0; j < 2; j++)
		{
			int child = mn->Child(j);
			if (child & nodetype::NF_SUBSECTOR)
			{
				child &= ~nodetype::NF_SUBSECTOR;
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
				Printf ("BSP node %d references invalid node %td.\n"
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
// SpawnMapThing
//
//===========================================================================
CVAR(Bool, dumpspawnedthings, false, 0)

AActor *SpawnMapThing(int index, FMapThing *mt, int position)
{
	AActor *spawned = P_SpawnMapThing(mt, position);
	if (dumpspawnedthings)
	{
		Printf("%5d: (%5d, %5d, %5d), doomednum = %5d, flags = %04x, type = %s\n",
			index, mt->x>>FRACBITS, mt->y>>FRACBITS, mt->z>>FRACBITS, mt->type, mt->flags, 
			spawned? spawned->GetClass()->TypeName.GetChars() : "(none)");
	}
	T_AddSpawnedThing(spawned);
	return spawned;
}

//===========================================================================
//
// SetMapThingUserData
//
//===========================================================================

static void SetMapThingUserData(AActor *actor, unsigned udi)
{
	if (actor == NULL)
	{
		return;
	}
	while (MapThingsUserData[udi].Property != NAME_None)
	{
		FName varname = MapThingsUserData[udi].Property;
		int value = MapThingsUserData[udi].Value;
		PSymbol *sym = actor->GetClass()->Symbols.FindSymbol(varname, true);
		PSymbolVariable *var;

		udi++;

		if (sym == NULL || sym->SymbolType != SYM_Variable ||
			!(var = static_cast<PSymbolVariable *>(sym))->bUserVar ||
			var->ValueType.Type != VAL_Int)
		{
			DPrintf("%s is not a user variable in class %s\n", varname.GetChars(),
				actor->GetClass()->TypeName.GetChars());
		}
		else
		{ // Set the value of the specified user variable.
			*(int *)(reinterpret_cast<BYTE *>(actor) + var->offset) = value;
		}
	}
}

//===========================================================================
//
// P_LoadThings
//
//===========================================================================

WORD MakeSkill(int flags)
{
	WORD res = 0;
	if (flags & 1) res |= 1+2;
	if (flags & 2) res |= 4;
	if (flags & 4) res |= 8+16;
	return res;
}

void P_LoadThings (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthing_t);

	char *mtp;
	mapthing_t *mt;

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mt = (mapthing_t*)mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	for (int i=0 ; i < numthings; i++, mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. P_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = LittleShort(mt->options);

		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].gravity = FRACUNIT;
		mti[i].Conversation = 0;
		mti[i].SkillFilter = MakeSkill(flags);
		mti[i].ClassFilter = 0xffff;	// Doom map format doesn't have class flags so spawn for all player classes
		mti[i].RenderStyle = STYLE_Count;
		mti[i].alpha = -1;
		mti[i].health = 1;
		flags &= ~MTF_SKILLMASK;
		mti[i].flags = (short)((flags & 0xf) | 0x7e0);
		if (gameinfo.gametype == GAME_Strife)
		{
			mti[i].flags &= ~MTF_AMBUSH;
			if (flags & STF_SHADOW)			mti[i].flags |= MTF_SHADOW;
			if (flags & STF_ALTSHADOW)		mti[i].flags |= MTF_ALTSHADOW;
			if (flags & STF_STANDSTILL)		mti[i].flags |= MTF_STANDSTILL;
			if (flags & STF_AMBUSH)			mti[i].flags |= MTF_AMBUSH;
			if (flags & STF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
		}
		else
		{
			if (flags & BTF_BADEDITORCHECK)
			{
				flags &= 0x1F;
			}
			if (flags & BTF_NOTDEATHMATCH)	mti[i].flags &= ~MTF_DEATHMATCH;
			if (flags & BTF_NOTCOOPERATIVE)	mti[i].flags &= ~MTF_COOPERATIVE;
			if (flags & BTF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
		}
		if (flags & BTF_NOTSINGLE)			mti[i].flags &= ~MTF_SINGLE;

		mti[i].x = LittleShort(mt->x) << FRACBITS;
		mti[i].y = LittleShort(mt->y) << FRACBITS;
		mti[i].angle = LittleShort(mt->angle);
		mti[i].type = LittleShort(mt->type);
	}
	delete [] mtp;
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

void P_LoadThings2 (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthinghexen_t);

	char *mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mapthinghexen_t *mth = (mapthinghexen_t*)mtp;

	for(int i = 0; i< numthings; i++)
	{
		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].thingid = LittleShort(mth[i].thingid);
		mti[i].x = LittleShort(mth[i].x)<<FRACBITS;
		mti[i].y = LittleShort(mth[i].y)<<FRACBITS;
		mti[i].z = LittleShort(mth[i].z)<<FRACBITS;
		mti[i].angle = LittleShort(mth[i].angle);
		mti[i].type = LittleShort(mth[i].type);
		mti[i].flags = LittleShort(mth[i].flags);
		mti[i].special = mth[i].special;
		for(int j=0;j<5;j++) mti[i].args[j] = mth[i].args[j];
		mti[i].SkillFilter = MakeSkill(mti[i].flags);
		mti[i].ClassFilter = (mti[i].flags & MTF_CLASS_MASK) >> MTF_CLASS_SHIFT;
		mti[i].flags &= ~(MTF_SKILLMASK|MTF_CLASS_MASK);
		mti[i].gravity = FRACUNIT;
		mti[i].RenderStyle = STYLE_Count;
		mti[i].alpha = -1;
		mti[i].health = 1;
	}
	delete[] mtp;
}


void P_SpawnThings (int position)
{
	int numthings = MapThingsConverted.Size();

	for (int i=0; i < numthings; i++)
	{
		AActor *actor = SpawnMapThing (i, &MapThingsConverted[i], position);
		unsigned *udi = MapThingsUserDataIndex.CheckKey((unsigned)i);
		if (udi != NULL)
		{
			SetMapThingUserData(actor, *udi);
		}
	}
	for(int i=0; i<MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo != NULL)
			P_PlayerStartStomp(players[i].mo);
	}
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
}

void P_SetLineID (line_t *ld)
{
	// [RH] Set line id (as appropriate) here
	// for Doom format maps this must be done in P_TranslateLineDef because
	// the tag doesn't always go into the first arg.
	if (level.maptype == MAPTYPE_HEXEN)	
	{
		switch (ld->special)
		{
		case Line_SetIdentification:
			if (!(level.flags2 & LEVEL2_HEXENHACK))
			{
				ld->id = ld->args[0] + 256 * ld->args[4];
				ld->flags |= ld->args[1]<<16;
			}
			else
			{
				ld->id = ld->args[0];
			}
			ld->special = 0;
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
			
		case Plane_Align:
			ld->id = ld->args[2];
			break;
			
		case Static_Init:
			if (ld->args[1] == Init_SectorLink) ld->id = ld->args[0];
			break;
		}
	}
}

void P_SaveLineSpecial (line_t *ld)
{
	if (ld->sidedef[0] == NULL)
		return;

	DWORD sidenum = DWORD(ld->sidedef[0]-sides);
	// killough 4/4/98: support special sidedef interpretation below
	// [RH] Save Static_Init only if it's interested in the textures
	if	(ld->special != Static_Init || ld->args[1] == Init_Color)
	{
		sidetemp[sidenum].a.special = ld->special;
		sidetemp[sidenum].a.tag = ld->args[0];
	}
	else
	{
		sidetemp[sidenum].a.special = 0;
	}
}

void P_FinishLoadingLineDef(line_t *ld, int alpha)
{
	bool additive = false;

	ld->frontsector = ld->sidedef[0] != NULL ? ld->sidedef[0]->sector : NULL;
	ld->backsector  = ld->sidedef[1] != NULL ? ld->sidedef[1]->sector : NULL;
	double dx = FIXED2DBL(ld->v2->x - ld->v1->x);
	double dy = FIXED2DBL(ld->v2->y - ld->v1->y);
	int linenum = int(ld-lines);

	if (ld->frontsector == NULL)
	{
		Printf ("Line %d has no front sector\n", linemap[linenum]);
	}

	// [RH] Set some new sidedef properties
	int len = (int)(sqrt (dx*dx + dy*dy) + 0.5f);

	if (ld->sidedef[0] != NULL)
	{
		ld->sidedef[0]->linedef = ld;
		ld->sidedef[0]->TexelLength = len;

	}
	if (ld->sidedef[1] != NULL)
	{
		ld->sidedef[1]->linedef = ld;
		ld->sidedef[1]->TexelLength = len;
	}

	switch (ld->special)
	{						// killough 4/11/98: handle special types
		int j;

	case TranslucentLine:			// killough 4/11/98: translucent 2s textures
		// [RH] Second arg controls how opaque it is.
		if (alpha == SHRT_MIN)
		{
			alpha = ld->args[1];
			additive = !!ld->args[2];
		}
		else if (alpha < 0)
		{
			alpha = -alpha;
			additive = true;
		}

		alpha = Scale(alpha, FRACUNIT, 255); 
		if (!ld->args[0])
		{
			ld->Alpha = alpha;
			if (additive)
			{
				ld->flags |= ML_ADDTRANS;
			}
		}
		else
		{
			for (j = 0; j < numlines; j++)
			{
				if (lines[j].id == ld->args[0])
				{
					lines[j].Alpha = alpha;
					if (additive)
					{
						lines[j].flags |= ML_ADDTRANS;
					}
				}
			}
		}
		ld->special = 0;
		break;
	}
}
// killough 4/4/98: delay using sidedefs until they are loaded
void P_FinishLoadingLineDefs ()
{
	for (int i = 0; i < numlines; i++)
	{
		P_FinishLoadingLineDef(&lines[i], sidetemp[lines[i].sidedef[0]-sides].a.alpha);
	}
}

static void P_SetSideNum (side_t **sidenum_p, WORD sidenum)
{
	if (sidenum == NO_INDEX)
	{
		*sidenum_p = NULL;
	}
	else if (sidecount < numsides)
	{
		sidetemp[sidecount].a.map = sidenum;
		*sidenum_p = &sides[sidecount++];
	}
	else
	{
		I_Error ("%d sidedefs is not enough\n", sidecount);
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
	linemap.Resize(numlines);
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
			// patch missing first sides instead of crashing out.
			// Visual glitches are better than not being able to play.
			if (LittleShort(mld->sidenum[0]) == NO_INDEX)
			{
				Printf("Line %d has no first side.\n", i);
				mld->sidenum[0] = 0;
			}
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
		ld->Alpha = FRACUNIT;	// [RH] Opaque by default

		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.
		P_TranslateLineDef (ld, mld);

		ld->v1 = &vertexes[LittleShort(mld->v1)];
		ld->v2 = &vertexes[LittleShort(mld->v2)];
		//ld->id = -1;		ID has been assigned in P_TranslateLineDef

		P_SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
		P_SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SaveLineSpecial (ld);
		if (level.flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (level.flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;
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
	linemap.Resize(numlines);
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
			// patch missing first sides instead of crashing out.
			// Visual glitches are better than not being able to play.
			if (LittleShort(mld->sidenum[0]) == NO_INDEX)
			{
				Printf("Line %d has no first side.\n", i);
				mld->sidenum[0] = 0;
			}
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
		ld->Alpha = FRACUNIT;	// [RH] Opaque by default
		ld->id = -1;

		P_SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
		P_SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SetLineID(ld);
		P_SaveLineSpecial (ld);
		if (level.flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (level.flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;

		// convert the activation type
		ld->activation = 1 << GET_SPAC(ld->flags);
		if (ld->activation == SPAC_AnyCross)
		{ // this is really PTouch
			ld->activation = SPAC_Impact | SPAC_PCross;
		}
		else if (ld->activation == SPAC_Impact)
		{ // In non-UMDF maps, Impact implies PCross
			ld->activation = SPAC_Impact | SPAC_PCross;
		}
		ld->flags &= ~ML_SPAC_MASK;
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
		sidetemp[i].a.alpha = SHRT_MIN;
		sidetemp[i].a.map = NO_SIDE;
	}
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}


// [RH] Group sidedefs into loops so that we can easily determine
// what walls any particular wall neighbors.

static void P_LoopSidedefs (bool firstloop)
{
	int i;

	if (sidetemp != NULL)
	{
		delete[] sidetemp;
	}
	sidetemp = new sidei_t[MAX(numvertexes, numsides)];

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
		line_t *line = sides[i].linedef;
		int lineside = (line->sidedef[0] != &sides[i]);
		int vert = int((lineside ? line->v2 : line->v1) - vertexes);
		
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
		line_t *line = sides[i].linedef;

		// If the side's line only exists in a single sector,
		// then consider that line to be a self-contained loop
		// instead of as part of another loop
		if (line->frontsector == line->backsector)
		{
			right = DWORD(line->sidedef[!sidetemp[i].b.lineside] - sides);
		}
		else
		{
			if (sidetemp[i].b.lineside)
			{
				right = int(line->v1 - vertexes);
			}
			else
			{
				right = int(line->v2 - vertexes);
			}

			right = sidetemp[right].b.first;

			if (right == NO_SIDE)
			{ 
				// There is no right side!
				if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[unsigned(line-lines)]);
				continue;
			}

			if (sidetemp[right].b.next != NO_SIDE)
			{
				int bestright = right;	// Shut up, GCC
				angle_t bestang = ANGLE_MAX;
				line_t *leftline, *rightline;
				angle_t ang1, ang2, ang;

				leftline = sides[i].linedef;
				ang1 = R_PointToAngle2 (0, 0, leftline->dx, leftline->dy);
				if (!sidetemp[i].b.lineside)
				{
					ang1 += ANGLE_180;
				}

				while (right != NO_SIDE)
				{
					if (sides[right].LeftSide == NO_SIDE)
					{
						rightline = sides[right].linedef;
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
		assert((unsigned)i<(unsigned)numsides);
		assert(right<(unsigned)numsides);
		sides[i].RightSide = right;
		sides[right].LeftSide = i;
	}

	// We keep the sidedef init info around until after polyobjects are initialized,
	// so don't delete just yet.
}

int P_DetermineTranslucency (int lumpnum)
{
	FWadLump tranmap = Wads.OpenLumpNum (lumpnum);
	BYTE index;
	PalEntry newcolor;
	PalEntry newcolor2;

	tranmap.Seek (GPalette.BlackIndex * 256 + GPalette.WhiteIndex, SEEK_SET);
	tranmap.Read (&index, 1);

	newcolor = GPalette.BaseColors[GPalette.Remap[index]];

	tranmap.Seek (GPalette.WhiteIndex * 256 + GPalette.BlackIndex, SEEK_SET);
	tranmap.Read (&index, 1);
	newcolor2 = GPalette.BaseColors[GPalette.Remap[index]];
	if (newcolor2.r == 255)	// if black on white results in white it's either
							// fully transparent or additive
	{
		if (developer)
		{
			char lumpname[9];
			lumpname[8] = 0;
			Wads.GetLumpName (lumpname, lumpnum);
			Printf ("%s appears to be additive translucency %d (%d%%)\n", lumpname, newcolor.r,
				newcolor.r*100/255);
		}
		return -newcolor.r;
	}

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

void P_ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, mapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex)
{
	char name[9];
	name[8] = 0;

	switch (special)
	{
	case Transfer_Heights:	// variable colormap via 242 linedef
		  // [RH] The colormap num we get here isn't really a colormap,
		  //	  but a packed ARGB word for blending, so we also allow
		  //	  the blend to be specified directly by the texture names
		  //	  instead of figuring something out from the colormap.
		if (sec != NULL)
		{
			SetTexture (sd, side_t::bottom, &sec->bottommap, msd->bottomtexture);
			SetTexture (sd, side_t::mid, &sec->midmap, msd->midtexture);
			SetTexture (sd, side_t::top, &sec->topmap, msd->toptexture);
		}
		break;

	case Static_Init:
		// [RH] Set sector color and fog
		// upper "texture" is light color
		// lower "texture" is fog color
		{
			DWORD color = MAKERGB(255,255,255), fog = 0;
			bool colorgood, foggood;

			SetTextureNoErr (sd, side_t::bottom, &fog, msd->bottomtexture, &foggood, true);
			SetTextureNoErr (sd, side_t::top, &color, msd->toptexture, &colorgood, false);
			strncpy (name, msd->midtexture, 8);
			SetTexture(sd, side_t::mid, msd->midtexture, missingtex);

			if (colorgood | foggood)
			{
				int s;
				FDynamicColormap *colormap = NULL;

				for (s = 0; s < numsectors; s++)
				{
					if (sectors[s].tag == tag)
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

#ifdef _3DFLOORS
	case Sector_Set3DFloor:
		if (msd->toptexture[0]=='#')
		{
			strncpy (name, msd->toptexture, 8);
			sd->SetTexture(side_t::top, FNullTextureID() +(-strtol(name+1, NULL, 10)));	// store the alpha as a negative texture index
														// This will be sorted out by the 3D-floor code later.
		}
		else
		{
			SetTexture(sd, side_t::top, msd->toptexture, missingtex);
		}

		SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
		SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
		break;
#endif

	case TranslucentLine:	// killough 4/11/98: apply translucency to 2s normal texture
		if (checktranmap)
		{
			int lumpnum;

			if (strnicmp ("TRANMAP", msd->midtexture, 8) == 0)
			{
				// The translator set the alpha argument already; no reason to do it again.
				sd->SetTexture(side_t::mid, FNullTextureID());
			}
			else if ((lumpnum = Wads.CheckNumForName (msd->midtexture)) > 0 &&
				Wads.LumpLength (lumpnum) == 65536)
			{
				*alpha = (short)P_DetermineTranslucency (lumpnum);
				sd->SetTexture(side_t::mid, FNullTextureID());
			}
			else
			{
				SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
			}

			SetTexture(sd, side_t::top, msd->toptexture, missingtex);
			SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
			break;
		}
		// Fallthrough for Hexen maps is intentional

	default:			// normal cases

		SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
		SetTexture(sd, side_t::top, msd->toptexture, missingtex);
		SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
		break;
	}
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2 (MapData *map, FMissingTextureTracker &missingtex)
{
	int  i;
	char * msdf = new char[map->Size(ML_SIDEDEFS)];
	map->Read(ML_SIDEDEFS, msdf);

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

		sd->SetTextureXOffset(LittleShort(msd->textureoffset)<<FRACBITS);
		sd->SetTextureYOffset(LittleShort(msd->rowoffset)<<FRACBITS);
		sd->SetTextureXScale(FRACUNIT);
		sd->SetTextureYScale(FRACUNIT);
		sd->linedef = NULL;
		sd->Flags = 0;
		sd->Index = i;

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
		P_ProcessSideTextures(!map->HasBehavior, sd, sec, msd, 
							  sidetemp[i].a.special, sidetemp[i].a.tag, &sidetemp[i].a.alpha, missingtex);
	}
	delete[] msdf;
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

static unsigned int BlockHash (TArray<int> *block)
{
	int hash = 0;
	int *ar = &(*block)[0];
	for (size_t i = 0; i < block->Size(); ++i)
	{
		hash = hash * 12235 + ar[i];
	}
	return hash & 0x7fffffff;
}

static bool BlockCompare (TArray<int> *block1, TArray<int> *block2)
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
	int *ar1 = &(*block1)[0];
	int *ar2 = &(*block2)[0];
	for (size_t i = 0; i < size; ++i)
	{
		if (ar1[i] != ar2[i])
		{
			return false;
		}
	}
	return true;
}

static void CreatePackedBlockmap (TArray<int> &BlockMap, TArray<int> *blocks, int bmapwidth, int bmapheight)
{
	int buckets[4096];
	int *hashes, hashblock;
	TArray<int> *block;
	int zero = 0;
	int terminator = -1;
	int *array;
	int i, hash;
	int hashed = 0, nothashed = 0;

	hashes = new int[bmapwidth * bmapheight];

	memset (hashes, 0xff, sizeof(int)*bmapwidth*bmapheight);
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
	TArray<int> *BlockLists, *block, *endblock;
	int adder;
	int bmapwidth, bmapheight;
	int minx, maxx, miny, maxy;
	int i;
	int line;

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

	BlockLists = new TArray<int>[bmapwidth * bmapheight];

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
				swapvalues (block, endblock);
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
				swapvalues (block, endblock);
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
// P_VerifyBlockMap
//
// haleyjd 03/04/10: do verification on validity of blockmap.
//
static bool P_VerifyBlockMap(int count)
{
	int x, y;
	int *maxoffs = blockmaplump + count;

	int bmapwidth = blockmaplump[2];
	int bmapheight = blockmaplump[3];

	for(y = 0; y < bmapheight; y++)
	{
		for(x = 0; x < bmapwidth; x++)
		{
			int offset;
			int *list, *tmplist;
			int *blockoffset;

			offset = y * bmapwidth + x;
			blockoffset = blockmaplump + offset + 4;


			// check that block offset is in bounds
			if(blockoffset >= maxoffs)
			{
				Printf(PRINT_HIGH, "P_VerifyBlockMap: block offset overflow\n");
				return false;
			}

			offset = *blockoffset;         

			// check that list offset is in bounds
			if(offset < 4 || offset >= count)
			{
				Printf(PRINT_HIGH, "P_VerifyBlockMap: list offset overflow\n");
				return false;
			}

			list   = blockmaplump + offset;

			// scan forward for a -1 terminator before maxoffs
			for(tmplist = list; ; tmplist++)
			{
				// we have overflowed the lump?
				if(tmplist >= maxoffs)
				{
					Printf(PRINT_HIGH, "P_VerifyBlockMap: open blocklist\n");
					return false;
				}
				if(*tmplist == -1) // found -1
					break;
			}

			// scan the list for out-of-range linedef indicies in list
			for(tmplist = list; *tmplist != -1; tmplist++)
			{
				if(*tmplist < 0 || *tmplist >= numlines)
				{
					Printf(PRINT_HIGH, "P_VerifyBlockMap: index >= numlines\n");
					return false;
				}
			}
		}
	}

	return true;
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
		Args->CheckParm("-blockmap")
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

		if (!P_VerifyBlockMap(count))
		{
			DPrintf ("Generating BLOCKMAP\n");
			P_CreateBlockMap();
		}

	}

	bmaporgx = blockmaplump[0] << FRACBITS;
	bmaporgy = blockmaplump[1] << FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];
	// MAES: set blockmapxneg and blockmapyneg
	// E.g. for a full 512x512 map, they should be both
	// -1. For a 257*257, they should be both -255 etc.
	bmapnegx = bmapwidth > 255 ? bmapwidth - 512 : -257;
	bmapnegy = bmapheight > 255 ? bmapheight - 512 : -257;

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
	cycle_t times[16];
	int*				linesDoneInEachSector;
	int 				i;
	int 				j;
	int 				total;
	int					totallights;
	line_t* 			li;
	sector_t*			sector;
	FBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	unsigned int		jj;

	for (i = 0; i < (int)countof(times); ++i)
	{
		times[i].Reset();
	}

	// look up sector number for each subsector
	times[0].Clock();
	for (i = 0; i < numsubsectors; i++)
	{
		subsectors[i].sector = subsectors[i].firstline->sidedef->sector;
	}
	if (glsegextras != NULL)
	{
		for (i = 0; i < numsubsectors; i++)
		{
			for (jj = 0; jj < subsectors[i].numlines; ++jj)
			{
				glsegextras[subsectors[i].firstline - segs + jj].Subsector = &subsectors[i];
			}
		}
	}
	times[0].Unclock();

	// count number of lines in each sector
	times[1].Clock();
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
	}
	if (flaggedNoFronts)
	{
		I_Error ("You need to fix these lines to play this map.\n");
	}
	times[1].Unclock();

	// build line tables for each sector
	times[3].Clock();
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

		// For triangular sectors the above does not calculate good points unless the longest of the triangle's lines is perfectly horizontal and vertical
		if (sector->linecount == 3)
		{
			vertex_t *Triangle[2];
			Triangle[0] = sector->lines[0]->v1;
			Triangle[1] = sector->lines[0]->v2;
			if (sector->linecount > 1)
			{
				fixed_t dx = Triangle[1]->x - Triangle[0]->x;
				fixed_t dy = Triangle[1]->y - Triangle[0]->y;
				// Find another point in the sector that does not lie
				// on the same line as the first two points.
				for (j = 0; j < 2; ++j)
				{
					vertex_t *v;

					v = (j == 1) ? sector->lines[1]->v1 : sector->lines[1]->v2;
					if (DMulScale32 (v->y - Triangle[0]->y, dx,
									Triangle[0]->x - v->x, dy) != 0)
					{
						sector->soundorg[0] = Triangle[0]->x / 3 + Triangle[1]->x / 3 + v->x / 3;
						sector->soundorg[1] = Triangle[0]->y / 3 + Triangle[1]->y / 3 + v->y / 3;
						break;
					}
				}
			}
		}

	}
	delete[] linesDoneInEachSector;
	times[3].Unclock();

	// [RH] Moved this here
	times[4].Clock();
	P_InitTagLists();   // killough 1/30/98: Create xref tables for tags
	times[4].Unclock();

	times[5].Clock();
	if (!buildmap)
	{
		P_SetSlopes ();
	}
	times[5].Unclock();

	if (showloadtimes)
	{
		Printf ("---Group Lines Times---\n");
		for (i = 0; i < 7; ++i)
		{
			Printf (" time %d:%9.4f ms\n", i, times[i].TimeMS());
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
			if (rejectmatrix[qwords + i] != 0)
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

		if (gameinfo.gametype == GAME_Hexen)
		{
			spot1 = PO_HEX_SPAWN_TYPE;
			spot2 = PO_HEX_SPAWNCRUSH_TYPE;
			anchor = PO_HEX_ANCHOR_TYPE;
		}
		else
		{
			spot1 = PO_SPAWN_TYPE;
			spot2 = PO_SPAWNCRUSH_TYPE;
			anchor = PO_ANCHOR_TYPE;
		}
		spot3 = PO_SPAWNHURT_TYPE;

		for (unsigned int i = 0; i < MapThingsConverted.Size(); ++i)
		{
			if (MapThingsConverted[i].type == spot1 || MapThingsConverted[i].type == spot2 || 
				MapThingsConverted[i].type == spot3 || MapThingsConverted[i].type == anchor)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = MapThingsConverted[i].x;
				newvert.y = MapThingsConverted[i].y;
				newvert.polynum = MapThingsConverted[i].angle;
				if (MapThingsConverted[i].type == anchor)
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

extern polyblock_t **PolyBlockMap;

void P_FreeLevelData ()
{
	Renderer->CleanLevelData();
	FPolyObj::ClearAllSubsectorLinks(); // can't be done as part of the polyobj deletion process.
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
	numvertexes = 0;
	if (segs != NULL)
	{
		delete[] segs;
		segs = NULL;
	}
	numsegs = 0;
	if (glsegextras != NULL)
	{
		delete[] glsegextras;
		glsegextras = NULL;
	}
	if (sectors != NULL)
	{
		delete[] sectors[0].e;
		delete[] sectors;
		sectors = NULL;
	}
	numsectors = 0;
	if (gamenodes != NULL && gamenodes != nodes)
	{
		delete[] gamenodes;
	}
	if (gamesubsectors != NULL && gamesubsectors != subsectors)
	{
		delete[] gamesubsectors;
	}
	if (subsectors != NULL)
	{
		for (int i = 0; i < numsubsectors; ++i)
		{
			if (subsectors[i].BSP != NULL)
			{
				delete subsectors[i].BSP;
			}
		}
		delete[] subsectors;
	}
	if (nodes != NULL)
	{
		delete[] nodes;
	}
	subsectors = gamesubsectors = NULL;
	numsubsectors = numgamesubsectors = 0;
	nodes = gamenodes = NULL;
	numnodes = numgamenodes = 0;
	if (lines != NULL)
	{
		delete[] lines;
		lines = NULL;
	}
	numlines = 0;
	if (sides != NULL)
	{
		delete[] sides;
		sides = NULL;
	}
	numsides = 0;

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
	if (linebuffer != NULL)
	{
		delete[] linebuffer;
		linebuffer = NULL;
	}
	if (polyobjs != NULL)
	{
		delete[] polyobjs;
		polyobjs = NULL;
	}
	po_NumPolyobjs = 0;
	if (zones != NULL)
	{
		delete[] zones;
		zones = NULL;
	}
	numzones = 0;
	P_FreeStrifeConversations ();
	if (level.Scrolls != NULL)
	{
		delete[] level.Scrolls;
		level.Scrolls = NULL;
	}
	P_ClearUDMFKeys();
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
		FBlockNode::FreeBlocks = NULL;
	}
	{
		msecnode_t *node = headsecnode;

		while (node != NULL)
		{
			msecnode_t *next = node->m_snext;
			M_Free (node);
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
	cycle_t times[20];
	FMapThing *buildthings;
	int numbuildthings;
	int i;
	bool buildmap;
	const int *oldvertextable = NULL;

	// This is motivated as follows:

	bool RequireGLNodes = Renderer->RequireGLNodes() || am_textured;

	for (i = 0; i < (int)countof(times); ++i)
	{
		times[i].Reset();
	}

	level.maptype = MAPTYPE_UNKNOWN;
	wminfo.partime = 180;

	MapThingsConverted.Clear();
	MapThingsUserDataIndex.Clear();
	MapThingsUserData.Clear();
	linemap.Clear();
	FCanvasTextureInfo::EmptyList ();
	R_FreePastViewers ();
	P_ClearUDMFKeys();

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
	// [RH] Clear any scripted translation colors the previous level may have set.
	for (i = 0; i < int(translationtables[TRANSLATION_LevelScripted].Size()); ++i)
	{
		FRemapTable *table = translationtables[TRANSLATION_LevelScripted][i];
		if (table != NULL)
		{
			delete table;
			translationtables[TRANSLATION_LevelScripted][i] = NULL;
		}
	}
	translationtables[TRANSLATION_LevelScripted].Clear();

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer].viewz = 1; 

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start ();
	// [RH] Clear all ThingID hash chains.
	AActor::ClearTIDHashes ();

	// [RH] clear out the mid-screen message
	C_MidPrint (NULL, NULL);

	// Free all level data from the previous map
	P_FreeLevelData ();
	interpolator.ClearInterpolations();	// [RH] Nothing to interpolate on a fresh level.

	MapData *map = P_OpenMapData(lumpname, true);
	if (map == NULL)
	{
		I_Error("Unable to open map '%s'\n", lumpname);
	}

	// find map num
	level.lumpnum = map->lumpnum;
	hasglnodes = false;

	// [RH] Support loading Build maps (because I felt like it. :-)
	buildmap = false;
	if (map->Size(0) > 0)
	{
		BYTE *mapdata = new BYTE[map->Size(0)];
		map->Seek(0);
		map->file->Read(mapdata, map->Size(0));
		times[0].Clock();
		buildmap = P_LoadBuildMap (mapdata, map->Size(0), &buildthings, &numbuildthings);
		times[0].Unclock();
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
			level.maptype = MAPTYPE_HEXEN;
		}
		else
		{
			// We need translators only for Doom format maps.
			const char *translator;

			if (!level.info->Translator.IsEmpty())
			{
				// The map defines its own translator.
				translator = level.info->Translator.GetChars();
			}
			else
			{
				// Has the user overridden the game's default translator with a commandline parameter?
				translator = Args->CheckValue("-xlat");
				if (translator == NULL) 
				{
					// Use the game's default.
					translator = gameinfo.translator.GetChars();
				}
			}
			P_LoadTranslator(translator);
			level.maptype = MAPTYPE_DOOM;
		}
		if (map->isText)
		{
			level.maptype = MAPTYPE_UDMF;
		}
		CheckCompatibility(map);
		if (ib_compatflags & BCOMPATF_REBUILDNODES)
		{
			ForceNodeBuild = true;
		}
		T_LoadScripts(map);

		if (!map->HasBehavior || map->isText)
		{
			// Doom format and UDMF text maps get strict monster activation unless the mapinfo
			// specifies differently.
			if (!(level.flags2 & LEVEL2_LAXACTIVATIONMAPINFO))
			{
				level.flags2 &= ~LEVEL2_LAXMONSTERACTIVATION;
			}
		}

		if (!map->HasBehavior && !map->isText)
		{
			// set compatibility flags
			if (gameinfo.gametype == GAME_Strife)
			{
				level.flags2 |= LEVEL2_RAILINGHACK;
			}
			level.flags2 |= LEVEL2_DUMMYSWITCHES;
		}

		FBehavior::StaticLoadDefaultModules ();

		P_LoadStrifeConversations (map, lumpname);

		FMissingTextureTracker missingtex;

		if (!map->isText)
		{
			times[0].Clock();
			P_LoadVertexes (map);
			times[0].Unclock();
			
			// Check for maps without any BSP data at all (e.g. SLIGE)
			times[1].Clock();
			P_LoadSectors (map, missingtex);
			times[1].Unclock();

			times[2].Clock();
			P_LoadSideDefs (map);
			times[2].Unclock();

			times[3].Clock();
			if (!map->HasBehavior)
				P_LoadLineDefs (map);
			else
				P_LoadLineDefs2 (map);	// [RH] Load Hexen-style linedefs
			times[3].Unclock();

			times[4].Clock();
			P_LoadSideDefs2 (map, missingtex);
			times[4].Unclock();

			times[5].Clock();
			P_FinishLoadingLineDefs ();
			times[5].Unclock();

			if (!map->HasBehavior)
				P_LoadThings (map);
			else
				P_LoadThings2 (map);	// [RH] Load Hexen-style things

			SetCompatibilityParams();
		}
		else
		{
			times[0].Clock();
			P_ParseTextMap(map, missingtex);
			times[0].Unclock();
		}

		times[6].Clock();
		P_LoopSidedefs (true);
		times[6].Unclock();

		linemap.Clear();
		linemap.ShrinkToFit();

		SummarizeMissingTextures(missingtex);
	}
	else
	{
		ForceNodeBuild = true;
		level.maptype = MAPTYPE_BUILD;
	}
	bool reloop = false;

	if (!ForceNodeBuild)
	{
		// Check for compressed nodes first, then uncompressed nodes
		FWadLump test;
		DWORD id = MAKE_ID('X','x','X','x'), idcheck = 0, idcheck2 = 0, idcheck3 = 0, idcheck4 = 0, idcheck5 = 0, idcheck6 = 0;

		if (map->Size(ML_ZNODES) != 0)
		{
			// Test normal nodes first
			map->Seek(ML_ZNODES);
			idcheck = MAKE_ID('Z','N','O','D');
			idcheck2 = MAKE_ID('X','N','O','D');
		}
		else if (map->Size(ML_GLZNODES) != 0)
		{
			map->Seek(ML_GLZNODES);
			idcheck = MAKE_ID('Z','G','L','N');
			idcheck2 = MAKE_ID('Z','G','L','2');
			idcheck3 = MAKE_ID('Z','G','L','3');
			idcheck4 = MAKE_ID('X','G','L','N');
			idcheck5 = MAKE_ID('X','G','L','2');
			idcheck6 = MAKE_ID('X','G','L','3');
		}

		map->file->Read (&id, 4);
		if (id != 0 && (id == idcheck || id == idcheck2 || id == idcheck3 || id == idcheck4 || id == idcheck5 || id == idcheck6))
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
		else if (!map->isText)	// regular nodes are not supported for text maps
		{
			// If all 3 node related lumps are empty there's no need to output a message.
			// This just means that the map has no nodes and the engine is supposed to build them.
			if (map->Size(ML_SEGS) != 0 || map->Size(ML_SSECTORS) != 0 || map->Size(ML_NODES) != 0)
			{
				if (!P_CheckV4Nodes(map))
				{
					times[7].Clock();
					P_LoadSubsectors<mapsubsector_t, mapseg_t> (map);
					times[7].Unclock();

					times[8].Clock();
					if (!ForceNodeBuild) P_LoadNodes<mapnode_t, mapsubsector_t> (map);
					times[8].Unclock();

					times[9].Clock();
					if (!ForceNodeBuild) P_LoadSegs<mapseg_t> (map);
					times[9].Unclock();
				}
				else
				{
					times[7].Clock();
					P_LoadSubsectors<mapsubsector4_t, mapseg4_t> (map);
					times[7].Unclock();

					times[8].Clock();
					if (!ForceNodeBuild) P_LoadNodes<mapnode4_t, mapsubsector4_t> (map);
					times[8].Unclock();

					times[9].Clock();
					if (!ForceNodeBuild) P_LoadSegs<mapseg4_t> (map);
					times[9].Unclock();
				}
			}
			else ForceNodeBuild = true;
		}
		else ForceNodeBuild = true;

		// If loading the regular nodes failed try GL nodes before considering a rebuild
		if (ForceNodeBuild)
		{
			if (P_LoadGLNodes(map)) 
			{
				ForceNodeBuild = false;
				reloop = true;
			}
		}
	}
	else reloop = true;

	unsigned int startTime=0, endTime=0;

	bool BuildGLNodes;
	if (ForceNodeBuild)
	{
		BuildGLNodes = RequireGLNodes || multiplayer || demoplayback || demorecording || genglnodes;

		startTime = I_FPSTime ();
		TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
		P_GetPolySpots (map, polyspots, anchors);
		FNodeBuilder::FLevel leveldata =
		{
			vertexes, numvertexes,
			sides, numsides,
			lines, numlines,
			0, 0, 0, 0
		};
		leveldata.FindMapBounds ();
		// We need GL nodes if am_textured is on.
		// In case a sync critical game mode is started, also build GL nodes to avoid problems
		// if the different machines' am_textured setting differs.
		FNodeBuilder builder (leveldata, polyspots, anchors, BuildGLNodes);
		delete[] vertexes;
		builder.Extract (nodes, numnodes,
			segs, glsegextras, numsegs,
			subsectors, numsubsectors,
			vertexes, numvertexes);
		endTime = I_FPSTime ();
		DPrintf ("BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, numsegs);
		oldvertextable = builder.GetOldVertexTable();
		reloop = true;
	}
	else
	{
		BuildGLNodes = false;
		// Older ZDBSPs had problems with compressed sidedefs and assigned wrong sides to the segs if both sides were the same sidedef.
		for(i=0;i<numsegs;i++)
		{
			seg_t * seg=&segs[i];
			if (seg->backsector == seg->frontsector && seg->linedef)
			{
				fixed_t d1=P_AproxDistance(seg->v1->x-seg->linedef->v1->x,seg->v1->y-seg->linedef->v1->y);
				fixed_t d2=P_AproxDistance(seg->v2->x-seg->linedef->v1->x,seg->v2->y-seg->linedef->v1->y);

				if (d2<d1)	// backside
				{
					seg->sidedef = seg->linedef->sidedef[1];
				}
				else	// front side
				{
					seg->sidedef = seg->linedef->sidedef[0];
				}
			}
		}
	}

	// Copy pointers to the old nodes so that R_PointInSubsector can use them
	if (nodes && subsectors)
	{
		gamenodes = nodes;
		numgamenodes = numnodes;
		gamesubsectors = subsectors;
		numgamesubsectors = numsubsectors;
	}
	else
	{
		gamenodes=NULL;
	}

	if (RequireGLNodes)
	{
		// Build GL nodes if we want a textured automap or GL nodes are forced to be built.
		// If the original nodes being loaded are not GL nodes they will be kept around for
		// use in P_PointInSubsector to avoid problems with maps that depend on the specific
		// nodes they were built with (P:AR E1M3 is a good example for a map where this is the case.)
		reloop |= P_CheckNodes(map, BuildGLNodes, endTime - startTime);
		hasglnodes = true;
	}
	else
	{
		hasglnodes = P_CheckForGLNodes();
	}

	times[10].Clock();
	P_LoadBlockMap (map);
	times[10].Unclock();

	times[11].Clock();
	P_LoadReject (map, buildmap);
	times[11].Unclock();

	times[12].Clock();
	P_GroupLines (buildmap);
	times[12].Unclock();

	times[13].Clock();
	P_FloodZones ();
	times[13].Unclock();

	if (hasglnodes)
	{
		P_SetRenderSector();
	}

	bodyqueslot = 0;
// phares 8/10/98: Clear body queue so the corpses from previous games are
// not assumed to be from this one.

	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	deathmatchstarts.Clear();
	AllPlayerStarts.Clear();
	memset(playerstarts, 0, sizeof(playerstarts));

	if (!buildmap)
	{
		// [RH] Spawn slope creating things first.
		P_SpawnSlopeMakers (&MapThingsConverted[0], &MapThingsConverted[MapThingsConverted.Size()], oldvertextable);
		P_CopySlopes();

		// Spawn 3d floors - must be done before spawning things so it can't be done in P_SpawnSpecials
		P_Spawn3DFloors();

		times[14].Clock();
		P_SpawnThings(position);

		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != NULL)
				players[i].health = players[i].mo->health;
		}
		times[14].Unclock();

		times[15].Clock();
		if (!map->HasBehavior && !map->isText)
			P_TranslateTeleportThings ();	// [RH] Assign teleport destination TIDs
		times[15].Unclock();
	}
	else
	{
		for (i = 0; i < numbuildthings; ++i)
		{
			SpawnMapThing (i, &buildthings[i], 0);
		}
		delete[] buildthings;
	}
	delete map;
	if (oldvertextable != NULL)
	{
		delete[] oldvertextable;
	}

	// set up world state
	P_SpawnSpecials ();

	// This must be done BEFORE the PolyObj Spawn!!!
	Renderer->PreprocessLevel();

	times[16].Clock();
	if (reloop) P_LoopSidedefs (false);
	PO_Init ();	// Initialize the polyobjs
	times[16].Unclock();

	assert(sidetemp != NULL);
	delete[] sidetemp;
	sidetemp = NULL;

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
	// the same, but for random single/coop player starts
	else if (level.flags2 & LEVEL2_RANDOMPLAYERSTARTS)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				players[i].mo = NULL;
				FPlayerStart *mthing = G_PickPlayerStart(i);
				P_SpawnPlayer(mthing, i, (level.flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
			}
		}
	}

	// Don't count monsters in end-of-level sectors if option is on
	if (dmflags2 & DF2_NOCOUNTENDMONST)
	{
		TThinkerIterator<AActor> it;
		AActor * mo;

		while ((mo=it.Next()))
		{
			if (mo->flags & MF_COUNTKILL)
			{
				if (mo->Sector->special == dDamage_End)
				{
					mo->ClearCounters();
				}
			}
		}
	}

	T_PreprocessScripts();        // preprocess FraggleScript scripts

	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	R_OldBlend = 0xffffffff;

	// [RH] Remove all particles
	P_ClearParticles ();

	times[17].Clock();
	// preload graphics and sounds
	if (precache)
	{
		TexMan.PrecacheLevel ();
		S_PrecacheLevel ();
	}
	times[17].Unclock();

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
			Printf ("Time%3d:%9.4f ms (%s)\n", i, times[i].TimeMS(), timenames[i]);
		}
	}
	MapThingsConverted.Clear();
	MapThingsUserDataIndex.Clear();
	MapThingsUserData.Clear();

	if (glsegextras != NULL)
	{
		delete[] glsegextras;
		glsegextras = NULL;
	}
}



//
// P_Init
//
void P_Init ()
{
	atterm (P_Shutdown);

	P_InitEffects ();		// [RH]
	P_InitTerrainTypes ();
	P_InitKeyMessages ();
	R_InitSprites ();
}

static void P_Shutdown ()
{
	R_DeinitSpriteData ();
	P_DeinitKeyMessages ();
	P_FreeLevelData ();
	P_FreeExtraLevelData ();
	ST_Clear();
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
