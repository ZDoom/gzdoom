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
#include "hwrenderer/data/flatvertices.h"


//==========================================================================
//
//
//
//==========================================================================
void GLWall::SetDecalAttributes(WallAttributeInfo &wri, HWDrawInfo *di, GLDecal *gldecal, DecalVertex *dv)
{
	auto decal = gldecal->decal;

	// alpha color only has an effect when using an alpha texture.
	if (decal->RenderStyle.Flags & STYLEF_RedIsAlpha)
	{
		wri.attrBuffer.SetObjectColor(decal->AlphaColor);
	}
	wri.attrBuffer.SetTextureMode(decal->RenderStyle, false);


	// If srcalpha is one it looks better with a higher alpha threshold
	if (decal->RenderStyle.SrcAlpha == STYLEALPHA_One) wri.attrBuffer.AlphaFunc(ALPHA_GEQUAL, gl_mask_sprite_threshold);
	else wri.attrBuffer.AlphaFunc(ALPHA_GREATER, 0.f);

	int lightlevel, rellight;
	if (decal->RenderFlags & RF_FULLBRIGHT)
	{
		lightlevel = 255;
		rellight = 0;
	}
	else
	{
		lightlevel = wri.lightlevel;
		rellight = wri.rellight + getExtraLight();
	}

	auto Colormap = wri.Colormap;

	if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING)
	{
		Colormap.Decolorize();
	}

	wri.attrBuffer.SetColor(lightlevel, rellight, di->isFullbrightScene(), Colormap, wri.alpha);
	// for additively drawn decals we must temporarily set the fog color to black.
	auto fc = wri.attrBuffer.uFogColor;
	if (decal->RenderStyle.BlendOp == STYLEOP_Add && decal->RenderStyle.DestAlpha == STYLEALPHA_One)
	{
		wri.attrBuffer.SetFog(0, -1);
	}

	if (wri.lightlist != nullptr)
	{
		auto &lightlist = *wri.lightlist;

		for (unsigned k = 0; k < lightlist.Size(); k++)
		{
			secplane_t &lowplane = k == lightlist.Size() - 1 ? *wri.bottomplane : lightlist[k + 1].plane;

			float low1 = lowplane.ZatPoint(dv[1].x, dv[1].y);
			float low2 = lowplane.ZatPoint(dv[2].x, dv[2].y);

			if (low1 < dv[1].z || low2 < dv[2].z)
			{
				int thisll = lightlist[k].caster != nullptr ? hw_ClampLight(*lightlist[k].p_lightlevel) : wri.lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = Colormap.FadeColor;
				thiscm.CopyFrom3DLight(&lightlist[k]);
				if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) thiscm.Decolorize();
				wri.attrBuffer.SetColor(thisll, rellight, di->isFullbrightScene(), thiscm, wri.alpha);
				wri.attrBuffer.SetFog(thisll, rellight, di->isFullbrightScene(), &thiscm, false);
				wri.attrBuffer.SetSplitPlanes(lightlist[k].plane, lowplane);
			}
			if (low1 <= dv[0].z && low2 <= dv[3].z) break;
		}
		wri.attrBuffer.DisableSplitPlanes();
		auto copydecal = di->AddDecal(type == RENDERWALL_MIRRORSURFACE);
		*copydecal = *gldecal;
		copydecal->attrindex = di->UploadAttributes(wri.attrBuffer);
	}
	else
	{
		gldecal->attrindex = di->UploadAttributes(wri.attrBuffer);
	}

	wri.attrBuffer.uTextureMode = TM_MODULATE;
	wri.attrBuffer.SetObjectColor(1, 1, 1);
	wri.attrBuffer.uFogColor = fc;
}

//==========================================================================
//
//
//
//==========================================================================

void GLWall::ProcessDecal(WallAttributeInfo &wri, HWDrawInfo *di, DBaseDecal *decal, const FVector3 &normal)
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

	
	FTexture *texture = TexMan[decalTile];
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
	dv[1].x = dv[0].x = glseg.x1 + vx * left;
	dv[1].y = dv[0].y = glseg.y1 + vy * left;
	
	dv[3].x = dv[2].x = glseg.x1 + vx * right;
	dv[3].y = dv[2].y = glseg.y1 + vy * right;
	
	zpos += (flipy ? decalheight - decaltopo : decaltopo);
	
	dv[1].z = dv[2].z = zpos;
	dv[0].z = dv[3].z = dv[1].z - decalheight;
	dv[1].v = dv[2].v = tex->GetVT();
	
	dv[1].u = dv[0].u = tex->GetU(lefttex / decal->ScaleX);
	dv[3].u = dv[2].u = tex->GetU(righttex / decal->ScaleX);
	dv[0].v = dv[3].v = tex->GetVB();
	
	// now clip to the top plane
	float vzt = (ztop[1] - ztop[0]) / linelength;
	float topleft = ztop[0] + vzt * left;
	float topright = ztop[0] + vzt * right;
	
	// completely below the wall
	if (topleft < dv[0].z && topright < dv[3].z)
		return;
	
	if (topleft < dv[1].z || topright < dv[2].z)
	{
		// decal has to be clipped at the top
		// let texture clamping handle all extreme cases
		dv[1].v = (dv[1].z - topleft) / (dv[1].z - dv[0].z)*dv[0].v;
		dv[2].v = (dv[2].z - topright) / (dv[2].z - dv[3].z)*dv[3].v;
		dv[1].z = topleft;
		dv[2].z = topright;
	}
	
	// now clip to the bottom plane
	float vzb = (zbottom[1] - zbottom[0]) / linelength;
	float bottomleft = zbottom[0] + vzb * left;
	float bottomright = zbottom[0] + vzb * right;
	
	// completely above the wall
	if (bottomleft > dv[1].z && bottomright > dv[2].z)
		return;
	
	if (bottomleft > dv[0].z || bottomright > dv[3].z)
	{
		// decal has to be clipped at the bottom
		// let texture clamping handle all extreme cases
		dv[0].v = (dv[1].z - bottomleft) / (dv[1].z - dv[0].z)*(dv[0].v - dv[1].v) + dv[1].v;
		dv[3].v = (dv[2].z - bottomright) / (dv[2].z - dv[3].z)*(dv[3].v - dv[2].v) + dv[2].v;
		dv[0].z = bottomleft;
		dv[3].z = bottomright;
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

	GLDecal *gldecal, gldecalbuffer;
	if (wri.lightlist)
	{
		// This will get split up so do not write directly to the destination buffer.
		gldecal = &gldecalbuffer;
	}
	else
	{
		gldecal = di->AddDecal(type == RENDERWALL_MIRRORSURFACE);
	}

	gldecal->gltexture = tex;
	gldecal->decal = decal;
	gldecal->Normal = normal;
	
	auto verts = di->AllocVertices(4);
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
void GLWall::ProcessDecals(WallAttributeInfo &wri, HWDrawInfo *di)
{
	if (seg->sidedef != nullptr)
	{
		DBaseDecal *decal = seg->sidedef->AttachedDecals;
		if (decal)
		{
			auto mywri = wri;	// we need a local copy for this so that the wall's data doesn't get overwritten

			mywri.attrBuffer.SetFog(wri.lightlevel, wri.rellight, di->isFullbrightScene(), &wri.Colormap, false);
			auto normal = glseg.Normal();	// calculate the normal only once per wall because it requires a square root.
			while (decal)
			{
				ProcessDecal(mywri, di, decal, normal);
				decal = decal->WallNext;
			}
		}
	}
}

