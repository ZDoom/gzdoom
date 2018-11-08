#pragma once

struct MapData;
class FileReader;

#include "r_defs.h"
#include "nodebuild.h"


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

    const int *oldvertextable;
    int firstglvertex = -1;
    bool format5 = false;

private:
	bool ForceNodeBuild = false;
    
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

    bool LoadBsp(MapData *map);

	
	
};
