//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

#include <stdlib.h>
#include <stddef.h>
#include "templates.h"

#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "r_sky.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_wallsetup.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_walldraw.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/line/r_line.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"

namespace swrenderer
{
	ProjectedWallCull ProjectedWallLine::Project(RenderViewport *viewport, double z, const FWallCoords *wallc)
	{
		return Project(viewport, z, z, wallc);
	}

	ProjectedWallCull ProjectedWallLine::Project(RenderViewport *viewport, double z1, double z2, const FWallCoords *wallc)
	{
		float y1 = (float)(viewport->CenterY - z1 * viewport->InvZtoScale / wallc->sz1);
		float y2 = (float)(viewport->CenterY - z2 * viewport->InvZtoScale / wallc->sz2);

		if (y1 < 0 && y2 < 0) // entire line is above screen
		{
			memset(&ScreenY[wallc->sx1], 0, (wallc->sx2 - wallc->sx1) * sizeof(ScreenY[0]));
			return ProjectedWallCull::OutsideAbove;
		}
		else if (y1 > viewheight && y2 > viewheight) // entire line is below screen
		{
			fillshort(&ScreenY[wallc->sx1], wallc->sx2 - wallc->sx1, viewheight);
			return ProjectedWallCull::OutsideBelow;
		}

		if (wallc->sx2 <= wallc->sx1)
			return ProjectedWallCull::Visible;

		float rcp_delta = 1.0f / (wallc->sx2 - wallc->sx1);
		if (y1 >= 0.0f && y2 >= 0.0f && xs_RoundToInt(y1) <= viewheight && xs_RoundToInt(y2) <= viewheight)
		{
			for (int x = wallc->sx1; x < wallc->sx2; x++)
			{
				float t = (x - wallc->sx1) * rcp_delta;
				float y = y1 * (1.0f - t) + y2 * t;
				ScreenY[x] = (short)xs_RoundToInt(y);
			}
		}
		else
		{
			for (int x = wallc->sx1; x < wallc->sx2; x++)
			{
				float t = (x - wallc->sx1) * rcp_delta;
				float y = y1 * (1.0f - t) + y2 * t;
				ScreenY[x] = (short)clamp(xs_RoundToInt(y), 0, viewheight);
			}
		}

		return ProjectedWallCull::Visible;
	}

	ProjectedWallCull ProjectedWallLine::Project(RenderViewport *viewport, const secplane_t &plane, const FWallCoords *wallc, seg_t *curline, bool xflip)
	{
		if (!plane.isSlope())
		{
			return Project(viewport, plane.Zat0() - viewport->viewpoint.Pos.Z, wallc);
		}
		else
		{
			// Get Z coordinates at both ends of the line
			double x, y, den, z1, z2;
			if (xflip)
			{
				x = curline->v2->fX();
				y = curline->v2->fY();
				if (wallc->sx1 == 0 && 0 != (den = wallc->tleft.X - wallc->tright.X + wallc->tleft.Y - wallc->tright.Y))
				{
					double frac = (wallc->tleft.Y + wallc->tleft.X) / den;
					x -= frac * (x - curline->v1->fX());
					y -= frac * (y - curline->v1->fY());
				}
				z1 = plane.ZatPoint(x, y) - viewport->viewpoint.Pos.Z;

				if (wallc->sx2 > wallc->sx1 + 1)
				{
					x = curline->v1->fX();
					y = curline->v1->fY();
					if (wallc->sx2 == viewwidth && 0 != (den = wallc->tleft.X - wallc->tright.X - wallc->tleft.Y + wallc->tright.Y))
					{
						double frac = (wallc->tright.Y - wallc->tright.X) / den;
						x += frac * (curline->v2->fX() - x);
						y += frac * (curline->v2->fY() - y);
					}
					z2 = plane.ZatPoint(x, y) - viewport->viewpoint.Pos.Z;
				}
				else
				{
					z2 = z1;
				}
			}
			else
			{
				x = curline->v1->fX();
				y = curline->v1->fY();
				if (wallc->sx1 == 0 && 0 != (den = wallc->tleft.X - wallc->tright.X + wallc->tleft.Y - wallc->tright.Y))
				{
					double frac = (wallc->tleft.Y + wallc->tleft.X) / den;
					x += frac * (curline->v2->fX() - x);
					y += frac * (curline->v2->fY() - y);
				}
				z1 = plane.ZatPoint(x, y) - viewport->viewpoint.Pos.Z;

				if (wallc->sx2 > wallc->sx1 + 1)
				{
					x = curline->v2->fX();
					y = curline->v2->fY();
					if (wallc->sx2 == viewwidth && 0 != (den = wallc->tleft.X - wallc->tright.X - wallc->tleft.Y + wallc->tright.Y))
					{
						double frac = (wallc->tright.Y - wallc->tright.X) / den;
						x -= frac * (x - curline->v1->fX());
						y -= frac * (y - curline->v1->fY());
					}
					z2 = plane.ZatPoint(x, y) - viewport->viewpoint.Pos.Z;
				}
				else
				{
					z2 = z1;
				}
			}

			return Project(viewport, z1, z2, wallc);
		}
	}

	void ProjectedWallLine::ClipTop(int x1, int x2, const DrawSegmentClipInfo& clip)
	{
		for (int i = x1; i < x2; i++)
		{
			ScreenY[i] = std::max(ScreenY[i], clip.sprtopclip[i]);
		}
	}

	void ProjectedWallLine::ClipBottom(int x1, int x2, const DrawSegmentClipInfo& clip)
	{
		for (int i = x1; i < x2; i++)
		{
			ScreenY[i] = std::min(ScreenY[i], clip.sprbottomclip[i]);
		}
	}

	/////////////////////////////////////////////////////////////////////////

	void FWallTmapVals::InitFromWallCoords(RenderThread* thread, const FWallCoords* wallc)
	{
		const FVector2* left = &wallc->tleft;
		const FVector2* right = &wallc->tright;

		if (thread->Portal->MirrorFlags & RF_XFLIP)
		{
			swapvalues(left, right);
		}

		UoverZorg = left->X * thread->Viewport->CenterX;
		UoverZstep = -left->Y;
		InvZorg = (left->X - right->X) * thread->Viewport->CenterX;
		InvZstep = right->Y - left->Y;
	}

	void FWallTmapVals::InitFromLine(RenderThread* thread, seg_t* line)
	{
		auto viewport = thread->Viewport.get();
		auto renderportal = thread->Portal.get();

		vertex_t* v1 = line->linedef->v1;
		vertex_t* v2 = line->linedef->v2;

		if (line->linedef->sidedef[0] != line->sidedef)
		{
			swapvalues(v1, v2);
		}

		DVector2 left = v1->fPos() - viewport->viewpoint.Pos;
		DVector2 right = v2->fPos() - viewport->viewpoint.Pos;

		double viewspaceX1 = left.X * viewport->viewpoint.Sin - left.Y * viewport->viewpoint.Cos;
		double viewspaceX2 = right.X * viewport->viewpoint.Sin - right.Y * viewport->viewpoint.Cos;
		double viewspaceY1 = left.X * viewport->viewpoint.TanCos + left.Y * viewport->viewpoint.TanSin;
		double viewspaceY2 = right.X * viewport->viewpoint.TanCos + right.Y * viewport->viewpoint.TanSin;

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			viewspaceX1 = -viewspaceX1;
			viewspaceX2 = -viewspaceX2;
		}

		UoverZorg = float(viewspaceX1 * viewport->CenterX);
		UoverZstep = float(-viewspaceY1);
		InvZorg = float((viewspaceX1 - viewspaceX2) * viewport->CenterX);
		InvZstep = float(viewspaceY2 - viewspaceY1);
	}

	/////////////////////////////////////////////////////////////////////////

	void ProjectedWallTexcoords::ProjectTop(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		side_t* sidedef = lineseg->sidedef;
		line_t* linedef = lineseg->linedef;

		yscale = GetYScale(sidedef, pic, side_t::top);
		double cameraZ = viewport->viewpoint.Pos.Z;

		if (yscale >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGTOP)
			{ // top of texture at top
				texturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale;
			}
			else
			{ // bottom of texture at bottom
				texturemid = (backsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale + pic->GetHeight();
			}
		}
		else
		{ // upside down
			if (linedef->flags & ML_DONTPEGTOP)
			{ // bottom of texture at top
				texturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale + pic->GetHeight();
			}
			else
			{ // top of texture at bottom
				texturemid = (backsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale;
			}
		}

		texturemid += GetRowOffset(lineseg, pic, side_t::top);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::top), x1, x2, WallT);
		xoffset = GetXOffset(lineseg, pic, side_t::top);
	}

	void ProjectedWallTexcoords::ProjectMid(RenderViewport* viewport, sector_t* frontsector, seg_t* lineseg, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		side_t* sidedef = lineseg->sidedef;
		line_t* linedef = lineseg->linedef;

		yscale = GetYScale(sidedef, pic, side_t::mid);
		double cameraZ = viewport->viewpoint.Pos.Z;

		if (yscale >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				texturemid = (frontsector->GetPlaneTexZ(sector_t::floor) - cameraZ) * yscale + pic->GetHeight();
			}
			else
			{ // top of texture at top
				texturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale;
			}
		}
		else
		{ // upside down
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // top of texture at bottom
				texturemid = (frontsector->GetPlaneTexZ(sector_t::floor) - cameraZ) * yscale;
			}
			else
			{ // bottom of texture at top
				texturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - cameraZ) * yscale + pic->GetHeight();
			}
		}

		texturemid += GetRowOffset(lineseg, pic, side_t::mid);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::mid), x1, x2, WallT);
		xoffset = GetXOffset(lineseg, pic, side_t::mid);
	}

	void ProjectedWallTexcoords::ProjectBottom(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		side_t* sidedef = lineseg->sidedef;
		line_t* linedef = lineseg->linedef;

		double frontlowertop = frontsector->GetPlaneTexZ(sector_t::ceiling);
		if (frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			// Putting sky ceilings on the front and back of a line alters the way unpegged
			// positioning works.
			frontlowertop = backsector->GetPlaneTexZ(sector_t::ceiling);
		}

		yscale = GetYScale(sidedef, pic, side_t::bottom);
		double cameraZ = viewport->viewpoint.Pos.Z;

		if (yscale >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				texturemid = (frontlowertop - cameraZ) * yscale;
			}
			else
			{ // top of texture at top
				texturemid = (backsector->GetPlaneTexZ(sector_t::floor) - cameraZ) * yscale;
			}
		}
		else
		{ // upside down
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // top of texture at bottom
				texturemid = (frontlowertop - cameraZ) * yscale;
			}
			else
			{ // bottom of texture at top
				texturemid = (backsector->GetPlaneTexZ(sector_t::floor) - cameraZ) * yscale + pic->GetHeight();
			}
		}

		texturemid += GetRowOffset(lineseg, pic, side_t::bottom);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::bottom), x1, x2, WallT);
		xoffset = GetXOffset(lineseg, pic, side_t::bottom);
	}

	void ProjectedWallTexcoords::ProjectTranslucent(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		line_t* linedef = lineseg->linedef;
		side_t* sidedef = lineseg->sidedef;

		yscale = GetYScale(sidedef, pic, side_t::mid);
		double cameraZ = viewport->viewpoint.Pos.Z;

		double texZFloor = MAX(frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor));
		double texZCeiling = MIN(frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));

		if (yscale >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				texturemid = (texZFloor - cameraZ) * yscale + pic->GetHeight();
			}
			else
			{ // top of texture at top
				texturemid = (texZCeiling - cameraZ) * yscale;
			}
		}
		else
		{ // upside down
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // top of texture at bottom
				texturemid = (texZFloor - cameraZ) * yscale;
			}
			else
			{ // bottom of texture at top
				texturemid = (texZCeiling - cameraZ) * yscale + pic->GetHeight();
			}
		}

		texturemid += GetRowOffset(lineseg, pic, side_t::mid);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::mid), x1, x2, WallT);
		xoffset = GetXOffset(lineseg, pic, side_t::mid);
	}

	void ProjectedWallTexcoords::Project3DFloor(RenderViewport* viewport, F3DFloor* rover, seg_t* lineseg, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		// find positioning
		side_t* scaledside;
		side_t::ETexpart scaledpart;
		if (rover->flags & FF_UPPERTEXTURE)
		{
			scaledside = lineseg->sidedef;
			scaledpart = side_t::top;
		}
		else if (rover->flags & FF_LOWERTEXTURE)
		{
			scaledside = lineseg->sidedef;
			scaledpart = side_t::bottom;
		}
		else
		{
			scaledside = rover->master->sidedef[0];
			scaledpart = side_t::mid;
		}

		double xscale = pic->GetScale().X * scaledside->GetTextureXScale(scaledpart);
		yscale = pic->GetScale().Y * scaledside->GetTextureYScale(scaledpart);

		double rowoffset = lineseg->sidedef->GetTextureYOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureYOffset(side_t::mid);
		double planez = rover->model->GetPlaneTexZ(sector_t::ceiling);

		xoffset = FLOAT2FIXED(lineseg->sidedef->GetTextureXOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureXOffset(side_t::mid));
		if (rowoffset < 0)
		{
			rowoffset += pic->GetHeight();
		}

		texturemid = (planez - viewport->viewpoint.Pos.Z) * yscale;
		if (pic->useWorldPanning(lineseg->GetLevel()))
		{
			// rowoffset is added before the multiply so that the masked texture will
			// still be positioned in world units rather than texels.

			texturemid = texturemid + rowoffset * yscale;
			xoffset = xs_RoundToInt(xoffset * xscale);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			texturemid += rowoffset;
		}

		Project(viewport, lineseg->sidedef->TexelLength * xscale, x1, x2, WallT);
	}

	void ProjectedWallTexcoords::ProjectSprite(RenderViewport* viewport, double topZ, double scale, bool flipX, bool flipY, int x1, int x2, const FWallTmapVals& WallT, FSoftwareTexture* pic)
	{
		yscale = 1.0 / scale;
		texturemid = pic->GetTopOffset(0) + (topZ - viewport->viewpoint.Pos.Z) * yscale;
		if (flipY)
		{
			yscale = -yscale;
			texturemid -= pic->GetHeight();
		}

		Project(viewport, pic->GetWidth(), x1, x2, WallT, flipX);
	}

	void ProjectedWallTexcoords::Project(RenderViewport *viewport, double walxrepeat, int x1, int x2, const FWallTmapVals &WallT, bool flipx)
	{
		this->walxrepeat = walxrepeat;
		this->x1 = x1;
		this->x2 = x2;
		this->WallT = WallT;
		this->flipx = flipx;
		CenterX = viewport->CenterX;
		WallTMapScale2 = viewport->WallTMapScale2;
		valid = true;
	}

	float ProjectedWallTexcoords::VStep(int x) const
	{
		float uOverZ = WallT.UoverZorg + WallT.UoverZstep * (float)(x1 + 0.5 - CenterX);
		float invZ = WallT.InvZorg + WallT.InvZstep * (float)(x1 + 0.5 - CenterX);
		float uGradient = WallT.UoverZstep;
		float zGradient = WallT.InvZstep;
		float depthScale = (float)(WallT.InvZstep * WallTMapScale2);
		float depthOrg = (float)(-WallT.UoverZstep * WallTMapScale2);
		float u = (uOverZ + uGradient * (x - x1)) / (invZ + zGradient * (x - x1));

		return depthOrg + u * depthScale;
	}

	fixed_t ProjectedWallTexcoords::UPos(int x) const
	{
		float uOverZ = WallT.UoverZorg + WallT.UoverZstep * (float)(x1 + 0.5 - CenterX);
		float invZ = WallT.InvZorg + WallT.InvZstep * (float)(x1 + 0.5 - CenterX);
		float uGradient = WallT.UoverZstep;
		float zGradient = WallT.InvZstep;
		float u = (uOverZ + uGradient * (x - x1)) / (invZ + zGradient * (x - x1));

		fixed_t value;
		if (walxrepeat < 0.0)
		{
			float xrepeat = -walxrepeat;
			value = (fixed_t)((xrepeat - u * xrepeat) * FRACUNIT);
		}
		else
		{
			value = (fixed_t)(u * walxrepeat * FRACUNIT);
		}

		if (flipx)
		{
			value = (int)walxrepeat - 1 - value;
		}

		return value + xoffset;
	}

	double ProjectedWallTexcoords::GetRowOffset(seg_t* lineseg, FSoftwareTexture* tex, side_t::ETexpart texpart)
	{
		double yrepeat = GetYScale(lineseg->sidedef, tex, texpart);
		double rowoffset = lineseg->sidedef->GetTextureYOffset(texpart);
		if (yrepeat >= 0)
		{
			// check if top of texture at top:
			bool top_at_top =
				(texpart == side_t::top && (lineseg->linedef->flags & ML_DONTPEGTOP)) ||
				(texpart != side_t::top && !(lineseg->linedef->flags & ML_DONTPEGBOTTOM));

			if (rowoffset < 0 && top_at_top)
			{
				rowoffset += tex->GetHeight();
			}
		}
		else
		{
			rowoffset = -rowoffset;
		}

		if (tex->useWorldPanning(lineseg->GetLevel()))
		{
			return rowoffset * yrepeat;
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			return rowoffset;
		}
	}

	fixed_t ProjectedWallTexcoords::GetXOffset(seg_t* lineseg, FSoftwareTexture* tex, side_t::ETexpart texpart)
	{
		fixed_t TextureOffsetU = FLOAT2FIXED(lineseg->sidedef->GetTextureXOffset(texpart));
		double xscale = GetXScale(lineseg->sidedef, tex, texpart);

		fixed_t xoffset;
		if (tex->useWorldPanning(lineseg->GetLevel()))
		{
			xoffset = xs_RoundToInt(TextureOffsetU * xscale);
		}
		else
		{
			xoffset = TextureOffsetU;
		}

		if (xscale < 0)
		{
			xoffset = -xoffset;
		}

		return xoffset;
	}

	double ProjectedWallTexcoords::GetXScale(side_t* sidedef, FSoftwareTexture* tex, side_t::ETexpart texpart)
	{
		double TextureScaleU = sidedef->GetTextureXScale(texpart);
		return tex->GetScale().X * TextureScaleU;
	}

	double ProjectedWallTexcoords::GetYScale(side_t* sidedef, FSoftwareTexture* tex, side_t::ETexpart texpart)
	{
		double TextureScaleV = sidedef->GetTextureYScale(texpart);
		return tex->GetScale().Y * TextureScaleV;
	}

	/////////////////////////////////////////////////////////////////////////

	void ProjectedWallLight::SetLightLeft(RenderThread *thread, const FWallCoords &wallc)
	{
		x1 = wallc.sx1;

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedColormap() == nullptr && cameraLight->FixedLightLevel() < 0)
		{
			lightleft = float(thread->Light->WallVis(wallc.sz1, foggy));
			lightstep = float((thread->Light->WallVis(wallc.sz2, foggy) - lightleft) / (wallc.sx2 - wallc.sx1));
		}
		else
		{
			lightleft = 1;
			lightstep = 0;
		}
	}

	void ProjectedWallLight::SetColormap(const sector_t *frontsector, seg_t *lineseg, lightlist_t *lit)
	{
		if (!lit)
		{
			basecolormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
			foggy = frontsector->Level->fadeto || frontsector->Colormap.FadeColor || (frontsector->Level->flags & LEVEL_HASFADETABLE);

			if (!(lineseg->sidedef->Flags & WALLF_POLYOBJ))
				lightlevel = lineseg->sidedef->GetLightLevel(foggy, frontsector->lightlevel);
			else
				lightlevel = frontsector->GetLightLevel();
		}
		else
		{
			basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
			foggy = frontsector->Level->fadeto || basecolormap->Fade || (frontsector->Level->flags & LEVEL_HASFADETABLE);
			lightlevel = lineseg->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr);
		}
	}
}
