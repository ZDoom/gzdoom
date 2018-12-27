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
#include "r_defs.h"
#include "nodebuild.h"


struct MapData
{
private:
	struct ResourceHolder
	{
		FResourceFile *data = nullptr;

		~ResourceHolder()
		{
			delete data;
		}

		ResourceHolder &operator=(FResourceFile *other) { data = other; return *this; }
		FResourceFile *operator->() { return data; }
		operator FResourceFile *() const { return data; }
	};

	// The order of members here is important
	// Resource should be destructed after MapLumps as readers may share FResourceLump objects
	// For example, this is the case when map .wad is loaded from .pk3 file
	ResourceHolder resource;

	struct MapLump
	{
		char Name[8] = { 0 };
		FileReader Reader;
	} MapLumps[ML_MAX];
	FileReader nofile;
public:
	bool HasBehavior = false;
	bool Encrypted = false;
	bool isText = false;
	bool InWad = false;
	int lumpnum = -1;

	/*
	void Seek(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps))
		{
			file = &MapLumps[lumpindex].Reader;
			file->Seek(0, FileReader::SeekSet);
		}
	}
	*/

	FileReader &Reader(unsigned int lumpindex)
	{
		if (lumpindex < countof(MapLumps))
		{
			auto &file = MapLumps[lumpindex].Reader;
			file.Seek(0, FileReader::SeekSet);
			return file;
		}
		return nofile;
	}

	void Read(unsigned int lumpindex, void * buffer, int size = -1)
	{
		if (lumpindex<countof(MapLumps))
		{
			if (size == -1) size = Size(lumpindex);
			if (size > 0)
			{
				auto &file = MapLumps[lumpindex].Reader;
				file.Seek(0, FileReader::SeekSet);
				file.Read(buffer, size);
			}
		}
	}

	TArray<uint8_t> Read(unsigned lumpindex)
	{
		TArray<uint8_t> buffer(Size(lumpindex), true);
 		Read(lumpindex, buffer.Data(), (int)buffer.Size());
		return buffer;
	}

	uint32_t Size(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps) && MapLumps[lumpindex].Reader.isOpen())
		{
			return (uint32_t)MapLumps[lumpindex].Reader.GetLength();
		}
		return 0;
	}

	bool CheckName(unsigned int lumpindex, const char *name)
	{
		if (lumpindex < countof(MapLumps))
		{
			return !strnicmp(MapLumps[lumpindex].Name, name, 8);
		}
		return false;
	}

	void GetChecksum(uint8_t cksum[16]);

	friend class MapLoader;
	friend MapData *P_OpenMapData(const char * mapname, bool justcheck);

};

MapData * P_OpenMapData(const char * mapname, bool justcheck);
bool P_CheckMapData(const char * mapname);


// NOT called by W_Ticker. Fixme. [RH] Is that bad?
//
// [RH] The only parameter used is mapname, so I removed playermask and skill.
//		On September 1, 1998, I added the position to indicate which set
//		of single-player start spots should be spawned in the level.
void P_SetupLevel (const char *mapname, int position, bool newGame);

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
FString GetUDMFString(int type, int index, FName key);

bool P_CheckForGLNodes();
void P_SetRenderSector();
void FixMinisegReferences();
void FixHoles();
void ReportUnpairedMinisegs();


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

struct FMissingCount
{
	FMissingCount() : Count(0) {}
	int Count;
};
typedef TMap<FString,FMissingCount> FMissingTextureTracker;

extern TMap<unsigned,unsigned> MapThingsUserDataIndex;	// from mapthing idx -> user data idx
extern TArray<FUDMFKey> MapThingsUserData;

struct FLevelLocals;

class MapLoader
{
	friend class UDMFParser;
	void *level;	// this is to hide the global variable and produce an error for referencing it.
	FLevelLocals *Level;

	void SetTexture(side_t *side, int position, const char *name, FMissingTextureTracker &track);
	void SetTexture(sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate);
	void SetTexture(side_t *side, int position, uint32_t *blend, const char *name);
	void SetTextureNoErr(side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog);

	void FloodZone(sector_t *sec, int zonenum);
	void LoadGLZSegs(FileReader &data, int type);
	void LoadZSegs(FileReader &data);
	void LoadZNodes(FileReader &data, int glnodes);

	int DetermineTranslucency(int lumpnum);
	void SetLineID(int i, line_t *ld);
	void SaveLineSpecial(line_t *ld);
	void FinishLoadingLineDef(line_t *ld, int alpha);
	void SetSideNum(side_t **sidenum_p, uint16_t sidenum);
	void AllocateSideDefs(MapData *map, int count);
	void ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex);
	void CreateBlockMap();

	void AddToList(uint8_t *hitlist, FTextureID texid, int bitmask);

public:

	void FloodZones();
	void LoadVertexes(MapData * map);
	void LoadExtendedNodes(FileReader &dalump, uint32_t id);
	template<class segtype> void LoadSegs(MapData * map);
	template<class subsectortype, class segtype> void LoadSubsectors(MapData * map);
	template<class nodetype, class subsectortype> void LoadNodes(MapData * map);
	bool LoadGLNodes(MapData * map);
	bool CheckCachedNodes(MapData *map);
	bool CheckNodes(MapData * map, bool rebuilt, int buildtime);

	void LoadSectors(MapData *map, FMissingTextureTracker &missingtex);
	void LoadThings(MapData * map);
	void LoadThings2(MapData * map);

	void SpawnThings(int position);
	void FinishLoadingLineDefs();
	void LoadLineDefs(MapData * map);
	void LoadLineDefs2(MapData * map);
	void LoopSidedefs(bool firstloop);
	void LoadSideDefs2(MapData *map, FMissingTextureTracker &missingtex);
	void LoadBlockMap(MapData * map);
	void LoadReject(MapData * map, bool junk);
	void LoadBehavior(MapData * map);
	void GetPolySpots(MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors);
	void GroupLines(bool buildmap);
	void PrecacheLevel();
	void ParseTextMap(MapData *map, FMissingTextureTracker &missingtex);
	void SummarizeMissingTextures(const FMissingTextureTracker &missing);

	MapLoader(FLevelLocals *lev)
	{
		Level = lev;
	}
};

#endif
