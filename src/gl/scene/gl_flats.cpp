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
#include "gl/scene/gl_scenedrawer.h"
#include "gl/renderer/gl_quaddrawer.h"

//==========================================================================
//
// Flats 
//
//==========================================================================

void FDrawInfo::SetupSubsectorLights(GLFlat *flat, int pass, subsector_t * sub, int *dli)
{
	if (FixedColormap != CM_DEFAULT) return;
	if (dli != NULL && *dli != -1)
	{
		gl_RenderState.ApplyLightIndex(GLRenderer->mLights->GetIndex(*dli));
		(*dli)++;
		return;
	}
	if (flat->SetupSubsectorLights(pass, sub, lightdata))
	{
		int d = GLRenderer->mLights->UploadLights(lightdata);
		if (pass == GLPASS_LIGHTSONLY)
		{
			GLRenderer->mLights->StoreIndex(d);
		}
		else
		{
			gl_RenderState.ApplyLightIndex(d);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::SetupSectorLights(GLFlat *flat, int pass, int *dli)
{
	if (FixedColormap != CM_DEFAULT) return;
	if (dli != NULL && *dli != -1)
	{
		gl_RenderState.ApplyLightIndex(GLRenderer->mLights->GetIndex(*dli));
		(*dli)++;
		return;
	}
	if (flat->SetupSectorLights(pass, flat->sector, lightdata))
	{
		int d = GLRenderer->mLights->UploadLights(lightdata);
		if (pass == GLPASS_LIGHTSONLY)
		{
			GLRenderer->mLights->StoreIndex(d);
		}
		else
		{
			gl_RenderState.ApplyLightIndex(d);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::DrawSubsector(GLFlat *flat, subsector_t * sub)
{
	if (gl.buffermethod != BM_DEFERRED)
	{
		FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
		for (unsigned int k = 0; k < sub->numlines; k++)
		{
			vertex_t *vt = sub->firstline[k].v1;
			ptr->x = vt->fX();
			ptr->z = flat->plane.plane.ZatPoint(vt) + flat->dz;
			ptr->y = vt->fY();
			ptr->u = vt->fX() / 64.f;
			ptr->v = -vt->fY() / 64.f;
			ptr++;
		}
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_FAN);
	}
	else
	{
		// if we cannot access the buffer, use the quad drawer as fallback by splitting the subsector into quads.
		// Trying to get this into the vertex buffer in the processing pass is too costly and this is only used for render hacks.
		FQuadDrawer qd;
		unsigned int vi[4];

		vi[0] = 0;
		for (unsigned int i = 0; i < sub->numlines - 2; i += 2)
		{
			for (unsigned int j = 1; j < 4; j++)
			{
				vi[j] = MIN(i + j, sub->numlines - 1);
			}
			for (unsigned int x = 0; x < 4; x++)
			{
				vertex_t *vt = sub->firstline[vi[x]].v1;
				qd.Set(x, vt->fX(), flat->plane.plane.ZatPoint(vt) + flat->dz, vt->fY(), vt->fX() / 64.f, -vt->fY() / 64.f);
			}
			qd.Render(GL_TRIANGLE_FAN);
		}
	}

	flatvertices += sub->numlines;
	flatprimitives++;
}


//==========================================================================
//
// this is only used by LM_DEFERRED
//
//==========================================================================

void FDrawInfo::ProcessLights(GLFlat *flat, bool istrans)
{
	flat->dynlightindex = GLRenderer->mLights->GetIndexPtr();
	
	if (flat->sector->ibocount > 0 && !gl_RenderState.GetClipLineShouldBeActive())
	{
		SetupSectorLights(flat, GLPASS_LIGHTSONLY, nullptr);
	}
	else
	{
		// Draw the subsectors belonging to this sector
		for (int i = 0; i < flat->sector->subsectorcount; i++)
		{
			subsector_t * sub = flat->sector->subsectors[i];
			if (ss_renderflags[sub->Index()] & flat->renderflags || istrans)
			{
				SetupSubsectorLights(flat, GLPASS_LIGHTSONLY, sub, nullptr);
			}
		}
	}
	
	// Draw the subsectors assigned to it due to missing textures
	if (!(flat->renderflags&SSRF_RENDER3DPLANES))
	{
		gl_subsectorrendernode * node = (flat->renderflags&SSRF_RENDERFLOOR)?
			GetOtherFloorPlanes(flat->sector->sectornum) :
			GetOtherCeilingPlanes(flat->sector->sectornum);

		while (node)
		{
			SetupSubsectorLights(flat, GLPASS_LIGHTSONLY, node->sub, nullptr);
			node = node->next;
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::DrawSubsectors(GLFlat *flat, int pass, bool processlights, bool istrans)
{
	int dli = flat->dynlightindex;
	auto vcount = flat->sector->ibocount;

	gl_RenderState.Apply();
	auto iboindex = flat->iboindex;
	if (gl.legacyMode)
	{
		processlights = false;
		iboindex = -1;
	}

	if (iboindex >= 0)
	{
		if (vcount > 0 && !gl_RenderState.GetClipLineShouldBeActive())
		{
			if (processlights) SetupSectorLights(flat, GLPASS_ALL, &dli);
			drawcalls.Clock();
			glDrawElements(GL_TRIANGLES, vcount, GL_UNSIGNED_INT, GLRenderer->mVBO->GetIndexPointer() + iboindex);
			drawcalls.Unclock();
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
					if (processlights) SetupSubsectorLights(flat, GLPASS_ALL, sub, &dli);
					drawcalls.Clock();
					glDrawElements(GL_TRIANGLES, (sub->numlines - 2) * 3, GL_UNSIGNED_INT, GLRenderer->mVBO->GetIndexPointer() + index);
					drawcalls.Unclock();
					flatvertices += sub->numlines;
					flatprimitives++;
				}
				index += (sub->numlines - 2) * 3;
			}
		}
	}
	else
	{
		// Draw the subsectors belonging to this sector
		for (int i=0; i<flat->sector->subsectorcount; i++)
		{
			subsector_t * sub = flat->sector->subsectors[i];
			if (ss_renderflags[sub->Index()]& flat->renderflags || istrans)
			{
				if (processlights) SetupSubsectorLights(flat, GLPASS_ALL, sub, &dli);
				DrawSubsector(flat, sub);
			}
		}
	}

	// Draw the subsectors assigned to it due to missing textures
	if (!(flat->renderflags&SSRF_RENDER3DPLANES))
	{
		gl_subsectorrendernode * node = (flat->renderflags&SSRF_RENDERFLOOR)?
			GetOtherFloorPlanes(flat->sector->sectornum) :
			GetOtherCeilingPlanes(flat->sector->sectornum);

		while (node)
		{
			if (processlights) SetupSubsectorLights(flat, GLPASS_ALL, node->sub, &dli);
			DrawSubsector(flat, node->sub);
			node = node->next;
		}
	}
}


//==========================================================================
//
// special handling for skyboxes which need texture clamping.
// This will find the bounding rectangle of the sector and just
// draw one single polygon filling that rectangle with a clamped
// texture.
//
//==========================================================================

void FDrawInfo::DrawSkyboxSector(GLFlat *flat, int pass, bool processlights)
{

	float minx = FLT_MAX, miny = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX;

	for (auto ln : flat->sector->Lines)
	{
		float x = ln->v1->fX();
		float y = ln->v1->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
		x = ln->v2->fX();
		y = ln->v2->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
	}

	float z = flat->plane.plane.ZatPoint(0., 0.) + flat->dz;
	static float uvals[] = { 0, 0, 1, 1 };
	static float vvals[] = { 1, 0, 0, 1 };
	int rot = -xs_FloorToInt(flat->plane.Angle / 90.f);

	FQuadDrawer qd;

	qd.Set(0, minx, z, miny, uvals[rot & 3], vvals[rot & 3]);
	qd.Set(1, minx, z, maxy, uvals[(rot + 1) & 3], vvals[(rot + 1) & 3]);
	qd.Set(2, maxx, z, maxy, uvals[(rot + 2) & 3], vvals[(rot + 2) & 3]);
	qd.Set(3, maxx, z, miny, uvals[(rot + 3) & 3], vvals[(rot + 3) & 3]);
	qd.Render(GL_TRIANGLE_FAN);

	flatvertices += 4;
	flatprimitives++;
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
		mDrawer->SetColor(flat->lightlevel, rel, flat->Colormap,1.0f);
		mDrawer->SetFog(flat->lightlevel, rel, &flat->Colormap, false);
		if (!flat->gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(flat->FlatColor | 0xff000000);
		if (flat->sector->special != GLSector_Skybox)
		{
			gl_RenderState.SetMaterial(flat->gltexture, CLAMP_NONE, 0, -1, false);
			gl_RenderState.SetPlaneTextureRotation(&plane, flat->gltexture);
			DrawSubsectors(flat, pass, (pass == GLPASS_ALL || flat->dynlightindex > -1), false);
			gl_RenderState.EnableTextureMatrix(false);
		}
		else
		{
			gl_RenderState.SetMaterial(flat->gltexture, CLAMP_XY, 0, -1, false);
			DrawSkyboxSector(flat, pass, (pass == GLPASS_ALL || flat->dynlightindex > -1));
		}
		gl_RenderState.SetObjectColor(0xffffffff);
		break;

	case GLPASS_LIGHTSONLY:
		if (!trans || flat->gltexture)
		{
			ProcessLights(flat, trans);
		}
		break;

	case GLPASS_TRANSLUCENT:
		if (flat->renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE);
		mDrawer->SetColor(flat->lightlevel, rel, flat->Colormap, flat->alpha);
		mDrawer->SetFog(flat->lightlevel, rel, &flat->Colormap, false);
		if (!flat->gltexture || !flat->gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(flat->FlatColor | 0xff000000);
		if (!flat->gltexture)
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderState.EnableTexture(false);
			DrawSubsectors(flat, pass, false, true);
			gl_RenderState.EnableTexture(true);
		}
		else 
		{
			if (!flat->gltexture->tex->GetTranslucency()) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
			else gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderState.SetMaterial(flat->gltexture, CLAMP_NONE, 0, -1, false);
			gl_RenderState.SetPlaneTextureRotation(&plane, flat->gltexture);
			DrawSubsectors(flat, pass, !gl.legacyMode && (gl.lightmethod == LM_DIRECT || flat->dynlightindex > -1), true);
			gl_RenderState.EnableTextureMatrix(false);
		}
		if (flat->renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.SetObjectColor(0xffffffff);
		break;

	case GLPASS_LIGHTTEX:
	case GLPASS_LIGHTTEX_ADDITIVE:
	case GLPASS_LIGHTTEX_FOGGY:
		DrawLightsCompat(flat, pass);
		break;

	case GLPASS_TEXONLY:
		gl_RenderState.SetMaterial(flat->gltexture, CLAMP_NONE, 0, -1, false);
		gl_RenderState.SetPlaneTextureRotation(&plane, flat->gltexture);
		DrawSubsectors(flat, pass, false, false);
		gl_RenderState.EnableTextureMatrix(false);
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

	if (gl.legacyMode)
	{
		if (PutFlatCompat(flat, fog)) return;
	}
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

