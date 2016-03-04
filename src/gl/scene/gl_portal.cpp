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
	gl_RenderState.Set2DMode(false);
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
	planestack.Push(gl_RenderState.GetClipHeightTop());
	planestack.Push(gl_RenderState.GetClipHeightBottom());
	glDisable(GL_CLIP_DISTANCE0);
	gl_RenderState.SetClipHeightBottom(-65536.f);
	gl_RenderState.SetClipHeightTop(65536.f);

	// save viewpoint
	savedviewx=viewx;
	savedviewy=viewy;
	savedviewz=viewz;
	savedviewactor=GLRenderer->mViewActor;
	savedviewangle=viewangle;
	savedviewarea=in_area;
	savedshowviewer = r_showviewer;

	NextPortal = GLRenderer->mCurrentPortal;
	GLRenderer->mCurrentPortal = NULL;	// Portals which need this have to set it themselves
	PortalAll.Unclock();
	return true;
}


inline void GLPortal::ClearClipper()
{
	fixed_t angleOffset = viewangle - savedviewangle;

	clipper.Clear();

	static int call=0;

	// Set the clipper to the minimal visible area
	clipper.SafeAddClipRange(0,0xffffffff);
	for(unsigned int i=0;i<lines.Size();i++)
	{
		angle_t startAngle = R_PointToAnglePrecise(savedviewx, savedviewy, 
												FLOAT2FIXED(lines[i].glseg.x2), FLOAT2FIXED(lines[i].glseg.y2));

		angle_t endAngle = R_PointToAnglePrecise(savedviewx, savedviewy, 
												FLOAT2FIXED(lines[i].glseg.x1), FLOAT2FIXED(lines[i].glseg.y1));

		if (startAngle-endAngle>0) 
		{
			clipper.SafeRemoveClipRangeRealAngles(startAngle + angleOffset, endAngle + angleOffset);
		}
	}

	// and finally clip it to the visible area
	angle_t a1 = GLRenderer->FrustumAngle();
	if (a1<ANGLE_180) clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);

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
	GLRenderer->mCurrentPortal = NextPortal;

	float f;
	planestack.Pop(f);
	gl_RenderState.SetClipHeightBottom(f);
	if (f > -65535.f) glEnable(GL_CLIP_DISTANCE0);
	planestack.Pop(f);
	gl_RenderState.SetClipHeightTop(f);
	if (f < 65535.f) glEnable(GL_CLIP_DISTANCE0);

	if (usestencil)
	{
		if (needdepth) FDrawInfo::EndDrawInfo();

		// Restore the old view
		viewx=savedviewx;
		viewy=savedviewy;
		viewz=savedviewz;
		viewangle=savedviewangle;
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		r_showviewer = savedshowviewer;
		GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));

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
		viewx=savedviewx;
		viewy=savedviewy;
		viewz=savedviewz;
		viewangle=savedviewangle;
		GLRenderer->mViewActor=savedviewactor;
		in_area=savedviewarea;
		r_showviewer = savedshowviewer;
		GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

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
	int old_pm=PlaneMirrorMode;
	int saved_extralight = extralight;

	if (skyboxrecursion>=3)
	{
		ClearScreen();
		return;
	}

	skyboxrecursion++;
	origin->flags|=MF_JUSTHIT;
	extralight = 0;

	PlaneMirrorMode=0;

	bool oldclamp = gl_RenderState.SetDepthClamp(false);
	fixedvec3 viewpos = origin->InterpolatedPosition(r_TicFrac);
	viewx = viewpos.x;
	viewy = viewpos.y;
	viewz = viewpos.z;
	viewangle += origin->PrevAngle + FixedMul(r_TicFrac, origin->angle - origin->PrevAngle);

	// Don't let the viewpoint be too close to a floor or ceiling
	fixed_t floorh = origin->Sector->floorplane.ZatPoint(origin);
	fixed_t ceilh = origin->Sector->ceilingplane.ZatPoint(origin);
	if (viewz<floorh+4*FRACUNIT) viewz=floorh+4*FRACUNIT;
	if (viewz>ceilh-4*FRACUNIT) viewz=ceilh-4*FRACUNIT;


	GLRenderer->mViewActor = origin;

	validcount++;
	inskybox=true;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	GLRenderer->SetViewArea();
	ClearClipper();

	int mapsection = R_PointInSubsector(viewx, viewy)->mapsection;

	SaveMapSection();
	currentmapsection[mapsection>>3] |= 1 << (mapsection & 7);

	GLRenderer->DrawScene();
	origin->flags&=~MF_JUSTHIT;
	inskybox=false;
	gl_RenderState.SetDepthClamp(oldclamp);
	skyboxrecursion--;

	PlaneMirrorMode=old_pm;
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

	viewx += origin->xDisplacement;
	viewy += origin->yDisplacement;
	GLRenderer->mViewActor = NULL;


	validcount++;

	// avoid recursions!
	if (origin->plane != -1) instack[origin->plane]++;

	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	SaveMapSection();
	SetupCoverage();
	ClearClipper();
	GLRenderer->DrawScene();
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

	fixed_t planez = origin->ZatPoint(viewx, viewy);
	viewz = 2*planez - viewz;
	GLRenderer->mViewActor = NULL;
	PlaneMirrorMode = ksgn(origin->c);
	r_showviewer = true;

	validcount++;

	PlaneMirrorFlag++;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));
	ClearClipper();

	float f = FIXED2FLOAT(planez);
	if (PlaneMirrorMode < 0)
	{
		gl_RenderState.SetClipHeightTop(f);	// ceiling mirror: clip everything with a z lower than the portal's ceiling
		gl_RenderState.SetClipHeightBottom(-65536.f);
	}
	else
	{
		gl_RenderState.SetClipHeightBottom(f);	// floor mirror: clip everything with a z higher than the portal's floor
		gl_RenderState.SetClipHeightTop(65536.f);
	}

	glEnable(GL_CLIP_DISTANCE0);
	GLRenderer->DrawScene();
	glDisable(GL_CLIP_DISTANCE0);
	gl_RenderState.SetClipHeightBottom(-65536.f);
	gl_RenderState.SetClipHeightTop(65536.f);
	PlaneMirrorFlag--;
	PlaneMirrorMode=old_pm;
}

//-----------------------------------------------------------------------------
//
// GLPlaneMirrorPortal::DrawContents
//
//-----------------------------------------------------------------------------



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

	GLRenderer->mCurrentPortal = this;
	angle_t startang = viewangle;
	fixed_t startx = viewx;
	fixed_t starty = viewy;

	vertex_t *v1 = linedef->v1;
	vertex_t *v2 = linedef->v2;

	// Reflect the current view behind the mirror.
	if (linedef->dx == 0)
	{ 
		// vertical mirror
		viewx = v1->x - startx + v1->x;

		// Compensation for reendering inaccuracies
		if (startx<v1->x)  viewx -= FRACUNIT/2;
		else viewx += FRACUNIT/2;
	}
	else if (linedef->dy == 0)
	{ 
		// horizontal mirror
		viewy = v1->y - starty + v1->y;

		// Compensation for reendering inaccuracies
		if (starty<v1->y)  viewy -= FRACUNIT/2;
		else viewy += FRACUNIT/2;
	}
	else
	{ 
		// any mirror--use floats to avoid integer overflow. 
		// Use doubles to avoid losing precision which is very important here.

		double dx = FIXED2DBL(v2->x - v1->x);
		double dy = FIXED2DBL(v2->y - v1->y);
		double x1 = FIXED2DBL(v1->x);
		double y1 = FIXED2DBL(v1->y);
		double x = FIXED2DBL(startx);
		double y = FIXED2DBL(starty);

		// the above two cases catch len == 0
		double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = FLOAT2FIXED((x1 + r * dx)*2 - x);
		viewy = FLOAT2FIXED((y1 + r * dy)*2 - y);

		// Compensation for reendering inaccuracies
		FVector2 v(-dx, dy);
		v.MakeUnit();

		viewx+= FLOAT2FIXED(v[1] * renderdepth / 2);
		viewy+= FLOAT2FIXED(v[0] * renderdepth / 2);
	}
	// we cannot afford any imprecisions caused by R_PointToAngle2 here. They'd be visible as seams around the mirror.
	viewangle = 2*R_PointToAnglePrecise (linedef->v1->x, linedef->v1->y,
										linedef->v2->x, linedef->v2->y) - startang;

	GLRenderer->mViewActor = NULL;
	r_showviewer = true;

	validcount++;

	MirrorFlag++;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	clipper.Clear();

	angle_t af = GLRenderer->FrustumAngle();
	if (af<ANGLE_180) clipper.SafeAddClipRangeRealAngles(viewangle+af, viewangle-af);

	angle_t a2 = linedef->v1->GetClipAngle();
	angle_t a1 = linedef->v2->GetClipAngle();
	clipper.SafeAddClipRange(a1,a2);

	GLRenderer->DrawScene();

	MirrorFlag--;
}


int GLLinePortal::ClipSeg(seg_t *seg) 
{ 
	return PClip_Inside;// P_ClipLineToPortal(seg->linedef, line(), viewx, viewy) ? PClip_InFront : PClip_Inside;
}

int GLLinePortal::ClipSubsector(subsector_t *sub)
{ 
	// this seg is completely behind the mirror!
	for(unsigned int i=0;i<sub->numlines;i++)
	{
		if (P_PointOnLineSidePrecise(sub->firstline[i].v1->x, sub->firstline[i].v1->y, line()) == 0) return PClip_Inside;
	}
	return PClip_InFront; 
}

int GLLinePortal::ClipPoint(fixed_t x, fixed_t y) 
{ 
	if (P_PointOnLineSidePrecise(x, y, line())) 
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
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

	GLRenderer->mCurrentPortal = this;

	line_t *origin = glport->reference->mOrigin;
	P_TranslatePortalXY(origin, viewx, viewy);
	P_TranslatePortalAngle(origin, viewangle);
	P_TranslatePortalZ(origin, viewz);

	SaveMapSection();

	for (unsigned i = 0; i < lines.Size(); i++)
	{
		line_t *line = lines[i].seg->linedef->getPortalDestination();
		subsector_t *sub;
		if (line->sidedef[0]->Flags & WALLF_POLYOBJ) 
			sub = R_PointInSubsector(line->v1->x, line->v1->y);
		else sub = line->frontsector->subsectors[0];
		int mapsection = sub->mapsection;
		currentmapsection[mapsection >> 3] |= 1 << (mapsection & 7);
	}

	GLRenderer->mViewActor = NULL;
	validcount++;
	GLRenderer->SetupView(viewx, viewy, viewz, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	ClearClipper();
	GLRenderer->DrawScene();
	RestoreMapSection();
}


/*
int GLLineToLinePortal::ClipSeg(seg_t *seg) 
{ 
	line_t *linedef = lines[0].seg->linedef->getPortalDestination();
	// this seg is completely behind the portal
	//we cannot use P_PointOnLineSide here because it loses the special meaning of 0 == 'on the line'.
	int side1 = DMulScale32(seg->v1->y - linedef->v1->y, linedef->dx, linedef->v1->x - seg->v1->x, linedef->dy);
	int side2 = DMulScale32(seg->v2->y - linedef->v1->y, linedef->dx, linedef->v1->x - seg->v2->x, linedef->dy);

	if (side1 >= 0 && side2 >= 0)
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
}

int GLLineToLinePortal::ClipSubsector(subsector_t *sub) 
{ 
	line_t *masterline = lines[0].seg->linedef->getPortalDestination();

	for(unsigned int i=0;i<sub->numlines;i++)
	{
		if (P_PointOnLineSidePrecise(sub->firstline[i].v1->x, sub->firstline[i].v1->y, masterline) == 0) return PClip_Inside;
	}
	return PClip_InFront; 
}

int GLLineToLinePortal::ClipPoint(fixed_t x, fixed_t y) 
{ 
	line_t *masterline = lines[0].seg->linedef->getPortalDestination();
	if (P_PointOnLineSidePrecise(x, y, masterline)) 
	{
		return PClip_InFront;
	}
	return PClip_Inside; 
}
*/

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
	gl_RenderState.SetCameraPos(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy), FIXED2FLOAT(viewz));


	z=FIXED2FLOAT(sp->texheight);


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



	float vx=FIXED2FLOAT(viewx);
	float vy=FIXED2FLOAT(viewy);

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

	float vz=FIXED2FLOAT(viewz);
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
	if (origin->Sector->GetTexture(sector_t::floor) == skyflatnum ||
		origin->Sector->GetTexture(sector_t::ceiling) == skyflatnum)
	{
		GLSkyInfo skyinfo;
		skyinfo.init(origin->Sector->sky, 0);
		GLSkyPortal sky(&skyinfo, true);
		sky.DrawContents();
	}
	if (origin->Sector->GetTexture(sector_t::ceiling) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(origin->Sector, true);
		horz.lightlevel = gl_ClampLight(origin->Sector->GetCeilingLight());
		horz.colormap = origin->Sector->ColorMap;
		if (origin->special1 == SKYBOX_PLANE)
		{
			horz.plane.texheight = viewz + abs(horz.plane.texheight);
		}
		GLHorizonPortal ceil(&horz, true);
		ceil.DrawContents();
	}
	if (origin->Sector->GetTexture(sector_t::floor) != skyflatnum)
	{
		GLHorizonInfo horz;
		horz.plane.GetFromSector(origin->Sector, false);
		horz.lightlevel = gl_ClampLight(origin->Sector->GetFloorLight());
		horz.colormap = origin->Sector->ColorMap;
		if (origin->special1 == SKYBOX_PLANE)
		{
			horz.plane.texheight = viewz - abs(horz.plane.texheight);
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

