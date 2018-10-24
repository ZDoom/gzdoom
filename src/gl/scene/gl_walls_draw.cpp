// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
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

#include "gl_load/gl_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddWall(GLWall *wall)
{
	if (wall->flags & GLWall::GLWF_TRANSLUCENT)
	{
		auto newwall = drawlists[GLDL_TRANSLUCENT].NewWall();
		*newwall = *wall;
	}
	else
	{
		bool masked = GLWall::passflag[wall->type] == 1 ? false : (wall->gltexture && wall->gltexture->isMasked());
		int list;

		if ((wall->flags & GLWall::GLWF_SKYHACK && wall->type == RENDERWALL_M2S))
		{
			list = GLDL_MASKEDWALLSOFS;
		}
		else
		{
			list = masked ? GLDL_MASKEDWALLS : GLDL_PLAINWALLS;
		}
		auto newwall = drawlists[list].NewWall();
		*newwall = *wall;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddMirrorSurface(GLWall *w)
{
	w->type = RENDERWALL_MIRRORSURFACE;
	auto newwall = drawlists[GLDL_TRANSLUCENTBORDER].NewWall();
	*newwall = *w;

	// Invalidate vertices to allow setting of texture coordinates
	newwall->vertcount = 0;

	FVector3 v = newwall->glseg.Normal();
	auto tcs = newwall->tcs;
	tcs[GLWall::LOLFT].u = tcs[GLWall::LORGT].u = tcs[GLWall::UPLFT].u = tcs[GLWall::UPRGT].u = v.X;
	tcs[GLWall::LOLFT].v = tcs[GLWall::LORGT].v = tcs[GLWall::UPLFT].v = tcs[GLWall::UPRGT].v = v.Z;
	newwall->MakeVertices(this, false);
	newwall->ProcessDecals(this);
}

