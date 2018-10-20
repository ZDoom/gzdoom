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
/*
** gl_flat.cpp
** Flat rendering
**
*/

#include "gl_load/gl_system.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "p_lnspec.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"


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

