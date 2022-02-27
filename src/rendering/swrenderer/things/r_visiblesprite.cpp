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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"

#include "doomdef.h"
#include "m_swap.h"

#include "filesystem.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/line/r_renderdrawsegment.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void VisibleSprite::Render(RenderThread *thread, Fake3DTranslucent clip3DFloor)
	{
		if (IsModel())
		{
			// Draw segments behind model
			DrawSegmentList *segmentlist = thread->DrawSegments.get();
			RenderPortal *renderportal = thread->Portal.get();
			for (unsigned int index = 0; index != segmentlist->TranslucentSegmentsCount(); index++)
			{
				DrawSegment *ds = segmentlist->TranslucentSegment(index);
				if (ds->drawsegclip.SubsectorDepth >= SubsectorDepth && ds->drawsegclip.CurrentPortalUniq == renderportal->CurrentPortalUniq)
				{
					int r1 = max<int>(ds->x1, 0);
					int r2 = min<int>(ds->x2, viewwidth - 1);

					RenderDrawSegment renderer(thread);
					renderer.Render(ds, r1, r2, clip3DFloor);
				}
			}

			Render(thread, nullptr, nullptr, 0, 0, clip3DFloor);
			return;
		}

		short *clipbot = thread->clipbot;
		short *cliptop = thread->cliptop;

		VisibleSprite *spr = this;

		int x1, x2;
		short topclip, botclip;
		FSWColormap *colormap = spr->Light.BaseColormap;
		int colormapnum = spr->Light.ColormapNum;
		F3DFloor *rover;

		Clip3DFloors *clip3d = thread->Clip3D.get();

		// [RH] Check for particles
		if (spr->IsParticle())
		{
			// kg3D - reject invisible parts
			if (clip3DFloor.clipBottom && spr->gpos.Z <= clip3DFloor.sclipBottom) return;
			if (clip3DFloor.clipTop && spr->gpos.Z >= clip3DFloor.sclipTop) return;

			spr->Render(thread, nullptr, nullptr, 0, 0, clip3DFloor);
			return;
		}

		x1 = spr->x1;
		x2 = spr->x2;

		// [RH] Quickly reject sprites with bad x ranges.
		if (x1 >= x2)
			return;

		// [RH] Sprites split behind a one-sided line can also be discarded.
		if (spr->sector == nullptr)
			return;

		// kg3D - reject invisible parts
		if (clip3DFloor.clipBottom && spr->gzt <= clip3DFloor.sclipBottom) return;
		if (clip3DFloor.clipTop && spr->gzb >= clip3DFloor.sclipTop) return;

		// kg3D - correct colors now
		CameraLight *cameraLight = CameraLight::Instance();
		if (!cameraLight->FixedColormap() && cameraLight->FixedLightLevel() < 0 && spr->sector->e && spr->sector->e->XFloor.lightlist.Size())
		{
			double clipTop;
			if (clip3DFloor.clipTop)
				clipTop = clip3DFloor.clipTop;
			else
				clipTop = spr->sector->ceilingplane.ZatPoint(thread->Viewport->viewpoint.Pos);

			sector_t *sec = nullptr;
			FDynamicColormap *mybasecolormap = nullptr;
			for (int i = spr->sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			{
				if (clipTop <= spr->sector->e->XFloor.lightlist[i].plane.Zat0())
				{
					rover = spr->sector->e->XFloor.lightlist[i].caster;
					if (rover)
					{
						if (rover->flags & FF_DOUBLESHADOW && clipTop <= rover->bottom.plane->Zat0())
						{
							break;
						}
						sec = rover->model;
						auto nc = !!(sec->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);

						if (rover->flags & FF_FADEWALLS)
						{
							mybasecolormap = GetSpriteColorTable(sec->Colormap, spr->sector->SpecialColors[sector_t::sprites], nc);
						}
						else
						{
							mybasecolormap = GetSpriteColorTable(spr->sector->e->XFloor.lightlist[i].extra_colormap, spr->sector->SpecialColors[sector_t::sprites], nc);
						}
					}
					break;
				}
			}
			// found new values, recalculate
			if (sec)
			{
				bool invertcolormap = (spr->RenderStyle.Flags & STYLEF_InvertOverlay) != 0;
				if (spr->RenderStyle.Flags & STYLEF_InvertSource)
					invertcolormap = !invertcolormap;

				if (RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
				{
					mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
				}

				bool isFullBright = !foggy && (renderflags & RF_FULLBRIGHT);
				bool fadeToBlack = spr->RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0;

				Light.SetColormap(thread, spr->depth, sec->lightlevel, foggy, mybasecolormap, isFullBright, invertcolormap, fadeToBlack, false, false);
			}
		}

		// [RH] Initialize the clipping arrays to their largest possible range
		// instead of using a special "not clipped" value. This eliminates
		// visual anomalies when looking down and should be faster, too.
		topclip = 0;
		botclip = viewheight;

		// killough 3/27/98:
		// Clip the sprite against deep water and/or fake ceilings.
		// [RH] rewrote this to be based on which part of the sector is really visible
		
		auto viewport = thread->Viewport.get();

		double scale = viewport->InvZtoScale * spr->idepth;
		double hzb = -DBL_MAX, hzt = DBL_MAX;

		if (spr->IsVoxel() && spr->floorclip != 0)
		{
			hzb = spr->gzb;
		}

		if (spr->heightsec)
		{ // only things in specially marked sectors
			if (spr->FakeFlatStat != WaterFakeSide::AboveCeiling)
			{
				double hz = spr->heightsec->floorplane.ZatPoint(spr->gpos);
				int h = xs_RoundToInt(viewport->CenterY - (hz - viewport->viewpoint.Pos.Z) * scale);

				if (spr->FakeFlatStat == WaterFakeSide::BelowFloor)
				{ // seen below floor: clip top
					if (!spr->IsVoxel() && h > topclip)
					{
						topclip = short(min(h, viewheight));
					}
					hzt = min(hzt, hz);
				}
				else
				{ // seen in the middle: clip bottom
					if (!spr->IsVoxel() && h < botclip)
					{
						botclip = max<short>(0, h);
					}
					hzb = max(hzb, hz);
				}
			}
			if (spr->FakeFlatStat != WaterFakeSide::BelowFloor && !(spr->heightsec->MoreFlags & SECMF_FAKEFLOORONLY))
			{
				double hz = spr->heightsec->ceilingplane.ZatPoint(spr->gpos);
				int h = xs_RoundToInt(viewport->CenterY - (hz - viewport->viewpoint.Pos.Z) * scale);

				if (spr->FakeFlatStat == WaterFakeSide::AboveCeiling)
				{ // seen above ceiling: clip bottom
					if (!spr->IsVoxel() && h < botclip)
					{
						botclip = max<short>(0, h);
					}
					hzb = max(hzb, hz);
				}
				else
				{ // seen in the middle: clip top
					if (!spr->IsVoxel() && h > topclip)
					{
						topclip = min(h, viewheight);
					}
					hzt = min(hzt, hz);
				}
			}
		}
		// killough 3/27/98: end special clipping for deep water / fake ceilings
		else if (!spr->IsVoxel() && spr->floorclip)
		{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
			int clip = xs_RoundToInt(viewport->CenterY - (spr->texturemid - spr->pic->GetHeight() + spr->floorclip) * spr->yscale);
			if (clip < botclip)
			{
				botclip = max<short>(0, clip);
			}
		}

		if (clip3DFloor.clipBottom)
		{
			if (!spr->IsVoxel())
			{
				double hz = clip3DFloor.sclipBottom;
				if (spr->fakefloor)
				{
					double floorz = spr->fakefloor->top.plane->Zat0();
					if (viewport->viewpoint.Pos.Z > floorz && floorz == clip3DFloor.sclipBottom)
					{
						hz = spr->fakefloor->bottom.plane->Zat0();
					}
				}
				int h = xs_RoundToInt(viewport->CenterY - (hz - viewport->viewpoint.Pos.Z) * scale);
				if (h < botclip)
				{
					botclip = max<short>(0, h);
				}
			}
			hzb = max(hzb, clip3DFloor.sclipBottom);
		}
		if (clip3DFloor.clipTop)
		{
			if (!spr->IsVoxel())
			{
				double hz = clip3DFloor.sclipTop;
				if (spr->fakeceiling != nullptr)
				{
					double ceilingZ = spr->fakeceiling->bottom.plane->Zat0();
					if (viewport->viewpoint.Pos.Z < ceilingZ && ceilingZ == clip3DFloor.sclipTop)
					{
						hz = spr->fakeceiling->top.plane->Zat0();
					}
				}
				int h = xs_RoundToInt(viewport->CenterY - (hz - viewport->viewpoint.Pos.Z) * scale);
				if (h > topclip)
				{
					topclip = short(min(h, viewheight));
				}
			}
			hzt = min(hzt, clip3DFloor.sclipTop);
		}

		// Make sure bottom clipping stays within the view size
		botclip = min<short>(botclip, viewheight);

		if (topclip >= botclip)
		{
			spr->Light.BaseColormap = colormap;
			spr->Light.ColormapNum = colormapnum;
			return;
		}

		{
			int i = x2 - x1;
			short *clip1 = clipbot + x1;
			short *clip2 = cliptop + x1;
			do
			{
				*clip1++ = botclip;
				*clip2++ = topclip;
			} while (--i);
		}

		// Scan drawsegs from end to start for obscuring segs.
		// The first drawseg that is closer than the sprite is the clip seg.

		DrawSegmentList *segmentlist = thread->DrawSegments.get();
		RenderPortal *renderportal = thread->Portal.get();

		// Render draw segments behind sprite
		if (r_modelscene)
		{
			int subsectordepth = spr->SubsectorDepth;
			for (unsigned int index = 0; index != segmentlist->TranslucentSegmentsCount(); index++)
			{
				DrawSegment *ds = segmentlist->TranslucentSegment(index);
				if (ds->drawsegclip.SubsectorDepth >= subsectordepth && ds->drawsegclip.CurrentPortalUniq == renderportal->CurrentPortalUniq)
				{
					int r1 = max<int>(ds->x1, 0);
					int r2 = min<int>(ds->x2, viewwidth - 1);

					RenderDrawSegment renderer(thread);
					renderer.Render(ds, r1, r2, clip3DFloor);
				}
			}
		}
		else
		{
			for (unsigned int index = 0; index != segmentlist->TranslucentSegmentsCount(); index++)
			{
				DrawSegment *ds = segmentlist->TranslucentSegment(index);

				if (ds->x1 >= x2 || ds->x2 <= x1)
				{
					continue;
				}

				float neardepth = min(ds->WallC.sz1, ds->WallC.sz2);
				float fardepth = max(ds->WallC.sz1, ds->WallC.sz2);

				// Check if sprite is in front of draw seg:
				if ((!spr->IsWallSprite() && neardepth > spr->depth) || ((spr->IsWallSprite() || fardepth > spr->depth) &&
					(spr->gpos.Y - ds->curline->v1->fY()) * (ds->curline->v2->fX() - ds->curline->v1->fX()) -
					(spr->gpos.X - ds->curline->v1->fX()) * (ds->curline->v2->fY() - ds->curline->v1->fY()) <= 0))
				{
					if (ds->drawsegclip.CurrentPortalUniq == renderportal->CurrentPortalUniq)
					{
						int r1 = max<int>(ds->x1, x1);
						int r2 = min<int>(ds->x2, x2);

						RenderDrawSegment renderer(thread);
						renderer.Render(ds, r1, r2, clip3DFloor);
					}
				}
			}
		}

		for (unsigned int groupIndex = 0; groupIndex < segmentlist->SegmentGroups.Size(); groupIndex++)
		{
			auto &group = segmentlist->SegmentGroups[groupIndex];

			if (group.x1 >= x2 || group.x2 <= x1 || group.neardepth > spr->depth)
				continue;

			if (group.fardepth < spr->depth) 
			{
				int r1 = max<int>(group.x1, x1);
				int r2 = min<int>(group.x2, x2);

				// Clip bottom
				short *clip1 = clipbot + r1;
				const short *clip2 = group.sprbottomclip + r1 - group.x1;
				int i = r2 - r1;
				do
				{
					if (*clip1 > *clip2)
						*clip1 = *clip2;
					clip1++;
					clip2++;
				} while (--i);

				// Clip top
				clip1 = cliptop + r1;
				clip2 = group.sprtopclip + r1 - group.x1;
				i = r2 - r1;
				do
				{
					if (*clip1 < *clip2)
						*clip1 = *clip2;
					clip1++;
					clip2++;
				} while (--i);
			}
			else
			{
				for (unsigned int index = group.BeginIndex; index != group.EndIndex; index++)
				{
					DrawSegment *ds = segmentlist->Segment(index);

					// determine if the drawseg obscures the sprite
					if (ds->x1 >= x2 || ds->x2 <= x1 || (!(ds->drawsegclip.silhouette & SIL_BOTH) && !ds->Has3DFloorWalls() && !ds->HasTranslucentMidTexture() && !ds->HasFogBoundary()))
					{
						// does not cover sprite
						continue;
					}

					int r1 = max<int>(ds->x1, x1);
					int r2 = min<int>(ds->x2, x2);

					float neardepth = min(ds->WallC.sz1, ds->WallC.sz2);
					float fardepth = max(ds->WallC.sz1, ds->WallC.sz2);

					// Check if sprite is in front of draw seg:
					if ((!spr->IsWallSprite() && neardepth > spr->depth) || ((spr->IsWallSprite() || fardepth > spr->depth) &&
						(spr->gpos.Y - ds->curline->v1->fY()) * (ds->curline->v2->fX() - ds->curline->v1->fX()) -
						(spr->gpos.X - ds->curline->v1->fX()) * (ds->curline->v2->fY() - ds->curline->v1->fY()) <= 0))
					{
						// seg is behind sprite
						continue;
					}

					// clip this piece of the sprite
					// killough 3/27/98: optimized and made much shorter
					// [RH] Optimized further (at least for VC++;
					// other compilers should be at least as good as before)

					if (ds->drawsegclip.silhouette & SIL_BOTTOM) //bottom sil
					{
						short *clip1 = clipbot + r1;
						const short *clip2 = ds->drawsegclip.sprbottomclip + r1;
						int i = r2 - r1;
						do
						{
							if (*clip1 > *clip2)
								*clip1 = *clip2;
							clip1++;
							clip2++;
						} while (--i);
					}

					if (ds->drawsegclip.silhouette & SIL_TOP)   // top sil
					{
						short *clip1 = cliptop + r1;
						const short *clip2 = ds->drawsegclip.sprtopclip + r1;
						int i = r2 - r1;
						do
						{
							if (*clip1 < *clip2)
								*clip1 = *clip2;
							clip1++;
							clip2++;
						} while (--i);
					}
				}
			}
		}

		// all clipping has been performed, so draw the sprite

		if (!spr->IsVoxel())
		{
			spr->Render(thread, clipbot, cliptop, 0, 0, clip3DFloor);
		}
		else
		{
			// If it is completely clipped away, don't bother drawing it.
			if (cliptop[x2] >= clipbot[x2])
			{
				int i;
				for (i = x1; i < x2; ++i)
				{
					if (cliptop[i] < clipbot[i])
					{
						break;
					}
				}
				if (i == x2)
				{
					spr->Light.BaseColormap = colormap;
					spr->Light.ColormapNum = colormapnum;
					return;
				}
			}
			// Add everything outside the left and right edges to the clipping array
			// for R_DrawVisVoxel().
			if (x1 > 0)
			{
				fillshort(cliptop, x1, viewheight);
			}
			if (x2 < viewwidth - 1)
			{
				fillshort(cliptop + x2, viewwidth - x2, viewheight);
			}
			int minvoxely = spr->gzt <= hzt ? 0 : xs_RoundToInt((spr->gzt - hzt) / spr->yscale);
			int maxvoxely = spr->gzb > hzb ? INT_MAX : xs_RoundToInt((spr->gzt - hzb) / spr->yscale);
			spr->Render(thread, cliptop, clipbot, minvoxely, maxvoxely, clip3DFloor);
		}
		spr->Light.BaseColormap = colormap;
		spr->Light.ColormapNum = colormapnum;
	}
}
