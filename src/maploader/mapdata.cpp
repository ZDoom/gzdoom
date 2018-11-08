//-----------------------------------------------------------------------------
//
// Copyright 2006-2018 Christoph Oelckers
// Copyright 2006-2016 Randy Heit
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
//		Map loader
//

#include "doomtype.h"
#include "mapdata.h"
#include "vm.h"
#include "files.h"
#include "w_wad.h"
#include "md5.h"
#include "g_levellocals.h"


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
				// it is not a multi-lump Doom level.
				map->MapLumps[0].Reader = Wads.ReopenLumpReader(lump_name);
				delete map;
				return NULL;
			}

			// This case can only happen if the lump is inside a real WAD file.
			// As such any special handling for other types of lumps is skipped.
			map->MapLumps[0].Reader = Wads.ReopenLumpReader(lump_name);
			strncpy(map->MapLumps[0].Name, Wads.GetLumpFullName(lump_name), 8);
			map->Encrypted = Wads.IsEncryptedFile(lump_name);
			map->InWad = true;

			if (map->Encrypted)
			{ // If it's encrypted, then it's a Blood file, presumably a map.
				delete map;
				return NULL;
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
		delete map;
		return NULL;
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

