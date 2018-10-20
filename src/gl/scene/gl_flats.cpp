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
//
//
//==========================================================================

void FDrawInfo::DrawSubsectors(GLFlat *flat, int pass, bool istrans)
{
	int dli = flat->dynlightindex;
	auto vcount = flat->sector->ibocount;

	gl_RenderState.Apply();
	auto iboindex = flat->iboindex;


	if (screen->BuffersArePersistent())
	{
		flat->SetupLights(this, flat->sector->lighthead, lightdata, flat->sector->PortalGroup);
	}
	gl_RenderState.ApplyLightIndex(flat->dynlightindex);
	if (vcount > 0 && !ClipLineShouldBeActive())
	{
		DrawIndexed(DT_Triangles, gl_RenderState, iboindex, vcount);
		flatvertices += vcount;
		flatprimitives++;
	}
	else
	{
		int index = iboindex;
		for (int i = 0; i < flat->sector->subsectorcount; i++)
		{
			subsector_t * sub = flat->sector->subsectors[i];
			if (sub->numlines <= 2) continue;

			if (ss_renderflags[sub->Index()] & flat->renderflags || istrans)
			{
				DrawIndexed(DT_Triangles, gl_RenderState, index, (sub->numlines - 2) * 3, false);
				drawcalls.Unclock();
				flatvertices += sub->numlines;
				flatprimitives++;
			}
			index += (sub->numlines - 2) * 3;
		}
	}

	if (!(flat->renderflags&SSRF_RENDER3DPLANES))
	{
		// Draw the subsectors assigned to it due to missing textures
		gl_subsectorrendernode * node = (flat->renderflags&SSRF_RENDERFLOOR)?
			GetOtherFloorPlanes(flat->sector->sectornum) :
			GetOtherCeilingPlanes(flat->sector->sectornum);

		while (node)
		{
			gl_RenderState.ApplyLightIndex(node->lightindex);
			auto num = node->sub->numlines;
			flatvertices += num;
			flatprimitives++;
			Draw(DT_TriangleFan, gl_RenderState, node->vertexindex, num);
			node = node->next;
		}
		// Flood gaps with the back side's ceiling/floor texture
		// This requires a stencil because the projected plane interferes with
		// the depth buffer
		gl_floodrendernode * fnode = (flat->renderflags&SSRF_RENDERFLOOR) ?
			GetFloodFloorSegs(flat->sector->sectornum) :
			GetFloodCeilingSegs(flat->sector->sectornum);

		gl_RenderState.ApplyLightIndex(flat->dynlightindex);
		while (fnode)
		{
			flatvertices += 12;
			flatprimitives+=3;

			// Push bleeding floor/ceiling textures back a little in the z-buffer
			// so they don't interfere with overlapping mid textures.
			gl_RenderState.SetDepthBias(1, 128);
			SetupFloodStencil(fnode->vertexindex);
			Draw(DT_TriangleFan, gl_RenderState, fnode->vertexindex + 4, 4);
			ClearFloodStencil(fnode->vertexindex);
			gl_RenderState.SetDepthBias(0, 0);

			fnode = fnode->next;
		}

	}
}

//==========================================================================
//
//
//==========================================================================

void FDrawInfo::SetupFloodStencil(int vindex)
{
	int recursion = GLRenderer->mPortalState.GetRecursion();

	// Create stencil 
	gl_RenderState.SetEffect(EFF_STENCIL);
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply();

	gl_RenderState.SetStencil(0, SOP_Increment, SF_ColorMaskOff);

	Draw(DT_TriangleFan, gl_RenderState, vindex, 4);

	gl_RenderState.SetStencil(1, SOP_Keep, SF_DepthMaskOff | SF_DepthTestOff);

	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetEffect(EFF_NONE);
	gl_RenderState.Apply();
}

void FDrawInfo::ClearFloodStencil(int vindex)
{
	int recursion = GLRenderer->mPortalState.GetRecursion();

	gl_RenderState.SetEffect(EFF_STENCIL);
	gl_RenderState.EnableTexture(false);
	gl_RenderState.Apply();

	gl_RenderState.SetStencil(1, SOP_Decrement, SF_ColorMaskOff | SF_DepthMaskOff | SF_DepthTestOff);

	Draw(DT_TriangleFan, gl_RenderState, vindex, 4);

	// restore old stencil op.
	gl_RenderState.SetStencil(0, SOP_Keep, SF_AllOn);
	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetEffect(EFF_NONE);
}

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::DrawFlat(GLFlat *flat, int pass, bool trans)	// trans only has meaning for GLPASS_LIGHTSONLY
{
	int rel = getExtraLight();

	auto &plane = flat->plane;
	gl_RenderState.SetNormal(plane.plane.Normal().X, plane.plane.Normal().Z, plane.plane.Normal().Y);

	switch (pass)
	{
	case GLPASS_ALL:	// Single-pass rendering
		SetColor(flat->lightlevel, rel, flat->Colormap,1.0f);
		SetFog(flat->lightlevel, rel, &flat->Colormap, false);
		if (!flat->gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(flat->FlatColor | 0xff000000);
		if (flat->sector->special != GLSector_Skybox)
		{
			gl_RenderState.ApplyMaterial(flat->gltexture, CLAMP_NONE, 0, -1);
			gl_RenderState.SetPlaneTextureRotation(&plane, flat->gltexture);
			DrawSubsectors(flat, pass, false);
			gl_RenderState.EnableTextureMatrix(false);
		}
		else
		{
			gl_RenderState.ApplyMaterial(flat->gltexture, CLAMP_XY, 0, -1);
			gl_RenderState.SetLightIndex(flat->dynlightindex);
			Draw(DT_TriangleFan, gl_RenderState, flat->iboindex, 4);
			flatvertices += 4;
			flatprimitives++;
		}
		gl_RenderState.SetObjectColor(0xffffffff);
		break;

	case GLPASS_TRANSLUCENT:
		gl_RenderState.SetRenderStyle(flat->renderstyle);
		SetColor(flat->lightlevel, rel, flat->Colormap, flat->alpha);
		SetFog(flat->lightlevel, rel, &flat->Colormap, false);
		if (!flat->gltexture || !flat->gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(flat->FlatColor | 0xff000000);
		if (!flat->gltexture)
		{
			gl_RenderState.AlphaFunc(Alpha_GEqual, 0.f);
			gl_RenderState.EnableTexture(false);
			DrawSubsectors(flat, pass, true);
			gl_RenderState.EnableTexture(true);
		}
		else 
		{
			if (!flat->gltexture->tex->GetTranslucency()) gl_RenderState.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
			else gl_RenderState.AlphaFunc(Alpha_GEqual, 0.f);
			gl_RenderState.ApplyMaterial(flat->gltexture, CLAMP_NONE, 0, -1);
			gl_RenderState.SetPlaneTextureRotation(&plane, flat->gltexture);
			DrawSubsectors(flat, pass, true);
			gl_RenderState.EnableTextureMatrix(false);
		}
		gl_RenderState.SetRenderStyle(DefaultRenderStyle());
		gl_RenderState.SetObjectColor(0xffffffff);
		break;
	}
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

