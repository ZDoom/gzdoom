/*
** gl_portal.cpp
**   Generalized portal maintenance classes for skyboxes, horizons etc.
**   Requires a stencil buffer!
**
**---------------------------------------------------------------------------
** Copyright 2004-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "p_local.h"
#include "vectors.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "a_sharedglobal.h"
#include "r_sky.h"
#include "portal.h"
#include "p_maputl.h"
#include "d_player.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/stereo3d/scoped_color_mask.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/utility/gl_geometric.h"

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

extern bool r_showviewer;

TArray<GLPortal *> GLPortal::portals;
TArray<float> GLPortal::planestack;
int GLPortal::recursion;
int GLPortal::MirrorFlag;
int GLPortal::PlaneMirrorFlag;
int GLPortal::renderdepth;
int GLPortal::PlaneMirrorMode;
GLuint GLPortal::QueryObject;

int		 GLPortal::instack[2];
bool	 GLPortal::inskybox;

UniqueList<GLSkyInfo> UniqueSkies;
UniqueList<GLHorizonInfo> UniqueHorizons;
UniqueList<secplane_t> UniquePlaneMirrors;
UniqueList<FGLLinePortal> UniqueLineToLines;

void gl_RenderActorsInPortal(FGLLinePortal *glport);

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
	UniqueLineToLines.Clear();
}

//==========================================================================
//
//
//
//==========================================================================
void GLPortal::ClearScreen()
{
	bool multi = !!glIsEnabled(GL_MULTISAMPLE);
	gl_MatrixStack.Push(gl_RenderState.mViewMatrix);
	gl_MatrixStack.Push(gl_RenderState.mProjectionMatrix);
	screen->Begin2D(false);
	screen->Dim(0, 1.f, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	glEnable(GL_DEPTH_TEST);
	gl_MatrixStack.Pop(gl_RenderState.mProjectionMatrix);
	gl_MatrixStack.Pop(gl_RenderState.mViewMatrix);
	gl_RenderState.ApplyMatrices();
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
		bool cap = NeedCap() && lines.Size() > 1;
		mPrimIndices.Resize(2 * lines.Size() + 4 * cap);

		for (unsigned int i = 0; i<lines.Size(); i++)
		{
			lines[i].RenderWall(GLWall::RWF_NORENDER, &mPrimIndices[i * 2]);
		}

		if (cap)
		{
			// Cap the stencil at the top and bottom
			int n = lines.Size() * 2;
			FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
			ptr[0].Set(-32767.0f, 32767.0f, -32767.0f, 0, 0);
			ptr[1].Set(-32767.0f, 32767.0f, 32767.0f, 0, 0);
			ptr[2].Set(32767.0f, 32767.0f, 32767.0f, 0, 0);
			ptr[3].Set(32767.0f, 32767.0f, -32767.0f, 0, 0);
			ptr += 4;
			mPrimIndices[n + 1] = GLRenderer->mVBO->GetCount(ptr, &mPrimIndices[n]);
			ptr[0].Set(-32767.0f, -32767.0f, -32767.0f, 0, 0);
			ptr[1].Set(-32767.0f, -32767.0f, 32767.0f, 0, 0);
			ptr[2].Set(32767.0f, -32767.0f, 32767.0f, 0, 0);
			ptr[3].Set(32767.0f, -32767.0f, -32767.0f, 0, 0);
			ptr += 4;
			mPrimIndices[n + 3] = GLRenderer->mVBO->GetCount(ptr, &mPrimIndices[n + 2]);
		}
	}
	gl_RenderState.Apply();
	for (unsigned int i = 0; i < mPrimIndices.Size(); i += 2)
	{
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, mPrimIndices[i], mPrimIndices[i + 1]);
	}
}






//-----------------------------------------------------------------------------
//
// Start
//
//-----------------------------------------------------------------------------

bool GLPortal::Start(bool usestencil, bool doquery)
{
	rendered_portals++;
	PortalAll.Clock();
	if (usestencil)
	{
		if (!gl_portals) 
		{
			PortalAll.Unclock();
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
				if (!QueryObject && doquery) glGenQueries(1, &QueryObject);
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
						PortalAll.Unclock();
						return false;
					}
				}
				FDrawInfo::StartDrawInfo();
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
		if (NeedDepthBuffer())
		{
			FDrawInfo::StartDrawInfo();
		}
		else
		{
			glDepthMask(false);
			glDisable(GL_DEPTH_TEST);
		}
	}

	// save viewpoint
	savedViewPos = ViewPos;
	savedAngle = ViewAngle;
	savedviewactor=GLRenderer->mViewActor;
	savedviewarea=in_area;
	savedviewpath[0] = ViewPath[0];
	savedviewpath[1] = ViewPath[1];
	savedvisibility = camera ? camera->renderflags & RF_INVISIBLE : ActorRenderFlags::FromInt(0);


	PrevPortal = GLRenderer->mCurrentPortal;
	PrevClipPortal = GLRenderer->mClipPortal;
	GLRenderer->mClipPortal = NULL;	// Portals which need this have to set it themselves
	GLRenderer->mCurrentPortal = this;

	if (PrevPortal != NULL) PrevPortal->PushState();
	PortalAll.Unclock();
	return true;
}


inline void GLPortal::ClearClipper()
{
	DAngle angleOffset = deltaangle(savedAngle, ViewAngle);

	clipper.Clear();

	static int call=0;

	// Set the clipper to the minimal visible area
	clipper.SafeAddClipRange(0,0xffffffff);
	for (unsigned int i = 0; i < lines.Size(); i++)
	{
		DAngle startAngle = (DVector2(lines[i].glseg.x2, lines[i].glseg.y2) - savedViewPos).Angle() + angleOffset;
		DAngle endAngle = (DVector2(lines[i].glseg.x1, lines[i].glseg.y1) - savedViewPos).Angle() + angleOffset;

		if (deltaangle(endAngle, startAngle) < 0)
		{
			clipper.SafeRemoveClipRangeRealAngles(startAngle.BAMs(), endAngle.BAMs());
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = GLRenderer->FrustumAngle();
	if (a1 < ANGLE_180) clipper.SafeAddClipRangeRealAngles(ViewAngle.BAMs() + a1, ViewAngle.BAMs() - a1);

	// lock the parts that have just been clipped out.
	clipper.SetSilhouette();
}

//-----------------------------------------------------------------------------
//
// End
//
//-----------------------------------------------------------------------------
void GLPortal::End(bool usestencil)
{
	bool needdepth = NeedDepthBuffer();

	PortalAll.Clock();
	if (PrevPortal != NULL) PrevPortal->PopState();
	GLRenderer->mCurrentPortal = PrevPortal;
	GLRenderer->mClipPortal = PrevClipPortal;

	if (usestencil)
	{
		if (needdepth) FDrawInfo::EndDrawInfo();

		// Restore the old view
		ViewPath[0] = savedviewpath[0];
		ViewPath[1] = savedviewpath[1];
		ViewPos = savedViewPos;
		ViewAngle = savedAngle;
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		if (camera != nullptr) camera->renderflags = (camera->renderflags & ~RF_INVISIBLE) | savedvisibility;
		GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));

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
			FDrawInfo::EndDrawInfo();
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glEnable(GL_DEPTH_TEST);
			glDepthMask(true);
		}
		// Restore the old view
		ViewPos = savedViewPos;
		ViewAngle = savedAngle;
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		if (camera != nullptr) camera->renderflags |= savedvisibility;
		GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

		// This draws a valid z-buffer into the stencil's contents to ensure it
		// doesn't get overwritten by the level's geometry.

		gl_RenderState.ResetColor();
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0, 1);
		{
			ScopedColorMask colorMask(0, 0, 0, 0); 
			// glColorMask(0,0,0,0);						// no graphics
			gl_RenderState.SetEffect(EFF_STENCIL);
			gl_RenderState.EnableTexture(false);
			DrawPortalStencil();
			gl_RenderState.SetEffect(EFF_NONE);
			gl_RenderState.EnableTexture(true);
		} // glColorMask(1, 1, 1, 1);
		glDepthFunc(GL_LESS);
	}
	PortalAll.Unclock();
}


//-----------------------------------------------------------------------------
//
// StartFrame
//
//-----------------------------------------------------------------------------
void GLPortal::StartFrame()
{
	GLPortal * p=NULL;
	portals.Push(p);
	if (renderdepth==0)
	{
		inskybox=false;
		instack[sector_t::floor]=instack[sector_t::ceiling]=0;
	}
	renderdepth++;
}


//-----------------------------------------------------------------------------
//
// Portal info
//
//-----------------------------------------------------------------------------

static bool gl_portalinfo;

CCMD(gl_portalinfo)
{
	gl_portalinfo = true;
}

FString indent;

//-----------------------------------------------------------------------------
//
// EndFrame
//
//-----------------------------------------------------------------------------

void GLPortal::EndFrame()
{
	GLPortal * p;

	if (gl_portalinfo)
	{
		Printf("%s%d portals, depth = %d\n%s{\n", indent.GetChars(), portals.Size(), renderdepth, indent.GetChars());
		indent += "  ";
	}

	// Only use occlusion query if there are more than 2 portals. 
	// Otherwise there's too much overhead.
	// (And don't forget to consider the separating NULL pointers!)
	bool usequery = portals.Size() > 2 + (unsigned)renderdepth;

	while (portals.Pop(p) && p)
	{
		if (gl_portalinfo) 
		{
			Printf("%sProcessing %s, depth = %d, query = %d\n", indent.GetChars(), p->GetName(), renderdepth, usequery);
		}
		if (p->lines.Size() > 0)
		{
			p->RenderPortal(true, usequery);
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
bool GLPortal::RenderFirstSkyPortal(int recursion)
{
	GLPortal * p;
	GLPortal * best = NULL;
	unsigned bestindex=0;

	// Find the one with the highest amount of lines.
	// Normally this is also the one that saves the largest amount
	// of time by drawing it before the scene itself.
	for(int i = portals.Size()-1; i >= 0 && portals[i] != NULL; --i)
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
		best->RenderPortal(false, false);
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
	return i>=0? portals[i]:NULL;
}


//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

void GLPortal::SaveMapSection()
{
	savedmapsection.Resize(currentmapsection.Size());
	memcpy(&savedmapsection[0], &currentmapsection[0], currentmapsection.Size());
	memset(&currentmapsection[0], 0, currentmapsection.Size());
}

void GLPortal::RestoreMapSection()
{
	memcpy(&currentmapsection[0], &savedmapsection[0], currentmapsection.Size());
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
static int skyboxrecursion=0;
void GLSkyboxPortal::DrawContents()
{
	int old_pm = PlaneMirrorMode;
	int saved_extralight = extralight;

	if (skyboxrecursion >= 3)
	{
		ClearScreen();
		return;
	}

	skyboxrecursion++;
	AActor *origin = portal->mSkybox;
	portal->mFlags |= PORTSF_INSKYBOX;
	extralight = 0;

	PlaneMirrorMode = 0;

	bool oldclamp = gl_RenderState.SetDepthClamp(false);
	ViewPos = origin->InterpolatedPosition(r_TicFracF);
	ViewAngle += (origin->PrevAngles.Yaw + deltaangle(origin->PrevAngles.Yaw, origin->Angles.Yaw) * r_TicFracF);

	// Don't let the viewpoint be too close to a floor or ceiling
	double floorh = origin->Sector->floorplane.ZatPoint(origin->Pos());
	double ceilh = origin->Sector->ceilingplane.ZatPoint(origin->Pos());
	if (ViewPos.Z < floorh + 4) ViewPos.Z = floorh + 4;
	if (ViewPos.Z > ceilh - 4) ViewPos.Z = ceilh - 4;

	GLRenderer->mViewActor = origin;

	inskybox = true;
	GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));
	GLRenderer->SetViewArea();
	ClearClipper();

	int mapsection = R_PointInSubsector(ViewPos)->mapsection;

	SaveMapSection();
	currentmapsection[mapsection >> 3] |= 1 << (mapsection & 7);

	GLRenderer->DrawScene(DM_PORTAL);
	portal->mFlags &= ~PORTSF_INSKYBOX;
	inskybox = false;
	gl_RenderState.SetDepthClamp(oldclamp);
	skyboxrecursion--;

	PlaneMirrorMode = old_pm;
	extralight = saved_extralight;

	RestoreMapSection();
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
GLSectorStackPortal::~GLSectorStackPortal()
{
	if (origin != NULL && origin->glportal == this)
	{
		origin->glportal = NULL;
	}
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::SetupCoverage
//
//-----------------------------------------------------------------------------

static BYTE SetCoverage(void *node)
{
	if (numnodes == 0)
	{
		return 0;
	}
	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		BYTE coverage = SetCoverage(bsp->children[0]) | SetCoverage(bsp->children[1]);
		gl_drawinfo->no_renderflags[bsp-nodes] = coverage;
		return coverage;
	}
	else
	{
		subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
		return gl_drawinfo->ss_renderflags[sub-subsectors] & SSRF_SEEN;
	}
}

void GLSectorStackPortal::SetupCoverage()
{
	for(unsigned i=0; i<subsectors.Size(); i++)
	{
		subsector_t *sub = subsectors[i];
		int plane = origin->plane;
		for(int j=0;j<sub->portalcoverage[plane].sscount; j++)
		{
			subsector_t *dsub = &::subsectors[sub->portalcoverage[plane].subsectors[j]];
			currentmapsection[dsub->mapsection>>3] |= 1 << (dsub->mapsection&7);
			gl_drawinfo->ss_renderflags[dsub-::subsectors] |= SSRF_SEEN;
		}
	}
	SetCoverage(&nodes[numnodes-1]);
}

//-----------------------------------------------------------------------------
//
// GLSectorStackPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLSectorStackPortal::DrawContents()
{
	FPortal *portal = origin;

	ViewPos += origin->mDisplacement;
	GLRenderer->mViewActor = NULL;

	// avoid recursions!
	if (origin->plane != -1) instack[origin->plane]++;

	GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	SaveMapSection();
	SetupCoverage();
	ClearClipper();
	
	// If the viewpoint is not within the portal, we need to invalidate the entire clip area.
	// The portal will re-validate the necessary parts when its subsectors get traversed.
	subsector_t *sub = R_PointInSubsector(ViewPos);
	if (!(gl_drawinfo->ss_renderflags[sub - ::subsectors] & SSRF_SEEN))
	{
		clipper.SafeAddClipRange(0, ANGLE_MAX);
		clipper.SetBlocked(true);
	}

	GLRenderer->DrawScene(DM_PORTAL);
	RestoreMapSection();

	if (origin->plane != -1) instack[origin->plane]--;
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

void GLPlaneMirrorPortal::DrawContents()
{
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen();
		return;
	}

	int old_pm=PlaneMirrorMode;

	double planez = origin->ZatPoint(ViewPos);
	ViewPos.Z = 2 * planez - ViewPos.Z;
	GLRenderer->mViewActor = NULL;
	PlaneMirrorMode = origin->fC() < 0 ? -1 : 1;
	
	PlaneMirrorFlag++;
	GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();

	gl_RenderState.SetClipHeight(planez, PlaneMirrorMode < 0? -1.f : 1.f);
	GLRenderer->DrawScene(DM_PORTAL);
	gl_RenderState.SetClipHeight(0.f, 0.f);
	PlaneMirrorFlag--;
	PlaneMirrorMode=old_pm;
}

void GLPlaneMirrorPortal::PushState()
{
	planestack.Push(gl_RenderState.GetClipHeight());
	planestack.Push(gl_RenderState.GetClipHeightDirection());
	gl_RenderState.SetClipHeight(0.f, 0.f);
}

void GLPlaneMirrorPortal::PopState()
{
	float d, f;
	planestack.Pop(d);
	planestack.Pop(f);
	gl_RenderState.SetClipHeight(f, d);
}

//-----------------------------------------------------------------------------
//
// Common code for line to line and mirror portals
//
//-----------------------------------------------------------------------------

void GLLinePortal::PushState()
{
	FStateVec4 &v = gl_RenderState.GetClipLine();
	planestack.Push(v.vec[0]);
	planestack.Push(v.vec[1]);
	planestack.Push(v.vec[2]);
	planestack.Push(v.vec[3]);
	planestack.Push(gl_RenderState.GetClipLineState());
	gl_RenderState.EnableClipLine(false);
}

void GLLinePortal::PopState()
{
	FStateVec4 &v = gl_RenderState.GetClipLine();
	float e;
	planestack.Pop(e);
	planestack.Pop(v.vec[3]);
	planestack.Pop(v.vec[2]);
	planestack.Pop(v.vec[1]);
	planestack.Pop(v.vec[0]);
	gl_RenderState.EnableClipLine(e != 0);
}

int GLLinePortal::ClipSeg(seg_t *seg) 
{ 
	line_t *linedef = seg->linedef;
	if (!linedef)
	{
		return PClip_Inside;	// should be handled properly.
	}
	return P_ClipLineToPortal(linedef, line(), ViewPos) ? PClip_InFront : PClip_Inside;
}

int GLLinePortal::ClipSubsector(subsector_t *sub)
{ 
	// this seg is completely behind the mirror!
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
void GLMirrorPortal::DrawContents()
{
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen();
		return;
	}

	GLRenderer->mClipPortal = this;
	DAngle StartAngle = ViewAngle;
	DVector3 StartPos = ViewPos;

	vertex_t *v1 = linedef->v1;
	vertex_t *v2 = linedef->v2;

	// Reflect the current view behind the mirror.
	if (linedef->Delta().X == 0)
	{
		// vertical mirror
		ViewPos.X = 2 * v1->fX() - StartPos.X;

		// Compensation for reendering inaccuracies
		if (StartPos.X < v1->fX())  ViewPos.X -= 0.1;
		else ViewPos.X += 0.1;
	}
	else if (linedef->Delta().Y == 0)
	{ 
		// horizontal mirror
		ViewPos.Y = 2*v1->fY() - StartPos.Y;

		// Compensation for reendering inaccuracies
		if (StartPos.Y<v1->fY())  ViewPos.Y -= 0.1;
		else ViewPos.Y += 0.1;
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

		ViewPos.X = (x1 + r * dx)*2 - x;
		ViewPos.Y = (y1 + r * dy)*2 - y;

		// Compensation for reendering inaccuracies
		FVector2 v(-dx, dy);
		v.MakeUnit();

		ViewPos.X+= v[1] * renderdepth / 2;
		ViewPos.Y+= v[0] * renderdepth / 2;
	}
	ViewAngle = linedef->Delta().Angle() * 2. - StartAngle;

	GLRenderer->mViewActor = NULL;

	MirrorFlag++;
	GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	clipper.Clear();

	angle_t af = GLRenderer->FrustumAngle();
	if (af<ANGLE_180) clipper.SafeAddClipRangeRealAngles(ViewAngle.BAMs()+af, ViewAngle.BAMs()-af);

	angle_t a2 = linedef->v1->GetClipAngle();
	angle_t a1 = linedef->v2->GetClipAngle();
	clipper.SafeAddClipRange(a1,a2);

	gl_RenderState.SetClipLine(linedef);
	gl_RenderState.EnableClipLine(true);
	GLRenderer->DrawScene(DM_PORTAL);
	gl_RenderState.EnableClipLine(false);

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
void GLLineToLinePortal::DrawContents()
{
	// TODO: Handle recursion more intelligently
	if (renderdepth>r_mirror_recursions) 
	{
		ClearScreen();
		return;
	}

	GLRenderer->mClipPortal = this;

	line_t *origin = glport->lines[0]->mOrigin;
	P_TranslatePortalXY(origin, ViewPos.X, ViewPos.Y);
	P_TranslatePortalAngle(origin, ViewAngle);
	P_TranslatePortalZ(origin, ViewPos.Z);
	P_TranslatePortalXY(origin, ViewPath[0].X, ViewPath[0].Y);
	P_TranslatePortalXY(origin, ViewPath[1].X, ViewPath[1].Y);
	if (!r_showviewer && camera != nullptr && P_PointOnLineSidePrecise(ViewPath[0], glport->lines[0]->mDestination) != P_PointOnLineSidePrecise(ViewPath[1], glport->lines[0]->mDestination))
	{
		double distp = (ViewPath[0] - ViewPath[1]).Length();
		if (distp > EQUAL_EPSILON)
		{
			double dist1 = (ViewPos - ViewPath[0]).Length();
			double dist2 = (ViewPos - ViewPath[1]).Length();

			if (dist1 + dist2 < distp + 1)
			{
				camera->renderflags |= RF_INVISIBLE;
			}
		}
	}


	SaveMapSection();

	for (unsigned i = 0; i < lines.Size(); i++)
	{
		line_t *line = lines[i].seg->linedef->getPortalDestination();
		subsector_t *sub;
		if (line->sidedef[0]->Flags & WALLF_POLYOBJ) 
			sub = R_PointInSubsector(line->v1->fixX(), line->v1->fixY());
		else sub = line->frontsector->subsectors[0];
		int mapsection = sub->mapsection;
		currentmapsection[mapsection >> 3] |= 1 << (mapsection & 7);
	}

	GLRenderer->mViewActor = nullptr;
	GLRenderer->SetupView(ViewPos.X, ViewPos.Y, ViewPos.Z, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	ClearClipper();
	gl_RenderState.SetClipLine(glport->lines[0]->mDestination);
	gl_RenderState.EnableClipLine(true);
	GLRenderer->DrawScene(DM_PORTAL);
	gl_RenderState.EnableClipLine(false);
	RestoreMapSection();
}

void GLLineToLinePortal::RenderAttached()
{
	gl_RenderActorsInPortal(glport);
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
//-----------------------------------------------------------------------------
//
// GLHorizonPortal::DrawContents
//
//-----------------------------------------------------------------------------
void GLHorizonPortal::DrawContents()
{
	PortalAll.Clock();

	GLSectorPlane * sp=&origin->plane;
	FMaterial * gltexture;
	PalEntry color;
	float z;
	player_t * player=&players[consoleplayer];

	gltexture=FMaterial::ValidateTexture(sp->texture, false, true);
	if (!gltexture) 
	{
		ClearScreen();
		PortalAll.Unclock();
		return;
	}
	gl_RenderState.SetCameraPos(ViewPos.X, ViewPos.Y, ViewPos.Z);


	z=sp->Texheight;


	if (gltexture && gltexture->tex->isFullbright())
	{
		// glowing textures are always drawn full bright without color
		gl_SetColor(255, 0, origin->colormap, 1.f);
		gl_SetFog(255, 0, &origin->colormap, false);
	}
	else 
	{
		int rel = getExtraLight();
		gl_SetColor(origin->lightlevel, rel, origin->colormap, 1.0f);
		gl_SetFog(origin->lightlevel, rel, &origin->colormap, false);
	}


	gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);

	gl_SetPlaneTextureRotation(sp, gltexture);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_ONE,GL_ZERO);
	gl_RenderState.Apply();



	float vx= ViewPos.X;
	float vy= ViewPos.Y;

	// Draw to some far away boundary
	// This is not drawn as larher strips because it causes visual glitches.
	for(float x=-32768+vx; x<32768+vx; x+=4096)
	{
		for(float y=-32768+vy; y<32768+vy;y+=4096)
		{
			FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
			ptr->Set(x, z, y, x / 64, -y / 64);
			ptr++;
			ptr->Set(x + 4096, z, y, x / 64 + 64, -y / 64);
			ptr++;
			ptr->Set(x, z, y + 4096, x / 64, -y / 64 - 64);
			ptr++;
			ptr->Set(x + 4096, z, y + 4096, x / 64 + 64, -y / 64 - 64);
			ptr++;
			GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
		}
	}

	float vz= ViewPos.Z;
	float tz=(z-vz);///64.0f;

	// fill the gap between the polygon and the true horizon
	// Since I can't draw into infinity there can always be a
	// small gap

	FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
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
	GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

	gl_RenderState.EnableTextureMatrix(false);
	PortalAll.Unclock();

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

void GLEEHorizonPortal::DrawContents()
{
	PortalAll.Clock();
	sector_t *sector = portal->mOrigin;
	if (sector->GetTexture(sector_t::floor) == skyflatnum ||
		sector->GetTexture(sector_t::ceiling) == skyflatnum)
	{
		GLSkyInfo skyinfo;
		skyinfo.init(sector->sky, 0);
		GLSkyPortal sky(&skyinfo, true);
		sky.DrawContents();
	}
	if (sector->GetTexture(sector_t::ceiling) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(sector, true);
		horz.lightlevel = gl_ClampLight(sector->GetCeilingLight());
		horz.colormap = sector->ColorMap;
		if (portal->mType == PORTS_PLANE)
		{
			horz.plane.Texheight = ViewPos.Z + fabs(horz.plane.Texheight);
		}
		GLHorizonPortal ceil(&horz, true);
		ceil.DrawContents();
	}
	if (sector->GetTexture(sector_t::floor) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(sector, false);
		horz.lightlevel = gl_ClampLight(sector->GetFloorLight());
		horz.colormap = sector->ColorMap;
		if (portal->mType == PORTS_PLANE)
		{
			horz.plane.Texheight = ViewPos.Z - fabs(horz.plane.Texheight);
		}
		GLHorizonPortal floor(&horz, true);
		floor.DrawContents();
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

