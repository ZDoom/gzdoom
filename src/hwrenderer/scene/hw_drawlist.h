#pragma once

#include "memarena.h"

extern FMemArena RenderDataAllocator;
void ResetRenderDataAllocator();
struct HWDrawInfo;
class GLWall;
class GLFlat;
class GLSprite;
class FRenderState;

//==========================================================================
//
// Intermediate struct to link one draw item into a draw list
//
// unfortunately this struct must not contain pointers because
// the arrays may be reallocated!
//
//==========================================================================

enum GLDrawItemType
{
	GLDIT_WALL,
	GLDIT_FLAT,
	GLDIT_SPRITE,
};

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

struct HWDrawList
{
	//private:
	TArray<GLWall*> walls;
	TArray<GLFlat*> flats;
	TArray<GLSprite*> sprites;
	TArray<GLDrawItem> drawitems;
	int SortNodeStart;
    float SortZ;
	SortNode * sorted;
	
public:
	HWDrawList()
	{
		next=NULL;
		SortNodeStart=-1;
		sorted=NULL;
	}
	
	~HWDrawList()
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
	void SortWallIntoWall(HWDrawInfo *di, SortNode * head,SortNode * sort);
	void SortSpriteIntoWall(HWDrawInfo *di, SortNode * head,SortNode * sort);
	int CompareSprites(SortNode * a,SortNode * b);
	SortNode * SortSpriteList(SortNode * head);
	SortNode * DoSort(HWDrawInfo *di, SortNode * head);
	void Sort(HWDrawInfo *di);

	void DoDraw(HWDrawInfo *di, FRenderState &state, bool translucent, int i);
	void Draw(HWDrawInfo *di, FRenderState &state, bool translucent);
	void DrawWalls(HWDrawInfo *di, FRenderState &state, bool translucent);
	void DrawFlats(HWDrawInfo *di, FRenderState &state, bool translucent);

	void DrawSorted(HWDrawInfo *di, FRenderState &state, SortNode * head);
	void DrawSorted(HWDrawInfo *di, FRenderState &state);

	HWDrawList * next;
} ;


