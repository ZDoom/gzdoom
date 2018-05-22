//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2010 James Haley
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Do all the WAD I/O, get map description,
//		set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


#include <math.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "templates.h"
#include "d_player.h"
#include "m_argv.h"
#include "g_game.h"
#include "w_wad.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "nodebuild.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "p_acs.h"
#include "announcer.h"
#include "wi_stuff.h"
#include "doomerrors.h"
#include "gi.h"
#include "p_conversation.h"
#include "a_keys.h"
#include "s_sndseq.h"
#include "sbar.h"
#include "p_setup.h"
#include "r_data/r_interpolate.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "md5.h"
#include "compatibility.h"
#include "po_man.h"
#include "r_renderer.h"
#include "p_blockmap.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "c_dispatch.h"
#include "a_dynlight.h"
#ifndef NO_EDATA
#include "edata.h"
#endif
#include "events.h"
#include "types.h"
#include "i_time.h"
#include "scripting/vm/vm.h"

#include "fragglescript/t_fs.h"

#define MISSING_TEXTURE_WARN_LIMIT		20

void P_SpawnSlopeMakers (FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
void P_SetSlopes ();
void P_CopySlopes();
void BloodCrypt (void *data, int key, int len);
void P_ClearUDMFKeys();
void InitRenderInfo();

extern AActor *P_SpawnMapThing (FMapThing *mthing, int position);

extern void P_TranslateTeleportThings (void);

void P_ParseTextMap(MapData *map, FMissingTextureTracker &);

extern int numinterpolations;
extern unsigned int R_OldBlend;

EXTERN_CVAR(Bool, am_textured)

CVAR (Bool, genblockmap, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, gennodes, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, genglnodes, false, CVAR_SERVERINFO);
CVAR (Bool, showloadtimes, false, 0);

static void P_Shutdown ();

inline bool P_IsBuildMap(MapData *map)
{
	return false;
}

inline bool P_LoadBuildMap(uint8_t *mapdata, size_t len, FMapThing **things, int *numthings)
{
	return false;
}


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
TArray<vertexdata_t> vertexdatas;

bool			hasglnodes;

TArray<FMapThing> MapThingsConverted;
TMap<unsigned,unsigned>  MapThingsUserDataIndex;	// from mapthing idx -> user data idx
TArray<FUDMFKey> MapThingsUserData;

int sidecount;
sidei_t *sidetemp;

TArray<int>		linemap;

bool		ForceNodeBuild;

static void P_AllocateSideDefs (MapData *map, int count);

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
	FileReader * wadReader = nullptr;
	bool externalfile = !strnicmp(mapname, "file:", 5);
	
	if (externalfile)
	{
		mapname += 5;
		if (!FileExists(mapname))
		{
			delete map;
			return NULL;
		}
		map->resource = FResourceFile::OpenResourceFile(mapname, true);
		wadReader = map->resource->GetReader();
	}
	else
	{
		FString fmt;
		int lump_wad;
		int lump_map;
		int lump_name = -1;
		
		// Check for both *.wad and *.map in order to load Build maps
		// as well. The higher one will take precedence.
		// Names with more than 8 characters will only be checked as .wad and .map.
		if (strlen(mapname) <= 8) lump_name = Wads.CheckNumForName(mapname);
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
				map->MapLumps[0].Reader = Wads.ReopenLumpReader(lump_name);
				if (!P_IsBuildMap(map))
				{
					delete map;
					return NULL;
				}
				return map;
			}

			// This case can only happen if the lump is inside a real WAD file.
			// As such any special handling for other types of lumps is skipped.
			map->MapLumps[0].Reader = Wads.ReopenLumpReader(lump_name);
			strncpy(map->MapLumps[0].Name, Wads.GetLumpFullName(lump_name), 8);
			map->Encrypted = Wads.IsEncryptedFile(lump_name);
			map->InWad = true;

			if (map->Encrypted)
			{ // If it's encrypted, then it's a Blood file, presumably a map.
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

					map->MapLumps[index].Reader = Wads.ReopenLumpReader(lump_name + i);
					strncpy(map->MapLumps[index].Name, lumpname, 8);
				}
			}
			else
			{
				map->isText = true;
				map->MapLumps[1].Reader = Wads.ReopenLumpReader(lump_name + 1);
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
					map->MapLumps[index].Reader = Wads.ReopenLumpReader(lump_name + i);
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
			auto reader = Wads.ReopenLumpReader(lump_wad);
			map->resource = FResourceFile::OpenResourceFile(Wads.GetLumpFullName(lump_wad), reader, true);
			wadReader = map->resource->GetReader();
		}
	}
	uint32_t id;

	// Although we're using the resource system, we still want to be sure we're
	// reading from a wad file.
	wadReader->Seek(0, FileReader::SeekSet);
	wadReader->Read(&id, sizeof(id));
	
	if (id == IWAD_ID || id == PWAD_ID)
	{
		char maplabel[9]="";
		int index=0;

		map->MapLumps[0].Reader = map->resource->GetLump(0)->NewReader();
		strncpy(map->MapLumps[0].Name, map->resource->GetLump(0)->Name, 8);

		for(uint32_t i = 1; i < map->resource->LumpCount(); i++)
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

void MapData::GetChecksum(uint8_t cksum[16])
{
	MD5Context md5;

	if (isText)
	{
		md5.Update(Reader(ML_TEXTMAP), Size(ML_TEXTMAP));
	}
	else
	{
		md5.Update(Reader(ML_LABEL), Size(ML_LABEL));
		md5.Update(Reader(ML_THINGS), Size(ML_THINGS));
		md5.Update(Reader(ML_LINEDEFS), Size(ML_LINEDEFS));
		md5.Update(Reader(ML_SIDEDEFS), Size(ML_SIDEDEFS));
		md5.Update(Reader(ML_SECTORS), Size(ML_SECTORS));
	}
	if (HasBehavior)
	{
		md5.Update(Reader(ML_BEHAVIOR), Size(ML_BEHAVIOR));
	}
	md5.Final(cksum);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, GetChecksum)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	char md5string[33];

	for(int j = 0; j < 16; ++j)
	{
        	sprintf(md5string + j * 2, "%02x", level.md5[j]);
	}

	ACTION_RETURN_STRING((const char*)md5string);
}

//===========================================================================
//
// Sets a sidedef's texture and prints a message if it's not present.
//
//===========================================================================

static void SetTexture (side_t *side, int position, const char *name, FMissingTextureTracker &track)
{
	static const char *positionnames[] = { "top", "middle", "bottom" };
	static const char *sidenames[] = { "first", "second" };

	FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Wall,
			FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!texture.Exists())
	{
		if (++track[name].Count <= MISSING_TEXTURE_WARN_LIMIT)
		{
			// Print an error that lists all references to this sidedef.
			// We must scan the linedefs manually for all references to this sidedef.
			for(unsigned i = 0; i < level.lines.Size(); i++)
			{
				for(int j = 0; j < 2; j++)
				{
					if (level.lines[i].sidedef[j] == side)
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

void SetTexture (sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate)
{
	static const char *positionnames[] = { "floor", "ceiling" };
	char name8[9];
	if (truncate)
	{
		strncpy(name8, name, 8);
		name8[8] = 0;
		name = name8;
	}

	FTextureID texture = TexMan.CheckForTexture (name, ETextureType::Flat,
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

static void SetTexture (side_t *side, int position, uint32_t *blend, const char *name)
{
	FTextureID texture;
	if ((*blend = R_ColormapNumForName (name)) == 0)
	{
		texture = TexMan.CheckForTexture (name, ETextureType::Wall,
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

//===========================================================================
//
//
//
//===========================================================================

static void SetTextureNoErr (side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog)
{
	FTextureID texture;
	*validcolor = false;
	texture = TexMan.CheckForTexture (name, ETextureType::Wall,	
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
	if (sec->ZoneNumber == zonenum)
		return;

	sec->ZoneNumber = zonenum;

	for (auto check : sec->Lines)
	{
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

	for (auto &sec : level.sectors)
	{
		if (sec.ZoneNumber == 0xFFFF)
		{
			P_FloodZone (&sec, z++);
		}
	}
	level.Zones.Resize(z);
	reverb = S_FindEnvironment(level.DefaultEnvironment);
	if (reverb == NULL)
	{
		Printf("Sound environment %d, %d not found\n", level.DefaultEnvironment >> 8, level.DefaultEnvironment & 255);
		reverb = DefaultEnvironments[0];
	}
	for (i = 0; i < z; ++i)
	{
		level.Zones[i].Environment = reverb;
	}
}

//===========================================================================
//
// P_LoadVertexes
//
//===========================================================================

void P_LoadVertexes (MapData * map)
{
	// Determine number of vertices:
	//	total lump length / vertex record length.
	unsigned numvertexes = map->Size(ML_VERTEXES) / sizeof(mapvertex_t);

	if (numvertexes == 0)
	{
		I_Error ("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	level.vertexes.Alloc(numvertexes);
	vertexdatas.Clear();

	auto &fr = map->Reader(ML_VERTEXES);
		
	// Copy and convert vertex coordinates, internal representation as fixed.
	for (auto &v : level.vertexes)
	{
		int16_t x = fr.ReadInt16();
		int16_t y = fr.ReadInt16();

		v.set(double(x), double(y));
	}
}

//===========================================================================
//
// P_LoadZSegs
//
//===========================================================================

void P_LoadZSegs (FileReader &data)
{
	for (auto &seg : level.segs)
	{
		line_t *ldef;
		uint32_t v1 = data.ReadUInt32();
		uint32_t v2 = data.ReadUInt32();
		uint16_t line = data.ReadUInt16();
		uint8_t side = data.ReadUInt8();

		seg.v1 = &level.vertexes[v1];
		seg.v2 = &level.vertexes[v2];
		seg.linedef = ldef = &level.lines[line];
		seg.sidedef = ldef->sidedef[side];
		seg.frontsector = ldef->sidedef[side]->sector;
		if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
		{
			seg.backsector = ldef->sidedef[side^1]->sector;
		}
		else
		{
			seg.backsector = 0;
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

void P_LoadGLZSegs (FileReader &data, int type)
{
	for (unsigned i = 0; i < level.subsectors.Size(); ++i)
	{

		for (size_t j = 0; j < level.subsectors[i].numlines; ++j)
		{
			seg_t *seg;
			uint32_t v1 = data.ReadUInt32();
			uint32_t partner = data.ReadUInt32();
			uint32_t line;

			if (type >= 2)
			{
				line = data.ReadUInt32();
			}
			else
			{
				line = data.ReadUInt16();
				if (line == 0xffff) line = 0xffffffff;
			}
			uint8_t side = data.ReadUInt8();

			seg = level.subsectors[i].firstline + j;
			seg->v1 = &level.vertexes[v1];
			if (j == 0)
			{
				seg[level.subsectors[i].numlines - 1].v2 = seg->v1;
			}
			else
			{
				seg[-1].v2 = seg->v1;
			}
			
			seg->PartnerSeg = partner == 0xffffffffu? nullptr : &level.segs[partner];
			if (line != 0xFFFFFFFF)
			{
				line_t *ldef;

				seg->linedef = ldef = &level.lines[line];
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
				seg->frontsector = seg->backsector = level.subsectors[i].firstline->frontsector;
			}
		}
	}
}

//===========================================================================
//
// P_LoadZNodes
//
//===========================================================================

void LoadZNodes(FileReader &data, int glnodes)
{
	// Read extra vertices added during node building
	unsigned int i;

	uint32_t orgVerts = data.ReadUInt32();
	uint32_t newVerts = data.ReadUInt32();
	if (orgVerts > level.vertexes.Size())
	{ // These nodes are based on a map with more vertex data than we have.
	  // We can't use them.
		throw CRecoverableError("Incorrect number of vertexes in nodes.\n");
	}
	auto oldvertexes = &level.vertexes[0];
	if (orgVerts + newVerts != level.vertexes.Size())
	{
		level.vertexes.Reserve(newVerts);
	}
	for (i = 0; i < newVerts; ++i)
	{
		fixed_t x = data.ReadInt32();
		fixed_t y = data.ReadInt32();
		level.vertexes[i + orgVerts].set(x, y);
	}
	if (oldvertexes != &level.vertexes[0])
	{
		for (auto &line : level.lines)
		{
			line.v1 = line.v1 - oldvertexes + &level.vertexes[0];
			line.v2 = line.v2 - oldvertexes + &level.vertexes[0];
		}
	}

	// Read the subsectors
	uint32_t currSeg;
	uint32_t numSubs = data.ReadUInt32();
	level.subsectors.Alloc(numSubs);
	memset (&level.subsectors[0], 0, level.subsectors.Size()*sizeof(subsector_t));

	for (i = currSeg = 0; i < numSubs; ++i)
	{
		uint32_t numsegs = data.ReadUInt32();
		level.subsectors[i].firstline = (seg_t *)(size_t)currSeg;		// Oh damn. I should have stored the seg count sooner.
		level.subsectors[i].numlines = numsegs;
		currSeg += numsegs;
	}

	// Read the segs
	uint32_t numSegs = data.ReadUInt32();

	// The number of segs stored should match the number of
	// segs used by subsectors.
	if (numSegs != currSeg)
	{
		throw CRecoverableError("Incorrect number of segs in nodes.\n");
	}

	level.segs.Alloc(numSegs);
	memset (&level.segs[0], 0, numSegs*sizeof(seg_t));

	for (auto &sub : level.subsectors)
	{
		sub.firstline = &level.segs[(size_t)sub.firstline];
	}

	if (glnodes == 0)
	{
		P_LoadZSegs (data);
	}
	else
	{
		P_LoadGLZSegs (data, glnodes);
	}

	// Read nodes
	uint32_t numNodes = data.ReadUInt32();

	auto &nodes = level.nodes;
	nodes.Alloc(numNodes);
	memset (&nodes[0], 0, sizeof(node_t)*numNodes);

	for (i = 0; i < numNodes; ++i)
	{
		if (glnodes < 3)
		{
			nodes[i].x = data.ReadInt16() * FRACUNIT;
			nodes[i].y = data.ReadInt16() * FRACUNIT;
			nodes[i].dx = data.ReadInt16() * FRACUNIT;
			nodes[i].dy = data.ReadInt16() * FRACUNIT;
		}
		else
		{
			nodes[i].x = data.ReadInt32();
			nodes[i].y = data.ReadInt32();
			nodes[i].dx = data.ReadInt32();
			nodes[i].dy = data.ReadInt32();
		}
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				nodes[i].bbox[j][k] = data.ReadInt16();
			}
		}
		for (int m = 0; m < 2; ++m)
		{
			uint32_t child = data.ReadUInt32();
			if (child & 0x80000000)
			{
				nodes[i].children[m] = (uint8_t *)&level.subsectors[child & 0x7FFFFFFF] + 1;
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
//
//
//===========================================================================

void P_LoadZNodes (FileReader &dalump, uint32_t id)
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
		FileReader zip;
		if (zip.OpenDecompressor(dalump, -1, METHOD_ZLIB, false))
		{
			LoadZNodes(zip, type);
		}
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
	uint8_t *data;
	int numvertexes = level.vertexes.Size();
	uint8_t *vertchanged = new uint8_t[numvertexes];	// phares 10/4/98
	uint32_t segangle;
	//int ptp_angle;		// phares 10/4/98
	//int delta_angle;	// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98
	int lumplen = map->Size(ML_SEGS);

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	unsigned numsegs = lumplen / sizeof(segtype);

	if (numsegs == 0)
	{
		Printf ("This map has no segs.\n");
		level.subsectors.Clear();
		level.nodes.Clear();
		delete[] vertchanged;
		ForceNodeBuild = true;
		return;
	}

	level.segs.Alloc(numsegs);
	auto &segs = level.segs;
	memset (&segs[0], 0, numsegs*sizeof(seg_t));

	data = new uint8_t[lumplen];
	map->Read(ML_SEGS, data);

	for (auto &sub : level.subsectors)
	{
		sub.firstline = &segs[(size_t)sub.firstline];
	}

	// phares: 10/4/98: Vertchanged is an array that represents the vertices.
	// Mark those used by linedefs. A marked vertex is one that is not a
	// candidate for movement further down.

	for (auto &line : level.lines)
	{
		vertchanged[line.v1->Index()] = vertchanged[line.v2->Index()] = 1;
	}

	try
	{
		for (unsigned i = 0; i < numsegs; i++)
		{
			seg_t *li = &segs[i];
			segtype *ml = ((segtype *) data) + i;

			int side, linedef;
			line_t *ldef;

			vnum1 = ml->V1();
			vnum2 = ml->V2();

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw badseg(0, i, MAX(vnum1, vnum2));
			}

			li->v1 = &level.vertexes[vnum1];
			li->v2 = &level.vertexes[vnum2];

			segangle = (uint16_t)LittleShort(ml->angle);

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

			DAngle ptp_angle = (li->v2->fPos() - li->v1->fPos()).Angle();
			DAngle seg_angle = AngleToFloat(segangle << 16);
			DAngle delta_angle = absangle(ptp_angle, seg_angle);

			if (delta_angle >= 1.)
			{
				double dis = (li->v2->fPos() - li->v1->fPos()).Length();
				DVector2 delta = seg_angle.ToVector(dis);
				if ((vnum2 > vnum1) && (vertchanged[vnum2] == 0))
				{
					li->v2->set(li->v1->fPos() + delta);
					vertchanged[vnum2] = 1; // this was changed
				}
				else if (vertchanged[vnum1] == 0)
				{
					li->v1->set(li->v2->fPos() - delta);
					vertchanged[vnum1] = 1; // this was changed
				}
			}

			linedef = LittleShort(ml->linedef);
			if ((unsigned)linedef >= level.lines.Size())
			{
				throw badseg(1, i, linedef);
			}
			ldef = &level.lines[linedef];
			li->linedef = ldef;
			side = LittleShort(ml->side);
			if ((unsigned)(ldef->sidedef[side]->Index()) >= level.sides.Size())
			{
				throw badseg(2, i, ldef->sidedef[side]->Index());
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
	catch (const badseg &bad) // the preferred way is to catch by (const) reference.
	{
		switch (bad.badtype)
		{
		case 0:
			Printf ("Seg %d references a nonexistant vertex %d (max %d).\n", bad.badsegnum, bad.baddata, numvertexes);
			break;

		case 1:
			Printf ("Seg %d references a nonexistant linedef %d (max %u).\n", bad.badsegnum, bad.baddata, level.lines.Size());
			break;

		case 2:
			Printf ("The linedef for seg %d references a nonexistant sidedef %d (max %d).\n", bad.badsegnum, bad.baddata, level.sides.Size());
			break;
		}
		Printf ("The BSP will be rebuilt.\n");
		level.segs.Clear();
		level.subsectors.Clear();
		level.nodes.Clear();
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
	uint32_t maxseg = map->Size(ML_SEGS) / sizeof(segtype);

	unsigned numsubsectors = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	if (numsubsectors == 0 || maxseg == 0 )
	{
		Printf ("This map has an incomplete BSP tree.\n");
		level.nodes.Clear();
		ForceNodeBuild = true;
		return;
	}

	auto &subsectors = level.subsectors;
	subsectors.Alloc(numsubsectors);
	auto &fr = map->Reader(ML_SSECTORS);
		
	memset (&subsectors[0], 0, numsubsectors*sizeof(subsector_t));
	
	for (unsigned i = 0; i < numsubsectors; i++)
	{
		subsectortype subd;

		subd.numsegs = sizeof(subd.numsegs) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();
		subd.firstseg = sizeof(subd.firstseg) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();

		if (subd.numsegs == 0)
		{
			Printf ("Subsector %i is empty.\n", i);
			level.subsectors.Clear();
			level.nodes.Clear();
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
			level.nodes.Clear();
			level.subsectors.Clear();
			break;
		}
		else if ((size_t)subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			level.nodes.Clear();
			level.subsectors.Clear();
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
	char				*msp;
	mapsector_t			*ms;
	sector_t*			ss;
	int					defSeqType;
	int					lumplen = map->Size(ML_SECTORS);

	unsigned numsectors = lumplen / sizeof(mapsector_t);
	level.sectors.Alloc(numsectors);
	auto sectors = &level.sectors[0];
	memset (sectors, 0, numsectors*sizeof(sector_t));

	if (level.flags & LEVEL_SNDSEQTOTALCTRL)
		defSeqType = 0;
	else
		defSeqType = -1;

	msp = new char[lumplen];
	map->Read(ML_SECTORS, msp);
	ms = (mapsector_t*)msp;
	ss = sectors;
	
	// Extended properties
	sectors[0].e = new extsector_t[numsectors];

	for (unsigned i = 0; i < numsectors; i++, ss++, ms++)
	{
		ss->e = &sectors[0].e[i];
		if (!map->HasBehavior) ss->Flags |= SECF_FLOORDROP;
		ss->SetPlaneTexZ(sector_t::floor, (double)LittleShort(ms->floorheight));
		ss->floorplane.set(0, 0, 1., -ss->GetPlaneTexZ(sector_t::floor));
		ss->SetPlaneTexZ(sector_t::ceiling, (double)LittleShort(ms->ceilingheight));
		ss->ceilingplane.set(0, 0, -1., ss->GetPlaneTexZ(sector_t::ceiling));
		SetTexture(ss, i, sector_t::floor, ms->floorpic, missingtex, true);
		SetTexture(ss, i, sector_t::ceiling, ms->ceilingpic, missingtex, true);
		ss->lightlevel = LittleShort(ms->lightlevel);
		if (map->HasBehavior)
			ss->special = LittleShort(ms->special);
		else	// [RH] Translate to new sector special
			ss->special = P_TranslateSectorSpecial (LittleShort(ms->special));
		tagManager.AddSectorTag(i, LittleShort(ms->tag));
		ss->thinglist = nullptr;
		ss->touching_thinglist = nullptr;		// phares 3/14/98
		ss->sectorportal_thinglist = nullptr;
		ss->touching_renderthings = nullptr;
		ss->seqType = defSeqType;
		ss->SeqName = NAME_None;
		ss->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		ss->prevsec = -1;	// stair retriggering until build completes
		memset(ss->SpecialColors, -1, sizeof(ss->SpecialColors));

		ss->SetAlpha(sector_t::floor, 1.);
		ss->SetAlpha(sector_t::ceiling, 1.);
		ss->SetXScale(sector_t::floor, 1.);	// [RH] floor and ceiling scaling
		ss->SetYScale(sector_t::floor, 1.);
		ss->SetXScale(sector_t::ceiling, 1.);
		ss->SetYScale(sector_t::ceiling, 1.);

		ss->heightsec = NULL;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		ss->gravity = 1.f;	// [RH] Default sector gravity of 1.0
		ss->ZoneNumber = 0xFFFF;
		ss->terrainnum[sector_t::ceiling] = ss->terrainnum[sector_t::floor] = -1;



		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		ss->Colormap.LightColor = PalEntry(255, 255, 255);
		if (level.outsidefog != 0xff000000 && (ss->GetTexture(sector_t::ceiling) == skyflatnum || (ss->special&0xff) == Sector_Outside))
		{
			ss->Colormap.FadeColor.SetRGB(level.outsidefog);
		}
		else  if (level.flags & LEVEL_HASFADETABLE)
		{
			ss->Colormap.FadeColor= 0x939393;	// The true color software renderer needs this. (The hardware renderer will ignore this value if LEVEL_HASFADETABLE is set.)
		}
		else
		{
			ss->Colormap.FadeColor.SetRGB(level.fadeto);
		}


		// killough 8/28/98: initialize all sectors to normal friction
		ss->friction = ORIG_FRICTION;
		ss->movefactor = ORIG_FRICTION_FACTOR;
		ss->sectornum = i;
		ss->ibocount = -1;
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
	int 		j;
	int 		k;
	char		*mnp;
	nodetype	*mn;
	node_t* 	no;
	uint16_t*		used;
	int			lumplen = map->Size(ML_NODES);
	int			maxss = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	unsigned numnodes = (lumplen - nodetype::NF_LUMPOFFSET) / sizeof(nodetype);

	if ((numnodes == 0 && maxss != 1) || maxss == 0)
	{
		ForceNodeBuild = true;
		return;
	}
	
	auto &nodes = level.nodes;
	nodes.Alloc(numnodes);		
	used = (uint16_t *)alloca (sizeof(uint16_t)*numnodes);
	memset (used, 0, sizeof(uint16_t)*numnodes);

	mnp = new char[lumplen];
	mn = (nodetype*)(mnp + nodetype::NF_LUMPOFFSET);
	map->Read(ML_NODES, mnp);
	no = &nodes[0];
	
	for (unsigned i = 0; i < numnodes; i++, no++, mn++)
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
					level.nodes.Clear();
					delete[] mnp;
					return;
				}
				no->children[j] = (uint8_t *)&level.subsectors[child] + 1;
			}
			else if ((unsigned)child >= numnodes)
			{
				Printf ("BSP node %d references invalid node %d.\n"
					"The BSP will be rebuilt.\n", i, ((node_t *)no->children[j])->Index());
				ForceNodeBuild = true;
				level.nodes.Clear();
				delete[] mnp;
				return;
			}
			else if (used[child])
			{
				Printf ("BSP node %d references node %d,\n"
					"which is already used by node %d.\n"
					"The BSP will be rebuilt.\n", i, child, used[child]-1);
				ForceNodeBuild = true;
				level.nodes.Clear();
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
				no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
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
		Printf("%5d: (%5f, %5f, %5f), doomednum = %5d, flags = %04x, type = %s\n",
			index, mt->pos.X, mt->pos.Y, mt->pos.Z, mt->EdNum, mt->flags, 
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
	while (MapThingsUserData[udi].Key != NAME_None)
	{
		FName varname = MapThingsUserData[udi].Key;
		PField *var = dyn_cast<PField>(actor->GetClass()->FindSymbol(varname, true));

		if (var == NULL || (var->Flags & (VARF_Native|VARF_Private|VARF_Protected|VARF_Static)) || !var->Type->isScalar())
		{
			DPrintf(DMSG_WARNING, "%s is not a writable user variable in class %s\n", varname.GetChars(),
				actor->GetClass()->TypeName.GetChars());
		}
		else
		{ // Set the value of the specified user variable.
			void *addr = reinterpret_cast<uint8_t *>(actor) + var->Offset;
			if (var->Type == TypeString)
			{
				var->Type->InitializeValue(addr, &MapThingsUserData[udi].StringVal);
			}
			else if (var->Type->isFloat())
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].FloatVal);
			}
			else if (var->Type->isInt() || var->Type == TypeBool)
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].IntVal);
			}
		}

		udi++;
	}
}

//===========================================================================
//
// P_LoadThings
//
//===========================================================================

uint16_t MakeSkill(int flags)
{
	uint16_t res = 0;
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

		mti[i].Gravity = 1;
		mti[i].Conversation = 0;
		mti[i].SkillFilter = MakeSkill(flags);
		mti[i].ClassFilter = 0xffff;	// Doom map format doesn't have class flags so spawn for all player classes
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;

		mti[i].pos.X = LittleShort(mt->x);
		mti[i].pos.Y = LittleShort(mt->y);
		mti[i].angle = LittleShort(mt->angle);
		mti[i].EdNum = LittleShort(mt->type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);


#ifndef NO_EDATA
		if (mti[i].info != NULL && mti[i].info->Special == SMT_EDThing)
		{
			ProcessEDMapthing(&mti[i], flags);
		}
		else
#endif
		{
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
		}
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
		mti[i].pos.X = LittleShort(mth[i].x);
		mti[i].pos.Y = LittleShort(mth[i].y);
		mti[i].pos.Z = LittleShort(mth[i].z);
		mti[i].angle = LittleShort(mth[i].angle);
		mti[i].EdNum = LittleShort(mth[i].type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);
		mti[i].flags = LittleShort(mth[i].flags);
		mti[i].special = mth[i].special;
		for(int j=0;j<5;j++) mti[i].args[j] = mth[i].args[j];
		mti[i].SkillFilter = MakeSkill(mti[i].flags);
		mti[i].ClassFilter = (mti[i].flags & MTF_CLASS_MASK) >> MTF_CLASS_SHIFT;
		mti[i].flags &= ~(MTF_SKILLMASK|MTF_CLASS_MASK);
		if (level.flags2 & LEVEL2_HEXENHACK)
		{
			mti[i].flags &= 0x7ff;	// mask out Strife flags if playing an original Hexen map.
		}

		mti[i].Gravity = 1;
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;
		mti[i].friendlyseeblocks = -1;
	}
	delete[] mtp;
}

//===========================================================================
//
//
//
//===========================================================================

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

	ld->setDelta(v2->fX() - v1->fX(), v2->fY() - v1->fY());
	
	if (v1->fX() < v2->fX())
	{
		ld->bbox[BOXLEFT] = v1->fX();
		ld->bbox[BOXRIGHT] = v2->fX();
	}
	else
	{
		ld->bbox[BOXLEFT] = v2->fX();
		ld->bbox[BOXRIGHT] = v1->fX();
	}

	if (v1->fY() < v2->fY())
	{
		ld->bbox[BOXBOTTOM] = v1->fY();
		ld->bbox[BOXTOP] = v2->fY();
	}
	else
	{
		ld->bbox[BOXBOTTOM] = v2->fY();
		ld->bbox[BOXTOP] = v1->fY();
	}
}

//===========================================================================
//
// [RH] Set line id (as appropriate) here
// for Doom format maps this must be done in P_TranslateLineDef because
// the tag doesn't always go into the first arg.
//
//===========================================================================

void P_SetLineID (int i, line_t *ld)
{
	if (level.maptype == MAPTYPE_HEXEN)	
	{
		int setid = -1;
		switch (ld->special)
		{
		case Line_SetIdentification:
			if (!(level.flags2 & LEVEL2_HEXENHACK))
			{
				setid = ld->args[0] + 256 * ld->args[4];
				ld->flags |= ld->args[1]<<16;
			}
			else
			{
				setid = ld->args[0];
			}
			ld->special = 0;
			break;

		case TranslucentLine:
			setid = ld->args[0];
			ld->flags |= ld->args[3]<<16;
			break;

		case Teleport_Line:
		case Scroll_Texture_Model:
			setid = ld->args[0];
			break;

		case Polyobj_StartLine:
			setid = ld->args[3];
			break;

		case Polyobj_ExplicitLine:
			setid = ld->args[4];
			break;
			
		case Plane_Align:
			if (!(ib_compatflags & BCOMPATF_NOSLOPEID)) setid = ld->args[2];
			break;
			
		case Static_Init:
			if (ld->args[1] == Init_SectorLink) setid = ld->args[0];
			break;

		case Line_SetPortal:
			setid = ld->args[1]; // 0 = target id, 1 = this id, 2 = plane anchor
			break;
		}
		if (setid != -1)
		{
			tagManager.AddLineID(i, setid);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_SaveLineSpecial (line_t *ld)
{
	if (ld->sidedef[0] == NULL)
		return;

	uint32_t sidenum = ld->sidedef[0]->Index();
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

//===========================================================================
//
//
//
//===========================================================================

void P_FinishLoadingLineDef(line_t *ld, int alpha)
{
	bool additive = false;

	ld->frontsector = ld->sidedef[0] != NULL ? ld->sidedef[0]->sector : NULL;
	ld->backsector  = ld->sidedef[1] != NULL ? ld->sidedef[1]->sector : NULL;
	double dx = (ld->v2->fX() - ld->v1->fX());
	double dy = (ld->v2->fY() - ld->v1->fY());
	int linenum = ld->Index();

	if (ld->frontsector == NULL)
	{
		Printf ("Line %d has no front sector\n", linemap[linenum]);
	}

	// [RH] Set some new sidedef properties
	int len = (int)(g_sqrt (dx*dx + dy*dy) + 0.5f);

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

		double dalpha = alpha / 255.;
		if (!ld->args[0])
		{
			ld->alpha = dalpha;
			if (additive)
			{
				ld->flags |= ML_ADDTRANS;
			}
		}
		else
		{
			for (unsigned j = 0; j < level.lines.Size(); j++)
			{
				if (tagManager.LineHasID(j, ld->args[0]))
				{
					level.lines[j].alpha = dalpha;
					if (additive)
					{
						level.lines[j].flags |= ML_ADDTRANS;
					}
				}
			}
		}
		ld->special = 0;
		break;
	}
}

//===========================================================================
//
// killough 4/4/98: delay using sidedefs until they are loaded
//
//===========================================================================

void P_FinishLoadingLineDefs ()
{
	for (auto &line : level.lines)
	{
		P_FinishLoadingLineDef(&line, sidetemp[line.sidedef[0]->Index()].a.alpha);
	}
}

//===========================================================================
//
//
//
//===========================================================================

static void P_SetSideNum (side_t **sidenum_p, uint16_t sidenum)
{
	if (sidenum == NO_INDEX)
	{
		*sidenum_p = NULL;
	}
	else if (sidecount < (int)level.sides.Size())
	{
		sidetemp[sidecount].a.map = sidenum;
		*sidenum_p = &level.sides[sidecount++];
	}
	else
	{
		I_Error ("%d sidedefs is not enough\n", sidecount);
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_LoadLineDefs (MapData * map)
{
	int i, skipped;
	line_t *ld;
	int lumplen = map->Size(ML_LINEDEFS);
	char * mldf;
	maplinedef_t *mld;
		
	int numlines = lumplen / sizeof(maplinedef_t);
	linemap.Resize(numlines);

	mldf = new char[lumplen];
	map->Read(ML_LINEDEFS, mldf);

	// [RH] Count the number of sidedef references. This is the number of
	// sidedefs we need. The actual number in the SIDEDEFS lump might be less.
	// Lines with 0 length are also removed.

	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef_t*)mldf) + i;
		unsigned v1 = LittleShort(mld->v1);
		unsigned v2 = LittleShort(mld->v2);

		if (v1 >= level.vertexes.Size() || v2 >= level.vertexes.Size())
		{
			delete [] mldf;
			I_Error ("Line %d has invalid vertices: %d and/or %d.\nThe map only contains %u vertices.", i+skipped, v1, v2, level.vertexes.Size());
		}
		else if (v1 == v2 ||
			(level.vertexes[v1].fX() == level.vertexes[v2].fX() && level.vertexes[v1].fY() == level.vertexes[v2].fY()))
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
	level.lines.Alloc(numlines);
	memset(&level.lines[0], 0, numlines * sizeof(line_t));

	P_AllocateSideDefs (map, sidecount);

	mld = (maplinedef_t *)mldf;
	ld = &level.lines[0];
	for (i = 0; i < numlines; i++, mld++, ld++)
	{
		ld->alpha = 1.;	// [RH] Opaque by default
		ld->portalindex = UINT_MAX;
		ld->portaltransferred = UINT_MAX;

		// [RH] Translate old linedef special and flags to be
		//		compatible with the new format.

		mld->special = LittleShort(mld->special);
		mld->tag = LittleShort(mld->tag);
		mld->flags = LittleShort(mld->flags);
		P_TranslateLineDef (ld, mld, -1);
		// do not assign the tag for Extradata lines.
		if (ld->special != Static_Init || (ld->args[1] != Init_EDLine && ld->args[1] != Init_EDSector))
		{
			tagManager.AddLineID(i, mld->tag);
		}
#ifndef NO_EDATA
		if (ld->special == Static_Init && ld->args[1] == Init_EDLine)
		{
			ProcessEDLinedef(ld, mld->tag);
		}
#endif

		ld->v1 = &level.vertexes[LittleShort(mld->v1)];
		ld->v2 = &level.vertexes[LittleShort(mld->v2)];

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

//===========================================================================
//
// [RH] Same as P_LoadLineDefs() except it uses Hexen-style LineDefs.
//
//===========================================================================

void P_LoadLineDefs2 (MapData * map)
{
	int i, skipped;
	line_t *ld;
	int lumplen = map->Size(ML_LINEDEFS);
	char * mldf;
	maplinedef2_t *mld;
		
	int numlines = lumplen / sizeof(maplinedef2_t);
	linemap.Resize(numlines);

	mldf = new char[lumplen];
	map->Read(ML_LINEDEFS, mldf);

	// [RH] Remove any lines that have 0 length and count sidedefs used
	for (skipped = sidecount = i = 0; i < numlines; )
	{
		mld = ((maplinedef2_t*)mldf) + i;

		if (mld->v1 == mld->v2 ||
			(level.vertexes[LittleShort(mld->v1)].fX() == level.vertexes[LittleShort(mld->v2)].fX() &&
			 level.vertexes[LittleShort(mld->v1)].fY() == level.vertexes[LittleShort(mld->v2)].fY()))
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
	level.lines.Alloc(numlines);
	memset(&level.lines[0], 0, numlines * sizeof(line_t));

	P_AllocateSideDefs (map, sidecount);

	mld = (maplinedef2_t *)mldf;
	ld = &level.lines[0];
	for (i = 0; i < numlines; i++, mld++, ld++)
	{
		int j;

		ld->portalindex = UINT_MAX;
		ld->portaltransferred = UINT_MAX;

		for (j = 0; j < 5; j++)
			ld->args[j] = mld->args[j];

		ld->flags = LittleShort(mld->flags);
		ld->special = mld->special;

		ld->v1 = &level.vertexes[LittleShort(mld->v1)];
		ld->v2 = &level.vertexes[LittleShort(mld->v2)];
		ld->alpha = 1.;	// [RH] Opaque by default

		P_SetSideNum (&ld->sidedef[0], LittleShort(mld->sidenum[0]));
		P_SetSideNum (&ld->sidedef[1], LittleShort(mld->sidenum[1]));

		P_AdjustLine (ld);
		P_SetLineID(i, ld);
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


//===========================================================================
//
//
//
//===========================================================================

static void P_AllocateSideDefs (MapData *map, int count)
{
	int i;

	level.sides.Alloc(count);
	memset(&level.sides[0], 0, count * sizeof(side_t));

	sidetemp = new sidei_t[MAX<int>(count, level.vertexes.Size())];
	for (i = 0; i < count; i++)
	{
		sidetemp[i].a.special = sidetemp[i].a.tag = 0;
		sidetemp[i].a.alpha = SHRT_MIN;
		sidetemp[i].a.map = NO_SIDE;
	}
	int numsides = map->Size(ML_SIDEDEFS) / sizeof(mapsidedef_t);
	if (count < numsides)
	{
		Printf ("Map has %d unused sidedefs\n", numsides - count);
	}
	numsides = count;
	sidecount = 0;
}

//===========================================================================
//
// [RH] Group sidedefs into loops so that we can easily determine
// what walls any particular wall neighbors.
//
//===========================================================================

static void P_LoopSidedefs (bool firstloop)
{
	int i;

	if (sidetemp != NULL)
	{
		delete[] sidetemp;
	}
	int numsides = level.sides.Size();
	sidetemp = new sidei_t[MAX<int>(level.vertexes.Size(), numsides)];

	for (i = 0; i < (int)level.vertexes.Size(); ++i)
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
		line_t *line = level.sides[i].linedef;
		int lineside = (line->sidedef[0] != &level.sides[i]);
		int vert = lineside ? line->v2->Index() : line->v1->Index();
		
		sidetemp[i].b.lineside = lineside;
		sidetemp[i].b.next = sidetemp[vert].b.first;
		sidetemp[vert].b.first = i;

		// Set each side so that it is the only member of its loop
		level.sides[i].LeftSide = NO_SIDE;
		level.sides[i].RightSide = NO_SIDE;
	}

	// For each side, find the side that is to its right and set the
	// loop pointers accordingly. If two sides share a left vertex, the
	// one that forms the smallest angle is assumed to be the right one.
	for (i = 0; i < numsides; ++i)
	{
		uint32_t right;
		line_t *line = level.sides[i].linedef;

		// If the side's line only exists in a single sector,
		// then consider that line to be a self-contained loop
		// instead of as part of another loop
		if (line->frontsector == line->backsector)
		{
			const side_t* const rightside = line->sidedef[!sidetemp[i].b.lineside];

			if (NULL == rightside)
			{
				// There is no right side!
				if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[line->Index()]);
				continue;
			}

			right = rightside->Index();
		}
		else
		{
			if (sidetemp[i].b.lineside)
			{
				right = line->v1->Index();
			}
			else
			{
				right = line->v2->Index();
			}

			right = sidetemp[right].b.first;

			if (right == NO_SIDE)
			{ 
				// There is no right side!
				if (firstloop) Printf ("Line %d's right edge is unconnected\n", linemap[line->Index()]);
				continue;
			}

			if (sidetemp[right].b.next != NO_SIDE)
			{
				int bestright = right;	// Shut up, GCC
				DAngle bestang = 360.;
				line_t *leftline, *rightline;
				DAngle ang1, ang2, ang;

				leftline = level.sides[i].linedef;
				ang1 = leftline->Delta().Angle();
				if (!sidetemp[i].b.lineside)
				{
					ang1 += 180;
				}

				while (right != NO_SIDE)
				{
					if (level.sides[right].LeftSide == NO_SIDE)
					{
						rightline = level.sides[right].linedef;
						if (rightline->frontsector != rightline->backsector)
						{
							ang2 = rightline->Delta().Angle();
							if (sidetemp[right].b.lineside)
							{
								ang2 += 180;
							}

							ang = (ang2 - ang1).Normalized360();

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
		level.sides[i].RightSide = right;
		level.sides[right].LeftSide = i;
	}

	// We keep the sidedef init info around until after polyobjects are initialized,
	// so don't delete just yet.
}

//===========================================================================
//
//
//
//===========================================================================

int P_DetermineTranslucency (int lumpnum)
{
	auto tranmap = Wads.OpenLumpReader (lumpnum);
	uint8_t index;
	PalEntry newcolor;
	PalEntry newcolor2;

	tranmap.Seek (GPalette.BlackIndex * 256 + GPalette.WhiteIndex, FileReader::SeekSet);
	tranmap.Read (&index, 1);

	newcolor = GPalette.BaseColors[GPalette.Remap[index]];

	tranmap.Seek (GPalette.WhiteIndex * 256 + GPalette.BlackIndex, FileReader::SeekSet);
	tranmap.Read (&index, 1);
	newcolor2 = GPalette.BaseColors[GPalette.Remap[index]];
	if (newcolor2.r == 255)	// if black on white results in white it's either
							// fully transparent or additive
	{
		if (developer >= DMSG_NOTIFY)
		{
			char lumpname[9];
			lumpname[8] = 0;
			Wads.GetLumpName (lumpname, lumpnum);
			Printf ("%s appears to be additive translucency %d (%d%%)\n", lumpname, newcolor.r,
				newcolor.r*100/255);
		}
		return -newcolor.r;
	}

	if (developer >= DMSG_NOTIFY)
	{
		char lumpname[9];
		lumpname[8] = 0;
		Wads.GetLumpName (lumpname, lumpnum);
		Printf ("%s appears to be translucency %d (%d%%)\n", lumpname, newcolor.r,
			newcolor.r*100/255);
	}
	return newcolor.r;
}

//===========================================================================
//
//
//
//===========================================================================

void P_ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex)
{
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
			uint32_t color = MAKERGB(255,255,255), fog = 0;
			bool colorgood, foggood;

			SetTextureNoErr (sd, side_t::bottom, &fog, msd->bottomtexture, &foggood, true);
			SetTextureNoErr (sd, side_t::top, &color, msd->toptexture, &colorgood, false);
			SetTexture(sd, side_t::mid, msd->midtexture, missingtex);

			if (colorgood | foggood)
			{
				for (unsigned s = 0; s < level.sectors.Size(); s++)
				{
					if (tagManager.SectorHasTag(s, tag))
					{
						if (colorgood)
						{
							level.sectors[s].Colormap.LightColor.SetRGB(color);
							level.sectors[s].Colormap.BlendFactor = APART(color);
						}
						if (foggood) level.sectors[s].Colormap.FadeColor.SetRGB(fog);
					}
				}
			}
		}
		break;

	case Sector_Set3DFloor:
		if (msd->toptexture[0]=='#')
		{
			sd->SetTexture(side_t::top, FNullTextureID() +(int)(-strtoll(&msd->toptexture[1], NULL, 10)));	// store the alpha as a negative texture index
														// This will be sorted out by the 3D-floor code later.
		}
		else
		{
			SetTexture(sd, side_t::top, msd->toptexture, missingtex);
		}

		SetTexture(sd, side_t::mid, msd->midtexture, missingtex);
		SetTexture(sd, side_t::bottom, msd->bottomtexture, missingtex);
		break;

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

//===========================================================================
//
// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

void P_LoadSideDefs2 (MapData *map, FMissingTextureTracker &missingtex)
{
	char * msdf = new char[map->Size(ML_SIDEDEFS)];
	map->Read(ML_SIDEDEFS, msdf);

	for (unsigned i = 0; i < level.sides.Size(); i++)
	{
		mapsidedef_t *msd = ((mapsidedef_t*)msdf) + sidetemp[i].a.map;
		side_t *sd = &level.sides[i];
		sector_t *sec;

		// [RH] The Doom renderer ignored the patch y locations when
		// drawing mid textures. ZDoom does not, so fix the laser beams in Strife.
		if (gameinfo.gametype == GAME_Strife &&
			strncmp (msd->midtexture, "LASERB01", 8) == 0)
		{
			msd->rowoffset += 102;
		}

		sd->SetTextureXOffset(LittleShort(msd->textureoffset));
		sd->SetTextureYOffset(LittleShort(msd->rowoffset));
		sd->SetTextureXScale(1.);
		sd->SetTextureYScale(1.);
		sd->linedef = NULL;
		sd->Flags = 0;
		sd->UDMFIndex = i;

		// killough 4/4/98: allow sidedef texture names to be overloaded
		// killough 4/11/98: refined to allow colormaps to work as wall
		// textures if invalid as colormaps but valid as textures.

		if ((unsigned)LittleShort(msd->sector)>=level.sectors.Size())
		{
			Printf (PRINT_HIGH, "Sidedef %d has a bad sector\n", i);
			sd->sector = sec = NULL;
		}
		else
		{
			sd->sector = sec = &level.sectors[LittleShort(msd->sector)];
		}

		intmapsidedef_t imsd;
		imsd.toptexture.CopyCStrPart(msd->toptexture, 8);
		imsd.midtexture.CopyCStrPart(msd->midtexture, 8);
		imsd.bottomtexture.CopyCStrPart(msd->bottomtexture, 8);

		P_ProcessSideTextures(!map->HasBehavior, sd, sec, &imsd, 
							  sidetemp[i].a.special, sidetemp[i].a.tag, &sidetemp[i].a.alpha, missingtex);
	}
	delete[] msdf;
}


//===========================================================================
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
//===========================================================================

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
	double dminx, dmaxx, dminy, dmaxy;
	int minx, maxx, miny, maxy;
	int line;

	if (level.vertexes.Size() == 0)
		return;

	// Find map extents for the blockmap
	dminx = dmaxx = level.vertexes[0].fX();
	dminy = dmaxy = level.vertexes[0].fY();

	for (auto &vert : level.vertexes)
	{
			 if (vert.fX() < dminx) dminx = vert.fX();
		else if (vert.fX() > dmaxx) dmaxx = vert.fX();
			 if (vert.fY() < dminy) dminy = vert.fY();
		else if (vert.fY() > dmaxy) dmaxy = vert.fY();
	}

	minx = int(dminx);
	miny = int(dminy);
	maxx = int(dmaxx);
	maxy = int(dmaxy);

	bmapwidth =	 ((maxx - minx) >> BLOCKBITS) + 1;
	bmapheight = ((maxy - miny) >> BLOCKBITS) + 1;

	TArray<int> BlockMap (bmapwidth * bmapheight * 3 + 4);

	adder = minx;			BlockMap.Push (adder);
	adder = miny;			BlockMap.Push (adder);
	adder = bmapwidth;		BlockMap.Push (adder);
	adder = bmapheight;		BlockMap.Push (adder);

	BlockLists = new TArray<int>[bmapwidth * bmapheight];

	for (line = 0; line < (int)level.lines.Size(); ++line)
	{
		int x1 = int(level.lines[line].v1->fX());
		int y1 = int(level.lines[line].v1->fY());
		int x2 = int(level.lines[line].v2->fX());
		int y2 = int(level.lines[line].v2->fY());
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

	level.blockmap.blockmaplump = new int[BlockMap.Size()];
	for (unsigned int ii = 0; ii < BlockMap.Size(); ++ii)
	{
		level.blockmap.blockmaplump[ii] = BlockMap[ii];
	}
}



//===========================================================================
//
// P_VerifyBlockMap
//
// haleyjd 03/04/10: do verification on validity of blockmap.
//
//===========================================================================

bool FBlockmap::VerifyBlockMap(int count)
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
				Printf(PRINT_HIGH, "VerifyBlockMap: block offset overflow\n");
				return false;
			}

			offset = *blockoffset;         

			// check that list offset is in bounds
			if(offset < 4 || offset >= count)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: list offset overflow\n");
				return false;
			}

			list   = blockmaplump + offset;

			// scan forward for a -1 terminator before maxoffs
			for(tmplist = list; ; tmplist++)
			{
				// we have overflowed the lump?
				if(tmplist >= maxoffs)
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: open blocklist\n");
					return false;
				}
				if(*tmplist == -1) // found -1
					break;
			}

			// there's some node builder which carelessly removed the initial 0-entry.
			// Rather than second-guessing the intent, let's just discard such blockmaps entirely
			// to be on the safe side.
			if (*list != 0)
			{
				Printf(PRINT_HIGH, "VerifyBlockMap: first entry is not 0.\n");
				return false;
			}

			// scan the list for out-of-range linedef indicies in list
			for(tmplist = list; *tmplist != -1; tmplist++)
			{
				if((unsigned)*tmplist >= level.lines.Size())
				{
					Printf(PRINT_HIGH, "VerifyBlockMap: index >= numlines\n");
					return false;
				}
			}
		}
	}

	return true;
}

//===========================================================================
//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
//===========================================================================

void P_LoadBlockMap (MapData * map)
{
	int count = map->Size(ML_BLOCKMAP);

	if (ForceNodeBuild || genblockmap ||
		count/2 >= 0x10000 || count == 0 ||
		Args->CheckParm("-blockmap")
		)
	{
		DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
		P_CreateBlockMap ();
	}
	else
	{
		uint8_t *data = new uint8_t[count];
		map->Read(ML_BLOCKMAP, data);
		const short *wadblockmaplump = (short *)data;
		int i;

		count/=2;
		level.blockmap.blockmaplump = new int[count];

		// killough 3/1/98: Expand wad blockmap into larger internal one,
		// by treating all offsets except -1 as unsigned and zero-extending
		// them. This potentially doubles the size of blockmaps allowed,
		// because Doom originally considered the offsets as always signed.

		level.blockmap.blockmaplump[0] = LittleShort(wadblockmaplump[0]);
		level.blockmap.blockmaplump[1] = LittleShort(wadblockmaplump[1]);
		level.blockmap.blockmaplump[2] = (uint32_t)(LittleShort(wadblockmaplump[2])) & 0xffff;
		level.blockmap.blockmaplump[3] = (uint32_t)(LittleShort(wadblockmaplump[3])) & 0xffff;

		for (i = 4; i < count; i++)
		{
			short t = LittleShort(wadblockmaplump[i]);          // killough 3/1/98
			level.blockmap.blockmaplump[i] = t == -1 ? (uint32_t)0xffffffff : (uint32_t) t & 0xffff;
		}
		delete[] data;

		if (!level.blockmap.VerifyBlockMap(count))
		{
			DPrintf (DMSG_SPAMMY, "Generating BLOCKMAP\n");
			P_CreateBlockMap();
		}

	}

	level.blockmap.bmaporgx = level.blockmap.blockmaplump[0];
	level.blockmap.bmaporgy = level.blockmap.blockmaplump[1];
	level.blockmap.bmapwidth = level.blockmap.blockmaplump[2];
	level.blockmap.bmapheight = level.blockmap.blockmaplump[3];

	// clear out mobj chains
	count = level.blockmap.bmapwidth*level.blockmap.bmapheight;
	level.blockmap.blocklinks = new FBlockNode *[count];
	memset (level.blockmap.blocklinks, 0, count*sizeof(*level.blockmap.blocklinks));
	level.blockmap.blockmap = level.blockmap.blockmaplump+4;
}

//===========================================================================
//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
//===========================================================================

static void P_GroupLines (bool buildmap)
{
	cycle_t times[16];
	unsigned int*		linesDoneInEachSector;
	int 				total;
	sector_t*			sector;
	FBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	unsigned int		jj;

	for (unsigned i = 0; i < countof(times); ++i)
	{
		times[i].Reset();
	}

	// look up sector number for each subsector
	times[0].Clock();
	for (auto &sub : level.subsectors)
	{
		sub.sector = sub.firstline->sidedef->sector;
	}
	for (auto &sub : level.subsectors)
	{
		for (jj = 0; jj < sub.numlines; ++jj)
		{
			sub.firstline[jj].Subsector = &sub;
		}
	}
	times[0].Unclock();

	// count number of lines in each sector
	times[1].Clock();
	total = 0;
	for (unsigned i = 0; i < level.lines.Size(); i++)
	{
		auto li = &level.lines[i];
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
			li->frontsector->Lines.Count++;
			total++;
		}

		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->Lines.Count++;
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
	level.linebuffer.Alloc(total);
	line_t **lineb_p = &level.linebuffer[0];
	auto numsectors = level.sectors.Size();
	linesDoneInEachSector = new unsigned int[numsectors];
	memset (linesDoneInEachSector, 0, sizeof(int)*numsectors);

	sector = &level.sectors[0];
	for (unsigned i = 0; i < numsectors; i++, sector++)
	{
		if (sector->Lines.Count == 0)
		{
			Printf ("Sector %i (tag %i) has no lines\n", i, tagManager.GetFirstSectorTag(sector));
			// 0 the sector's tag so that no specials can use it
			tagManager.RemoveSectorTags(i);
		}
		else
		{
			sector->Lines.Array = lineb_p;
			lineb_p += sector->Lines.Count;
		}
	}

	for (unsigned i = 0; i < level.lines.Size(); i++)
	{
		auto li = &level.lines[i];
		if (li->frontsector != NULL)
		{
			li->frontsector->Lines[linesDoneInEachSector[li->frontsector->Index()]++] = li;
		}
		if (li->backsector != NULL && li->backsector != li->frontsector)
		{
			li->backsector->Lines[linesDoneInEachSector[li->backsector->Index()]++] = li;
		}
	}
	
	sector = &level.sectors[0];
	for (unsigned i = 0; i < numsectors; ++i, ++sector)
	{
		if (linesDoneInEachSector[i] != sector->Lines.Size())
		{
			I_Error("P_GroupLines: miscounted");
		}
		if (sector->Lines.Size() > 3)
		{
			bbox.ClearBox();
			for (auto li : sector->Lines)
			{
				bbox.AddToBox(li->v1->fPos());
				bbox.AddToBox(li->v2->fPos());
			}

			// set the center to the middle of the bounding box
			sector->centerspot.X = (bbox.Right() + bbox.Left()) / 2;
			sector->centerspot.Y = (bbox.Top() + bbox.Bottom()) / 2;
		}
		else if (sector->Lines.Size() > 0)
		{
			// For triangular sectors the above does not calculate good points unless the longest of the triangle's lines is perfectly horizontal and vertical
			DVector2 pos = { 0,0 };
			for (auto ln : sector->Lines)
			{
				pos += ln->v1->fPos() + ln->v2->fPos();
			}
			sector->centerspot = pos / (2 * sector->Lines.Size());
		}
	}
	delete[] linesDoneInEachSector;
	times[3].Unclock();

	// [RH] Moved this here
	times[4].Clock();
	// killough 1/30/98: Create xref tables for tags
	tagManager.HashTags();
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
		for (int i = 0; i < 7; ++i)
		{
			Printf (" time %d:%9.4f ms\n", i, times[i].TimeMS());
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_LoadReject (MapData * map, bool junk)
{
	const int neededsize = (level.sectors.Size() * level.sectors.Size() + 7) >> 3;
	int rejectsize;

	if (!map->CheckName(ML_REJECT, "REJECT"))
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
		level.rejectmatrix.Reset();
	}
	else
	{
		// Check if the reject has some actual content. If not, free it.
		rejectsize = MIN (rejectsize, neededsize);
		level.rejectmatrix.Alloc(rejectsize);

		map->Read (ML_REJECT, &level.rejectmatrix[0], rejectsize);

		int qwords = rejectsize / 8;
		int i;

		if (qwords > 0)
		{
			const uint64_t *qreject = (const uint64_t *)&level.rejectmatrix[0];

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
			if (level.rejectmatrix[qwords + i] != 0)
				return;
		}

		// Reject has no data, so pretend it isn't there.
		level.rejectmatrix.Reset();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_LoadBehavior(MapData * map)
{
	if (map->Size(ML_BEHAVIOR) > 0)
	{
		FBehavior::StaticLoadModule(-1, &map->Reader(ML_BEHAVIOR), map->Size(ML_BEHAVIOR));
	}
	if (!FBehavior::StaticCheckAllGood())
	{
		Printf("ACS scripts unloaded.\n");
		FBehavior::StaticUnloadModules();
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_GetPolySpots (MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors)
{
	//if (map->HasBehavior)
	{
		for (unsigned int i = 0; i < MapThingsConverted.Size(); ++i)
		{
			FDoomEdEntry *mentry = MapThingsConverted[i].info;
			if (mentry != NULL && mentry->Type == NULL && mentry->Special >= SMT_PolyAnchor && mentry->Special <= SMT_PolySpawnHurt)
			{
				FNodeBuilder::FPolyStart newvert;
				newvert.x = FLOAT2FIXED(MapThingsConverted[i].pos.X);
				newvert.y = FLOAT2FIXED(MapThingsConverted[i].pos.Y);
				newvert.polynum = MapThingsConverted[i].angle;
				if (mentry->Special == SMT_PolyAnchor)
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


//===========================================================================
//
// P_PrecacheLevel
//
// Preloads all relevant graphics for the level.
//
//===========================================================================
void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist);

static void P_PrecacheLevel()
{
	int i;
	uint8_t *hitlist;
	TMap<PClassActor *, bool> actorhitlist;
	int cnt = TexMan.NumTextures();

	if (demoplayback)
		return;

	hitlist = new uint8_t[cnt];
	memset(hitlist, 0, cnt);

	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ((actor = iterator.Next()))
	{
		actorhitlist[actor->GetClass()] = true;
	}

	for (auto n : gameinfo.PrecachedClasses)
	{
		PClassActor *cls = PClass::FindActor(n);
		if (cls != NULL) actorhitlist[cls] = true;
	}
	for (unsigned i = 0; i < level.info->PrecacheClasses.Size(); i++)
	{
		// level.info can only store names, no class pointers.
		PClassActor *cls = PClass::FindActor(level.info->PrecacheClasses[i]);
		if (cls != NULL) actorhitlist[cls] = true;
	}

	for (i = level.sectors.Size() - 1; i >= 0; i--)
	{
		hitlist[level.sectors[i].GetTexture(sector_t::floor).GetIndex()] |= FTextureManager::HIT_Flat;
		hitlist[level.sectors[i].GetTexture(sector_t::ceiling).GetIndex()] |= FTextureManager::HIT_Flat;
	}

	for (i = level.sides.Size() - 1; i >= 0; i--)
	{
		hitlist[level.sides[i].GetTexture(side_t::top).GetIndex()] |= FTextureManager::HIT_Wall;
		hitlist[level.sides[i].GetTexture(side_t::mid).GetIndex()] |= FTextureManager::HIT_Wall;
		hitlist[level.sides[i].GetTexture(side_t::bottom).GetIndex()] |= FTextureManager::HIT_Wall;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.

	if (sky1texture.isValid())
	{
		hitlist[sky1texture.GetIndex()] |= FTextureManager::HIT_Sky;
	}
	if (sky2texture.isValid())
	{
		hitlist[sky2texture.GetIndex()] |= FTextureManager::HIT_Sky;
	}

	for (auto n : gameinfo.PrecachedTextures)
	{
		FTextureID tex = TexMan.CheckForTexture(n, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) hitlist[tex.GetIndex()] |= FTextureManager::HIT_Wall;
	}
	for (unsigned i = 0; i < level.info->PrecacheTextures.Size(); i++)
	{
		FTextureID tex = TexMan.CheckForTexture(level.info->PrecacheTextures[i], ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) hitlist[tex.GetIndex()] |= FTextureManager::HIT_Wall;
	}

	// This is just a temporary solution, until the hardware renderer's texture manager is in a better state.
	if (!V_IsHardwareRenderer())
		SWRenderer->Precache(hitlist, actorhitlist);
	else
		hw_PrecacheTexture(hitlist, actorhitlist);

	delete[] hitlist;
}

extern polyblock_t **PolyBlockMap;


//==========================================================================
//
//
//
//==========================================================================

void P_FreeLevelData ()
{
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	auto mo = it.Next();
	while (mo)
	{
		auto next = it.Next();
		mo->Destroy();
		mo = next;
	}

	// [ZZ] delete per-map event handlers
	E_Shutdown(true);
	MapThingsConverted.Clear();
	MapThingsUserDataIndex.Clear();
	MapThingsUserData.Clear();
	linemap.Clear();
	FCanvasTextureInfo::EmptyList();
	R_FreePastViewers();
	P_ClearUDMFKeys();

	// [RH] Clear all ThingID hash chains.
	AActor::ClearTIDHashes();

	interpolator.ClearInterpolations();	// [RH] Nothing to interpolate on a fresh level.
	FPolyObj::ClearAllSubsectorLinks(); // can't be done as part of the polyobj deletion process.
	SN_StopAllSequences ();
	DThinker::DestroyAllThinkers ();
	P_ClearPortals();
	tagManager.Clear();
	level.total_monsters = level.total_items = level.total_secrets =
		level.killed_monsters = level.found_items = level.found_secrets =
		wminfo.maxfrags = 0;
		
	// delete allocated data in the level arrays.
	if (level.sectors.Size() > 0)
	{
		delete[] level.sectors[0].e;
		if (level.sectors[0].subsectors)
		{
			delete[] level.sectors[0].subsectors;
			level.sectors[0].subsectors = nullptr;
		}
	}
	for (auto &sub : level.subsectors)
	{
		if (sub.BSP != nullptr) delete sub.BSP;
	}
	if (level.sides.Size() > 0 && level.sides[0].segs)
	{
		delete[] level.sides[0].segs;
		level.sides[0].segs = nullptr;
	}


	FBehavior::StaticUnloadModules ();
	level.segs.Clear();
	level.sectors.Clear();
	level.lines.Clear();
	level.sides.Clear();
	level.loadsectors.Clear();
	level.loadlines.Clear();
	level.loadsides.Clear();
	level.vertexes.Clear();
	level.nodes.Clear();
	level.gamenodes.Reset();
	level.subsectors.Clear();
	level.gamesubsectors.Reset();
	level.rejectmatrix.Clear();
	level.Zones.Clear();
	level.blockmap.Clear();

	if (PolyBlockMap != NULL)
	{
		for (int i = level.blockmap.bmapwidth*level.blockmap.bmapheight-1; i >= 0; --i)
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
	if (polyobjs != NULL)
	{
		delete[] polyobjs;
		polyobjs = NULL;
	}
	po_NumPolyobjs = 0;

	level.deathmatchstarts.Clear();
	level.AllPlayerStarts.Clear();
	memset(level.playerstarts, 0, sizeof(level.playerstarts));

	P_FreeStrifeConversations ();
	level.Scrolls.Clear();
	P_ClearUDMFKeys();
}

//===========================================================================
//
//
//
//===========================================================================

extern FMemArena secnodearena;
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
	secnodearena.FreeAllBlocks();
	headsecnode = nullptr;
}


//===========================================================================
//
// P_SetupLevel
//
// [RH] position indicates the start spot to spawn at
//
//===========================================================================

void P_SetupLevel (const char *lumpname, int position)
{
	cycle_t times[20];
#if 0
	FMapThing *buildthings;
	int numbuildthings;
#endif
	int i;
	bool buildmap;
	const int *oldvertextable = NULL;

	level.ShaderStartTime = I_msTimeFS(); // indicate to the shader system that the level just started

	// This is motivated as follows:

	bool RequireGLNodes = true;	// Even the software renderer needs GL nodes now.

	for (i = 0; i < (int)countof(times); ++i)
	{
		times[i].Reset();
	}

	level.maptype = MAPTYPE_UNKNOWN;
	wminfo.partime = 180;

	if (!savegamerestore)
	{
		level.SetMusicVolume(level.MusicVolume);
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
	players[consoleplayer].viewz = NO_VALUE; 

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start ();

	// [RH] clear out the mid-screen message
	C_MidPrint (NULL, NULL);

	// Free all level data from the previous map
	P_FreeLevelData ();

	MapData *map = P_OpenMapData(lumpname, true);
	if (map == NULL)
	{
		I_Error("Unable to open map '%s'\n", lumpname);
	}

	// [ZZ] init per-map static handlers. we need to call this before everything is set up because otherwise scripts don't receive PlayerEntered event
	//      (which happens at god-knows-what stage in this function, but definitely not the last part, because otherwise it'd work to put E_InitStaticHandlers before the player spawning)
	E_InitStaticHandlers(true);

	// generate a checksum for the level, to be included and checked with savegames.
	map->GetChecksum(level.md5);
	// find map num
	level.lumpnum = map->lumpnum;
	hasglnodes = false;

	// [RH] Support loading Build maps (because I felt like it. :-)
	buildmap = false;
#if 0
	// deactivated because broken.
	if (map->Size(0) > 0)
	{
		uint8_t *mapdata = new uint8_t[map->Size(0)];
		map->Read(0, mapdata);
		times[0].Clock();
		buildmap = P_LoadBuildMap (mapdata, map->Size(0), &buildthings, &numbuildthings);
		times[0].Unclock();
		delete[] mapdata;
	}
#endif

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
		FName checksum = CheckCompatibility(map);
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
#ifndef NO_EDATA
		LoadMapinfoACSLump();
#endif


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
		}
		else
		{
			times[0].Clock();
			P_ParseTextMap(map, missingtex);
			times[0].Unclock();
		}

		SetCompatibilityParams(checksum);

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
		FileReader *fr = nullptr;
		uint32_t id = MAKE_ID('X','x','X','x'), idcheck = 0, idcheck2 = 0, idcheck3 = 0, idcheck4 = 0, idcheck5 = 0, idcheck6 = 0;

		if (map->Size(ML_ZNODES) != 0)
		{
			// Test normal nodes first
			fr = &map->Reader(ML_ZNODES);
			idcheck = MAKE_ID('Z','N','O','D');
			idcheck2 = MAKE_ID('X','N','O','D');
		}
		else if (map->Size(ML_GLZNODES) != 0)
		{
			fr = &map->Reader(ML_GLZNODES);
			idcheck = MAKE_ID('Z','G','L','N');
			idcheck2 = MAKE_ID('Z','G','L','2');
			idcheck3 = MAKE_ID('Z','G','L','3');
			idcheck4 = MAKE_ID('X','G','L','N');
			idcheck5 = MAKE_ID('X','G','L','2');
			idcheck6 = MAKE_ID('X','G','L','3');
		}

		if (fr != nullptr && fr->isOpen()) fr->Read (&id, 4);
		if (id != 0 && (id == idcheck || id == idcheck2 || id == idcheck3 || id == idcheck4 || id == idcheck5 || id == idcheck6))
		{
			try
			{
				P_LoadZNodes (*fr, id);
			}
			catch (CRecoverableError &error)
			{
				Printf ("Error loading nodes: %s\n", error.GetMessage());

				ForceNodeBuild = true;
				level.subsectors.Clear();
				level.segs.Clear();
				level.nodes.Clear();
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

	uint64_t startTime=0, endTime=0;

	bool BuildGLNodes;
	if (ForceNodeBuild)
	{
		BuildGLNodes = RequireGLNodes || multiplayer || demoplayback || demorecording || genglnodes;

		startTime = I_msTime ();
		TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
		P_GetPolySpots (map, polyspots, anchors);
		FNodeBuilder::FLevel leveldata =
		{
			&level.vertexes[0], (int)level.vertexes.Size(),
			&level.sides[0], (int)level.sides.Size(),
			&level.lines[0], (int)level.lines.Size(),
			0, 0, 0, 0
		};
		leveldata.FindMapBounds ();
		// We need GL nodes if am_textured is on.
		// In case a sync critical game mode is started, also build GL nodes to avoid problems
		// if the different machines' am_textured setting differs.
		FNodeBuilder builder (leveldata, polyspots, anchors, BuildGLNodes);
		builder.Extract (level);
		endTime = I_msTime ();
		DPrintf (DMSG_NOTIFY, "BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, level.segs.Size());
		oldvertextable = builder.GetOldVertexTable();
		reloop = true;
	}
	else
	{
		BuildGLNodes = false;
		// Older ZDBSPs had problems with compressed sidedefs and assigned wrong sides to the segs if both sides were the same sidedef.
		for(auto &seg : level.segs)
		{
			if (seg.backsector == seg.frontsector && seg.linedef)
			{
				double d1 = (seg.v1->fPos() - seg.linedef->v1->fPos()).LengthSquared();
				double d2 = (seg.v2->fPos() - seg.linedef->v1->fPos()).LengthSquared();

				if (d2<d1)	// backside
				{
					seg.sidedef = seg.linedef->sidedef[1];
				}
				else	// front side
				{
					seg.sidedef = seg.linedef->sidedef[0];
				}
			}
		}
	}

	if (RequireGLNodes)
	{
		// Build GL nodes if we want a textured automap or GL nodes are forced to be built.
		// If the original nodes being loaded are not GL nodes they will be kept around for
		// use in P_PointInSubsector to avoid problems with maps that depend on the specific
		// nodes they were built with (P:AR E1M3 is a good example for a map where this is the case.)
		reloop |= P_CheckNodes(map, BuildGLNodes, (uint32_t)(endTime - startTime));
		hasglnodes = true;
	}
	else
	{
		hasglnodes = P_CheckForGLNodes();
	}

	// set the head node for gameplay purposes. If the separate gamenodes array is not empty, use that, otherwise use the render nodes.
	level.headgamenode = level.gamenodes.Size() > 0 ? &level.gamenodes[level.gamenodes.Size() - 1] : level.nodes.Size()? &level.nodes[level.nodes.Size() - 1] : nullptr;

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
#if 0	// There is no such thing as a build map.
	else
	{
		for (i = 0; i < numbuildthings; ++i)
		{
			SpawnMapThing (i, &buildthings[i], 0);
		}
		delete[] buildthings;
	}
#endif
	delete map;
	if (oldvertextable != NULL)
	{
		delete[] oldvertextable;
	}

	// set up world state
	P_SpawnSpecials ();

	// disable reflective planes on sloped sectors.
	for (auto &sec : level.sectors)
	{
		if (sec.floorplane.isSlope()) sec.reflect[sector_t::floor] = 0;
		if (sec.ceilingplane.isSlope()) sec.reflect[sector_t::ceiling] = 0;
	}
	for (auto &node : level.nodes)
	{
		double fdx = FIXED2DBL(node.dx);
		double fdy = FIXED2DBL(node.dy);
		node.len = (float)g_sqrt(fdx * fdx + fdy * fdy);
	}

	// This must be done BEFORE the PolyObj Spawn!!!
	InitRenderInfo();			// create hardware independent renderer resources for the level.
	screen->InitForLevel();		// create hardware dependent level resources (e.g. the vertex buffer)
	SWRenderer->SetColormap();	//The SW renderer needs to do some special setup for the level's default colormap.
	InitPortalGroups();

	times[16].Clock();
	if (reloop) P_LoopSidedefs (false);
	PO_Init ();				// Initialize the polyobjs
	if (!savegamerestore)
		P_FinalizePortals();	// finalize line portals after polyobjects have been initialized. This info is needed for properly flagging them.
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

	// [SP] move unfriendly players around
	// horribly hacky - yes, this needs rewritten.
	if (level.deathmatchstarts.Size () > 0)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != NULL)
			{
				if (!(players[i].mo->flags & MF_FRIENDLY))
				{
					AActor * oldSpawn = players[i].mo;
					G_DeathMatchSpawnPlayer (i);
					oldSpawn->Destroy();
				}
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
				if (mo->Sector->damageamount > 0 && (mo->Sector->Flags & (SECF_ENDGODMODE|SECF_ENDLEVEL)) == (SECF_ENDGODMODE|SECF_ENDLEVEL))
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
		P_PrecacheLevel ();
		S_PrecacheLevel ();
	}
	times[17].Unclock();

	if (deathmatch)
	{
		AnnounceGameStart ();
	}

	// This check was previously done at run time each time the heightsec was checked.
	// However, since 3D floors are static data, we can easily precalculate this and store it in the sector's flags for quick access.
	for (auto &s : level.sectors)
	{
		if (s.heightsec != nullptr)
		{
			// If any of these 3D floors render their planes, ignore heightsec.
			for (auto &ff : s.e->XFloor.ffloors)
			{
				if ((ff->flags & (FF_EXISTS | FF_RENDERPLANES)) == (FF_EXISTS | FF_RENDERPLANES))
				{
					s.MoreFlags |= SECMF_IGNOREHEIGHTSEC;	// mark the heightsec inactive.
				}
			}
		}
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

	// Create a backup of the map data so the savegame code can toss out all fields that haven't changed in order to reduce processing time and file size.
	// Note that we want binary identity here, so assignment is not sufficient because it won't initialize any padding bytes.
	// Note that none of these structures may contain non POD fields anyway.
	level.loadsectors.Resize(level.sectors.Size());
	memcpy(&level.loadsectors[0], &level.sectors[0], level.sectors.Size() * sizeof(level.sectors[0]));
	level.loadlines.Resize(level.lines.Size());
	memcpy(&level.loadlines[0], &level.lines[0], level.lines.Size() * sizeof(level.lines[0]));
	level.loadsides.Resize(level.sides.Size());
	memcpy(&level.loadsides[0], &level.sides[0], level.sides.Size() * sizeof(level.sides[0]));
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
	// [ZZ] delete global event handlers
	DThinker::DestroyThinkersInList(STAT_STATIC);
	E_Shutdown(false);
	P_DeinitKeyMessages ();
	P_FreeLevelData ();
	P_FreeExtraLevelData ();
	ST_Clear();
	FS_Close();
	for (auto &p : players)
	{
		if (p.psprites != nullptr) p.psprites->Destroy();
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
	Printf ("(%f,%f) -> (%f,%f)\n", lines[linenum].v1->fX(),
		lines[linenum].v1->fY(),
		lines[linenum].v2->fX(),
		lines[linenum].v2->fY());
}
#endif

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	for (auto &sector : level.sectors)
	{
		Printf(PRINT_LOG, "Sector %d\n", sector.sectornum);
		for (int j = 0; j<sector.subsectorcount; j++)
		{
			subsector_t * sub = sector.subsectors[j];

			Printf(PRINT_LOG, "    Subsector %d - real sector = %d - %s\n", int(sub->Index()), sub->sector->sectornum, sub->hacked & 1 ? "hacked" : "");
			for (uint32_t k = 0; k<sub->numlines; k++)
			{
				seg_t * seg = sub->firstline + k;
				if (seg->linedef)
				{
					Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d",
						seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(),
						seg->Index(), seg->linedef->Index(), seg->sidedef != seg->linedef->sidedef[0]);
				}
				else
				{
					Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg",
						seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(), seg->Index());
				}
				if (seg->PartnerSeg)
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;
					Printf(PRINT_LOG, ", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
				}
				else if (seg->backsector)
				{
					Printf(PRINT_LOG, ", back sector = %d (no partnerseg)", seg->backsector->sectornum);
				}

				Printf(PRINT_LOG, "\n");
			}
		}
	}
}
