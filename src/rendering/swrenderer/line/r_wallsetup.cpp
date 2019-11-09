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
