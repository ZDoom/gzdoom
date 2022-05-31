#pragma once

#include "nodebuild.h"
#include "g_levellocals.h"

class FileReader;
struct FStrifeDialogueNode;
struct FStrifeDialogueReply;
struct Response;

struct EDMapthing
{
	int recordnum;
	int tid;
	int type;
	double height;
	int args[5];
	uint16_t skillfilter;
	uint32_t flags;
};

struct EDLinedef
{
	int recordnum;
	int special;
	int tag;
	int id;
	int args[5];
	double alpha;
	uint32_t flags;
	uint32_t activation;
};



struct EDSector
{
	int recordnum;

	uint32_t flags;
	uint32_t flagsRemove;
	uint32_t flagsAdd;

	int damageamount;
	int damageinterval;
	FName damagetype;
	uint8_t leaky;
	uint8_t leakyadd;
	uint8_t leakyremove;
	int floorterrain;
	int ceilingterrain;

	uint32_t color;

	uint32_t damageflags;
	uint32_t damageflagsAdd;
	uint32_t damageflagsRemove;

	bool flagsSet;
	bool damageflagsSet;
	bool colorSet;

	// colormaptop//bottom cannot be used because ZDoom has no corresponding properties.
	double xoffs[2], yoffs[2];
	DAngle angle[2];
	uint32_t portalflags[2];
	double Overlayalpha[2];
};

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

struct FMissingCount
{
	int Count = 0;
};

typedef TMap<FString,FMissingCount> FMissingTextureTracker;
struct FLevelLocals;
struct MapData;

class MapLoader
{
	friend class UDMFParser;
	friend class USDFParser;
	void *level;	// this is to hide the global variable and produce an error for referencing it.
public:
	FLevelLocals *Level;
private:

	int firstglvertex;	// helpers for loading GL nodes from GWA files.
	bool format5;

	TArray<vertexdata_t> vertexdatas;

	TMap<unsigned, unsigned>  MapThingsUserDataIndex;	// from mapthing idx -> user data idx
	TArray<FUDMFKey> MapThingsUserData;
	int sidecount = 0;
	TArray<int>		linemap;
	TArray<sidei_t> sidetemp;
public:	// for the scripted compatibility system these two members need to be public.
	TArray<FMapThing> MapThingsConverted;
	bool ForceNodeBuild = false;
private:

	// Extradata loader
	TMap<int, EDLinedef> EDLines;
	TMap<int, EDSector> EDSectors;
	TMap<int, EDMapthing> EDThings;

	// Polyobject init
	TArray<int32_t> KnownPolySides;

	FName CheckCompatibility(MapData *map);
	void PostProcessLevel(FName checksum);

	// Slopes
	void SlopeLineToPoint(int lineid, const DVector3 &pos, bool slopeCeil);
	void CopyPlane(int tag, sector_t *dest, bool copyCeil);
	void CopyPlane(int tag, const DVector2 &pos, bool copyCeil);
	void SetSlope(secplane_t *plane, bool setCeil, int xyangi, int zangi, const DVector3 &pos);
	void VavoomSlope(sector_t * sec, int id, const DVector3 &pos, int which);
	void SetSlopesFromVertexHeights(FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
	void AlignPlane(sector_t *sec, line_t *line, int which);

	// Extradata
	void InitED();
	void ProcessEDMapthing(FMapThing *mt, int recordnum);
	void ProcessEDLinedef(line_t *line, int recordnum);
	void ProcessEDSector(sector_t *sec, int recordnum);
	void parseEDLinedef(FScanner &sc, TMap<int, EDLinedef> &EDLines);

	// Polyobjects
	void InitSideLists();
	void IterFindPolySides(FPolyObj *po, side_t *side);
	void SpawnPolyobj(int index, int tag, int type);
	void TranslateToStartSpot(int tag, const DVector2 &origin);
	void InitPolyBlockMap(void);

	// GL nodes
	int checkGLVertex(int num);
	int checkGLVertex3(int num);
	int CheckForMissingSegs();
	bool LoadGLVertexes(FileReader &lump);
	bool LoadGLSegs(FileReader &lump);
	bool LoadGLSubsectors(FileReader &lump);
	bool LoadNodes(FileReader &lump);
	bool DoLoadGLNodes(FileReader * lumps);
	void CreateCachedNodes(MapData *map);

	// Render info
	void PrepareSectorData();
	void PrepareTransparentDoors(sector_t * sector);
	void InitVertexData();
	void GetSideVertices(int sdnum, DVector2 *v1, DVector2 *v2);
	void PrepareSegs();
	void FloodSectorStacks();
	void InitRenderInfo();
	void FixMinisegReferences();
	void FixHoles();
	void ReportUnpairedMinisegs();
	void CalcIndices();
	
	// Strife dialogue
	void LoadStrifeConversations (MapData *map, const char *mapname);
	bool LoadScriptFile (const char *name, bool include, int type);
	bool LoadScriptFile(const char *name, int lumpnum, FileReader &lump, int numnodes, bool include, int type);
	FStrifeDialogueNode *ReadRetailNode (const char *name, FileReader &lump, uint32_t &prevSpeakerType);
	FStrifeDialogueNode *ReadTeaserNode (const char *name, FileReader &lump, uint32_t &prevSpeakerType);
	void ParseReplies (const char *name, int pos, FStrifeDialogueReply **replyptr, Response *responses);
	
	bool ParseUSDF(int lumpnum, FileReader &lump, int lumplen);
	
	// Specials
	void SpawnSpecials();
	void InitSectorSpecial(sector_t *sector, int special);
	void SpawnLights(sector_t *sector);
	void CreateScroller(EScroll type, double dx, double dy, sector_t *affectee, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	void SpawnScrollers();
	void SpawnFriction();
	void SpawnPushers();
	AActor *GetPushThing (int s);
	void SpawnPortal(line_t *line, int sectortag, int plane, int bytealpha, int linked);
	void CopyPortal(int sectortag, int plane, unsigned pnum, double alpha, bool tolines);
	void SetPortal(sector_t *sector, int plane, unsigned pnum, double alpha);
	void SpawnLinePortal(line_t* line);
	void SetupPortals();
	void SpawnSkybox(AActor *origin);
	void SetupFloorPortal (AActor *point);
	void SetupCeilingPortal (AActor *point);
	void TranslateTeleportThings();
	int Set3DFloor(line_t * line, int param, int param2, int alpha);
	void Spawn3DFloors ();

	void SetTexture(side_t *side, int position, const char *name, FMissingTextureTracker &track);
	void SetTexture(sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate);
	void SetTexture(side_t *side, int position, uint32_t *blend, const char *name);
	void SetTextureNoErr(side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog);

	void FloodZone(sector_t *sec, int zonenum);
	void LoadGLZSegs(FileReader &data, int type);
	void LoadZSegs(FileReader &data);
	void LoadZNodes(FileReader &data, int glnodes);

	void SetLineID(int i, line_t *ld);
	void SaveLineSpecial(line_t *ld);
	void FinishLoadingLineDef(line_t *ld, int alpha);
	void SetSideNum(side_t **sidenum_p, uint16_t sidenum);
	void AllocateSideDefs(MapData *map, int count);
	void ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex);
	void SetMapThingUserData(AActor *actor, unsigned udi);
	void CreateBlockMap();
	void PO_Init(void);

	// During map init the items' own Index functions should not be used.
	inline int Index(vertex_t *v) const
	{
		return int(v - &Level->vertexes[0]);
	}

	inline int Index(side_t *v) const
	{
		return int(v - &Level->sides[0]);
	}

	inline int Index(line_t *v) const
	{
		return int(v - &Level->lines[0]);
	}

	inline int Index(seg_t *v) const
	{
		return int(v - &Level->segs[0]);
	}

	inline int Index(subsector_t *v) const
	{
		return int(v - &Level->subsectors[0]);
	}

	inline int Index(node_t *v) const
	{
		return int(v - &Level->nodes[0]);
	}

	inline int Index(sector_t *v) const
	{
		return int(v - &Level->sectors[0]);
	}

public:
	void LoadMapinfoACSLump();
	void ProcessEDSectors();

	void FloodZones();
	void LoadVertexes(MapData * map);
	bool LoadExtendedNodes(FileReader &dalump, uint32_t id);
	template<class segtype> bool LoadSegs(MapData * map);
	template<class subsectortype, class segtype> bool LoadSubsectors(MapData * map);
	template<class nodetype, class subsectortype> bool LoadNodes(MapData * map);
	bool LoadGLNodes(MapData * map);
	bool CheckCachedNodes(MapData *map);
	bool CheckNodes(MapData * map, bool rebuilt, int buildtime);
	bool CheckForGLNodes();

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
	void ParseTextMap(MapData *map, FMissingTextureTracker &missingtex);
	void SummarizeMissingTextures(const FMissingTextureTracker &missing);
	void SetRenderSector();
	void SpawnSlopeMakers(FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
	void SetSlopes();
	void CopySlopes();

	void SetSubsectorLightmap(const LightmapSurface &surface);
	void SetSideLightmap(const LightmapSurface &surface);
	void LoadLightmap(MapData *map);

	void LoadLevel(MapData *map, const char *lumpname, int position);

	MapLoader(FLevelLocals *lev)
	{
		Level = lev;
	}
};

