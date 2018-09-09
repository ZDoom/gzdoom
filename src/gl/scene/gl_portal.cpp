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
#include "gl/data/gl_viewpointbuffer.h"

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

//==========================================================================
//
//
//
//==========================================================================
void GLPortal::ClearScreen(HWDrawInfo *di)
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

//-----------------------------------------------------------------------------
//
// DrawPortalStencil
//
//-----------------------------------------------------------------------------
void GLPortal::DrawPortalStencil(int pass)
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
		if (pass == STP_AllInOne) glDepthMask(false);
		else if (pass == STP_DepthRestore) glDepthRange(1, 1);
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, FFlatVertexBuffer::STENCILTOP_INDEX, 4);
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, FFlatVertexBuffer::STENCILBOTTOM_INDEX, 4);
		if (pass == STP_DepthRestore) glDepthRange(0, 1);
	}
}


//-----------------------------------------------------------------------------
//
// Start
//
//-----------------------------------------------------------------------------

bool GLPortal::Start(bool usestencil, bool doquery, HWDrawInfo *outer_di, HWDrawInfo **pDi)
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
		glStencilFunc(GL_EQUAL, mState->recursion, ~0);		// create stencil
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);		// increment stencil of valid pixels
		{
			glColorMask(0,0,0,0);						// don't write to the graphics buffer
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

				// Use occlusion query to avoid rendering portals that aren't visible
				if (doquery) glBeginQuery(GL_SAMPLES_PASSED, GLRenderer->PortalQueryObject);

				DrawPortalStencil(STP_Stencil);

				if (doquery) glEndQuery(GL_SAMPLES_PASSED);

				// Clear Z-buffer
				glStencilFunc(GL_EQUAL, mState->recursion + 1, ~0);		// draw sky into stencil
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil
				glDepthMask(true);							// enable z-buffer again
				glDepthRange(1, 1);
				glDepthFunc(GL_ALWAYS);
				DrawPortalStencil(STP_DepthClear);

				// set normal drawing mode
				gl_RenderState.EnableTexture(true);
				glDepthFunc(GL_LESS);
				glColorMask(1, 1, 1, 1);
				gl_RenderState.SetEffect(EFF_NONE);
				glDepthRange(0, 1);

				GLuint sampleCount = 1;

				if (doquery) glGetQueryObjectuiv(GLRenderer->PortalQueryObject, GL_QUERY_RESULT, &sampleCount);

				if (sampleCount == 0) 	// not visible
				{
					// restore default stencil op.
					glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
					glStencilFunc(GL_EQUAL, mState->recursion, ~0);		// draw sky into stencil
					return false;
				}
			}
			else
			{
				// No z-buffer is needed therefore we can skip all the complicated stuff that is involved
				// No occlusion queries will be done here. For these portals the overhead is far greater
				// than the benefit.
				// Note: We must draw the stencil with z-write enabled here because there is no second pass!

				glDepthMask(true);
				DrawPortalStencil(STP_AllInOne);
				glStencilFunc(GL_EQUAL, mState->recursion + 1, ~0);		// draw sky into stencil
				glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil
				gl_RenderState.EnableTexture(true);
				glColorMask(1,1,1,1);
				gl_RenderState.SetEffect(EFF_NONE);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(false);							// don't write to Z-buffer!
			}
		}
		mState->recursion++;


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
	(*pDi)->mCurrentPortal = this;

	// save viewpoint
	savedvisibility = outer_di->Viewpoint.camera ? outer_di->Viewpoint.camera->renderflags & RF_MAYBEINVISIBLE : ActorRenderFlags::FromInt(0);

	return true;
}


//-----------------------------------------------------------------------------
//
// End
//
//-----------------------------------------------------------------------------
void GLPortal::End(HWDrawInfo *di, bool usestencil)
{
	bool needdepth = NeedDepthBuffer();

	Clocker c(PortalAll);

	di = static_cast<FDrawInfo*>(di)->EndDrawInfo();
	GLRenderer->mViewpoints->Bind(static_cast<FDrawInfo*>(di)->vpIndex);
	if (usestencil)
	{
		auto &vp = di->Viewpoint;

		// Restore the old view
		if (vp.camera != nullptr) vp.camera->renderflags = (vp.camera->renderflags & ~RF_MAYBEINVISIBLE) | savedvisibility;

		glColorMask(0, 0, 0, 0);						// no graphics
		gl_RenderState.SetEffect(EFF_NONE);
		gl_RenderState.ResetColor();
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply();

		if (needdepth)
		{
			// first step: reset the depth buffer to max. depth
			glDepthRange(1, 1);							// always
			glDepthFunc(GL_ALWAYS);						// write the farthest depth value
			DrawPortalStencil(STP_DepthClear);
		}
		else
		{
			glEnable(GL_DEPTH_TEST);
		}

		// second step: restore the depth buffer to the previous values and reset the stencil
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		glStencilFunc(GL_EQUAL, mState->recursion, ~0);		// draw sky into stencil
		DrawPortalStencil(STP_DepthRestore);
		glDepthFunc(GL_LESS);


		gl_RenderState.EnableTexture(true);
		gl_RenderState.SetEffect(EFF_NONE);
		glColorMask(1, 1, 1, 1);
		mState->recursion--;

		// restore old stencil op.
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_EQUAL, mState->recursion, ~0);		// draw sky into stencil
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

		// This draws a valid z-buffer into the stencil's contents to ensure it
		// doesn't get overwritten by the level's geometry.

		gl_RenderState.ResetColor();
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0, 1);
		glColorMask(0, 0, 0, 1); // mark portal in alpha channel but don't touch color
		gl_RenderState.SetEffect(EFF_STENCIL);
		gl_RenderState.EnableTexture(false);
		gl_RenderState.BlendFunc(GL_ONE, GL_ZERO);
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
		gl_RenderState.Apply();
		DrawPortalStencil(STP_DepthRestore);
		gl_RenderState.SetEffect(EFF_NONE);
		gl_RenderState.EnableTexture(true);
		glColorMask(1, 1, 1, 1); // mark portal in alpha channel but don't touch color

		glDepthFunc(GL_LESS);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// Horizon Portal
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

GLHorizonPortal::GLHorizonPortal(FPortalSceneState *s, GLHorizonInfo * pt, FRenderViewpoint &vp, bool local)
	: GLPortal(s, local)
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
	for (int xx = -32768; xx < 32768; xx += 4096)
	{
		float x = xx + vx;
		for (int yy = -32768; yy < 32768; yy += 4096)
		{
			float y = yy + vy;
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
void GLHorizonPortal::DrawContents(HWDrawInfo *hwdi)
{
	auto di = static_cast<FDrawInfo *>(hwdi);
	Clocker c(PortalAll);

	FMaterial * gltexture;
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

void GLEEHorizonPortal::DrawContents(HWDrawInfo *di)
{
	auto &vp = di->Viewpoint;
	sector_t *sector = portal->mOrigin;
	if (sector->GetTexture(sector_t::floor) == skyflatnum ||
		sector->GetTexture(sector_t::ceiling) == skyflatnum)
	{
		GLSkyInfo skyinfo;
		skyinfo.init(sector->sky, 0);
		GLSkyPortal sky(mState, &skyinfo, true);
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
		GLHorizonPortal ceil(mState, &horz, di->Viewpoint, true);
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
		GLHorizonPortal floor(mState, &horz, di->Viewpoint, true);
		floor.DrawContents(di);
	}
}

const char *GLSkyPortal::GetName() { return "Sky"; }
const char *GLHorizonPortal::GetName() { return "Horizon"; }
const char *GLEEHorizonPortal::GetName() { return "EEHorizon"; }

