#pragma once

struct MapData;
class FileReader;

#include "r_defs.h"
#include "nodebuild.h"
#include "g_level.h"

struct FLevelLocals;
class FTagManager;

struct sidei_t    // [RH] Only keep BOOM sidedef init stuff around for init
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

class MapLoader
{
public:
	TArray<vertex_t> &vertexes;
	TArray<line_t> &lines;
	TArray<side_t> &sides;
	TArray<sector_t> &sectors;
	TArray<seg_t> &segs;
	TArray<subsector_t> &subsectors;
	TArray<node_t> &nodes;
    TArray<subsector_t> &gamesubsectors;
    TArray<node_t> &gamenodes;

    const int *oldvertextable = nullptr;
    int firstglvertex = -1;
    bool format5 = false;
    int maptype = MAPTYPE_UNKNOWN;
    TArray<sidei_t> sidetemp;
    TArray<int> linemap;

//private:
    FLevelLocals *level = nullptr;
	bool ForceNodeBuild = false;
    FTagManager *tagManager = nullptr;
    int sidecount = 0;

    // maploader_bsp.cpp
    int CheckForMissingSegs();
    bool CheckForGLNodes();
    void LoadZSegs (FileReader &data);
    void LoadGLZSegs (FileReader &data, int type);
    void LoadZNodes(FileReader &data, int glnodes);
    void LoadZNodes (FileReader &dalump, uint32_t id);
    bool CheckV4Nodes(MapData *map);
    template<class segtype> void LoadSegs (MapData * map);
    template<class subsectortype, class segtype> void LoadSubsectors (MapData * map);
    template<class nodetype, class subsectortype> void LoadNodes (MapData * map);
    void GetPolySpots (MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors);
    bool CheckNodes(MapData * map, bool rebuilt, int buildtime);
    void CreateCachedNodes(MapData *map);
    bool CheckCachedNodes(MapData *map);
    
    // maploader_glnodes.cpp
    bool LoadGLVertexes(FileReader &lump);
    int checkGLVertex(int num);
    int checkGLVertex3(int num);
    bool LoadGLSegs(FileReader &lump);
    bool LoadGLSubsectors(FileReader &lump);
    bool LoadGLNodeLump (FileReader &lump);
    bool DoLoadGLNodes(FileReader * lumps);
    bool LoadGLNodes(MapData * map);
    
    // maploader.cpp
    void SetLineID (int i, line_t *ld);
    void SaveLineSpecial (line_t *ld);
    void FinishLoadingLineDef(line_t *ld, int alpha);
    void SetSideNum (side_t **sidenum_p, uint16_t sidenum);
    void AllocateSideDefs (MapData *map, int count);
    int DetermineTranslucency (int lumpnum);
    void SetTexture (sector_t *sector, int index, int position, const char *name, FMissingTextureTracker &track, bool truncate);
    void SetTexture (side_t *side, int position, const char *name, FMissingTextureTracker &track);
    void SetTexture (side_t *side, int position, uint32_t *blend, const char *name);
    void SetTextureNoErr (side_t *side, int position, uint32_t *color, const char *name, bool *validcolor, bool isFog);
    void ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, intmapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &missingtex);



public:
    template<class T>MapLoader(T &store)
    : vertexes(store.vertexes),
      lines(store.lines),
      sides(store.sides),
      sectors(store.sectors),
      segs(store.segs),
      subsectors(store.subsectors),
      nodes(store.nodes),
      gamesubsectors(store.gamesubsectors),
      gamenodes(store.gamenodes)
    {
        
    }
    
    ~MapLoader()
    {
        if (oldvertextable) delete[] oldvertextable;
    }
	
    void LoadVertexes (MapData * map);
    void LoadSectors (MapData *map, FMissingTextureTracker &missingtex);
    void LoadLineDefs (MapData * map);
    void LoadLineDefs2 (MapData * map);
    void FinishLoadingLineDefs ();
    void LoadSideDefs2 (MapData *map, FMissingTextureTracker &missingtex);
    void LoopSidedefs (bool firstloop);

    bool LoadBsp(MapData *map);

    void ParseTextMap(MapData *map, FMissingTextureTracker &missingtex);

	
};
