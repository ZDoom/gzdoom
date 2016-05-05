#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/scene/gl_wall.h"

enum GLDrawItemType
{
	GLDIT_WALL,
	GLDIT_FLAT,
	GLDIT_SPRITE,
};

enum DrawListType
{
	GLDL_PLAINWALLS,
	GLDL_PLAINFLATS,
	GLDL_MASKEDWALLS,
	GLDL_MASKEDFLATS,
	GLDL_MASKEDWALLSOFS,
	GLDL_MODELS,

	GLDL_TRANSLUCENT,
	GLDL_TRANSLUCENTBORDER,

	GLDL_TYPES,
};

// more lists for handling of dynamic lights
enum DLDrawListType
{
	// These are organized so that the various multipass rendering modes have to be set as few times as possible
	GLLDL_WALLS_PLAIN,			// dynamic lights on normal walls
	GLLDL_WALLS_BRIGHT,			// dynamic lights on brightmapped walls
	GLLDL_WALLS_MASKED,			// dynamic lights on masked midtextures

	GLLDL_FLATS_PLAIN,			// dynamic lights on normal flats
	GLLDL_FLATS_BRIGHT,			// dynamic lights on brightmapped flats
	GLLDL_FLATS_MASKED,			// dynamic lights on masked flats

	GLLDL_WALLS_FOG,			// lights on fogged walls
	GLLDL_WALLS_FOGMASKED,		// lights on fogged masked midtextures

	GLLDL_FLATS_FOG,			// lights on fogged walls
	GLLDL_FLATS_FOGMASKED,		// lights on fogged masked midtextures

	GLLDL_TYPES,
};


enum Drawpasses
{
	GLPASS_ALL,			// Main pass with dynamic lights
	GLPASS_LIGHTSONLY,	// only collect dynamic lights
	GLPASS_PLAIN,		// Main pass without dynamic lights
	GLPASS_DECALS,		// Draws a decal
	GLPASS_TRANSLUCENT,	// Draws translucent objects

	// these are only used with texture based dynamic lights
	GLPASS_BASE,		// untextured base for dynamic lights
	GLPASS_BASE_MASKED,	// same but with active texture
	GLPASS_LIGHTTEX,	// lighttexture pass
	GLPASS_TEXONLY,		// finishing texture pass
	GLPASS_LIGHTTEX_ADDITIVE,	// lighttexture pass (additive)
	GLPASS_LIGHTTEX_FOGGY,	// lighttexture pass on foggy surfaces (forces all lights to be additive)

};

//==========================================================================
//
// Intermediate struct to link one draw item into a draw list
//
// unfortunately this struct must not contain pointers because
// the arrays may be reallocated!
//
//==========================================================================

struct GLDrawItem
{
	GLDrawItemType rendertype;
	int index;

	GLDrawItem(GLDrawItemType _rendertype,int _index) : rendertype(_rendertype),index(_index) {}
};

struct SortNode
{
	int itemindex;
	SortNode * parent;
	SortNode * next;		// unsorted successor
	SortNode * left;		// left side of this node
	SortNode * equal;		// equal to this node
	SortNode * right;		// right side of this node


	void UnlinkFromChain();
	void Link(SortNode * hook);
	void AddToEqual(SortNode * newnode);
	void AddToLeft (SortNode * newnode);
	void AddToRight(SortNode * newnode);
};

//==========================================================================
//
// One draw list. This contains all info for one type of rendering data
//
//==========================================================================

struct GLDrawList
{
//private:
	TArray<GLWall> walls;
	TArray<GLFlat> flats;
	TArray<GLSprite> sprites;
	TArray<GLDrawItem> drawitems;
	int SortNodeStart;
	SortNode * sorted;

public:
	GLDrawList()
	{
		next=NULL;
		SortNodeStart=-1;
		sorted=NULL;
	}

	~GLDrawList()
	{
		Reset();
	}

	unsigned int Size()
	{
		return drawitems.Size();
	}

	void AddWall(GLWall * wall);
	void AddFlat(GLFlat * flat);
	void AddSprite(GLSprite * sprite);
	void Reset();
	void SortWalls();
	void SortFlats();


	void MakeSortList();
	SortNode * FindSortPlane(SortNode * head);
	SortNode * FindSortWall(SortNode * head);
	void SortPlaneIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoPlane(SortNode * head,SortNode * sort);
	void SortSpriteIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoWall(SortNode * head,SortNode * sort);
	void SortSpriteIntoWall(SortNode * head,SortNode * sort);
	int CompareSprites(SortNode * a,SortNode * b);
	SortNode * SortSpriteList(SortNode * head);
	SortNode * DoSort(SortNode * head);
	
	void DoDraw(int pass, int index, bool trans);
	void DoDrawSorted(SortNode * node);
	void DrawSorted();
	void Draw(int pass, bool trans = false);
	void DrawWalls(int pass);
	void DrawFlats(int pass);
	void DrawDecals();
	
	GLDrawList * next;
} ;


//==========================================================================
//
// these are used to link faked planes due to missing textures to a sector
//
//==========================================================================
struct gl_subsectorrendernode
{
	gl_subsectorrendernode *	next;
	subsector_t *				sub;
};


struct FDrawInfo
{
	struct wallseg
	{
		float x1, y1, z1, x2, y2, z2;
	};

	bool temporary;



	struct MissingTextureInfo
	{
		seg_t * seg;
		subsector_t * sub;
		float Planez;
		float Planezfront;
	};

	struct MissingSegInfo
	{
		seg_t * seg;
		int MTI_Index;	// tells us which MissingTextureInfo represents this seg.
	};

	struct SubsectorHackInfo
	{
		subsector_t * sub;
		BYTE flags;
	};

	TArray<BYTE> sectorrenderflags;
	TArray<BYTE> ss_renderflags;
	TArray<BYTE> no_renderflags;

	TArray<MissingTextureInfo> MissingUpperTextures;
	TArray<MissingTextureInfo> MissingLowerTextures;

	TArray<MissingSegInfo> MissingUpperSegs;
	TArray<MissingSegInfo> MissingLowerSegs;

	TArray<SubsectorHackInfo> SubsectorHacks;

	TArray<gl_subsectorrendernode*> otherfloorplanes;
	TArray<gl_subsectorrendernode*> otherceilingplanes;

	TArray<sector_t *> CeilingStacks;
	TArray<sector_t *> FloorStacks;

	TArray<subsector_t *> HandledSubsectors;

	FDrawInfo * next;
	GLDrawList drawlists[GLDL_TYPES];
	GLDrawList *dldrawlists = NULL;	// only gets allocated when needed.

	FDrawInfo();
	~FDrawInfo();
	void ClearBuffers();

	bool DoOneSectorUpper(subsector_t * subsec, float planez);
	bool DoOneSectorLower(subsector_t * subsec, float planez);
	bool DoFakeBridge(subsector_t * subsec, float planez);
	bool DoFakeCeilingBridge(subsector_t * subsec, float planez);

	bool CheckAnchorFloor(subsector_t * sub);
	bool CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor);
	bool CheckAnchorCeiling(subsector_t * sub);
	bool CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor);

	void AddUpperMissingTexture(side_t * side, subsector_t *sub, float backheight);
	void AddLowerMissingTexture(side_t * side, subsector_t *sub, float backheight);
	void HandleMissingTextures();
	void DrawUnhandledMissingTextures();
	void AddHackedSubsector(subsector_t * sub);
	void HandleHackedSubsectors();
	void AddFloorStack(sector_t * sec);
	void AddCeilingStack(sector_t * sec);
	void ProcessSectorStacks();

	void AddOtherFloorPlane(int sector, gl_subsectorrendernode * node);
	void AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node);

	void StartScene();
	void SetupFloodStencil(wallseg * ws);
	void ClearFloodStencil(wallseg * ws);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg);
	void FloodLowerGap(seg_t * seg);

	static void StartDrawInfo();
	static void EndDrawInfo();

	gl_subsectorrendernode * GetOtherFloorPlanes(unsigned int sector)
	{
		if (sector<otherfloorplanes.Size()) return otherfloorplanes[sector];
		else return NULL;
	}

	gl_subsectorrendernode * GetOtherCeilingPlanes(unsigned int sector)
	{
		if (sector<otherceilingplanes.Size()) return otherceilingplanes[sector];
		else return NULL;
	}
};

class FDrawInfoList
{
	TDeletingArray<FDrawInfo *> mList;

public:

	FDrawInfo *GetNew();
	void Release(FDrawInfo *);
};


extern FDrawInfo * gl_drawinfo;

void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture);
void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

#endif