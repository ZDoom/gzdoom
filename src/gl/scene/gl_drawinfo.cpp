// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_drawinfo.cpp
** Implements the draw info structure which contains most of the
** data in a scene and the draw lists - including a very thorough BSP 
** style sorting algorithm for translucent objects.
**
*/

#include "gl_load/gl_system.h"
#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "tarray.h"
#include "hwrenderer/scene/hw_drawstructs.h"

#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "gl/scene/gl_portal.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/dynlights/gl_lightbuffer.h"

class FDrawInfoList
{
public:
	TDeletingArray<FDrawInfo *> mList;


	FDrawInfo * GetNew();
	void Release(FDrawInfo *);
};


static FDrawInfo * gl_drawinfo;
FDrawInfoList di_list;

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::DoDrawSorted(HWDrawList *dl, SortNode * head)
{
	float clipsplit[2];
	int relation = 0;
	float z = 0.f;

	gl_RenderState.GetClipSplit(clipsplit);

	if (dl->drawitems[head->itemindex].rendertype == GLDIT_FLAT)
	{
		z = dl->flats[dl->drawitems[head->itemindex].index]->z;
		relation = z > Viewpoint.Pos.Z ? 1 : -1;
	}


	// left is further away, i.e. for stuff above viewz its z coordinate higher, for stuff below viewz its z coordinate is lower
	if (head->left) 
	{
		if (relation == -1)
		{
			gl_RenderState.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == 1)
		{
			gl_RenderState.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DoDrawSorted(dl, head->left);
		gl_RenderState.SetClipSplit(clipsplit);
	}
	dl->DoDraw(this, gl_RenderState, true, GLPASS_TRANSLUCENT, head->itemindex, true);
	if (head->equal)
	{
		SortNode * ehead=head->equal;
		while (ehead)
		{
			dl->DoDraw(this, gl_RenderState, true, GLPASS_TRANSLUCENT, ehead->itemindex, true);
			ehead=ehead->equal;
		}
	}
	// right is closer, i.e. for stuff above viewz its z coordinate is lower, for stuff below viewz its z coordinate is higher
	if (head->right)
	{
		if (relation == 1)
		{
			gl_RenderState.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == -1)
		{
			gl_RenderState.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DoDrawSorted(dl, head->right);
		gl_RenderState.SetClipSplit(clipsplit);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::DrawSorted(int listindex)
{
	HWDrawList *dl = &drawlists[listindex];
	if (dl->drawitems.Size()==0) return;

	if (!dl->sorted)
	{
		GLRenderer->mVBO->Map();
		dl->Sort(this);
		GLRenderer->mVBO->Unmap();
	}
	gl_RenderState.ClearClipSplit();
	if (!(gl.flags & RFL_NO_CLIP_PLANES))
	{
		glEnable(GL_CLIP_DISTANCE1);
		glEnable(GL_CLIP_DISTANCE2);
	}
	DoDrawSorted(dl, dl->sorted);
	if (!(gl.flags & RFL_NO_CLIP_PLANES))
	{
		glDisable(GL_CLIP_DISTANCE1);
		glDisable(GL_CLIP_DISTANCE2);
	}
	gl_RenderState.ClearClipSplit();
}

//==========================================================================
//
// Try to reuse the lists as often as possible as they contain resources that
// are expensive to create and delete.
//
// Note: If multithreading gets used, this class needs synchronization.
//
//==========================================================================

FDrawInfo *FDrawInfoList::GetNew()
{
	if (mList.Size() > 0)
	{
		FDrawInfo *di;
		mList.Pop(di);
		return di;
	}
	return new FDrawInfo;
}

void FDrawInfoList::Release(FDrawInfo * di)
{
	di->ClearBuffers();
	mList.Push(di);
}

//==========================================================================
//
// Sets up a new drawinfo struct
//
//==========================================================================

// OpenGL has no use for multiple clippers so use the same one for all DrawInfos.
static Clipper staticClipper;

FDrawInfo *FDrawInfo::StartDrawInfo(FRenderViewpoint &parentvp, HWViewpointUniforms *uniforms)
{
	FDrawInfo *di=di_list.GetNew();
	di->mVBO = GLRenderer->mVBO;
	di->mClipper = &staticClipper;
	di->Viewpoint = parentvp;
	if (uniforms)
	{
		di->VPUniforms = *uniforms;
		// The clip planes will never be inherited from the parent drawinfo.
		di->VPUniforms.mClipLine.X = -1000001.f;
		di->VPUniforms.mClipHeight = 0;
	}
	else di->VPUniforms.SetDefaults();
    di->mClipper->SetViewpoint(di->Viewpoint);
	staticClipper.Clear();
	di->StartScene();
	return di;
}

void FDrawInfo::StartScene()
{
	ClearBuffers();

	outer = gl_drawinfo;
	gl_drawinfo = this;
	for (int i = 0; i < GLDL_TYPES; i++) drawlists[i].Reset();
	decals[0].Clear();
	decals[1].Clear();
	hudsprites.Clear();
	vpIndex = 0;

	// Fullbright information needs to be propagated from the main view.
	if (outer != nullptr) FullbrightFlags = outer->FullbrightFlags;
	else FullbrightFlags = 0;

}

//==========================================================================
//
//
//
//==========================================================================
FDrawInfo *FDrawInfo::EndDrawInfo()
{
	assert(this == gl_drawinfo);
	for(int i=0;i<GLDL_TYPES;i++) drawlists[i].Reset();
	gl_drawinfo=static_cast<FDrawInfo*>(outer);
	di_list.Release(this);
	if (gl_drawinfo == nullptr) 
		ResetRenderDataAllocator();
	return gl_drawinfo;
}

// Same here for the dependency on the portal.
void FDrawInfo::AddSubsectorToPortal(FSectorPortalGroup *ptg, subsector_t *sub)
{
	auto portal = FindPortal(ptg);
	if (!portal)
	{
		portal = new GLScenePortal(&GLRenderer->mPortalState, new HWSectorStackPortal(ptg));
		Portals.Push(portal);
	}
	auto ptl = static_cast<HWSectorStackPortal*>(static_cast<GLScenePortal*>(portal)->mScene);
	ptl->AddSubsector(sub);
}

std::pair<FFlatVertex *, unsigned int> FDrawInfo::AllocVertices(unsigned int count)
{
	unsigned int index = -1;
	auto p = GLRenderer->mVBO->Alloc(count, &index);
	return std::make_pair(p, index);
}

GLDecal *FDrawInfo::AddDecal(bool onmirror)
{
	auto decal = (GLDecal*)RenderDataAllocator.Alloc(sizeof(GLDecal));
	decals[onmirror ? 1 : 0].Push(decal);
	return decal;
}

int FDrawInfo::UploadLights(FDynLightData &data)
{
	return GLRenderer->mLights->UploadLights(data);
}

bool FDrawInfo::SetDepthClamp(bool on)
{
	return gl_RenderState.SetDepthClamp(on);
}

static int dt2gl[] = { GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP };

void FDrawInfo::Draw(EDrawType dt, FRenderState &state, int index, int count, bool apply)
{
	assert(&state == &gl_RenderState);
	if (apply)
	{
		gl_RenderState.Apply();
		gl_RenderState.ApplyLightIndex(-2);
	}
	drawcalls.Clock();
	glDrawArrays(dt2gl[dt], index, count);
	drawcalls.Unclock();
}

void FDrawInfo::DrawIndexed(EDrawType dt, FRenderState &state, int index, int count, bool apply)
{
	assert(&state == &gl_RenderState);
	if (apply)
	{
		gl_RenderState.Apply();
		gl_RenderState.ApplyLightIndex(-2);
	}
	drawcalls.Clock();
	glDrawElements(dt2gl[dt], count, GL_UNSIGNED_INT, GLRenderer->mVBO->GetIndexPointer() + index);
	drawcalls.Unclock();
}



