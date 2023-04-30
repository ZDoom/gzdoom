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

#include "hw_dynlightdata.h"
#include "hw_cvars.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hw_material.h"
#include "actor.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// 
//
//==========================================================================

void HWDrawInfo::AddWall(HWWall *wall)
{
	if (wall->flags & HWWall::HWF_TRANSLUCENT)
	{
		auto newwall = drawlists[GLDL_TRANSLUCENT].NewWall();
		*newwall = *wall;
	}
	else
	{
		bool masked = HWWall::passflag[wall->type] == 1 ? false : (wall->texture && wall->texture->isMasked());
		int list;

		if (wall->flags & HWWall::HWF_SKYHACK && wall->type == RENDERWALL_M2S)
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

void HWDrawInfo::AddMirrorSurface(HWWall *w, FRenderState& state)
{
	w->type = RENDERWALL_MIRRORSURFACE;
	auto newwall = drawlists[GLDL_TRANSLUCENTBORDER].NewWall();
	*newwall = *w;

	// Invalidate vertices to allow setting of texture coordinates
	newwall->vertcount = 0;

	FVector3 v = newwall->glseg.Normal();
	auto tcs = newwall->tcs;
	tcs[HWWall::LOLFT].u = tcs[HWWall::LORGT].u = tcs[HWWall::UPLFT].u = tcs[HWWall::UPRGT].u = v.X;
	tcs[HWWall::LOLFT].v = tcs[HWWall::LORGT].v = tcs[HWWall::UPLFT].v = tcs[HWWall::UPRGT].v = v.Z;
	newwall->MakeVertices(this, state, false);

	bool hasDecals = newwall->seg->sidedef && newwall->seg->sidedef->AttachedDecals;
	if (hasDecals && Level->HasDynamicLights && !isFullbrightScene())
	{
		newwall->SetupLights(this, state, lightdata);
	}
	newwall->ProcessDecals(this, state);
	newwall->dynlightindex = -1; // the environment map should not be affected by lights - only the decals.
}

//==========================================================================
//
// FDrawInfo::AddFlat
//
// Checks texture, lighting and translucency settings and puts this
// plane in the appropriate render list.
//
//==========================================================================

void HWDrawInfo::AddFlat(HWFlat *flat, bool fog)
{
	int list;

	if (flat->renderstyle != STYLE_Translucent || flat->alpha < 1.f - FLT_EPSILON || fog || flat->texture == nullptr)
	{
		// translucent 3D floors go into the regular translucent list, translucent portals go into the translucent border list.
		list = (flat->renderflags&SSRF_RENDER3DPLANES) ? GLDL_TRANSLUCENT : GLDL_TRANSLUCENTBORDER;
	}
	else if (flat->texture->GetTranslucency())
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
	else //if (flat->hacktype != SSRF_FLOODHACK) // The flood hack may later need different treatment but with the current setup can go into the existing render list.
	{
		bool masked = flat->texture->isMasked() && ((flat->renderflags&SSRF_RENDER3DPLANES) || flat->stack);
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
void HWDrawInfo::AddSprite(HWSprite *sprite, bool translucent)
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

