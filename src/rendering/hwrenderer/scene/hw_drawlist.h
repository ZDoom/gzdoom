#pragma once

#include "memarena.h"

struct HWDrawInfo;
class HWWall;
class HWFlat;
class HWSprite;
class HWDrawContext;
class FRenderState;

//==========================================================================
//
// Intermediate struct to link one draw item into a draw list
//
// unfortunately this struct must not contain pointers because
// the arrays may be reallocated!
//
//==========================================================================

enum HWDrawItemType
{
	DrawType_WALL,
	DrawType_FLAT,
	DrawType_SPRITE,
};

struct HWDrawItem
{
	HWDrawItemType rendertype;
	int index;
	
	HWDrawItem() = default; // we need this for dynamic arrays.
	HWDrawItem(HWDrawItemType _rendertype,int _index) : rendertype(_rendertype),index(_index) {}
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
	HWDrawContext* drawctx = nullptr;
	TArray<HWWall*> walls;
	TArray<HWFlat*> flats;
	TArray<HWSprite*> sprites;
	TArray<HWDrawItem> drawitems;
	int SortNodeStart = 0;
	float SortZ = 0.0f;
	SortNode* sorted = nullptr;
	bool reverseSort = false;
	
	HWDrawList()
	{
		next=nullptr;
		SortNodeStart=-1;
		sorted=nullptr;
	}
	
	~HWDrawList()
	{
		Reset();
	}
	
	unsigned int Size()
	{
		return drawitems.Size();
	}
	
	HWWall *NewWall();
	HWFlat *NewFlat();
	HWSprite *NewSprite();
	void Reset();
	void SortWalls();
	void SortFlats();
	
	
	void MakeSortList();
	SortNode * FindSortPlane(SortNode * head);
	SortNode * FindSortWall(SortNode * head);
	void SortPlaneIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoPlane(HWDrawInfo* di, FRenderState& state, SortNode * head,SortNode * sort);
	void SortSpriteIntoPlane(SortNode * head,SortNode * sort);
	void SortWallIntoWall(HWDrawInfo *di, FRenderState& state, SortNode * head,SortNode * sort);
	void SortSpriteIntoWall(HWDrawInfo *di, FRenderState& state, SortNode * head,SortNode * sort);
	int CompareSprites(SortNode * a,SortNode * b);
	SortNode * SortSpriteList(SortNode * head);
	SortNode * DoSort(HWDrawInfo *di, FRenderState& state, SortNode * head);
	void Sort(HWDrawInfo *di, FRenderState& state);

	void DoDraw(HWDrawInfo *di, FRenderState &state, bool translucent, int i);
	void Draw(HWDrawInfo *di, FRenderState &state, bool translucent);
	void DrawWalls(HWDrawInfo *di, FRenderState &state, bool translucent);
	void DrawFlats(HWDrawInfo *di, FRenderState &state, bool translucent);

	void DrawSorted(HWDrawInfo *di, FRenderState &state, SortNode * head);
	void DrawSorted(HWDrawInfo *di, FRenderState &state);

	HWDrawList *next = nullptr;
} ;


