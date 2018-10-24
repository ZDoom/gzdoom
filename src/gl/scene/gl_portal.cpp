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
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "gl/scene/gl_portal.h"
#include "gl/data/gl_viewpointbuffer.h"
#include "hwrenderer/utility/hw_lighting.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//
//
// General portal handling code
//
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

EXTERN_CVAR(Int, r_mirror_recursions)

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
	: HWPortal(s, local)
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
		di->ClearScreen();
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


	gl_RenderState.ApplyMaterial(gltexture, CLAMP_NONE, 0, -1);
	gl_RenderState.SetObjectColor(origin->specialcolor);

	gl_RenderState.SetPlaneTextureRotation(sp, gltexture);
	gl_RenderState.AlphaFunc(Alpha_GEqual, 0.f);
	gl_RenderState.SetRenderStyle(STYLE_Source);
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

