#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/scene/gl_wall.h"
#include "hwrenderer/scene/hw_drawinfo.h"

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
	GLLDL_WALLS_MASKED,			// dynamic lights on masked midtextures
	
	GLLDL_FLATS_PLAIN,			// dynamic lights on normal flats
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
	TArray<GLWall*> walls;
	TArray<GLFlat*> flats;
	TArray<GLSprite*> sprites;
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
	
	GLWall *NewWall();
	GLFlat *NewFlat();
	GLSprite *NewSprite();
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


struct FDrawInfo : public HWDrawInfo
{
	
	GLSceneDrawer *mDrawer;
	
	
	FDrawInfo * next;
	GLDrawList drawlists[GLDL_TYPES];
	GLDrawList *dldrawlists = NULL;	// only gets allocated when needed.
	
	FDrawInfo();
	~FDrawInfo();
	
	void AddWall(GLWall *wall) override;
    void AddMirrorSurface(GLWall *w) override;
    void ProcessActorsInPortal(FLinePortalSpan *glport) override;

	// Legacy GL only. 
	bool PutWallCompat(GLWall *wall, int passflag);
	void RenderFogBoundaryCompat(GLWall *wall);
	void RenderLightsCompat(GLWall *wall, int pass);

	void DrawDecal(GLWall *wall, DBaseDecal *decal);
	void DoDrawDecals(GLWall *wall);

	
	void StartScene();
	void SetupFloodStencil(wallseg * ws);
	void ClearFloodStencil(wallseg * ws);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg) override;
	void FloodLowerGap(seg_t * seg) override;
	
	// These two may be moved to the API independent part of the renderer later.
	void ProcessLowerMinisegs(TArray<seg_t *> &lowersegs) override;
	void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub) override;

	static void StartDrawInfo(GLSceneDrawer *drawer);
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
