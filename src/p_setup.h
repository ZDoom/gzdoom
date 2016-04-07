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

	DWORD Size(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps) && MapLumps[lumpindex].Reader)
		{
			return MapLumps[lumpindex].Reader->GetLength();
		}
		return 0;
	}

	void GetChecksum(BYTE cksum[16]);
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

int GetUDMFInt(int type, int index, const char *key);
double GetUDMFFloat(int type, int index, const char *key);

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
			DWORD map;
		} a;

		// Used when grouping sidedefs into loops.
		struct
		{
			DWORD first, next;
			char lineside;
		} b;
	};
};
extern sidei_t *sidetemp;
extern bool hasglnodes;
extern struct glsegextra_t *glsegextras;

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
