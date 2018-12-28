#pragma once

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


struct FMissingCount
{
	int Count = 0;
};

typedef TMap<FString,FMissingCount> FMissingTextureTracker;
struct FLevelLocals;

class MapLoader
{
	friend class UDMFParser;
	void *level;	// this is to hide the global variable and produce an error for referencing it.
	FLevelLocals *Level;

	int firstglvertex;	// helpers for loading GL nodes from GWA files.
	bool format5;

	TArray<vertexdata_t> vertexdatas;

	TMap<unsigned, unsigned>  MapThingsUserDataIndex;	// from mapthing idx -> user data idx
	TArray<FUDMFKey> MapThingsUserData;
	int sidecount;
	TArray<int>		linemap;

	TMap<int, EDLinedef> EDLines;
	TMap<int, EDSector> EDSectors;
	TMap<int, EDMapthing> EDThings;

	void SlopeLineToPoint(int lineid, const DVector3 &pos, bool slopeCeil);
	void CopyPlane(int tag, sector_t *dest, bool copyCeil);
	void CopyPlane(int tag, const DVector2 &pos, bool copyCeil);
	void SetSlope(secplane_t *plane, bool setCeil, int xyangi, int zangi, const DVector3 &pos);
	void VavoomSlope(sector_t * sec, int id, const DVector3 &pos, int which);
	void SetSlopesFromVertexHeights(FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
	void AlignPlane(sector_t *sec, line_t *line, int which);

	void InitED();
	void ProcessEDMapthing(FMapThing *mt, int recordnum);
	void ProcessEDLinedef(line_t *line, int recordnum);
	void ProcessEDSector(sector_t *sec, int recordnum);

	int checkGLVertex(int num);
	int checkGLVertex3(int num);
	int CheckForMissingSegs();
	bool LoadGLVertexes(FileReader &lump);
	bool LoadGLSegs(FileReader &lump);
	bool LoadGLSubsectors(FileReader &lump);
	bool LoadNodes(FileReader &lump);
	bool DoLoadGLNodes(FileReader * lumps);
	void CreateCachedNodes(MapData *map);

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
	void SetMapThingUserData(AActor *actor, unsigned udi);
	void CreateBlockMap();

public:
	void LoadMapinfoACSLump();
	void ProcessEDSectors();

	void FloodZones();
	void LoadVertexes(MapData * map);
	void LoadExtendedNodes(FileReader &dalump, uint32_t id);
	template<class segtype> void LoadSegs(MapData * map);
	template<class subsectortype, class segtype> void LoadSubsectors(MapData * map);
	template<class nodetype, class subsectortype> void LoadNodes(MapData * map);
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

	void LoadLevel(MapData *map, const char *lumpname, int position);

	MapLoader(FLevelLocals *lev)
	{
		Level = lev;
	}
};

