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
	// Transform and clip coordinates. Returns true if it was clipped away
	bool FWallCoords::Init(RenderThread* thread, const DVector2& pt1, const DVector2& pt2, seg_t* lineseg)
	{
		auto viewport = thread->Viewport.get();
		RenderPortal* renderportal = thread->Portal.get();

		tleft.X = float(pt1.X * viewport->viewpoint.Sin - pt1.Y * viewport->viewpoint.Cos);
		tright.X = float(pt2.X * viewport->viewpoint.Sin - pt2.Y * viewport->viewpoint.Cos);

		tleft.Y = float(pt1.X * viewport->viewpoint.TanCos + pt1.Y * viewport->viewpoint.TanSin);
		tright.Y = float(pt2.X * viewport->viewpoint.TanCos + pt2.Y * viewport->viewpoint.TanSin);

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			float t = -tleft.X;
			tleft.X = -tright.X;
			tright.X = t;
			swapvalues(tleft.Y, tright.Y);
		}

		float fsx1, fsz1, fsx2, fsz2;

		if (tleft.X >= -tleft.Y)
		{
			if (tleft.X > tleft.Y) return true;	// left edge is off the right side
			if (tleft.Y == 0) return true;
			fsx1 = viewport->CenterX + tleft.X * viewport->CenterX / tleft.Y;
			fsz1 = tleft.Y;
			tx1 = 0.0f;
		}
		else
		{
			if (tright.X < -tright.Y) return true;	// wall is off the left side
			float den = tleft.X - tright.X - tright.Y + tleft.Y;
			if (den == 0) return true;
			fsx1 = 0;
			tx1 = (tleft.X + tleft.Y) / den;
			fsz1 = tleft.Y + (tright.Y - tleft.Y) * tx1;
		}

		if (fsz1 < TOO_CLOSE_Z)
			return true;

		if (tright.X <= tright.Y)
		{
			if (tright.X < -tright.Y) return true;	// right edge is off the left side
			if (tright.Y == 0) return true;
			fsx2 = viewport->CenterX + tright.X * viewport->CenterX / tright.Y;
			fsz2 = tright.Y;
			tx2 = 1.0f;
		}
		else
		{
			if (tleft.X > tleft.Y) return true;	// wall is off the right side
			float den = tright.Y - tleft.Y - tright.X + tleft.X;
			if (den == 0) return true;
			fsx2 = viewwidth;
			tx2 = (tleft.X - tleft.Y) / den;
			fsz2 = tleft.Y + (tright.Y - tleft.Y) * tx2;
		}

		if (fsz2 < TOO_CLOSE_Z)
			return true;

		sx1 = xs_RoundToInt(fsx1);
		sx2 = xs_RoundToInt(fsx2);

		float delta = fsx2 - fsx1;
		float t1 = (sx1 + 0.5f - fsx1) / delta;
		float t2 = (sx2 + 0.5f - fsx1) / delta;
		float invZ1 = 1.0f / fsz1;
		float invZ2 = 1.0f / fsz2;
		sz1 = 1.0f / (invZ1 * (1.0f - t1) + invZ2 * t1);
		sz2 = 1.0f / (invZ1 * (1.0f - t2) + invZ2 * t2);

		if (sx2 <= sx1)
			return true;

		if (lineseg && lineseg->linedef)
		{
			line_t* line = lineseg->linedef;
			if (fabs(line->delta.X) > fabs(line->delta.Y))
			{
				t1 = (lineseg->v1->fX() - line->v1->fX()) / line->delta.X;
				t2 = (lineseg->v2->fX() - line->v1->fX()) / line->delta.X;
			}
			else
			{
				t1 = (lineseg->v1->fY() - line->v1->fY()) / line->delta.Y;
				t2 = (lineseg->v2->fY() - line->v1->fY()) / line->delta.Y;
			}

			if (line->sidedef[0] != lineseg->sidedef)
			{
				t1 = 1.0f - t1;
				t2 = 1.0f - t2;
			}

			tx1 = t1 + tx1 * (t2 - t1);
			tx2 = t1 + tx2 * (t2 - t1);
		}

		return false;
	}

	////////////////////////////////////////////////////////////////////////////

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

	void ProjectedWallTexcoords::ProjectTop(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic)
	{
		side_t* sidedef = lineseg->sidedef;
		line_t* linedef = lineseg->linedef;

		float yscale = GetYScale(sidedef, pic, side_t::top);
		double cameraZ = viewport->viewpoint.Pos.Z;

		double texturemid;
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
		fixed_t xoffset = GetXOffset(lineseg, pic, side_t::top);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::top), WallC, pic, xoffset, texturemid, yscale, false);
	}

	void ProjectedWallTexcoords::ProjectMid(RenderViewport* viewport, sector_t* frontsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic)
	{
		side_t* sidedef = lineseg->sidedef;
		line_t* linedef = lineseg->linedef;

		float yscale = GetYScale(sidedef, pic, side_t::mid);
		double cameraZ = viewport->viewpoint.Pos.Z;

		double texturemid;
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
		fixed_t xoffset = GetXOffset(lineseg, pic, side_t::mid);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::mid), WallC, pic, xoffset, texturemid, yscale, false);
	}

	void ProjectedWallTexcoords::ProjectBottom(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic)
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

		float yscale = GetYScale(sidedef, pic, side_t::bottom);
		double cameraZ = viewport->viewpoint.Pos.Z;

		double texturemid;
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
		fixed_t xoffset = GetXOffset(lineseg, pic, side_t::bottom);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::bottom), WallC, pic, xoffset, texturemid, yscale, false);
	}

	void ProjectedWallTexcoords::ProjectTranslucent(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic)
	{
		line_t* linedef = lineseg->linedef;
		side_t* sidedef = lineseg->sidedef;

		float yscale = GetYScale(sidedef, pic, side_t::mid);
		double cameraZ = viewport->viewpoint.Pos.Z;

		double texZFloor = MAX(frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor));
		double texZCeiling = MIN(frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));

		double texturemid;
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
		fixed_t xoffset = GetXOffset(lineseg, pic, side_t::mid);

		Project(viewport, sidedef->TexelLength * GetXScale(sidedef, pic, side_t::mid), WallC, pic, xoffset, texturemid, yscale, false);
	}

	void ProjectedWallTexcoords::Project3DFloor(RenderViewport* viewport, F3DFloor* rover, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic)
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
		float yscale = pic->GetScale().Y * scaledside->GetTextureYScale(scaledpart);

		double rowoffset = lineseg->sidedef->GetTextureYOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureYOffset(side_t::mid);
		double planez = rover->model->GetPlaneTexZ(sector_t::ceiling);

		fixed_t xoffset = FLOAT2FIXED(lineseg->sidedef->GetTextureXOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureXOffset(side_t::mid));
		if (rowoffset < 0)
		{
			rowoffset += pic->GetHeight();
		}

		double texturemid = (planez - viewport->viewpoint.Pos.Z) * yscale;
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

		Project(viewport, lineseg->sidedef->TexelLength * xscale, WallC, pic, xoffset, texturemid, yscale, false);
	}

	void ProjectedWallTexcoords::Project(RenderViewport *viewport, double walxrepeat, const FWallCoords& WallC, FSoftwareTexture* pic, fixed_t xoffset, double texturemid, float yscale, bool flipx)
	{
		float texwidth = pic->GetWidth();
		float texheight = pic->GetHeight();

		float texU1 = FIXED2FLOAT(xoffset) / texwidth;
		float texU2 = texU1 + walxrepeat / texwidth;
		if (walxrepeat < 0.0)
		{
			texU1 += 1.0f;
			texU2 += 1.0f;
		}
		if (flipx)
		{
			texU1 = 1.0f - texU1;
			texU2 = 1.0f - texU2;
		}

		float texV = texturemid / texheight;

		// Set up some fake vertices as that makes it easier to calculate the gradients using code already known to work.

		Vertex v1;
		v1.x = WallC.sx1 + 0.5f;// WallC.tleft.X;
		v1.y = 0.0f;
		v1.w = WallC.sz1;//WallC.tleft.Y;

		Vertex v2;
		v2.x = WallC.sx2 + 0.5f;// WallC.tright.X;
		v2.y = 0.0f;
		v2.w = WallC.sz2;// WallC.tright.Y;

		v1.u = texU1 * (1.0f - WallC.tx1) + texU2 * WallC.tx1;
		v2.u = texU1 * (1.0f - WallC.tx2) + texU2 * WallC.tx2;
		v1.v = texV;
		v2.v = texV;

		Vertex v3;
		v3.x = v1.x;
		v3.y = v1.y - 100.0f;
		v3.w = v1.w;
		v3.u = v1.u;
		v3.v = v1.v + yscale * 100.0f / texheight;

		// Project to screen space

		v1.w = 1.0f / v1.w;
		v2.w = 1.0f / v2.w;
		v3.w = 1.0f / v3.w;
		v1.y = viewport->CenterY - v1.y * v1.w * viewport->InvZtoScale;
		v2.y = viewport->CenterY - v2.y * v2.w * viewport->InvZtoScale;
		v3.y = viewport->CenterY - v3.y * v3.w * viewport->InvZtoScale;

		// Calculate gradients

		float bottomX = (v2.x - v3.x) * (v1.y - v3.y) - (v1.x - v3.x) * (v2.y - v3.y);
		float bottomY = (v1.x - v3.x) * (v2.y - v3.y) - (v2.x - v3.x) * (v1.y - v3.y);

		wstepX = FindGradientX(bottomX, 1.0f, 1.0f, 1.0f, v1, v2, v3);
		ustepX = FindGradientX(bottomX, v1.u, v2.u, v3.u, v1, v2, v3);
		vstepX = FindGradientX(bottomX, v1.v, v2.v, v3.v, v1, v2, v3);

		wstepY = FindGradientY(bottomY, 1.0f, 1.0f, 1.0f, v1, v2, v3);
		ustepY = FindGradientY(bottomY, v1.u, v2.u, v3.u, v1, v2, v3);
		vstepY = FindGradientY(bottomY, v1.v, v2.v, v3.v, v1, v2, v3);

		startX = v1.x;
		upos = v1.u * v1.w;
		vpos = v1.v * v1.w;
		wpos = v1.w;
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
		spritelight = false;

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
