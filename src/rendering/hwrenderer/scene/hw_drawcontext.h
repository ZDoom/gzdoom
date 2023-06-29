#pragma once

#include "common/utility/tarray.h"
#include "hw_clipper.h"
#include "hw_portal.h"

struct HWDrawInfo;
struct SortNode;
struct FDynamicLight;
class HWDrawContext;

class FDrawInfoList
{
public:
	HWDrawContext* drawctx = nullptr;
	TDeletingArray<HWDrawInfo*> mList;

	HWDrawInfo* GetNew();
	void Release(HWDrawInfo*);
};

class StaticSortNodeArray : public TDeletingArray<SortNode*>
{
public:
	unsigned int Size() { return usecount; }
	void Clear() { usecount = 0; }
	void Release(int start) { usecount = start; }
	SortNode* GetNew();

private:
	unsigned int usecount = 0;
};

class HWDrawContext
{
public:
	HWDrawContext();
	~HWDrawContext();

	void ResetRenderDataAllocator();

	FDrawInfoList di_list;
	Clipper staticClipper; // Since all scenes are processed sequentially we only need one clipper.
	HWDrawInfo* gl_drawinfo = nullptr; // This is a linked list of all active DrawInfos and needed to free the memory arena after the last one goes out of scope.

	FMemArena RenderDataAllocator;	// Use large blocks to reduce allocation time.
	StaticSortNodeArray SortNodes;

	sector_t** fakesectorbuffer = nullptr;
	FMemArena FakeSectorAllocator;

	FPortalSceneState portalState;

	TArray<FDynamicLight*> addedLightsArray;
};
