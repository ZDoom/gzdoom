// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
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
** gl_portal.cpp
**   Generalized portal maintenance classes for skyboxes, horizons etc.
**
*/

#include "gl_load/gl_system.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "a_sharedglobal.h"
#include "r_sky.h"
#include "p_maputl.h"
#include "d_player.h"
#include "g_levellocals.h"

#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "gl/scene/gl_portal.h"
#include "gl/stereo3d/scoped_color_mask.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// General portal handling code
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

EXTERN_CVAR(Bool, gl_portals)
EXTERN_CVAR(Bool, gl_noquery)
EXTERN_CVAR(Int, r_mirror_recursions)

TArray<GLPortal *> GLPortal::portals;
TArray<float> GLPortal::planestack;
int GLPortal::recursion;
int GLPortal::MirrorFlag;
int GLPortal::PlaneMirrorFlag;
int GLPortal::renderdepth;
int GLPortal::PlaneMirrorMode;
GLuint GLPortal::QueryObject;

bool	 GLPortal::inskybox;

UniqueList<GLSkyInfo> UniqueSkies;
UniqueList<GLHorizonInfo> UniqueHorizons;
UniqueList<secplane_t> UniquePlaneMirrors;

int skyboxrecursion = 0;

//==========================================================================
//
//
//
//==========================================================================

void GLPortal::BeginScene()
{
	UniqueSkies.Clear();
	UniqueHorizons.Clear();
	UniquePlaneMirrors.Clear();
}

//==========================================================================
//
//
//
//==========================================================================
void GLPortal::ClearScreen(FDrawInfo *di)
{
	bool multi = !!glIsEnabled(GL_MULTISAMPLE);

	di->VPUniforms.mViewMatrix.loadIdentity();
	di->VPUniforms.mProjectionMatrix.ortho(0, SCREENWIDTH, SCREENHEIGHT, 0, -1.0f, 1.0f);
	di->ApplyVPUniforms();
	gl_RenderState.SetColor(0, 0, 0);
	gl_RenderState.Apply();

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);

	glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);

	glEnable(GL_DEPTH_TEST);
	if (multi) glEnable(GL_MULTISAMPLE);
}

//-----------------------------------------------------------------------------
//
// DrawPortalStencil
//
//-----------------------------------------------------------------------------
void GLPortal::DrawPortalStencil()
{
	if (mPrimIndices.Size() == 0)
	{
		mPrimIndices.Resize(2 * lines.Size());

		for (unsigned int i = 0; i < lines.Size(); i++)
		{
			mPrimIndices[i * 2] = lines[i].vertindex;
			mPrimIndices[i * 2 + 1] = lines[i].vertcount;
		}
	}
	gl_RenderState.Apply();
	for (unsigned int i = 0; i < mPrimIndices.Size(); i += 2)
	{
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, mPrimIndices[i], mPrimIndices[i + 1]);
	}
	if (NeedCap() && lines.Size() > 1)
	{
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, FFlatVertexBuffer::STENCILTOP_INDEX, 4);
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, FFlatVertexBuffer::STENCILBOTTOM_INDEX, 4);
	}
}






//-----------------------------------------------------------------------------
//
// Start
//
//-----------------------------------------------------------------------------

bool GLPortal::Start(bool usestencil, bool doquery, FDrawInfo *outer_di, FDrawInfo **pDi)
{
	*pDi = nullptr;
	rendered_portals++;
	Clocker c(PortalAll);

	if (usestencil)
	{
		if (!gl_portals) 
		{
			return false;
		}
	
		// Create stencil 
		glStencilFunc(GL_EQUAL,recursion,~0);		// create stencil
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);		// increment stencil of valid pixels
		{
			ScopedColorMask colorMask(0, 0, 0, 0); // glColorMask(0,0,0,0);						// don't write to the graphics buffer
			gl_RenderState.SetEffect(EFF_STENCIL);
			gl_RenderState.EnableTexture(false);
			gl_RenderState.ResetColor();
			glDepthFunc(GL_LESS);
			gl_RenderState.Apply();

			if (NeedDepthBuffer())
			{
				glDepthMask(false);							// don't write to Z-buffer!
				if (!NeedDepthBuffer()) doquery = false;		// too much overhead and nothing to gain.
				else if (gl_noquery) doquery = false;

				// If occlusion query is supported let's use it to avoid rendering portals that aren't visible
				if (QueryObject)
				{
					glBeginQuery(GL_SAMPLES_PASSED, QueryObject);
				}
				else doquery = false;	// some kind of error happened

				DrawPortalStencil();

				glEndQuery(GL_SAMPLES_PASSED);

				// Clear Z-buffer
				glStencilFunc(GL_EQUAL, recursion + 1, ~0);		// draw sky into stencil
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil
				glDepthMask(true);							// enable z-buffer again
				glDepthRange(1, 1);
				glDepthFunc(GL_ALWAYS);
				DrawPortalStencil();

				// set normal drawing mode
				gl_RenderState.EnableTexture(true);
				glDepthFunc(GL_LESS);
				// glColorMask(1, 1, 1, 1);
				gl_RenderState.SetEffect(EFF_NONE);
				glDepthRange(0, 1);

				GLuint sampleCount;

				if (QueryObject)
				{
					glGetQueryObjectuiv(QueryObject, GL_QUERY_RESULT, &sampleCount);

					if (sampleCount == 0) 	// not visible
					{
						// restore default stencil op.
						glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
						glStencilFunc(GL_EQUAL, recursion, ~0);		// draw sky into stencil
						return false;
					}
				}
			}
			else
			{
				// No z-buffer is needed therefore we can skip all the complicated stuff that is involved
				// No occlusion queries will be done here. For these portals the overhead is far greater
				// than the benefit.
				// Note: We must draw the stencil with z-write enabled here because there is no second pass!

				glDepthMask(true);
				DrawPortalStencil();
				glStencilFunc(GL_EQUAL, recursion + 1, ~0);		// draw sky into stencil
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil
				gl_RenderState.EnableTexture(true);
				// glColorMask(1,1,1,1);
				gl_RenderState.SetEffect(EFF_NONE);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(false);							// don't write to Z-buffer!
			}
		}
		recursion++;


	}
	else
	{
		if (!NeedDepthBuffer())
		{
			glDepthMask(false);
			glDisable(GL_DEPTH_TEST);
		}
	}
	*pDi = FDrawInfo::StartDrawInfo(outer_di->Viewpoint, &outer_di->VPUniforms);

	// save viewpoint
	savedvisibility = outer_di->Viewpoint.camera ? outer_di->Viewpoint.camera->renderflags & RF_MAYBEINVISIBLE : ActorRenderFlags::FromInt(0);


	PrevPortal = GLRenderer->mCurrentPortal;
	GLRenderer->mCurrentPortal = this;

	if (PrevPortal != nullptr) PrevPortal->PushState();
	return true;
}


inline void GLPortal::ClearClipper(FDrawInfo *di)
{
	auto outer_di = di->next;
	DAngle angleOffset = deltaangle(outer_di->Viewpoint.Angles.Yaw, di->Viewpoint.Angles.Yaw);

	di->mClipper->Clear();

	// Set the clipper to the minimal visible area
	di->mClipper->SafeAddClipRange(0,0xffffffff);
	for (unsigned int i = 0; i < lines.Size(); i++)
	{
		DAngle startAngle = (DVector2(lines[i].glseg.x2, lines[i].glseg.y2) - outer_di->Viewpoint.Pos).Angle() + angleOffset;
		DAngle endAngle = (DVector2(lines[i].glseg.x1, lines[i].glseg.y1) - outer_di->Viewpoint.Pos).Angle() + angleOffset;

		if (deltaangle(endAngle, startAngle) < 0)
		{
			di->mClipper->SafeRemoveClipRangeRealAngles(startAngle.BAMs(), endAngle.BAMs());
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = di->FrustumAngle();
	if (a1 < ANGLE_180) di->mClipper->SafeAddClipRangeRealAngles(di->Viewpoint.Angles.Yaw.BAMs() + a1, di->Viewpoint.Angles.Yaw.BAMs() - a1);

	// lock the parts that have just been clipped out.
	di->mClipper->SetSilhouette();
}

//-----------------------------------------------------------------------------
//
// End
//
//-----------------------------------------------------------------------------
void GLPortal::End(FDrawInfo *di, bool usestencil)
{
	bool needdepth = NeedDepthBuffer();

	Clocker c(PortalAll);
	if (PrevPortal != nullptr) PrevPortal->PopState();
	GLRenderer->mCurrentPortal = PrevPortal;

	di = di->EndDrawInfo();
	di->ApplyVPUniforms();
	if (usestencil)
	{
		auto &vp = di->Viewpoint;

		// Restore the old view
		if (vp.camera != nullptr) vp.camera->renderflags = (vp.camera->renderflags & ~RF_MAYBEINVISIBLE) | savedvisibility;
		di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));

		{
			ScopedColorMask colorMask(0, 0, 0, 0); // glColorMask(0, 0, 0, 0);						// no graphics
			gl_RenderState.SetEffect(EFF_NONE);
			gl_RenderState.ResetColor();
			gl_RenderState.EnableTexture(false);
			gl_RenderState.Apply();

			if (needdepth)
			{
				// first step: reset the depth buffer to max. depth
				glDepthRange(1, 1);							// always
				glDepthFunc(GL_ALWAYS);						// write the farthest depth value
				DrawPortalStencil();
			}
			else
			{
				glEnable(GL_DEPTH_TEST);
			}

			// second step: restore the depth buffer to the previous values and reset the stencil
			glDepthFunc(GL_LEQUAL);
			glDepthRange(0, 1);
			glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
			glStencilFunc(GL_EQUAL, recursion, ~0);		// draw sky into stencil
			DrawPortalStencil();
			glDepthFunc(GL_LESS);


			gl_RenderState.EnableTexture(true);
			gl_RenderState.SetEffect(EFF_NONE);
		}  // glColorMask(1, 1, 1, 1);
		recursion--;

		// restore old stencil op.
		glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
		glStencilFunc(GL_EQUAL,recursion,~0);		// draw sky into stencil
	}
	else
	{
		if (needdepth) 
		{
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glEnable(GL_DEPTH_TEST);
			glDepthMask(true);
		}
		auto &vp = di->Viewpoint;

		// Restore the old view
		if (vp.camera != nullptr) vp.camera->renderflags = (vp.camera->renderflags & ~RF_MAYBEINVISIBLE) | savedvisibility;
		di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));

		// This draws a valid z-buffer into the stencil's contents to ensure it
		// doesn't get overwritten by the level's geometry.

		gl_RenderState.ResetColor();
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0, 1);
		{
			ScopedColorMask colorMask(0, 0, 0, 1); // mark portal in alpha channel but don't touch color
			gl_RenderState.SetEffect(EFF_STENCIL);
			gl_RenderState.EnableTexture(false);
			gl_RenderState.BlendFunc(GL_ONE, GL_ZERO);
			gl_RenderState.BlendEquation(GL_FUNC_ADD);
			gl_RenderState.Apply();
			DrawPortalStencil();
			gl_RenderState.SetEffect(EFF_NONE);
			gl_RenderState.EnableTexture(true);
		}
		glDepthFunc(GL_LESS);
	}
}


//-----------------------------------------------------------------------------
//
// StartFrame
//
//-----------------------------------------------------------------------------
void GLPortal::StartFrame()
{
	GLPortal * p = nullptr;
	portals.Push(p);
	if (renderdepth == 0)
	{
		inskybox = false;
		screen->instack[sector_t::floor] = screen->instack[sector_t::ceiling] = 0;
	}
	renderdepth++;
}


//-----------------------------------------------------------------------------
//
// printing portal info
//
//-----------------------------------------------------------------------------

static bool gl_portalinfo;

CCMD(gl_portalinfo)
{
	gl_portalinfo = true;
}

static FString indent;

//-----------------------------------------------------------------------------
//
// EndFrame
//
//-----------------------------------------------------------------------------

void GLPortal::EndFrame(FDrawInfo *outer_di)
{
	GLPortal * p;

	if (gl_portalinfo)
	{
		Printf("%s%d portals, depth = %d\n%s{\n", indent.GetChars(), portals.Size(), renderdepth, indent.GetChars());
		indent += "  ";
	}

	// Only use occlusion query if there are more than 2 portals. 
	// Otherwise there's too much overhead.
	// (And don't forget to consider the separating nullptr pointers!)
	bool usequery = portals.Size() > 2 + (unsigned)renderdepth;

	while (portals.Pop(p) && p)
	{
		if (gl_portalinfo) 
		{
			Printf("%sProcessing %s, depth = %d, query = %d\n", indent.GetChars(), p->GetName(), renderdepth, usequery);
		}
		if (p->lines.Size() > 0)
		{
			p->RenderPortal(true, usequery, outer_di);
		}
		delete p;
	}
	renderdepth--;

	if (gl_portalinfo)
	{
		indent.Truncate(long(indent.Len()-2));
		Printf("%s}\n", indent.GetChars());
		if (portals.Size() == 0) gl_portalinfo = false;
	}
}


//-----------------------------------------------------------------------------
//
// Renders one sky portal without a stencil.
// In more complex scenes using a stencil for skies can severely stall
// the GPU and there's rarely more than one sky visible at a time.
//
//-----------------------------------------------------------------------------
bool GLPortal::RenderFirstSkyPortal(int recursion, FDrawInfo *outer_di)
{
	GLPortal * p;
	GLPortal * best = nullptr;
	unsigned bestindex=0;

	// Find the one with the highest amount of lines.
	// Normally this is also the one that saves the largest amount
	// of time by drawing it before the scene itself.
	for(int i = portals.Size()-1; i >= 0 && portals[i] != nullptr; --i)
	{
		p=portals[i];
		if (p->lines.Size() > 0 && p->IsSky())
		{
			// Cannot clear the depth buffer inside a portal recursion
			if (recursion && p->NeedDepthBuffer()) continue;

			if (!best || p->lines.Size()>best->lines.Size())
			{
				best=p;
				bestindex=i;
			}
		}
	}

	if (best)
	{
		portals.Delete(bestindex);
		best->RenderPortal(false, false, outer_di);
		delete best;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
//
// FindPortal
//
//-----------------------------------------------------------------------------

GLPortal * GLPortal::FindPortal(const void * src)
{
	int i=portals.Size()-1;

	while (i>=0 && portals[i] && portals[i]->GetSource()!=src) i--;
	return i>=0? portals[i]:nullptr;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Skybox Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLSkyboxPortal::DrawContents
//
//-----------------------------------------------------------------------------

void GLSkyboxPortal::DrawContents(FDrawInfo *di)
{
	int old_pm = PlaneMirrorMode;

	if (skyboxrecursion >= 3)
	{
		ClearScreen(di);
		return;
	}
	auto &vp = di->Viewpoint;

	skyboxrecursion++;
	AActor *origin = portal->mSkybox;
	portal->mFlags |= PORTSF_INSKYBOX;
	vp.extralight = 0;

	PlaneMirrorMode = 0;

	bool oldclamp = gl_RenderState.SetDepthClamp(false);
	vp.Pos = origin->InterpolatedPosition(vp.TicFrac);
	vp.ActorPos = origin->Pos();
	vp.Angles.Yaw += (origin->PrevAngles.Yaw + deltaangle(origin->PrevAngles.Yaw, origin->Angles.Yaw) * vp.TicFrac);

	// Don't let the viewpoint be too close to a floor or ceiling
	double floorh = origin->Sector->floorplane.ZatPoint(origin->Pos());
	double ceilh = origin->Sector->ceilingplane.ZatPoint(origin->Pos());
	if (vp.Pos.Z < floorh + 4) vp.Pos.Z = floorh + 4;
	if (vp.Pos.Z > ceilh - 4) vp.Pos.Z = ceilh - 4;

	vp.ViewActor = origin;

	inskybox = true;
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));
	di->SetViewArea();
	ClearClipper(di);

	di->UpdateCurrentMapSection();

	di->DrawScene(DM_SKYPORTAL);
	portal->mFlags &= ~PORTSF_INSKYBOX;
	inskybox = false;
	gl_RenderState.SetDepthClamp(oldclamp);
	skyboxrecursion--;

	PlaneMirrorMode = old_pm;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Sector stack Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//==========================================================================
//
// Fixme: This needs abstraction.
//
//==========================================================================

GLSectorStackPortal *FSectorPortalGroup::GetRenderState()
{
	if (glportal == nullptr) glportal = new GLSectorStackPortal(this);
	return glportal;
}



GLSectorStackPortal::~GLSectorStackPortal()
{
	if (origin != nullptr && origin->glportal == this)
	{
		origin->glportal = nullptr;
	}
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::SetupCoverage
//
//-----------------------------------------------------------------------------

static uint8_t SetCoverage(FDrawInfo *di, void *node)
{
	if (level.nodes.Size() == 0)
	{
		return 0;
	}
	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		uint8_t coverage = SetCoverage(di, bsp->children[0]) | SetCoverage(di, bsp->children[1]);
		di->no_renderflags[bsp->Index()] = coverage;
		return coverage;
	}
	else
	{
		subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
		return di->ss_renderflags[sub->Index()] & SSRF_SEEN;
	}
}

void GLSectorStackPortal::SetupCoverage(FDrawInfo *di)
{
	for(unsigned i=0; i<subsectors.Size(); i++)
	{
		subsector_t *sub = subsectors[i];
		int plane = origin->plane;
		for(int j=0;j<sub->portalcoverage[plane].sscount; j++)
		{
			subsector_t *dsub = &::level.subsectors[sub->portalcoverage[plane].subsectors[j]];
			di->CurrentMapSections.Set(dsub->mapsection);
			di->ss_renderflags[dsub->Index()] |= SSRF_SEEN;
		}
	}
	SetCoverage(di, ::level.HeadNode());
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLSectorStackPortal::DrawContents(FDrawInfo *di)
{
	FSectorPortalGroup *portal = origin;
	auto &vp = di->Viewpoint;

	vp.Pos += origin->mDisplacement;
	vp.ActorPos += origin->mDisplacement;
	vp.ViewActor = nullptr;

	// avoid recursions!
	if (origin->plane != -1) screen->instack[origin->plane]++;

	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	SetupCoverage(di);
	ClearClipper(di);
	
	// If the viewpoint is not within the portal, we need to invalidate the entire clip area.
	// The portal will re-validate the necessary parts when its subsectors get traversed.
	subsector_t *sub = R_PointInSubsector(vp.Pos);
	if (!(di->ss_renderflags[sub->Index()] & SSRF_SEEN))
	{
		di->mClipper->SafeAddClipRange(0, ANGLE_MAX);
		di->mClipper->SetBlocked(true);
	}

	di->DrawScene(DM_PORTAL);

	if (origin->plane != -1) screen->instack[origin->plane]--;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Plane Mirror Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// GLPlaneMirrorPortal::DrawContents
//
//-----------------------------------------------------------------------------

void GLPlaneMirrorPortal::DrawContents(FDrawInfo *di)
{
	if (renderdepth > r_mirror_recursions)
	{
		ClearScreen(di);
		return;
	}
	// A plane mirror needs to flip the portal exclusion logic because inside the mirror, up is down and down is up.
	std::swap(screen->instack[sector_t::floor], screen->instack[sector_t::ceiling]);

	auto &vp = di->Viewpoint;
	int old_pm = PlaneMirrorMode;

	// the player is always visible in a mirror.
	vp.showviewer = true;

	double planez = origin->ZatPoint(vp.Pos);
	vp.Pos.Z = 2 * planez - vp.Pos.Z;
	vp.ViewActor = nullptr;
	PlaneMirrorMode = origin->fC() < 0 ? -1 : 1;

	PlaneMirrorFlag++;
	di->SetClipHeight(planez, PlaneMirrorMode < 0 ? -1.f : 1.f);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));
	ClearClipper(di);

	di->UpdateCurrentMapSection();

	di->DrawScene(DM_PORTAL);
	PlaneMirrorFlag--;
	PlaneMirrorMode = old_pm;
	std::swap(screen->instack[sector_t::floor], screen->instack[sector_t::ceiling]);
}

//-----------------------------------------------------------------------------
//
// Common code for line to line and mirror portals
//
//-----------------------------------------------------------------------------

int GLLinePortal::ClipSeg(seg_t *seg, const DVector3 &viewpos)
{ 
	line_t *linedef = seg->linedef;
	if (!linedef)
	{
		return PClip_Inside;	// should be handled properly.
	}
	return P_ClipLineToPortal(linedef, line(), viewpos) ? PClip_InFront : PClip_Inside;
}

int GLLinePortal::ClipSubsector(subsector_t *sub)
{ 
	// this seg is completely behind the mirror
	for(unsigned int i=0;i<sub->numlines;i++)
	{
		if (P_PointOnLineSidePrecise(sub->firstline[i].v1->fPos(), line()) == 0) return PClip_Inside;
	}
	return PClip_InFront; 
}

int GLLinePortal::ClipPoint(const DVector2 &pos) 
{ 
	if (P_PointOnLineSidePrecise(pos, line())) 
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Mirror Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// R_EnterMirror
//
//-----------------------------------------------------------------------------
void GLMirrorPortal::DrawContents(FDrawInfo *di)
{
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen(di);
		return;
	}

	auto &vp = di->Viewpoint;
	di->UpdateCurrentMapSection();

	di->mClipPortal = this;
	DAngle StartAngle = vp.Angles.Yaw;
	DVector3 StartPos = vp.Pos;

	vertex_t *v1 = linedef->v1;
	vertex_t *v2 = linedef->v2;

	// the player is always visible in a mirror.
	vp.showviewer = true;
	// Reflect the current view behind the mirror.
	if (linedef->Delta().X == 0)
	{
		// vertical mirror
		vp.Pos.X = 2 * v1->fX() - StartPos.X;

		// Compensation for reendering inaccuracies
		if (StartPos.X < v1->fX()) vp.Pos.X -= 0.1;
		else vp.Pos.X += 0.1;
	}
	else if (linedef->Delta().Y == 0)
	{ 
		// horizontal mirror
		vp.Pos.Y = 2*v1->fY() - StartPos.Y;

		// Compensation for reendering inaccuracies
		if (StartPos.Y<v1->fY())  vp.Pos.Y -= 0.1;
		else vp.Pos.Y += 0.1;
	}
	else
	{ 
		// any mirror--use floats to avoid integer overflow. 
		// Use doubles to avoid losing precision which is very important here.

		double dx = v2->fX() - v1->fX();
		double dy = v2->fY() - v1->fY();
		double x1 = v1->fX();
		double y1 = v1->fY();
		double x = StartPos.X;
		double y = StartPos.Y;

		// the above two cases catch len == 0
		double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		vp.Pos.X = (x1 + r * dx)*2 - x;
		vp.Pos.Y = (y1 + r * dy)*2 - y;

		// Compensation for reendering inaccuracies
		FVector2 v(-dx, dy);
		v.MakeUnit();

		vp.Pos.X+= v[1] * renderdepth / 2;
		vp.Pos.Y+= v[0] * renderdepth / 2;
	}
	vp.Angles.Yaw = linedef->Delta().Angle() * 2. - StartAngle;

	vp.ViewActor = nullptr;

	MirrorFlag++;
	di->SetClipLine(linedef);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	di->mClipper->Clear();

	angle_t af = di->FrustumAngle();
	if (af<ANGLE_180) di->mClipper->SafeAddClipRangeRealAngles(vp.Angles.Yaw.BAMs()+af, vp.Angles.Yaw.BAMs()-af);

    di->mClipper->SafeAddClipRange(linedef->v1, linedef->v2);

	di->DrawScene(DM_PORTAL);

	MirrorFlag--;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Line to line Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLLineToLinePortal::DrawContents(FDrawInfo *di)
{
	// TODO: Handle recursion more intelligently
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen(di);
		return;
	}
	auto &vp = di->Viewpoint;
	di->mClipPortal = this;

	line_t *origin = glport->lines[0]->mOrigin;
	P_TranslatePortalXY(origin, vp.Pos.X, vp.Pos.Y);
	P_TranslatePortalXY(origin, vp.ActorPos.X, vp.ActorPos.Y);
	P_TranslatePortalAngle(origin, vp.Angles.Yaw);
	P_TranslatePortalZ(origin, vp.Pos.Z);
	P_TranslatePortalXY(origin, vp.Path[0].X, vp.Path[0].Y);
	P_TranslatePortalXY(origin, vp.Path[1].X, vp.Path[1].Y);
	if (!vp.showviewer && vp.camera != nullptr && P_PointOnLineSidePrecise(vp.Path[0], glport->lines[0]->mDestination) != P_PointOnLineSidePrecise(vp.Path[1], glport->lines[0]->mDestination))
	{
		double distp = (vp.Path[0] - vp.Path[1]).Length();
		if (distp > EQUAL_EPSILON)
		{
			double dist1 = (vp.Pos - vp.Path[0]).Length();
			double dist2 = (vp.Pos - vp.Path[1]).Length();

			if (dist1 + dist2 < distp + 1)
			{
				vp.camera->renderflags |= RF_MAYBEINVISIBLE;
			}
		}
	}


	for (unsigned i = 0; i < lines.Size(); i++)
	{
		line_t *line = lines[i].seg->linedef->getPortalDestination();
		subsector_t *sub;
		if (line->sidedef[0]->Flags & WALLF_POLYOBJ) 
			sub = R_PointInSubsector(line->v1->fixX(), line->v1->fixY());
		else sub = line->frontsector->subsectors[0];
		di->CurrentMapSections.Set(sub->mapsection);
	}

	vp.ViewActor = nullptr;
	di->SetClipLine(glport->lines[0]->mDestination);
	di->SetupView(vp.Pos.X, vp.Pos.Y, vp.Pos.Z, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	ClearClipper(di);
	di->DrawScene(DM_PORTAL);
}

void GLLineToLinePortal::RenderAttached(FDrawInfo *di)
{
	di->ProcessActorsInPortal(glport, di->in_area);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Horizon Portal
//
// This simply draws the area in medium sized squares. Drawing it as a whole
// polygon creates visible inaccuracies.
//
// Originally I tried to minimize the amount of data to be drawn but there
// are 2 problems with it:
//
// 1. Setting this up completely negates any performance gains.
// 2. It doesn't work with a 360° field of view (as when you are looking up.)
//
//
// So the brute force mechanism is just as good.
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

GLHorizonPortal::GLHorizonPortal(GLHorizonInfo * pt, FRenderViewpoint &vp, bool local)
	: GLPortal(local)
{
	origin = pt;

	// create the vertex data for this horizon portal.
	GLSectorPlane * sp = &origin->plane;
	const float vx = vp.Pos.X;
	const float vy = vp.Pos.Y;
	const float vz = vp.Pos.Z;
	const float z = sp->Texheight;
	const float tz = (z - vz);

	// Draw to some far away boundary
	// This is not drawn as larger strips because it causes visual glitches.
	FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
	for (float x = -32768 + vx; x<32768 + vx; x += 4096)
	{
		for (float y = -32768 + vy; y<32768 + vy; y += 4096)
		{
			ptr->Set(x, z, y, x / 64, -y / 64);
			ptr++;
			ptr->Set(x + 4096, z, y, x / 64 + 64, -y / 64);
			ptr++;
			ptr->Set(x, z, y + 4096, x / 64, -y / 64 - 64);
			ptr++;
			ptr->Set(x + 4096, z, y + 4096, x / 64 + 64, -y / 64 - 64);
			ptr++;
		}
	}

	// fill the gap between the polygon and the true horizon
	// Since I can't draw into infinity there can always be a
	// small gap
	ptr->Set(-32768 + vx, z, -32768 + vy, 512.f, 0);
	ptr++;
	ptr->Set(-32768 + vx, vz, -32768 + vy, 512.f, tz);
	ptr++;
	ptr->Set(-32768 + vx, z, 32768 + vy, -512.f, 0);
	ptr++;
	ptr->Set(-32768 + vx, vz, 32768 + vy, -512.f, tz);
	ptr++;
	ptr->Set(32768 + vx, z, 32768 + vy, 512.f, 0);
	ptr++;
	ptr->Set(32768 + vx, vz, 32768 + vy, 512.f, tz);
	ptr++;
	ptr->Set(32768 + vx, z, -32768 + vy, -512.f, 0);
	ptr++;
	ptr->Set(32768 + vx, vz, -32768 + vy, -512.f, tz);
	ptr++;
	ptr->Set(-32768 + vx, z, -32768 + vy, 512.f, 0);
	ptr++;
	ptr->Set(-32768 + vx, vz, -32768 + vy, 512.f, tz);
	ptr++;

	vcount = GLRenderer->mVBO->GetCount(ptr, &voffset) - 10;

}

//-----------------------------------------------------------------------------
//
// GLHorizonPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLHorizonPortal::DrawContents(FDrawInfo *di)
{
	Clocker c(PortalAll);

	FMaterial * gltexture;
	PalEntry color;
	player_t * player=&players[consoleplayer];
	GLSectorPlane * sp = &origin->plane;
	auto &vp = di->Viewpoint;

	gltexture=FMaterial::ValidateTexture(sp->texture, false, true);
	if (!gltexture) 
	{
		ClearScreen(di);
		return;
	}
	di->SetCameraPos(vp.Pos);


	if (gltexture && gltexture->tex->isFullbright())
	{
		// glowing textures are always drawn full bright without color
		di->SetColor(255, 0, origin->colormap, 1.f);
		di->SetFog(255, 0, &origin->colormap, false);
	}
	else 
	{
		int rel = getExtraLight();
		di->SetColor(origin->lightlevel, rel, origin->colormap, 1.0f);
		di->SetFog(origin->lightlevel, rel, &origin->colormap, false);
	}


	gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);
	gl_RenderState.SetObjectColor(origin->specialcolor);

	gl_RenderState.SetPlaneTextureRotation(sp, gltexture);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);
	gl_RenderState.Apply();


	for (unsigned i = 0; i < vcount; i += 4)
	{
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, voffset + i, 4);
	}
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_STRIP, voffset + vcount, 10);

	gl_RenderState.EnableTextureMatrix(false);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Eternity-style horizon portal
//
// To the rest of the engine these masquerade as a skybox portal
// Internally they need to draw two horizon or sky portals
// and will use the respective classes to achieve that.
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

void GLEEHorizonPortal::DrawContents(FDrawInfo *di)
{
	auto &vp = di->Viewpoint;
	sector_t *sector = portal->mOrigin;
	if (sector->GetTexture(sector_t::floor) == skyflatnum ||
		sector->GetTexture(sector_t::ceiling) == skyflatnum)
	{
		GLSkyInfo skyinfo;
		skyinfo.init(sector->sky, 0);
		GLSkyPortal sky(&skyinfo, true);
		sky.DrawContents(di);
	}
	if (sector->GetTexture(sector_t::ceiling) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(sector, sector_t::ceiling);
		horz.lightlevel = hw_ClampLight(sector->GetCeilingLight());
		horz.colormap = sector->Colormap;
		horz.specialcolor = 0xffffffff;
		if (portal->mType == PORTS_PLANE)
		{
			horz.plane.Texheight = vp.Pos.Z + fabs(horz.plane.Texheight);
		}
		GLHorizonPortal ceil(&horz, di->Viewpoint, true);
		ceil.DrawContents(di);
	}
	if (sector->GetTexture(sector_t::floor) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(sector, sector_t::floor);
		horz.lightlevel = hw_ClampLight(sector->GetFloorLight());
		horz.colormap = sector->Colormap;
		horz.specialcolor = 0xffffffff;
		if (portal->mType == PORTS_PLANE)
		{
			horz.plane.Texheight = vp.Pos.Z - fabs(horz.plane.Texheight);
		}
		GLHorizonPortal floor(&horz, di->Viewpoint, true);
		floor.DrawContents(di);
	}
}

void GLPortal::Initialize()
{
	assert(0 == QueryObject);
	glGenQueries(1, &QueryObject);
}

void GLPortal::Shutdown()
{
	if (0 != QueryObject)
	{
		glDeleteQueries(1, &QueryObject);
		QueryObject = 0;
	}
}

const char *GLSkyPortal::GetName() { return "Sky"; }
const char *GLSkyboxPortal::GetName() { return "Skybox"; }
const char *GLSectorStackPortal::GetName() { return "Sectorstack"; }
const char *GLPlaneMirrorPortal::GetName() { return "Planemirror"; }
const char *GLMirrorPortal::GetName() { return "Mirror"; }
const char *GLLineToLinePortal::GetName() { return "LineToLine"; }
const char *GLHorizonPortal::GetName() { return "Horizon"; }
const char *GLEEHorizonPortal::GetName() { return "EEHorizon"; }

// This needs to remain on the renderer side until the portal interface can be abstracted.
void FSectorPortalGroup::AddSubsector(subsector_t *sub)
{
	GLSectorStackPortal *glportal = GetRenderState();
	glportal->AddSubsector(sub);
}
