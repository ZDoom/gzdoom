//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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
//	 Setup a game, startup stuff.
//
//-----------------------------------------------------------------------------


#ifndef __P_SETUP__
#define __P_SETUP__

#include "resourcefiles/resourcefile.h"
#include "doomdata.h"


struct MapData
{
	struct MapLump
	{
		char Name[8];
		FileReader *Reader;
	} MapLumps[ML_MAX];
	bool HasBehavior;
	bool Encrypted;
	bool isText;
	bool InWad;
	int lumpnum;
	FileReader * file;
	FResourceFile * resource;
	
	MapData()
	{
		memset(MapLumps, 0, sizeof(MapLumps));
		file = NULL;
		resource = NULL;
		lumpnum = -1;
		HasBehavior = false;
		Encrypted = false;
		isText = false;
		InWad = false;
	}
	
	~MapData()
	{
		for (unsigned int i = 0;i < ML_MAX;++i)
			delete MapLumps[i].Reader;

		delete resource;
		resource = NULL;
	}

	void Seek(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps))
		{
			file = MapLumps[lumpindex].Reader;
			file->Seek(0, SEEK_SET);
		}
	}

	void Read(unsigned int lumpindex, void * buffer, int size = -1)
	{
		if (lumpindex<countof(MapLumps))
		{
			if (size == -1) size = MapLumps[lumpindex].Reader->GetLength();
			Seek(lumpindex);
			file->Read(buffer, size);
		}
	}

	uint32_t Size(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps) && MapLumps[lumpindex].Reader)
		{
			return MapLumps[lumpindex].Reader->GetLength();
		}
		return 0;
	}

	void GetChecksum(uint8_t cksum[16]);
};

MapData * P_OpenMapData(const char * mapname, bool justcheck);
bool P_CheckMapData(const char * mapname);


// NOT called by W_Ticker. Fixme. [RH] Is that bad?
//
// [RH] The only parameter used is mapname, so I removed playermask and skill.
//		On September 1, 1998, I added the position to indicate which set
//		of single-player start spots should be spawned in the level.
void P_SetupLevel (const char *mapname, int position);

void P_FreeLevelData();
void P_FreeExtraLevelData();

// Called by startup code.
void P_Init (void);

struct line_t;
struct maplinedef_t;

void P_LoadTranslator(const char *lumpname);
void P_TranslateLineDef (line_t *ld, maplinedef_t *mld, int lineindexforid = -1);
int P_TranslateSectorSpecial (int);

int GetUDMFInt(int type, int index, FName key);
double GetUDMFFloat(int type, int index, FName key);

bool P_LoadGLNodes(MapData * map);
bool P_CheckNodes(MapData * map, bool rebuilt, int buildtime);
bool P_CheckForGLNodes();
void P_SetRenderSector();


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
			uint32_t map;
		} a;

		// Used when grouping sidedefs into loops.
		struct
		{
			uint32_t first, next;
			char lineside;
		} b;
	};
};
extern sidei_t *sidetemp;
extern bool hasglnodes;

struct FMissingCount
{
	FMissingCount() : Count(0) {}
	int Count;
};
typedef TMap<FString,FMissingCount> FMissingTextureTracker;

// Record of user data for UDMF maps
struct FMapThingUserData
{
	FName Property;
	int Value;
};
extern TMap<unsigned,unsigned> MapThingsUserDataIndex;	// from mapthing idx -> user data idx
extern TArray<FMapThingUserData> MapThingsUserData;


#endif
