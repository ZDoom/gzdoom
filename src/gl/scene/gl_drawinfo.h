#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/scene/gl_wall.h"

enum GLDrawItemType
{
	GLDIT_WALL,
	GLDIT_FLAT,
	GLDIT_SPRITE,
	GLDIT_POLY,
};

enum DrawListType
{
	// These are organized so that the various multipass rendering modes
	// have to be set as few times as possible
	GLDL_LIGHT,	
	GLDL_LIGHTBRIGHT,
	GLDL_LIGHTMASKED,
	GLDL_LIGHTFOG,
	GLDL_LIGHTFOGMASKED,

	GLDL_PLAIN,
	GLDL_MASKED,
	GLDL_FOG,
	GLDL_FOGMASKED,

	GLDL_TRANSLUCENT,
	GLDL_TRANSLUCENTBORDER,

	GLDL_TYPES,

	GLDL_FIRSTLIGHT = GLDL_LIGHT,
	GLDL_LASTLIGHT = GLDL_LIGHTFOGMASKED,
	GLDL_FIRSTNOLIGHT = GLDL_PLAIN,
	GLDL_LASTNOLIGHT = GLDL_FOGMASKED,
};

enum Drawpasses
{
	GLPASS_BASE,		// Draws the untextured surface only
	GLPASS_BASE_MASKED,	// Draws an untextured surface that is masked by the texture
	GLPASS_PLAIN,		// Draws a texture that isn't affected by dynamic lights with sector light settings
	GLPASS_LIGHT,		// Draws dynamic lights
	GLPASS_LIGHT_ADDITIVE,	// Draws additive dynamic lights
	GLPASS_TEXTURE,		// Draws the texture to be modulated with the light information on the base surface
	GLPASS_DECALS,		// Draws a decal
	GLPASS_DECALS_NOFOG,// Draws a decal without setting the fog (used for passes that need a fog layer)
	GLPASS_TRANSLUCENT,	// Draws translucent objects
	GLPASS_ALL			// Everything at once, using shaders for dynamic lights
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

	void AddWall(GLWall * wall);
	void AddFlat(GLFlat * flat);
	void AddSprite(GLSprite * sprite);
	void Reset();
	void Sort();


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
	
	void DoDraw(int pass, int index);
	void DoDrawSorted(SortNode * node);
	void DrawSorted();
	void Draw(int pass);
	
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
		fixed_t planez;
		fixed_t planezfront;
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

	FDrawInfo();
	~FDrawInfo();
	void ClearBuffers();

	bool DoOneSectorUpper(subsector_t * subsec, fixed_t planez);
	bool DoOneSectorLower(subsector_t * subsec, fixed_t planez);
	bool DoFakeBridge(subsector_t * subsec, fixed_t planez);
	bool DoFakeCeilingBridge(subsector_t * subsec, fixed_t planez);

	bool CheckAnchorFloor(subsector_t * sub);
	bool CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor);
	bool CheckAnchorCeiling(subsector_t * sub);
	bool CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor);

	void AddUpperMissingTexture(side_t * side, subsector_t *sub, fixed_t backheight);
	void AddLowerMissingTexture(side_t * side, subsector_t *sub, fixed_t backheight);
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

bool gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture);
void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

#endif