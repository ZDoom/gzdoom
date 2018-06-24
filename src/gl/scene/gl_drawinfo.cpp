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
#include "gl/renderer/gl_quaddrawer.h"
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
	dl->DoDraw(this, GLPASS_TRANSLUCENT, head->itemindex, true);
	if (head->equal)
	{
		SortNode * ehead=head->equal;
		while (ehead)
		{
			dl->DoDraw(this, GLPASS_TRANSLUCENT, ehead->itemindex, true);
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


//==========================================================================
//
// Flood gaps with the back side's ceiling/floor texture
// This requires a stencil because the projected plane interferes with
// the depth buffer
//
//==========================================================================

void FDrawInfo::SetupFloodStencil(wallseg * ws)
{
	int recursion = GLRenderer->mPortalState.GetRecursion();

	// Create stencil 
	glStencilFunc(GL_EQUAL, recursion, ~0);		// create stencil
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);		// increment stencil of valid pixels
		// Use revertible color mask, to avoid stomping on anaglyph 3D state
	glColorMask(0, 0, 0, 0);						// don't write to the graphics buffer
	gl_RenderState.EnableTexture(false);
	gl_RenderState.ResetColor();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);

	gl_RenderState.Apply();
	FQuadDrawer qd;
	qd.Set(0, ws->x1, ws->z1, ws->y1, 0, 0);
	qd.Set(1, ws->x1, ws->z2, ws->y1, 0, 0);
	qd.Set(2, ws->x2, ws->z2, ws->y2, 0, 0);
	qd.Set(3, ws->x2, ws->z1, ws->y2, 0, 0);
	qd.Render(GL_TRIANGLE_FAN);

	glStencilFunc(GL_EQUAL, recursion + 1, ~0);		// draw sky into stencil
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil

	glColorMask(1, 1, 1, 1);						// don't write to the graphics buffer
	gl_RenderState.EnableTexture(true);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);
}

void FDrawInfo::ClearFloodStencil(wallseg * ws)
{
	int recursion = GLRenderer->mPortalState.GetRecursion();

	glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
	gl_RenderState.EnableTexture(false);
	// Use revertible color mask, to avoid stomping on anaglyph 3D state
	glColorMask(0, 0, 0, 0);						// don't write to the graphics buffer
	gl_RenderState.ResetColor();

	gl_RenderState.Apply();
	FQuadDrawer qd;
	qd.Set(0, ws->x1, ws->z1, ws->y1, 0, 0);
	qd.Set(1, ws->x1, ws->z2, ws->y1, 0, 0);
	qd.Set(2, ws->x2, ws->z2, ws->y2, 0, 0);
	qd.Set(3, ws->x2, ws->z1, ws->y2, 0, 0);
	qd.Render(GL_TRIANGLE_FAN);

	// restore old stencil op.
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_EQUAL, recursion, ~0);
	gl_RenderState.EnableTexture(true);
	glColorMask(1, 1, 1, 1);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
}

//==========================================================================
//
// Draw the plane segment into the gap
//
//==========================================================================
void FDrawInfo::DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling)
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap Colormap;
	FMaterial * gltexture;

	plane.GetFromSector(sec, ceiling);

	gltexture=FMaterial::ValidateTexture(plane.texture, false, true);
	if (!gltexture) return;

	if (isFullbrightScene()) 
	{
		Colormap.Clear();
		lightlevel=255;
	}
	else
	{
		Colormap = sec->Colormap;
		if (gltexture->tex->isFullbright())
		{
			Colormap.MakeWhite();
			lightlevel=255;
		}
		else lightlevel=abs(ceiling? sec->GetCeilingLight() : sec->GetFloorLight());
	}

	int rel = getExtraLight();
	SetColor(lightlevel, rel, Colormap, 1.0f);
	SetFog(lightlevel, rel, &Colormap, false);
	gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);

	float fviewx = Viewpoint.Pos.X;
	float fviewy = Viewpoint.Pos.Y;
	float fviewz = Viewpoint.Pos.Z;

	gl_RenderState.SetPlaneTextureRotation(&plane, gltexture);
	gl_RenderState.Apply();

	float prj_fac1 = (planez-fviewz)/(ws->z1-fviewz);
	float prj_fac2 = (planez-fviewz)/(ws->z2-fviewz);

	float px1 = fviewx + prj_fac1 * (ws->x1-fviewx);
	float py1 = fviewy + prj_fac1 * (ws->y1-fviewy);

	float px2 = fviewx + prj_fac2 * (ws->x1-fviewx);
	float py2 = fviewy + prj_fac2 * (ws->y1-fviewy);

	float px3 = fviewx + prj_fac2 * (ws->x2-fviewx);
	float py3 = fviewy + prj_fac2 * (ws->y2-fviewy);

	float px4 = fviewx + prj_fac1 * (ws->x2-fviewx);
	float py4 = fviewy + prj_fac1 * (ws->y2-fviewy);

	FQuadDrawer qd;
	qd.Set(0, px1, planez, py1, px1 / 64, -py1 / 64);
	qd.Set(1, px2, planez, py2, px2 / 64, -py2 / 64);
	qd.Set(2, px3, planez, py3, px3 / 64, -py3 / 64);
	qd.Set(3, px4, planez, py4, px4 / 64, -py4 / 64);
	qd.Render(GL_TRIANGLE_FAN);

	gl_RenderState.EnableTextureMatrix(false);
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::FloodUpperGap(seg_t * seg)
{
	wallseg ws;
	sector_t ffake, bfake;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, &ffake, in_area, true);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, &bfake, in_area, false);

	vertex_t * v1, * v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->ceilingplane.ZatPoint(seg->v1);
	double frontz = fakefsector->ceilingplane.ZatPoint(seg->v1);

	if (fakebsector->GetTexture(sector_t::ceiling)==skyflatnum) return;
	if (backz < Viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1=seg->linedef->v1;
		v2=seg->linedef->v2;
	}
	else
	{
		v1=seg->linedef->v2;
		v2=seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z1= frontz;
	ws.z2= backz;

	// Step1: Draw a stencil into the gap
	SetupFloodStencil(&ws);

	// Step2: Project the ceiling plane into the gap
	DrawFloodedPlane(&ws, ws.z2, fakebsector, true);

	// Step3: Delete the stencil
	ClearFloodStencil(&ws);
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::FloodLowerGap(seg_t * seg)
{
	wallseg ws;
	sector_t ffake, bfake;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, &ffake, in_area, true);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, &bfake, in_area, false);

	vertex_t * v1, * v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->floorplane.ZatPoint(seg->v1);
	double frontz = fakefsector->floorplane.ZatPoint(seg->v1);


	if (fakebsector->GetTexture(sector_t::floor) == skyflatnum) return;
	if (fakebsector->GetPlaneTexZ(sector_t::floor) > Viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1=seg->linedef->v1;
		v2=seg->linedef->v2;
	}
	else
	{
		v1=seg->linedef->v2;
		v2=seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z2= frontz;
	ws.z1= backz;

	// Step1: Draw a stencil into the gap
	SetupFloodStencil(&ws);

	// Step2: Project the ceiling plane into the gap
	DrawFloodedPlane(&ws, ws.z1, fakebsector, false);

	// Step3: Delete the stencil
	ClearFloodStencil(&ws);
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



