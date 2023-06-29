
#include "hw_drawcontext.h"
#include "hw_drawinfo.h"

//==========================================================================
//
// Try to reuse the lists as often as possible as they contain resources that
// are expensive to create and delete.
//
// Note: If multithreading gets used, this class needs synchronization.
//
//==========================================================================

HWDrawInfo* FDrawInfoList::GetNew()
{
	if (mList.Size() > 0)
	{
		HWDrawInfo* di;
		mList.Pop(di);
		return di;
	}
	return new HWDrawInfo(drawctx);
}

void FDrawInfoList::Release(HWDrawInfo* di)
{
	di->ClearBuffers();
	di->Level = nullptr;
	mList.Push(di);
}

//==========================================================================

SortNode* StaticSortNodeArray::GetNew()
{
	if (usecount == TArray<SortNode*>::Size())
	{
		Push(new SortNode);
	}
	return operator[](usecount++);
}

//==========================================================================

HWDrawContext::HWDrawContext() : RenderDataAllocator(1024 * 1024), FakeSectorAllocator(20 * sizeof(sector_t))
{
	di_list.drawctx = this;
}

HWDrawContext::~HWDrawContext()
{
}

void HWDrawContext::ResetRenderDataAllocator()
{
	RenderDataAllocator.FreeAll();
}
