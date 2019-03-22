// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2003-2018 Christoph Oelckers
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
** gl_decal.cpp
** OpenGL decal processing code
**
*/

#include "doomdata.h"
#include "a_sharedglobal.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"
#include "hw_renderstate.h"

//==========================================================================
//
//
//
//==========================================================================

void HWDecal::DrawDecal(HWDrawInfo *di, FRenderState &state)
{
	auto tex = gltexture;

	// calculate dynamic light effect.
	if (di->Level->HasDynamicLights && !di->isFullbrightScene() && gl_light_sprites)
	{
		// Note: This should be replaced with proper shader based lighting.
		double x, y;
		float out[3];
		decal->GetXY(decal->Side, x, y);
		di->GetDynSpriteLight(nullptr, x, y, zcenter, decal->Side->lighthead, decal->Side->sector->PortalGroup, out);
		state.SetDynLight(out[0], out[1], out[2]);
	}

	// alpha color only has an effect when using an alpha texture.
	if (decal->RenderStyle.Flags & (STYLEF_RedIsAlpha | STYLEF_ColorIsFixed))
	{
		state.SetObjectColor(decal->AlphaColor | 0xff000000);
	}

	state.SetTextureMode(decal->RenderStyle);
	state.SetRenderStyle(decal->RenderStyle);
	state.SetMaterial(tex, CLAMP_XY, decal->Translation, -1);


	// If srcalpha is one it looks better with a higher alpha threshold
	if (decal->RenderStyle.SrcAlpha == STYLEALPHA_One) state.AlphaFunc(Alpha_GEqual, gl_mask_sprite_threshold);
	else state.AlphaFunc(Alpha_Greater, 0.f);


	di->SetColor(state, lightlevel, rellight, di->isFullbrightScene(), Colormap, alpha);
	// for additively drawn decals we must temporarily set the fog color to black.
	PalEntry fc = state.GetFogColor();
	if (decal->RenderStyle.BlendOp == STYLEOP_Add && decal->RenderStyle.DestAlpha == STYLEALPHA_One)
	{
		state.SetFog(0, -1);
	}

	state.SetNormal(Normal);

	if (lightlist == nullptr)
	{
		state.Draw(DT_TriangleStrip, vertindex, 4);
	}
	else
	{
		auto &lightlist = *this->lightlist;

		for (unsigned k = 0; k < lightlist.Size(); k++)
		{
			secplane_t &lowplane = k == lightlist.Size() - 1 ? frontsector->floorplane : lightlist[k + 1].plane;

			float low1 = lowplane.ZatPoint(dv[1].x, dv[1].y);
			float low2 = lowplane.ZatPoint(dv[2].x, dv[2].y);

			if (low1 < dv[1].z || low2 < dv[2].z)
			{
				int thisll = lightlist[k].caster != nullptr ? hw_ClampLight(*lightlist[k].p_lightlevel) : lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = Colormap.FadeColor;
				thiscm.CopyFrom3DLight(&lightlist[k]);
				di->SetColor(state, thisll, rellight, di->isFullbrightScene(), thiscm, alpha);
				if (di->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) thiscm.Decolorize();
				di->SetFog(state, thisll, rellight, di->isFullbrightScene(), &thiscm, false);
				state.SetSplitPlanes(lightlist[k].plane, lowplane);

				state.Draw(DT_TriangleStrip, vertindex, 4);
			}
			if (low1 <= dv[0].z && low2 <= dv[3].z) break;
		}
	}

	rendered_decals++;
	state.SetTextureMode(TM_NORMAL);
	state.SetObjectColor(0xffffffff);
	state.SetFog(fc, -1);
	state.SetDynLight(0, 0, 0);
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawInfo::DrawDecals(FRenderState &state, TArray<HWDecal *> &decals)
{
	side_t *wall = nullptr;
	state.SetDepthMask(false);
	state.SetDepthBias(-1, -128);
	state.SetLightIndex(-1);
	for (auto gldecal : decals)
	{
		if (gldecal->decal->Side != wall)
		{
			wall = gldecal->decal->Side;
			if (gldecal->lightlist != nullptr)
			{
				state.EnableSplit(true);
			}
			else
			{
				state.EnableSplit(false);
				SetFog(state, gldecal->lightlevel, gldecal->rellight, isFullbrightScene(), &gldecal->Colormap, false);
			}
		}
		gldecal->DrawDecal(this, state);
	}
	state.EnableSplit(false);
	state.ClearDepthBias();
	state.SetTextureMode(TM_NORMAL);
	state.SetDepthMask(true);
}

//==========================================================================
//
// This list will never get long, so this code should be ok.
//
//==========================================================================

void HWWall::DrawDecalsForMirror(HWDrawInfo *di, FRenderState &state, TArray<HWDecal *> &decals)
{
	state.SetDepthMask(false);
	state.SetDepthBias(-1, -128);
	state.SetLightIndex(-1);
	di->SetFog(state, lightlevel, rellight + getExtraLight(), di->isFullbrightScene(), &Colormap, false);
	for (auto gldecal : decals)
	{
		if (gldecal->decal->Side == seg->sidedef)
		{
			gldecal->DrawDecal(di, state);
		}
	}
	state.ClearDepthBias();
	state.SetTextureMode(TM_NORMAL);
	state.SetDepthMask(true);
}

//==========================================================================
//
//
//
//==========================================================================

void HWWall::ProcessDecal(HWDrawInfo *di, DBaseDecal *decal, const FVector3 &normal)
{
	line_t * line = seg->linedef;
	side_t * side = seg->sidedef;
	int i;
	float zpos;
	bool flipx, flipy;
	FTextureID decalTile;
	
	
	if (decal->RenderFlags & RF_INVISIBLE) return;
	if (type == RENDERWALL_FFBLOCK && gltexture->isMasked()) return;	// No decals on 3D floors with transparent textures.
	if (seg == nullptr) return;
	
	
	decalTile = decal->PicNum;
	flipx = !!(decal->RenderFlags & RF_XFLIP);
	flipy = !!(decal->RenderFlags & RF_YFLIP);

	
	FTexture *texture = TexMan.GetTexture(decalTile);
	if (texture == NULL) return;

	
	// the sectors are only used for their texture origin coordinates
	// so we don't need the fake sectors for deep water etc.
	// As this is a completely split wall fragment no further splits are
	// necessary for the decal.
	sector_t *frontsector;
	
	// for 3d-floor segments use the model sector as reference
	if ((decal->RenderFlags&RF_CLIPMASK) == RF_CLIPMID) frontsector = decal->Sector;
	else frontsector = seg->frontsector;
	
	switch (decal->RenderFlags & RF_RELMASK)
	{
		default:
			// No valid decal can have this type. If one is encountered anyway
			// it is in some way invalid so skip it.
			return;
			//zpos = decal->z;
			//break;
			
		case RF_RELUPPER:
			if (type != RENDERWALL_TOP) return;
			if (line->flags & ML_DONTPEGTOP)
			{
				zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
			}
			else
			{
				zpos = decal->Z + seg->backsector->GetPlaneTexZ(sector_t::ceiling);
			}
			break;
		case RF_RELLOWER:
			if (type != RENDERWALL_BOTTOM) return;
			if (line->flags & ML_DONTPEGBOTTOM)
			{
				zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
			}
			else
			{
				zpos = decal->Z + seg->backsector->GetPlaneTexZ(sector_t::floor);
			}
			break;
		case RF_RELMID:
			if (type == RENDERWALL_TOP || type == RENDERWALL_BOTTOM) return;
			if (line->flags & ML_DONTPEGBOTTOM)
			{
				zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::floor);
			}
			else
			{
				zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
			}
	}
	FMaterial *tex = FMaterial::ValidateTexture(texture, false);

	// now clip the decal to the actual polygon

	float decalwidth = tex->TextureWidth()  * decal->ScaleX;
	float decalheight = tex->TextureHeight() * decal->ScaleY;
	float decallefto = tex->GetLeftOffset() * decal->ScaleX;
	float decaltopo = tex->GetTopOffset()  * decal->ScaleY;
	
	float leftedge = glseg.fracleft * side->TexelLength;
	float linelength = glseg.fracright * side->TexelLength - leftedge;
	
	// texel index of the decal's left edge
	float decalpixpos = (float)side->TexelLength * decal->LeftDistance - (flipx ? decalwidth - decallefto : decallefto) - leftedge;
	
	float left, right;
	float lefttex, righttex;
	
	// decal is off the left edge
	if (decalpixpos < 0)
	{
		left = 0;
		lefttex = -decalpixpos;
	}
	else
	{
		left = decalpixpos;
		lefttex = 0;
	}
	
	// decal is off the right edge
	if (decalpixpos + decalwidth > linelength)
	{
		right = linelength;
		righttex = right - decalpixpos;
	}
	else
	{
		right = decalpixpos + decalwidth;
		righttex = decalwidth;
	}
	if (right <= left) 
		return;	// nothing to draw
	

	// one texture unit on the wall as vector
	float vx = (glseg.x2 - glseg.x1) / linelength;
	float vy = (glseg.y2 - glseg.y1) / linelength;
	
	DecalVertex dv[4];
	enum
	{
		LL, UL, LR,	UR
	};
	dv[UL].x = dv[LL].x = glseg.x1 + vx * left;
	dv[UL].y = dv[LL].y = glseg.y1 + vy * left;
	
	dv[LR].x = dv[UR].x = glseg.x1 + vx * right;
	dv[LR].y = dv[UR].y = glseg.y1 + vy * right;
	
	zpos += (flipy ? decalheight - decaltopo : decaltopo);
	
	dv[UL].z = dv[UR].z = zpos;
	dv[LL].z = dv[LR].z = dv[UL].z - decalheight;
	dv[UL].v = dv[UR].v = tex->GetVT();
	
	dv[UL].u = dv[LL].u = tex->GetU(lefttex / decal->ScaleX);
	dv[LR].u = dv[UR].u = tex->GetU(righttex / decal->ScaleX);
	dv[LL].v = dv[LR].v = tex->GetVB();
	
	// now clip to the top plane
	float vzt = (ztop[UL] - ztop[LL]) / linelength;
	float topleft = ztop[LL] + vzt * left;
	float topright = ztop[LL] + vzt * right;
	
	// completely below the wall
	if (topleft < dv[LL].z && topright < dv[LR].z)
		return;
	
	if (topleft < dv[UL].z || topright < dv[UR].z)
	{
		// decal has to be clipped at the top
		// let texture clamping handle all extreme cases
		dv[UL].v = (dv[UL].z - topleft) / (dv[UL].z - dv[LL].z)*dv[LL].v;
		dv[UR].v = (dv[UR].z - topright) / (dv[UR].z - dv[LR].z)*dv[LR].v;
		dv[UL].z = topleft;
		dv[UR].z = topright;
	}
	
	// now clip to the bottom plane
	float vzb = (zbottom[UL] - zbottom[LL]) / linelength;
	float bottomleft = zbottom[LL] + vzb * left;
	float bottomright = zbottom[LL] + vzb * right;
	
	// completely above the wall
	if (bottomleft > dv[UL].z && bottomright > dv[UR].z)
		return;
	
	if (bottomleft > dv[LL].z || bottomright > dv[LR].z)
	{
		// decal has to be clipped at the bottom
		// let texture clamping handle all extreme cases
		dv[LL].v = (dv[UL].z - bottomleft) / (dv[UL].z - dv[LL].z)*(dv[LL].v - dv[UL].v) + dv[UL].v;
		dv[LR].v = (dv[UR].z - bottomright) / (dv[UR].z - dv[LR].z)*(dv[LR].v - dv[UR].v) + dv[UR].v;
		dv[LL].z = bottomleft;
		dv[LR].z = bottomright;
	}
	
	
	if (flipx)
	{
		float ur = tex->GetUR();
		for (i = 0; i < 4; i++) dv[i].u = ur - dv[i].u;
	}
	if (flipy)
	{
		float vb = tex->GetVB();
		for (i = 0; i < 4; i++) dv[i].v = vb - dv[i].v;
	}

	HWDecal *gldecal = di->AddDecal(type == RENDERWALL_MIRRORSURFACE);
	gldecal->gltexture = tex;
	gldecal->decal = decal;

	if (decal->RenderFlags & RF_FULLBRIGHT)
	{
		gldecal->lightlevel = 255;
		gldecal->rellight = 0;
	}
	else
	{
		gldecal->lightlevel = lightlevel;
		gldecal->rellight = rellight + getExtraLight();
	}

	gldecal->Colormap = Colormap;

	if (di->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING)
	{
		gldecal->Colormap.Decolorize();
	}

	gldecal->alpha = decal->Alpha;
	gldecal->zcenter = zpos - decalheight * 0.5f;
	gldecal->frontsector = frontsector;
	gldecal->Normal = normal;
	gldecal->lightlist = lightlist;
	memcpy(gldecal->dv, dv, sizeof(dv));
	
	auto verts = screen->mVertexData->AllocVertices(4);
	gldecal->vertindex = verts.second;
	
	for (i = 0; i < 4; i++)
	{
		verts.first[i].Set(dv[i].x, dv[i].z, dv[i].y, dv[i].u, dv[i].v);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void HWWall::ProcessDecals(HWDrawInfo *di)
{
	if (seg->sidedef != nullptr)
	{
		DBaseDecal *decal = seg->sidedef->AttachedDecals;
		if (decal)
		{
			auto normal = glseg.Normal();	// calculate the normal only once per wall because it requires a square root.
			while (decal)
			{
				ProcessDecal(di, decal, normal);
				decal = decal->WallNext;
			}
		}
	}
}

