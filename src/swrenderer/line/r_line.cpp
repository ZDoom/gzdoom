//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#include <stdlib.h>
#include <stddef.h>
#include "templates.h"
#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "r_sky.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "r_wallsetup.h"
#include "v_palette.h"
#include "r_utility.h"
#include "r_data/colormaps.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/line/r_line.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/things/r_decal.h"

CVAR(Bool, r_fogboundary, true, 0)
CVAR(Bool, r_drawmirrors, true, 0)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void SWRenderLine::Render(seg_t *line, subsector_t *subsector, sector_t *sector, sector_t *fakebacksector, VisiblePlane *linefloorplane, VisiblePlane *lineceilingplane, bool infog, FDynamicColormap *colormap)
	{
		static sector_t tempsec;	// killough 3/8/98: ceiling/water hack
		bool			solid;
		DVector2		pt1, pt2;

		InSubsector = subsector;
		frontsector = sector;
		backsector = fakebacksector;
		floorplane = linefloorplane;
		ceilingplane = lineceilingplane;
		foggy = infog;
		basecolormap = colormap;

		curline = line;

		pt1 = line->v1->fPos() - ViewPos;
		pt2 = line->v2->fPos() - ViewPos;

		// Reject lines not facing viewer
		if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
			return;

		if (WallC.Init(pt1, pt2, 32.0 / (1 << 12)))
			return;

		RenderPortal *renderportal = RenderPortal::Instance();

		if (WallC.sx1 >= renderportal->WindowRight || WallC.sx2 <= renderportal->WindowLeft)
			return;

		if (line->linedef == NULL)
		{
			if (RenderClipSegment::Instance()->Check(WallC.sx1, WallC.sx2))
			{
				InSubsector->flags |= SSECF_DRAWN;
			}
			return;
		}

		// reject lines that aren't seen from the portal (if any)
		// [ZZ] 10.01.2016: lines inside a skybox shouldn't be clipped, although this imposes some limitations on portals in skyboxes.
		if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && P_ClipLineToPortal(line->linedef, renderportal->CurrentPortal->dst, ViewPos))
			return;

		vertex_t *v1, *v2;
		v1 = line->linedef->v1;
		v2 = line->linedef->v2;

		if ((v1 == line->v1 && v2 == line->v2) || (v2 == line->v1 && v1 == line->v2))
		{ // The seg is the entire wall.
			WallT.InitFromWallCoords(&WallC);
		}
		else
		{ // The seg is only part of the wall.
			if (line->linedef->sidedef[0] != line->sidedef)
			{
				swapvalues(v1, v2);
			}
			WallT.InitFromLine(v1->fPos() - ViewPos, v2->fPos() - ViewPos);
		}

		Clip3DFloors *clip3d = Clip3DFloors::Instance();

		if (!(clip3d->fake3D & FAKE3D_FAKEBACK))
		{
			backsector = line->backsector;
		}
		rw_frontcz1 = frontsector->ceilingplane.ZatPoint(line->v1);
		rw_frontfz1 = frontsector->floorplane.ZatPoint(line->v1);
		rw_frontcz2 = frontsector->ceilingplane.ZatPoint(line->v2);
		rw_frontfz2 = frontsector->floorplane.ZatPoint(line->v2);

		rw_mustmarkfloor = rw_mustmarkceiling = false;
		rw_havehigh = rw_havelow = false;

		// Single sided line?
		if (backsector == NULL)
		{
			solid = true;
		}
		else
		{
			// kg3D - its fake, no transfer_heights
			if (!(clip3d->fake3D & FAKE3D_FAKEBACK))
			{ // killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
				backsector = RenderOpaquePass::Instance()->FakeFlat(backsector, &tempsec, nullptr, nullptr, curline, WallC.sx1, WallC.sx2, rw_frontcz1, rw_frontcz2);
			}
			doorclosed = false;		// killough 4/16/98

			rw_backcz1 = backsector->ceilingplane.ZatPoint(line->v1);
			rw_backfz1 = backsector->floorplane.ZatPoint(line->v1);
			rw_backcz2 = backsector->ceilingplane.ZatPoint(line->v2);
			rw_backfz2 = backsector->floorplane.ZatPoint(line->v2);

			if (clip3d->fake3D & FAKE3D_FAKEBACK)
			{
				if (rw_frontfz1 >= rw_backfz1 && rw_frontfz2 >= rw_backfz2)
				{
					clip3d->fake3D |= FAKE3D_CLIPBOTFRONT;
				}
				if (rw_frontcz1 <= rw_backcz1 && rw_frontcz2 <= rw_backcz2)
				{
					clip3d->fake3D |= FAKE3D_CLIPTOPFRONT;
				}
			}

			// Cannot make these walls solid, because it can result in
			// sprite clipping problems for sprites near the wall
			if (rw_frontcz1 > rw_backcz1 || rw_frontcz2 > rw_backcz2)
			{
				rw_havehigh = true;
				wallupper.Project(backsector->ceilingplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			}
			if (rw_frontfz1 < rw_backfz1 || rw_frontfz2 < rw_backfz2)
			{
				rw_havelow = true;
				walllower.Project(backsector->floorplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			}

			// Portal
			if (line->linedef->isVisualPortal() && line->sidedef == line->linedef->sidedef[0])
			{
				solid = true;
			}
			// Closed door.
			else if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
				(rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
			{
				solid = true;
			}
			else if (
				// properly render skies (consider door "open" if both ceilings are sky):
				(backsector->GetTexture(sector_t::ceiling) != skyflatnum || frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

				// if door is closed because back is shut:
				&& rw_backcz1 <= rw_backfz1 && rw_backcz2 <= rw_backfz2

				// preserve a kind of transparent door/lift special effect:
				&& ((rw_backcz1 >= rw_frontcz1 && rw_backcz2 >= rw_frontcz2) || line->sidedef->GetTexture(side_t::top).isValid())
				&& ((rw_backfz1 <= rw_frontfz1 && rw_backfz2 <= rw_frontfz2) || line->sidedef->GetTexture(side_t::bottom).isValid()))
			{
				// killough 1/18/98 -- This function is used to fix the automap bug which
				// showed lines behind closed doors simply because the door had a dropoff.
				//
				// It assumes that Doom has already ruled out a door being closed because
				// of front-back closure (e.g. front floor is taller than back ceiling).

				// This fixes the automap floor height bug -- killough 1/18/98:
				// killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
				doorclosed = true;
				solid = true;
			}
			else if (frontsector->ceilingplane != backsector->ceilingplane ||
				frontsector->floorplane != backsector->floorplane)
			{
				// Window.
				solid = false;
			}
			else if (SkyboxCompare(frontsector, backsector))
			{
				solid = false;
			}
			else if (backsector->lightlevel != frontsector->lightlevel
				|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)
				|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)
				|| curline->sidedef->GetTexture(side_t::mid).isValid()

				// killough 3/7/98: Take flats offsets into account:
				|| backsector->planes[sector_t::floor].xform != frontsector->planes[sector_t::floor].xform
				|| backsector->planes[sector_t::ceiling].xform != frontsector->planes[sector_t::ceiling].xform

				|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
				|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
				|| backsector->GetVisFlags(sector_t::floor) != frontsector->GetVisFlags(sector_t::floor)
				|| backsector->GetVisFlags(sector_t::ceiling) != frontsector->GetVisFlags(sector_t::ceiling)

				// [RH] Also consider colormaps
				|| backsector->ColorMap != frontsector->ColorMap



				// kg3D - and fake lights
				|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
				|| (backsector->e && backsector->e->XFloor.lightlist.Size())
				)
			{
				solid = false;
			}
			else
			{
				// Reject empty lines used for triggers and special events.
				// Identical floor and ceiling on both sides, identical light levels
				// on both sides, and no middle texture.

				// When using GL nodes, do a clipping test for these lines so we can
				// mark their subsectors as visible for automap texturing.
				if (hasglnodes && !(InSubsector->flags & SSECF_DRAWN))
				{
					if (RenderClipSegment::Instance()->Check(WallC.sx1, WallC.sx2))
					{
						InSubsector->flags |= SSECF_DRAWN;
					}
				}
				return;
			}
		}

		rw_prepped = false;

		if (line->linedef->special == Line_Horizon)
		{
			// Be aware: Line_Horizon does not work properly with sloped planes
			fillshort(walltop.ScreenY + WallC.sx1, WallC.sx2 - WallC.sx1, centery);
			fillshort(wallbottom.ScreenY + WallC.sx1, WallC.sx2 - WallC.sx1, centery);
		}
		else
		{
			rw_ceilstat = walltop.Project(frontsector->ceilingplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
			rw_floorstat = wallbottom.Project(frontsector->floorplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
		}

		static SWRenderLine *self = this;
		bool visible = RenderClipSegment::Instance()->Clip(WallC.sx1, WallC.sx2, solid, [](int x1, int x2) -> bool
		{
			return self->RenderWallSegment(x1, x2);
		});

		if (visible)
		{
			InSubsector->flags |= SSECF_DRAWN;
		}
	}

	bool SWRenderLine::SkyboxCompare(sector_t *frontsector, sector_t *backsector) const
	{
		FSectorPortal *frontc = frontsector->GetPortal(sector_t::ceiling);
		FSectorPortal *frontf = frontsector->GetPortal(sector_t::floor);
		FSectorPortal *backc = backsector->GetPortal(sector_t::ceiling);
		FSectorPortal *backf = backsector->GetPortal(sector_t::floor);

		// return true if any of the planes has a linedef-based portal (unless both sides have the same one.
		// Ideally this should also check thing based portals but the omission of this check had been abused to hell and back for those.
		// (Note: This may require a compatibility option if some maps ran into this for line based portals as well.)
		if (!frontc->MergeAllowed()) return (frontc != backc);
		if (!frontf->MergeAllowed()) return (frontf != backf);
		if (!backc->MergeAllowed()) return true;
		if (!backf->MergeAllowed()) return true;
		return false;
	}

	// A wall segment will be drawn between start and stop pixels (inclusive).
	bool SWRenderLine::RenderWallSegment(int start, int stop)
	{
		int i;
		bool maskedtexture = false;

#ifdef RANGECHECK
		if (start >= viewwidth || start >= stop)
			I_FatalError("Bad R_StoreWallRange: %i to %i", start, stop);
#endif

		DrawSegment *draw_segment = DrawSegmentList::Instance()->Add();

		if (!rw_prepped)
		{
			rw_prepped = true;
			SetWallVariables(true);
		}

		rw_offset = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::mid));
		rw_light = rw_lightleft + rw_lightstep * (start - WallC.sx1);
		
		RenderPortal *renderportal = RenderPortal::Instance();

		draw_segment->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		draw_segment->sx1 = WallC.sx1;
		draw_segment->sx2 = WallC.sx2;
		draw_segment->sz1 = WallC.sz1;
		draw_segment->sz2 = WallC.sz2;
		draw_segment->cx = WallC.tleft.X;;
		draw_segment->cy = WallC.tleft.Y;
		draw_segment->cdx = WallC.tright.X - WallC.tleft.X;
		draw_segment->cdy = WallC.tright.Y - WallC.tleft.Y;
		draw_segment->tmapvals = WallT;
		draw_segment->siz1 = 1 / WallC.sz1;
		draw_segment->siz2 = 1 / WallC.sz2;
		draw_segment->x1 = start;
		draw_segment->x2 = stop;
		draw_segment->curline = curline;
		draw_segment->bFogBoundary = false;
		draw_segment->bFakeBoundary = false;
		draw_segment->foggy = foggy;

		Clip3DFloors *clip3d = Clip3DFloors::Instance();
		if (clip3d->fake3D & FAKE3D_FAKEMASK) draw_segment->fake = 1;
		else draw_segment->fake = 0;

		draw_segment->sprtopclip = nullptr;
		draw_segment->sprbottomclip = nullptr;
		draw_segment->maskedtexturecol = nullptr;
		draw_segment->bkup = nullptr;
		draw_segment->swall = nullptr;

		if (rw_markportal)
		{
			draw_segment->silhouette = SIL_BOTH;
		}
		else if (backsector == NULL)
		{
			draw_segment->sprtopclip = RenderMemory::AllocMemory<short>(stop - start);
			draw_segment->sprbottomclip = RenderMemory::AllocMemory<short>(stop - start);
			fillshort(draw_segment->sprtopclip, stop - start, viewheight);
			memset(draw_segment->sprbottomclip, -1, (stop - start) * sizeof(short));
			draw_segment->silhouette = SIL_BOTH;
		}
		else
		{
			// two sided line
			draw_segment->silhouette = 0;

			if (rw_frontfz1 > rw_backfz1 || rw_frontfz2 > rw_backfz2 ||
				backsector->floorplane.PointOnSide(ViewPos) < 0)
			{
				draw_segment->silhouette = SIL_BOTTOM;
			}

			if (rw_frontcz1 < rw_backcz1 || rw_frontcz2 < rw_backcz2 ||
				backsector->ceilingplane.PointOnSide(ViewPos) < 0)
			{
				draw_segment->silhouette |= SIL_TOP;
			}

			// killough 1/17/98: this test is required if the fix
			// for the automap bug (r_bsp.c) is used, or else some
			// sprites will be displayed behind closed doors. That
			// fix prevents lines behind closed doors with dropoffs
			// from being displayed on the automap.
			//
			// killough 4/7/98: make doorclosed external variable

			{
				if (doorclosed || (rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2))
				{
					draw_segment->sprbottomclip = RenderMemory::AllocMemory<short>(stop - start);
					memset(draw_segment->sprbottomclip, -1, (stop - start) * sizeof(short));
					draw_segment->silhouette |= SIL_BOTTOM;
				}
				if (doorclosed || (rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
				{						// killough 1/17/98, 2/8/98
					draw_segment->sprtopclip = RenderMemory::AllocMemory<short>(stop - start);
					fillshort(draw_segment->sprtopclip, stop - start, viewheight);
					draw_segment->silhouette |= SIL_TOP;
				}
			}

			if (!draw_segment->fake && r_3dfloors && backsector->e && backsector->e->XFloor.ffloors.Size()) {
				for (i = 0; i < (int)backsector->e->XFloor.ffloors.Size(); i++) {
					F3DFloor *rover = backsector->e->XFloor.ffloors[i];
					if (rover->flags & FF_RENDERSIDES && (!(rover->flags & FF_INVERTSIDES) || rover->flags & FF_ALLSIDES)) {
						draw_segment->bFakeBoundary |= 1;
						break;
					}
				}
			}
			if (!draw_segment->fake && r_3dfloors && frontsector->e && frontsector->e->XFloor.ffloors.Size()) {
				for (i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++) {
					F3DFloor *rover = frontsector->e->XFloor.ffloors[i];
					if (rover->flags & FF_RENDERSIDES && (rover->flags & FF_ALLSIDES || rover->flags & FF_INVERTSIDES)) {
						draw_segment->bFakeBoundary |= 2;
						break;
					}
				}
			}
			// kg3D - no for fakes
			if (!draw_segment->fake)
				// allocate space for masked texture tables, if needed
				// [RH] Don't just allocate the space; fill it in too.
				if ((TexMan(sidedef->GetTexture(side_t::mid), true)->UseType != FTexture::TEX_Null || draw_segment->bFakeBoundary || IsFogBoundary(frontsector, backsector)) &&
					(rw_ceilstat != ProjectedWallCull::OutsideBelow || !sidedef->GetTexture(side_t::top).isValid()) &&
					(rw_floorstat != ProjectedWallCull::OutsideAbove || !sidedef->GetTexture(side_t::bottom).isValid()) &&
					(WallC.sz1 >= TOO_CLOSE_Z && WallC.sz2 >= TOO_CLOSE_Z))
				{
					float *swal;
					fixed_t *lwal;
					int i;

					maskedtexture = true;

					// kg3D - backup for mid and fake walls
					draw_segment->bkup = RenderMemory::AllocMemory<short>(stop - start);
					memcpy(draw_segment->bkup, &RenderOpaquePass::Instance()->ceilingclip[start], sizeof(short)*(stop - start));

					draw_segment->bFogBoundary = IsFogBoundary(frontsector, backsector);
					if (sidedef->GetTexture(side_t::mid).isValid() || draw_segment->bFakeBoundary)
					{
						if (sidedef->GetTexture(side_t::mid).isValid())
							draw_segment->bFakeBoundary |= 4; // it is also mid texture

						draw_segment->maskedtexturecol = RenderMemory::AllocMemory<fixed_t>(stop - start);
						draw_segment->swall = RenderMemory::AllocMemory<float>(stop - start);

						lwal = draw_segment->maskedtexturecol;
						swal = draw_segment->swall;
						FTexture *pic = TexMan(sidedef->GetTexture(side_t::mid), true);
						double yscale = pic->Scale.Y * sidedef->GetTextureYScale(side_t::mid);
						fixed_t xoffset = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::mid));

						if (pic->bWorldPanning)
						{
							xoffset = xs_RoundToInt(xoffset * lwallscale);
						}

						for (i = start; i < stop; i++)
						{
							*lwal++ = walltexcoords.UPos[i] + xoffset;
							*swal++ = walltexcoords.VStep[i];
						}

						double istart = draw_segment->swall[0] * yscale;
						double iend = *(swal - 1) * yscale;
#if 0
						///This was for avoiding overflow when using fixed point. It might not be needed anymore.
						const double mini = 3 / 65536.0;
						if (istart < mini && istart >= 0) istart = mini;
						if (istart > -mini && istart < 0) istart = -mini;
						if (iend < mini && iend >= 0) iend = mini;
						if (iend > -mini && iend < 0) iend = -mini;
#endif
						istart = 1 / istart;
						iend = 1 / iend;
						draw_segment->yscale = (float)yscale;
						draw_segment->iscale = (float)istart;
						if (stop - start > 0)
						{
							draw_segment->iscalestep = float((iend - istart) / (stop - start));
						}
						else
						{
							draw_segment->iscalestep = 0;
						}
					}
					draw_segment->light = rw_light;
					draw_segment->lightstep = rw_lightstep;

					// Masked midtextures should get the light level from the sector they reference,
					// not from the current subsector, which is what the current wallshade value
					// comes from. We make an exeption for polyobjects, however, since their "home"
					// sector should be whichever one they move into.
					if (curline->sidedef->Flags & WALLF_POLYOBJ)
					{
						draw_segment->shade = wallshade;
					}
					else
					{
						draw_segment->shade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, curline->frontsector->lightlevel) + R_ActualExtraLight(foggy));
					}

					if (draw_segment->bFogBoundary || draw_segment->maskedtexturecol != nullptr)
					{
						size_t drawsegnum = draw_segment - DrawSegmentList::Instance()->drawsegs;
						DrawSegmentList::Instance()->InterestingDrawsegs.Push(drawsegnum);
					}
				}
		}

		// render it
		if (markceiling)
		{
			if (ceilingplane)
			{	// killough 4/11/98: add NULL ptr checks
				ceilingplane = VisiblePlaneList::Instance()->GetRange(ceilingplane, start, stop);
			}
			else
			{
				markceiling = false;
			}
		}

		if (markfloor)
		{
			if (floorplane)
			{	// killough 4/11/98: add NULL ptr checks
				floorplane = VisiblePlaneList::Instance()->GetRange(floorplane, start, stop);
			}
			else
			{
				markfloor = false;
			}
		}

		RenderWallSegmentTextures(start, stop);

		if (clip3d->fake3D & FAKE3D_FAKEMASK)
		{
			return (clip3d->fake3D & FAKE3D_FAKEMASK) == 0;
		}

		// save sprite clipping info
		if (((draw_segment->silhouette & SIL_TOP) || maskedtexture) && draw_segment->sprtopclip == nullptr)
		{
			draw_segment->sprtopclip = RenderMemory::AllocMemory<short>(stop - start);
			memcpy(draw_segment->sprtopclip, &RenderOpaquePass::Instance()->ceilingclip[start], sizeof(short)*(stop - start));
		}

		if (((draw_segment->silhouette & SIL_BOTTOM) || maskedtexture) && draw_segment->sprbottomclip == nullptr)
		{
			draw_segment->sprbottomclip = RenderMemory::AllocMemory<short>(stop - start);
			memcpy(draw_segment->sprbottomclip, &RenderOpaquePass::Instance()->floorclip[start], sizeof(short)*(stop - start));
		}

		if (maskedtexture && curline->sidedef->GetTexture(side_t::mid).isValid())
		{
			draw_segment->silhouette |= SIL_TOP | SIL_BOTTOM;
		}

		// [RH] Draw any decals bound to the seg
		// [ZZ] Only if not an active mirror
		if (!rw_markportal)
		{
			RenderDecal::RenderDecals(curline->sidedef, draw_segment, wallshade, rw_lightleft, rw_lightstep, curline, WallC, foggy, basecolormap, walltop.ScreenY, wallbottom.ScreenY);
		}

		if (rw_markportal)
		{
			RenderPortal::Instance()->AddLinePortal(curline->linedef, draw_segment->x1, draw_segment->x2, draw_segment->sprtopclip, draw_segment->sprbottomclip);
		}

		return (clip3d->fake3D & FAKE3D_FAKEMASK) == 0;
	}

	void SWRenderLine::SetWallVariables(bool needlights)
	{
		double rowoffset;
		double yrepeat;

		rw_markportal = false;

		sidedef = curline->sidedef;
		linedef = curline->linedef;

		// mark the segment as visible for auto map
		if (!RenderScene::Instance()->DontMapLines()) linedef->flags |= ML_MAPPED;

		midtexture = toptexture = bottomtexture = 0;

		if (sidedef == linedef->sidedef[0] &&
			(linedef->special == Line_Mirror && r_drawmirrors)) // [ZZ] compatibility with r_drawmirrors cvar that existed way before portals
		{
			markfloor = markceiling = true; // act like a one-sided wall here (todo: check how does this work with transparency)
			rw_markportal = true;
		}
		else if (backsector == NULL)
		{
			// single sided line
			// a single sided line is terminal, so it must mark ends
			markfloor = markceiling = true;
			// [RH] Horizon lines do not need to be textured
			if (linedef->isVisualPortal())
			{
				rw_markportal = true;
			}
			else if (linedef->special != Line_Horizon)
			{
				midtexture = TexMan(sidedef->GetTexture(side_t::mid), true);
				rw_offset_mid = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::mid));
				rowoffset = sidedef->GetTextureYOffset(side_t::mid);
				rw_midtexturescalex = sidedef->GetTextureXScale(side_t::mid);
				rw_midtexturescaley = sidedef->GetTextureYScale(side_t::mid);
				yrepeat = midtexture->Scale.Y * rw_midtexturescaley;
				if (yrepeat >= 0)
				{ // normal orientation
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // bottom of texture at bottom
						rw_midtexturemid = (frontsector->GetPlaneTexZ(sector_t::floor) - ViewPos.Z) * yrepeat + midtexture->GetHeight();
					}
					else
					{ // top of texture at top
						rw_midtexturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat;
						if (rowoffset < 0 && midtexture != NULL)
						{
							rowoffset += midtexture->GetHeight();
						}
					}
				}
				else
				{ // upside down
					rowoffset = -rowoffset;
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // top of texture at bottom
						rw_midtexturemid = (frontsector->GetPlaneTexZ(sector_t::floor) - ViewPos.Z) * yrepeat;
					}
					else
					{ // bottom of texture at top
						rw_midtexturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat + midtexture->GetHeight();
					}
				}
				if (midtexture->bWorldPanning)
				{
					rw_midtexturemid += rowoffset * yrepeat;
				}
				else
				{
					// rowoffset is added outside the multiply so that it positions the texture
					// by texels instead of world units.
					rw_midtexturemid += rowoffset;
				}
			}
		}
		else
		{ // two-sided line
		  // hack to allow height changes in outdoor areas

			double rw_frontlowertop = frontsector->GetPlaneTexZ(sector_t::ceiling);

			if (frontsector->GetTexture(sector_t::ceiling) == skyflatnum &&
				backsector->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				if (rw_havehigh)
				{ // front ceiling is above back ceiling
					memcpy(&walltop.ScreenY[WallC.sx1], &wallupper.ScreenY[WallC.sx1], (WallC.sx2 - WallC.sx1) * sizeof(walltop.ScreenY[0]));
					rw_havehigh = false;
				}
				else if (rw_havelow && frontsector->ceilingplane != backsector->ceilingplane)
				{ // back ceiling is above front ceiling
				  // The check for rw_havelow is not Doom-compliant, but it avoids HoM that
				  // would otherwise occur because there is space made available for this
				  // wall but nothing to draw for it.
				  // Recalculate walltop so that the wall is clipped by the back sector's
				  // ceiling instead of the front sector's ceiling.
				  	RenderPortal *renderportal = RenderPortal::Instance();
					walltop.Project(backsector->ceilingplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
				}
				// Putting sky ceilings on the front and back of a line alters the way unpegged
				// positioning works.
				rw_frontlowertop = backsector->GetPlaneTexZ(sector_t::ceiling);
			}

			if (linedef->isVisualPortal())
			{
				markceiling = markfloor = true;
			}
			else if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
				(rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
			{
				// closed door
				markceiling = markfloor = true;
			}
			else
			{
				markfloor = rw_mustmarkfloor
					|| backsector->floorplane != frontsector->floorplane
					|| backsector->lightlevel != frontsector->lightlevel
					|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)
					|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)

					// killough 3/7/98: Add checks for (x,y) offsets
					|| backsector->planes[sector_t::floor].xform != frontsector->planes[sector_t::floor].xform
					|| backsector->GetAlpha(sector_t::floor) != frontsector->GetAlpha(sector_t::floor)

					// killough 4/15/98: prevent 2s normals
					// from bleeding through deep water
					|| frontsector->heightsec

					|| backsector->GetVisFlags(sector_t::floor) != frontsector->GetVisFlags(sector_t::floor)

					// [RH] Add checks for colormaps
					|| backsector->ColorMap != frontsector->ColorMap


					// kg3D - add fake lights
					|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
					|| (backsector->e && backsector->e->XFloor.lightlist.Size())

					|| (sidedef->GetTexture(side_t::mid).isValid() &&
					((linedef->flags & (ML_CLIP_MIDTEX | ML_WRAP_MIDTEX)) ||
						(sidedef->Flags & (WALLF_CLIP_MIDTEX | WALLF_WRAP_MIDTEX))))
					;

				markceiling = (frontsector->GetTexture(sector_t::ceiling) != skyflatnum ||
					backsector->GetTexture(sector_t::ceiling) != skyflatnum) &&
					(rw_mustmarkceiling
						|| backsector->ceilingplane != frontsector->ceilingplane
						|| backsector->lightlevel != frontsector->lightlevel
						|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)

						// killough 3/7/98: Add checks for (x,y) offsets
						|| backsector->planes[sector_t::ceiling].xform != frontsector->planes[sector_t::ceiling].xform
						|| backsector->GetAlpha(sector_t::ceiling) != frontsector->GetAlpha(sector_t::ceiling)

						// killough 4/15/98: prevent 2s normals
						// from bleeding through fake ceilings
						|| (frontsector->heightsec && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

						|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
						|| backsector->GetFlags(sector_t::ceiling) != frontsector->GetFlags(sector_t::ceiling)

						// [RH] Add check for colormaps
						|| backsector->ColorMap != frontsector->ColorMap

						// kg3D - add fake lights
						|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
						|| (backsector->e && backsector->e->XFloor.lightlist.Size())

						|| (sidedef->GetTexture(side_t::mid).isValid() &&
						((linedef->flags & (ML_CLIP_MIDTEX | ML_WRAP_MIDTEX)) ||
							(sidedef->Flags & (WALLF_CLIP_MIDTEX | WALLF_WRAP_MIDTEX))))
						);
			}

			if (rw_havehigh)
			{ // top texture
				toptexture = TexMan(sidedef->GetTexture(side_t::top), true);

				rw_offset_top = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::top));
				rowoffset = sidedef->GetTextureYOffset(side_t::top);
				rw_toptexturescalex = sidedef->GetTextureXScale(side_t::top);
				rw_toptexturescaley = sidedef->GetTextureYScale(side_t::top);
				yrepeat = toptexture->Scale.Y * rw_toptexturescaley;
				if (yrepeat >= 0)
				{ // normal orientation
					if (linedef->flags & ML_DONTPEGTOP)
					{ // top of texture at top
						rw_toptexturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat;
						if (rowoffset < 0 && toptexture != NULL)
						{
							rowoffset += toptexture->GetHeight();
						}
					}
					else
					{ // bottom of texture at bottom
						rw_toptexturemid = (backsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat + toptexture->GetHeight();
					}
				}
				else
				{ // upside down
					rowoffset = -rowoffset;
					if (linedef->flags & ML_DONTPEGTOP)
					{ // bottom of texture at top
						rw_toptexturemid = (frontsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat + toptexture->GetHeight();
					}
					else
					{ // top of texture at bottom
						rw_toptexturemid = (backsector->GetPlaneTexZ(sector_t::ceiling) - ViewPos.Z) * yrepeat;
					}
				}
				if (toptexture->bWorldPanning)
				{
					rw_toptexturemid += rowoffset * yrepeat;
				}
				else
				{
					rw_toptexturemid += rowoffset;
				}
			}
			if (rw_havelow)
			{ // bottom texture
				bottomtexture = TexMan(sidedef->GetTexture(side_t::bottom), true);

				rw_offset_bottom = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::bottom));
				rowoffset = sidedef->GetTextureYOffset(side_t::bottom);
				rw_bottomtexturescalex = sidedef->GetTextureXScale(side_t::bottom);
				rw_bottomtexturescaley = sidedef->GetTextureYScale(side_t::bottom);
				yrepeat = bottomtexture->Scale.Y * rw_bottomtexturescaley;
				if (yrepeat >= 0)
				{ // normal orientation
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // bottom of texture at bottom
						rw_bottomtexturemid = (rw_frontlowertop - ViewPos.Z) * yrepeat;
					}
					else
					{ // top of texture at top
						rw_bottomtexturemid = (backsector->GetPlaneTexZ(sector_t::floor) - ViewPos.Z) * yrepeat;
						if (rowoffset < 0 && bottomtexture != NULL)
						{
							rowoffset += bottomtexture->GetHeight();
						}
					}
				}
				else
				{ // upside down
					rowoffset = -rowoffset;
					if (linedef->flags & ML_DONTPEGBOTTOM)
					{ // top of texture at bottom
						rw_bottomtexturemid = (rw_frontlowertop - ViewPos.Z) * yrepeat;
					}
					else
					{ // bottom of texture at top
						rw_bottomtexturemid = (backsector->GetPlaneTexZ(sector_t::floor) - ViewPos.Z) * yrepeat + bottomtexture->GetHeight();
					}
				}
				if (bottomtexture->bWorldPanning)
				{
					rw_bottomtexturemid += rowoffset * yrepeat;
				}
				else
				{
					rw_bottomtexturemid += rowoffset;
				}
			}
			rw_markportal = linedef->isVisualPortal();
		}

		// if a floor / ceiling plane is on the wrong side of the view plane,
		// it is definitely invisible and doesn't need to be marked.

		// killough 3/7/98: add deep water check
		if (frontsector->GetHeightSec() == NULL)
		{
			int planeside;

			planeside = frontsector->floorplane.PointOnSide(ViewPos);
			if (frontsector->floorplane.fC() < 0)	// 3D floors have the floor backwards
				planeside = -planeside;
			if (planeside <= 0)		// above view plane
				markfloor = false;

			if (frontsector->GetTexture(sector_t::ceiling) != skyflatnum)
			{
				planeside = frontsector->ceilingplane.PointOnSide(ViewPos);
				if (frontsector->ceilingplane.fC() > 0)	// 3D floors have the ceiling backwards
					planeside = -planeside;
				if (planeside <= 0)		// below view plane
					markceiling = false;
			}
		}

		FTexture *midtex = TexMan(sidedef->GetTexture(side_t::mid), true);

		bool segtextured = midtex != NULL || toptexture != NULL || bottomtexture != NULL;

		// calculate light table
		if (needlights && (segtextured || (backsector && IsFogBoundary(frontsector, backsector))))
		{
			lwallscale =
				midtex ? (midtex->Scale.X * sidedef->GetTextureXScale(side_t::mid)) :
				toptexture ? (toptexture->Scale.X * sidedef->GetTextureXScale(side_t::top)) :
				bottomtexture ? (bottomtexture->Scale.X * sidedef->GetTextureXScale(side_t::bottom)) :
				1.;

			walltexcoords.Project(sidedef->TexelLength * lwallscale, WallC.sx1, WallC.sx2, WallT);

			CameraLight *cameraLight = CameraLight::Instance();
			if (cameraLight->FixedColormap() == nullptr && cameraLight->FixedLightLevel() < 0)
			{
				wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, frontsector->lightlevel) + R_ActualExtraLight(foggy));
				double GlobVis = LightVisibility::Instance()->WallGlobVis();
				rw_lightleft = float(GlobVis / WallC.sz1);
				rw_lightstep = float((GlobVis / WallC.sz2 - rw_lightleft) / (WallC.sx2 - WallC.sx1));
			}
			else
			{
				rw_lightleft = 1;
				rw_lightstep = 0;
			}
		}
	}

	bool SWRenderLine::IsFogBoundary(sector_t *front, sector_t *back) const
	{
		return r_fogboundary && CameraLight::Instance()->FixedColormap() == nullptr && front->ColorMap->Fade &&
			front->ColorMap->Fade != back->ColorMap->Fade &&
			(front->GetTexture(sector_t::ceiling) != skyflatnum || back->GetTexture(sector_t::ceiling) != skyflatnum);
	}

	// Draws zero, one, or two textures for walls.
	// Can draw or mark the starting pixel of floor and ceiling textures.
	void SWRenderLine::RenderWallSegmentTextures(int x1, int x2)
	{
		int x;
		double xscale;
		double yscale;
		fixed_t xoffset = rw_offset;

		WallDrawerArgs drawerargs;

		drawerargs.SetStyle(false, false, OPAQUE);

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
			drawerargs.SetLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, FIXEDLIGHT2SHADE(cameraLight->FixedLightLevel()));
		else if (cameraLight->FixedColormap() != nullptr)
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);

		// clip wall to the floor and ceiling
		auto ceilingclip = RenderOpaquePass::Instance()->ceilingclip;
		auto floorclip = RenderOpaquePass::Instance()->floorclip;
		for (x = x1; x < x2; ++x)
		{
			if (walltop.ScreenY[x] < ceilingclip[x])
			{
				walltop.ScreenY[x] = ceilingclip[x];
			}
			if (wallbottom.ScreenY[x] > floorclip[x])
			{
				wallbottom.ScreenY[x] = floorclip[x];
			}
		}

		Clip3DFloors *clip3d = Clip3DFloors::Instance();

		// mark ceiling areas
		if (markceiling)
		{
			for (x = x1; x < x2; ++x)
			{
				short top = (clip3d->fakeFloor && clip3d->fake3D & FAKE3D_FAKECEILING) ? clip3d->fakeFloor->ceilingclip[x] : ceilingclip[x];
				short bottom = MIN(walltop.ScreenY[x], floorclip[x]);
				if (top < bottom)
				{
					ceilingplane->top[x] = top;
					ceilingplane->bottom[x] = bottom;
				}
			}
		}

		// mark floor areas
		if (markfloor)
		{
			for (x = x1; x < x2; ++x)
			{
				short top = MAX(wallbottom.ScreenY[x], ceilingclip[x]);
				short bottom = (clip3d->fakeFloor && clip3d->fake3D & FAKE3D_FAKEFLOOR) ? clip3d->fakeFloor->floorclip[x] : floorclip[x];
				if (top < bottom)
				{
					assert(bottom <= viewheight);
					floorplane->top[x] = top;
					floorplane->bottom[x] = bottom;
				}
			}
		}

		// kg3D - fake planes clipping
		if (clip3d->fake3D & FAKE3D_REFRESHCLIP)
		{
			if (clip3d->fake3D & FAKE3D_CLIPBOTFRONT)
			{
				memcpy(clip3d->fakeFloor->floorclip + x1, wallbottom.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else
			{
				for (x = x1; x < x2; ++x)
				{
					walllower.ScreenY[x] = MIN(MAX(walllower.ScreenY[x], ceilingclip[x]), wallbottom.ScreenY[x]);
				}
				memcpy(clip3d->fakeFloor->floorclip + x1, walllower.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			if (clip3d->fake3D & FAKE3D_CLIPTOPFRONT)
			{
				memcpy(clip3d->fakeFloor->ceilingclip + x1, walltop.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else
			{
				for (x = x1; x < x2; ++x)
				{
					wallupper.ScreenY[x] = MAX(MIN(wallupper.ScreenY[x], floorclip[x]), walltop.ScreenY[x]);
				}
				memcpy(clip3d->fakeFloor->ceilingclip + x1, wallupper.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
		}
		if (clip3d->fake3D & FAKE3D_FAKEMASK) return;

		FLightNode *light_list = (curline && curline->sidedef) ? curline->sidedef->lighthead : nullptr;

		// draw the wall tiers
		if (midtexture)
		{ // one sided line
			if (midtexture->UseType != FTexture::TEX_Null && viewactive)
			{
				rw_pic = midtexture;
				xscale = rw_pic->Scale.X * rw_midtexturescalex;
				yscale = rw_pic->Scale.Y * rw_midtexturescaley;
				if (xscale != lwallscale)
				{
					walltexcoords.ProjectPos(curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
					lwallscale = xscale;
				}
				if (midtexture->bWorldPanning)
				{
					rw_offset = xs_RoundToInt(rw_offset_mid * xscale);
				}
				else
				{
					rw_offset = rw_offset_mid;
				}
				if (xscale < 0)
				{
					rw_offset = -rw_offset;
				}

				RenderWallPart renderWallpart;
				renderWallpart.Render(drawerargs, frontsector, curline, WallC, rw_pic, x1, x2, walltop.ScreenY, wallbottom.ScreenY, rw_midtexturemid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(rw_frontcz1, rw_frontcz2), MIN(rw_frontfz1, rw_frontfz2), false, wallshade, rw_offset, rw_light, rw_lightstep, light_list, foggy, basecolormap);
			}
			fillshort(ceilingclip + x1, x2 - x1, viewheight);
			fillshort(floorclip + x1, x2 - x1, 0xffff);
		}
		else
		{ // two sided line
			if (toptexture != NULL && toptexture->UseType != FTexture::TEX_Null)
			{ // top wall
				for (x = x1; x < x2; ++x)
				{
					wallupper.ScreenY[x] = MAX(MIN(wallupper.ScreenY[x], floorclip[x]), walltop.ScreenY[x]);
				}
				if (viewactive)
				{
					rw_pic = toptexture;
					xscale = rw_pic->Scale.X * rw_toptexturescalex;
					yscale = rw_pic->Scale.Y * rw_toptexturescaley;
					if (xscale != lwallscale)
					{
						walltexcoords.ProjectPos(curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
						lwallscale = xscale;
					}
					if (toptexture->bWorldPanning)
					{
						rw_offset = xs_RoundToInt(rw_offset_top * xscale);
					}
					else
					{
						rw_offset = rw_offset_top;
					}
					if (xscale < 0)
					{
						rw_offset = -rw_offset;
					}

					RenderWallPart renderWallpart;
					renderWallpart.Render(drawerargs, frontsector, curline, WallC, rw_pic, x1, x2, walltop.ScreenY, wallupper.ScreenY, rw_toptexturemid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(rw_frontcz1, rw_frontcz2), MIN(rw_backcz1, rw_backcz2), false, wallshade, rw_offset, rw_light, rw_lightstep, light_list, foggy, basecolormap);
				}
				memcpy(ceilingclip + x1, wallupper.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else if (markceiling)
			{ // no top wall
				memcpy(ceilingclip + x1, walltop.ScreenY + x1, (x2 - x1) * sizeof(short));
			}


			if (bottomtexture != NULL && bottomtexture->UseType != FTexture::TEX_Null)
			{ // bottom wall
				for (x = x1; x < x2; ++x)
				{
					walllower.ScreenY[x] = MIN(MAX(walllower.ScreenY[x], ceilingclip[x]), wallbottom.ScreenY[x]);
				}
				if (viewactive)
				{
					rw_pic = bottomtexture;
					xscale = rw_pic->Scale.X * rw_bottomtexturescalex;
					yscale = rw_pic->Scale.Y * rw_bottomtexturescaley;
					if (xscale != lwallscale)
					{
						walltexcoords.ProjectPos(curline->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
						lwallscale = xscale;
					}
					if (bottomtexture->bWorldPanning)
					{
						rw_offset = xs_RoundToInt(rw_offset_bottom * xscale);
					}
					else
					{
						rw_offset = rw_offset_bottom;
					}
					if (xscale < 0)
					{
						rw_offset = -rw_offset;
					}

					RenderWallPart renderWallpart;
					renderWallpart.Render(drawerargs, frontsector, curline, WallC, rw_pic, x1, x2, walllower.ScreenY, wallbottom.ScreenY, rw_bottomtexturemid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(rw_backfz1, rw_backfz2), MIN(rw_frontfz1, rw_frontfz2), false, wallshade, rw_offset, rw_light, rw_lightstep, light_list, foggy, basecolormap);
				}
				memcpy(floorclip + x1, walllower.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else if (markfloor)
			{ // no bottom wall
				memcpy(floorclip + x1, wallbottom.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
		}
		rw_offset = xoffset;
	}

	////////////////////////////////////////////////////////////////////////////

	// Transform and clip coordinates. Returns true if it was clipped away
	bool FWallCoords::Init(const DVector2 &pt1, const DVector2 &pt2, double too_close)
	{
		tleft.X = float(pt1.X * ViewSin - pt1.Y * ViewCos);
		tright.X = float(pt2.X * ViewSin - pt2.Y * ViewCos);

		tleft.Y = float(pt1.X * ViewTanCos + pt1.Y * ViewTanSin);
		tright.Y = float(pt2.X * ViewTanCos + pt2.Y * ViewTanSin);
		
		RenderPortal *renderportal = RenderPortal::Instance();
		auto viewport = RenderViewport::Instance();

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			float t = -tleft.X;
			tleft.X = -tright.X;
			tright.X = t;
			swapvalues(tleft.Y, tright.Y);
		}

		if (tleft.X >= -tleft.Y)
		{
			if (tleft.X > tleft.Y) return true;	// left edge is off the right side
			if (tleft.Y == 0) return true;
			sx1 = xs_RoundToInt(viewport->CenterX + tleft.X * viewport->CenterX / tleft.Y);
			sz1 = tleft.Y;
		}
		else
		{
			if (tright.X < -tright.Y) return true;	// wall is off the left side
			float den = tleft.X - tright.X - tright.Y + tleft.Y;
			if (den == 0) return true;
			sx1 = 0;
			sz1 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X + tleft.Y) / den;
		}

		if (sz1 < too_close)
			return true;

		if (tright.X <= tright.Y)
		{
			if (tright.X < -tright.Y) return true;	// right edge is off the left side
			if (tright.Y == 0) return true;
			sx2 = xs_RoundToInt(viewport->CenterX + tright.X * viewport->CenterX / tright.Y);
			sz2 = tright.Y;
		}
		else
		{
			if (tleft.X > tleft.Y) return true;	// wall is off the right side
			float den = tright.Y - tleft.Y - tright.X + tleft.X;
			if (den == 0) return true;
			sx2 = viewwidth;
			sz2 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X - tleft.Y) / den;
		}

		if (sz2 < too_close || sx2 <= sx1)
			return true;

		return false;
	}

	/////////////////////////////////////////////////////////////////////////

	void FWallTmapVals::InitFromWallCoords(const FWallCoords *wallc)
	{
		const FVector2 *left = &wallc->tleft;
		const FVector2 *right = &wallc->tright;
		
		RenderPortal *renderportal = RenderPortal::Instance();

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			swapvalues(left, right);
		}
		UoverZorg = left->X * centerx;
		UoverZstep = -left->Y;
		InvZorg = (left->X - right->X) * centerx;
		InvZstep = right->Y - left->Y;
	}

	void FWallTmapVals::InitFromLine(const DVector2 &left, const DVector2 &right)
	{
		// Coordinates should have already had viewx,viewy subtracted

		double fullx1 = left.X * ViewSin - left.Y * ViewCos;
		double fullx2 = right.X * ViewSin - right.Y * ViewCos;
		double fully1 = left.X * ViewTanCos + left.Y * ViewTanSin;
		double fully2 = right.X * ViewTanCos + right.Y * ViewTanSin;
		
		RenderPortal *renderportal = RenderPortal::Instance();

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			fullx1 = -fullx1;
			fullx2 = -fullx2;
		}

		UoverZorg = float(fullx1 * centerx);
		UoverZstep = float(-fully1);
		InvZorg = float((fullx1 - fullx2) * centerx);
		InvZstep = float(fully2 - fully1);
	}
}
