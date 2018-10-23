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
#include "gl/data/gl_viewpointbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/models/gl_models.h"

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
	dl->DoDraw(this, gl_RenderState, true, head->itemindex);
	if (head->equal)
	{
		SortNode * ehead=head->equal;
		while (ehead)
		{
			dl->DoDraw(this, gl_RenderState, true, ehead->itemindex);
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

int FDrawInfo::UploadLights(FDynLightData &data)
{
	return GLRenderer->mLights->UploadLights(data);
}

bool FDrawInfo::SetDepthClamp(bool on)
{
	return gl_RenderState.SetDepthClamp(on);
}

//==========================================================================
//
//
//
//==========================================================================

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

void FDrawInfo::DrawModel(GLSprite *spr, FRenderState &state)
{
	FGLModelRenderer renderer(this, spr->dynlightindex);
	renderer.RenderModel(spr->x, spr->y, spr->z, spr->modelframe, spr->actor, Viewpoint.TicFrac);
}

void FDrawInfo::DrawHUDModel(HUDSprite *huds, FRenderState &state)
{
	FGLModelRenderer renderer(this, huds->lightindex);
	renderer.RenderHUDModel(huds->weapon, huds->mx, huds->my);
}

void FDrawInfo::RenderPortal(IPortal *p, bool usestencil)
{
	auto gp = static_cast<GLPortal *>(p);
	gp->SetupStencil(this, gl_RenderState, usestencil);
	auto new_di = StartDrawInfo(Viewpoint, &VPUniforms);
	new_di->mCurrentPortal = gp;
	gp->DrawContents(new_di);
	new_di->EndDrawInfo();
	GLRenderer->mViewpoints->Bind(vpIndex);
	gp->RemoveStencil(this, gl_RenderState, usestencil);

}

void FDrawInfo::SetDepthMask(bool on)
{
	glDepthMask(on);
}

void FDrawInfo::SetDepthFunc(int func)
{
	static int df2gl[] = { GL_LESS, GL_LEQUAL, GL_ALWAYS };
	glDepthFunc(df2gl[func]);
}

void FDrawInfo::SetDepthRange(float min, float max)
{
	glDepthRange(min, max);
}

void FDrawInfo::EnableDrawBufferAttachments(bool on)
{
	gl_RenderState.EnableDrawBuffers(on? gl_RenderState.GetPassDrawBufferCount() : 1);
}

void FDrawInfo::SetStencil(int offs, int op, int flags)
{
	static int op2gl[] = { GL_KEEP, GL_INCR, GL_DECR };

	glStencilFunc(GL_EQUAL, screen->stencilValue + offs, ~0);		// draw sky into stencil
	glStencilOp(GL_KEEP, GL_KEEP, op2gl[op]);		// this stage doesn't modify the stencil

	bool cmon = !(flags & SF_ColorMaskOff);
	glColorMask(cmon, cmon, cmon, cmon);						// don't write to the graphics buffer
	glDepthMask(!(flags & SF_DepthMaskOff));
	if (flags & SF_DepthTestOff)
		glDisable(GL_DEPTH_TEST);
	else
		glEnable(GL_DEPTH_TEST);
	if (flags & SF_DepthClear)
		glClear(GL_DEPTH_BUFFER_BIT);
}


//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::ClearScreen()
{
	bool multi = !!glIsEnabled(GL_MULTISAMPLE);

	GLRenderer->mViewpoints->Set2D(SCREENWIDTH, SCREENHEIGHT);
	gl_RenderState.SetColor(0, 0, 0);
	gl_RenderState.Apply();

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);

	glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);

	glEnable(GL_DEPTH_TEST);
	if (multi) glEnable(GL_MULTISAMPLE);
}


//==========================================================================
//
// FDrawInfo::AddFlat
//
// Checks texture, lighting and translucency settings and puts this
// plane in the appropriate render list.
//
//==========================================================================

void FDrawInfo::AddFlat(GLFlat *flat, bool fog)
{
	int list;

	if (flat->renderstyle != STYLE_Translucent || flat->alpha < 1.f - FLT_EPSILON || fog || flat->gltexture == nullptr)
	{
		// translucent 3D floors go into the regular translucent list, translucent portals go into the translucent border list.
		list = (flat->renderflags&SSRF_RENDER3DPLANES) ? GLDL_TRANSLUCENT : GLDL_TRANSLUCENTBORDER;
	}
	else if (flat->gltexture->tex->GetTranslucency())
	{
		if (flat->stack)
		{
			list = GLDL_TRANSLUCENTBORDER;
		}
		else if ((flat->renderflags&SSRF_RENDER3DPLANES) && !flat->plane.plane.isSlope())
		{
			list = GLDL_TRANSLUCENT;
		}
		else
		{
			list = GLDL_PLAINFLATS;
		}
	}
	else
	{
		bool masked = flat->gltexture->isMasked() && ((flat->renderflags&SSRF_RENDER3DPLANES) || flat->stack);
		list = masked ? GLDL_MASKEDFLATS : GLDL_PLAINFLATS;
	}
	auto newflat = drawlists[list].NewFlat();
	*newflat = *flat;
}


//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::AddSprite(GLSprite *sprite, bool translucent)
{
	int list;
	// [BB] Allow models to be drawn in the GLDL_TRANSLUCENT pass.
	if (translucent || sprite->actor == nullptr || (!sprite->modelframe && (sprite->actor->renderflags & RF_SPRITETYPEMASK) != RF_WALLSPRITE))
	{
		list = GLDL_TRANSLUCENT;
	}
	else
	{
		list = GLDL_MODELS;
	}

	auto newsprt = drawlists[list].NewSprite();
	*newsprt = *sprite;
}

