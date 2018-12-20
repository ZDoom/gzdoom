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
#include "swrenderer/r_renderthread.h"

CVAR(Bool, r_fogboundary, true, 0)
CVAR(Bool, r_drawmirrors, true, 0)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	SWRenderLine::SWRenderLine(RenderThread *thread)
	{
		Thread = thread;
	}

	void SWRenderLine::Render(seg_t *line, subsector_t *subsector, sector_t *sector, sector_t *fakebacksector, VisiblePlane *linefloorplane, VisiblePlane *lineceilingplane, bool infog, FDynamicColormap *colormap, Fake3DOpaque opaque3dfloor)
	{
		mSubsector = subsector;
		mFrontSector = sector;
		mBackSector = fakebacksector;
		mFloorPlane = linefloorplane;
		mCeilingPlane = lineceilingplane;
		foggy = infog;
		basecolormap = colormap;
		mLineSegment = line;
		m3DFloor = opaque3dfloor;

		DVector2 pt1 = line->v1->fPos() - Thread->Viewport->viewpoint.Pos;
		DVector2 pt2 = line->v2->fPos() - Thread->Viewport->viewpoint.Pos;

		// Reject lines not facing viewer
		if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
			return;

		if (WallC.Init(Thread, pt1, pt2, 32.0 / (1 << 12)))
			return;

		RenderPortal *renderportal = Thread->Portal.get();
		if (WallC.sx1 >= renderportal->WindowRight || WallC.sx2 <= renderportal->WindowLeft)
			return;

		if (line->linedef == nullptr)
		{
			if (Thread->ClipSegments->Check(WallC.sx1, WallC.sx2))
			{
				mSubsector->flags |= SSECMF_DRAWN;
			}
			return;
		}

		// reject lines that aren't seen from the portal (if any)
		// [ZZ] 10.01.2016: lines inside a skybox shouldn't be clipped, although this imposes some limitations on portals in skyboxes.
		if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && P_ClipLineToPortal(line->linedef, renderportal->CurrentPortal->dst, Thread->Viewport->viewpoint.Pos))
			return;

		vertex_t *v1 = line->linedef->v1;
		vertex_t *v2 = line->linedef->v2;

		if ((v1 == line->v1 && v2 == line->v2) || (v2 == line->v1 && v1 == line->v2))
		{ // The seg is the entire wall.
			WallT.InitFromWallCoords(Thread, &WallC);
		}
		else
		{ // The seg is only part of the wall.
			if (line->linedef->sidedef[0] != line->sidedef)
			{
				swapvalues(v1, v2);
			}
			WallT.InitFromLine(Thread, v1->fPos() - Thread->Viewport->viewpoint.Pos, v2->fPos() - Thread->Viewport->viewpoint.Pos);
		}

		mFrontCeilingZ1 = mFrontSector->ceilingplane.ZatPoint(line->v1);
		mFrontFloorZ1 = mFrontSector->floorplane.ZatPoint(line->v1);
		mFrontCeilingZ2 = mFrontSector->ceilingplane.ZatPoint(line->v2);
		mFrontFloorZ2 = mFrontSector->floorplane.ZatPoint(line->v2);

		Clip3DFloors *clip3d = Thread->Clip3D.get();

		if (m3DFloor.type != Fake3DOpaque::FakeBack)
		{
			mBackSector = line->backsector;
		}

		if (mBackSector)
		{
			// kg3D - its fake, no transfer_heights
			if (m3DFloor.type != Fake3DOpaque::FakeBack)
			{ // killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
				mBackSector = Thread->OpaquePass->FakeFlat(mBackSector, &tempsec, nullptr, nullptr, mLineSegment, WallC.sx1, WallC.sx2, mFrontCeilingZ1, mFrontCeilingZ2);
			}

			mBackCeilingZ1 = mBackSector->ceilingplane.ZatPoint(line->v1);
			mBackFloorZ1 = mBackSector->floorplane.ZatPoint(line->v1);
			mBackCeilingZ2 = mBackSector->ceilingplane.ZatPoint(line->v2);
			mBackFloorZ2 = mBackSector->floorplane.ZatPoint(line->v2);

			if (m3DFloor.type == Fake3DOpaque::FakeBack)
			{
				if (mFrontFloorZ1 >= mBackFloorZ1 && mFrontFloorZ2 >= mBackFloorZ2)
				{
					m3DFloor.clipBotFront = true;
				}
				if (mFrontCeilingZ1 <= mBackCeilingZ1 && mFrontCeilingZ2 <= mBackCeilingZ2)
				{
					m3DFloor.clipTopFront = true;
				}
			}
		}

		mDoorClosed = IsDoorClosed();

		if (IsInvisibleLine())
		{
			// When using GL nodes, do a clipping test for these lines so we can
			// mark their subsectors as visible for automap texturing.
			if (!(mSubsector->flags & SSECMF_DRAWN))
			{
				if (Thread->ClipSegments->Check(WallC.sx1, WallC.sx2))
				{
					mSubsector->flags |= SSECMF_DRAWN;
				}
			}
			return;
		}

		rw_prepped = false;

		bool visible = Thread->ClipSegments->Clip(WallC.sx1, WallC.sx2, IsSolid(), this);

		if (visible)
		{
			mSubsector->flags |= SSECMF_DRAWN;
		}
	}

	bool SWRenderLine::IsInvisibleLine() const
	{
		// Reject empty lines used for triggers and special events.
		// Identical floor and ceiling on both sides, identical light levels
		// on both sides, and no middle texture.

		if (!mBackSector) return false;

		// Portal
		if (mLineSegment->linedef->isVisualPortal() && mLineSegment->sidedef == mLineSegment->linedef->sidedef[0]) return false;

		// Closed door.
		if (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2) return false;
		if (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2) return false;
		if (IsDoorClosed()) return false;

		// Window.
		if (mFrontSector->ceilingplane != mBackSector->ceilingplane || mFrontSector->floorplane != mBackSector->floorplane) return false;
		if (SkyboxCompare(mFrontSector, mBackSector)) return false;

		if (mBackSector->lightlevel != mFrontSector->lightlevel) return false;
		if (mBackSector->GetTexture(sector_t::floor) != mFrontSector->GetTexture(sector_t::floor)) return false;
		if (mBackSector->GetTexture(sector_t::ceiling) != mFrontSector->GetTexture(sector_t::ceiling)) return false;
		if (mLineSegment->sidedef->GetTexture(side_t::mid).isValid()) return false;

		if (mBackSector->planes[sector_t::floor].xform != mFrontSector->planes[sector_t::floor].xform) return false;
		if (mBackSector->planes[sector_t::ceiling].xform != mFrontSector->planes[sector_t::ceiling].xform) return false;

		if (mBackSector->GetPlaneLight(sector_t::floor) != mFrontSector->GetPlaneLight(sector_t::floor)) return false;
		if (mBackSector->GetPlaneLight(sector_t::ceiling) != mFrontSector->GetPlaneLight(sector_t::ceiling)) return false;
		if (mBackSector->GetVisFlags(sector_t::floor) != mFrontSector->GetVisFlags(sector_t::floor)) return false;
		if (mBackSector->GetVisFlags(sector_t::ceiling) != mFrontSector->GetVisFlags(sector_t::ceiling)) return false;

		if (mBackSector->Colormap != mFrontSector->Colormap) return false;

		if (mFrontSector->e && mFrontSector->e->XFloor.lightlist.Size()) return false;
		if (mBackSector->e && mBackSector->e->XFloor.lightlist.Size()) return false;

		return true;
	}

	bool SWRenderLine::IsSolid() const
	{
		// One sided
		if (mBackSector == nullptr) return true;

		// Portal
		if (mLineSegment->linedef->isVisualPortal() && mLineSegment->sidedef == mLineSegment->linedef->sidedef[0]) return true;

		// Closed door
		if (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2) return true;
		if (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2) return true;
		if (IsDoorClosed()) return true;

		return false;
	}

	bool SWRenderLine::IsDoorClosed() const
	{
		if (!mBackSector) return false;

		// Portal
		if (mLineSegment->linedef->isVisualPortal() && mLineSegment->sidedef == mLineSegment->linedef->sidedef[0]) return false;

		// Closed door.
		if (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2) return false;
		if (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2) return false;

		// properly render skies (consider door "open" if both ceilings are sky)
		if (mBackSector->GetTexture(sector_t::ceiling) == skyflatnum && mFrontSector->GetTexture(sector_t::ceiling) == skyflatnum) return false;

		// if door is closed because back is shut:
		if (!(mBackCeilingZ1 <= mBackFloorZ1 && mBackCeilingZ2 <= mBackFloorZ2)) return false;

		// preserve a kind of transparent door/lift special effect:
		if (((mBackCeilingZ1 >= mFrontCeilingZ1 && mBackCeilingZ2 >= mFrontCeilingZ2) || mLineSegment->sidedef->GetTexture(side_t::top).isValid())
			&& ((mBackFloorZ1 <= mFrontFloorZ1 && mBackFloorZ2 <= mFrontFloorZ2) || mLineSegment->sidedef->GetTexture(side_t::bottom).isValid()))
		{
			// killough 1/18/98 -- This function is used to fix the automap bug which
			// showed lines behind closed doors simply because the door had a dropoff.
			//
			// It assumes that Doom has already ruled out a door being closed because
			// of front-back closure (e.g. front floor is taller than back ceiling).

			// This fixes the automap floor height bug -- killough 1/18/98:
			// killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
			return true;
		}

		return false;
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
			I_Error("Bad R_StoreWallRange: %i to %i", start, stop);
#endif

		if (!rw_prepped)
		{
			rw_prepped = true;
			SetWallVariables(true);
		}

		side_t *sidedef = mLineSegment->sidedef;

		RenderPortal *renderportal = Thread->Portal.get();
		Clip3DFloors *clip3d = Thread->Clip3D.get();

		DrawSegment *draw_segment = Thread->FrameMemory->NewObject<DrawSegment>();

		// 3D floors code abuses the line render code to update plane clipping
		// lists but doesn't actually draw anything.
		bool onlyUpdatePlaneClip = (m3DFloor.type != Fake3DOpaque::Normal);
		if (!onlyUpdatePlaneClip)
			Thread->DrawSegments->Push(draw_segment);

		draw_segment->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		draw_segment->sx1 = WallC.sx1;
		draw_segment->sx2 = WallC.sx2;
		draw_segment->sz1 = WallC.sz1;
		draw_segment->sz2 = WallC.sz2;
		draw_segment->cx = WallC.tleft.X;
		draw_segment->cy = WallC.tleft.Y;
		draw_segment->cdx = WallC.tright.X - WallC.tleft.X;
		draw_segment->cdy = WallC.tright.Y - WallC.tleft.Y;
		draw_segment->tmapvals = WallT;
		draw_segment->siz1 = 1 / WallC.sz1;
		draw_segment->siz2 = 1 / WallC.sz2;
		draw_segment->x1 = start;
		draw_segment->x2 = stop;
		draw_segment->curline = mLineSegment;
		draw_segment->foggy = foggy;
		draw_segment->SubsectorDepth = Thread->OpaquePass->GetSubsectorDepth(mSubsector->Index());

		bool markportal = ShouldMarkPortal();

		if (markportal)
		{
			draw_segment->silhouette = SIL_BOTH;
		}
		else if (mBackSector == NULL)
		{
			draw_segment->sprtopclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
			draw_segment->sprbottomclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
			fillshort(draw_segment->sprtopclip, stop - start, viewheight);
			memset(draw_segment->sprbottomclip, -1, (stop - start) * sizeof(short));
			draw_segment->silhouette = SIL_BOTH;
		}
		else
		{
			// two sided line
			if (mFrontFloorZ1 > mBackFloorZ1 || mFrontFloorZ2 > mBackFloorZ2 ||
				mBackSector->floorplane.PointOnSide(Thread->Viewport->viewpoint.Pos) < 0)
			{
				draw_segment->silhouette = SIL_BOTTOM;
			}

			if (mFrontCeilingZ1 < mBackCeilingZ1 || mFrontCeilingZ2 < mBackCeilingZ2 ||
				mBackSector->ceilingplane.PointOnSide(Thread->Viewport->viewpoint.Pos) < 0)
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
				if (mDoorClosed || (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2))
				{
					draw_segment->sprbottomclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
					memset(draw_segment->sprbottomclip, -1, (stop - start) * sizeof(short));
					draw_segment->silhouette |= SIL_BOTTOM;
				}
				if (mDoorClosed || (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2))
				{						// killough 1/17/98, 2/8/98
					draw_segment->sprtopclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
					fillshort(draw_segment->sprtopclip, stop - start, viewheight);
					draw_segment->silhouette |= SIL_TOP;
				}
			}

			if (!onlyUpdatePlaneClip && r_3dfloors)
			{
				if (mBackSector->e && mBackSector->e->XFloor.ffloors.Size()) {
					for (i = 0; i < (int)mBackSector->e->XFloor.ffloors.Size(); i++) {
						F3DFloor *rover = mBackSector->e->XFloor.ffloors[i];
						if (rover->flags & FF_RENDERSIDES && (!(rover->flags & FF_INVERTSIDES) || rover->flags & FF_ALLSIDES)) {
							draw_segment->SetHas3DFloorBackSectorWalls();
							break;
						}
					}
				}
				if (mFrontSector->e && mFrontSector->e->XFloor.ffloors.Size()) {
					for (i = 0; i < (int)mFrontSector->e->XFloor.ffloors.Size(); i++) {
						F3DFloor *rover = mFrontSector->e->XFloor.ffloors[i];
						if (rover->flags & FF_RENDERSIDES && (rover->flags & FF_ALLSIDES || rover->flags & FF_INVERTSIDES)) {
							draw_segment->SetHas3DFloorFrontSectorWalls();
							break;
						}
					}
				}
			}

			if (!onlyUpdatePlaneClip)
				// allocate space for masked texture tables, if needed
				// [RH] Don't just allocate the space; fill it in too.
				if ((sidedef->GetTexture(side_t::mid).isValid() || draw_segment->Has3DFloorWalls() || IsFogBoundary(mFrontSector, mBackSector)) &&
					(mCeilingClipped != ProjectedWallCull::OutsideBelow || !sidedef->GetTexture(side_t::top).isValid()) &&
					(mFloorClipped != ProjectedWallCull::OutsideAbove || !sidedef->GetTexture(side_t::bottom).isValid()) &&
					(WallC.sz1 >= TOO_CLOSE_Z && WallC.sz2 >= TOO_CLOSE_Z))
				{
					float *swal;
					fixed_t *lwal;
					int i;

					maskedtexture = true;

					// kg3D - backup for mid and fake walls
					draw_segment->bkup = Thread->FrameMemory->AllocMemory<short>(stop - start);
					memcpy(draw_segment->bkup, &Thread->OpaquePass->ceilingclip[start], sizeof(short)*(stop - start));

					draw_segment->bFogBoundary = IsFogBoundary(mFrontSector, mBackSector);
					if (sidedef->GetTexture(side_t::mid).isValid() || draw_segment->Has3DFloorWalls())
					{
						if (sidedef->GetTexture(side_t::mid).isValid())
							draw_segment->SetHas3DFloorMidTexture();

						draw_segment->maskedtexturecol = Thread->FrameMemory->AllocMemory<fixed_t>(stop - start);
						draw_segment->swall = Thread->FrameMemory->AllocMemory<float>(stop - start);

						lwal = draw_segment->maskedtexturecol;
						swal = draw_segment->swall;
						FTexture *tex = TexMan.GetPalettedTexture(sidedef->GetTexture(side_t::mid), true);
						FSoftwareTexture *pic = tex && tex->isValid()? tex->GetSoftwareTexture() : nullptr;
						double yscale = (pic? pic->GetScale().Y : 1.0) * sidedef->GetTextureYScale(side_t::mid);
						fixed_t xoffset = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::mid));

						if (pic && pic->useWorldPanning())
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
						if (stop - start > 1)
						{
							draw_segment->iscalestep = float((iend - istart) / (stop - start - 1));
						}
						else
						{
							draw_segment->iscalestep = 0;
						}
					}
					draw_segment->light = rw_lightleft + rw_lightstep * (start - WallC.sx1);
					draw_segment->lightstep = rw_lightstep;

					// Masked mMiddlePart.Textures should get the light level from the sector they reference,
					// not from the current subsector, which is what the current lightlevel value
					// comes from. We make an exeption for polyobjects, however, since their "home"
					// sector should be whichever one they move into.
					if (mLineSegment->sidedef->Flags & WALLF_POLYOBJ)
					{
						draw_segment->lightlevel = lightlevel;
					}
					else
					{
						draw_segment->lightlevel = mLineSegment->sidedef->GetLightLevel(foggy, mLineSegment->frontsector->lightlevel);
					}

					if (draw_segment->bFogBoundary || draw_segment->maskedtexturecol != nullptr)
					{
						Thread->DrawSegments->PushTranslucent(draw_segment);
					}
				}
		}

		ClipSegmentTopBottom(start, stop);

		MarkCeilingPlane(start, stop);
		MarkFloorPlane(start, stop);
		Mark3DFloors(start, stop);

		if (m3DFloor.type != Fake3DOpaque::Normal)
		{
			return false;
		}

		MarkOpaquePassClip(start, stop);

		// save sprite clipping info
		if (((draw_segment->silhouette & SIL_TOP) || maskedtexture) && draw_segment->sprtopclip == nullptr)
		{
			draw_segment->sprtopclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
			memcpy(draw_segment->sprtopclip, &Thread->OpaquePass->ceilingclip[start], sizeof(short)*(stop - start));
		}

		if (((draw_segment->silhouette & SIL_BOTTOM) || maskedtexture) && draw_segment->sprbottomclip == nullptr)
		{
			draw_segment->sprbottomclip = Thread->FrameMemory->AllocMemory<short>(stop - start);
			memcpy(draw_segment->sprbottomclip, &Thread->OpaquePass->floorclip[start], sizeof(short)*(stop - start));
		}

		if (maskedtexture && mLineSegment->sidedef->GetTexture(side_t::mid).isValid())
		{
			draw_segment->silhouette |= SIL_TOP | SIL_BOTTOM;
		}

		RenderMiddleTexture(start, stop);
		RenderTopTexture(start, stop);
		RenderBottomTexture(start, stop);

		// [RH] Draw any decals bound to the seg
		// [ZZ] Only if not an active mirror
		if (!markportal)
		{
			RenderDecal::RenderDecals(Thread, mLineSegment->sidedef, draw_segment, lightlevel, rw_lightleft, rw_lightstep, mLineSegment, WallC, foggy, basecolormap, walltop.ScreenY, wallbottom.ScreenY, false);
		}

		if (markportal)
		{
			Thread->Portal->AddLinePortal(mLineSegment->linedef, draw_segment->x1, draw_segment->x2, draw_segment->sprtopclip, draw_segment->sprbottomclip);
		}

		return true;
	}

	bool SWRenderLine::ShouldMarkFloor() const
	{
		if (!mFloorPlane)
			return false;
		
		// deep water check
		if (mFrontSector->GetHeightSec() == nullptr)
		{
			int planeside = mFrontSector->floorplane.PointOnSide(Thread->Viewport->viewpoint.Pos);
			if (mFrontSector->floorplane.fC() < 0)	// 3D floors have the floor backwards
				planeside = -planeside;
			if (planeside <= 0)		// above view plane
				return false;
		}

		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;

		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors))
		{
			return true;
		}
		else if (mBackSector == nullptr) // single sided line
		{
			return true;
		}
		else // two-sided line
		{
			if (linedef->isVisualPortal()) return true;

			// closed door
			if (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2) return true;
			if (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2) return true;

			if (mBackSector->floorplane != mFrontSector->floorplane) return true;
			if (mBackSector->lightlevel != mFrontSector->lightlevel) return true;
			if (mBackSector->GetTexture(sector_t::floor) != mFrontSector->GetTexture(sector_t::floor)) return true;
			if (mBackSector->GetPlaneLight(sector_t::floor) != mFrontSector->GetPlaneLight(sector_t::floor)) return true;

			// Add checks for (x,y) offsets
			if (mBackSector->planes[sector_t::floor].xform != mFrontSector->planes[sector_t::floor].xform) return true;
			if (mBackSector->GetAlpha(sector_t::floor) != mFrontSector->GetAlpha(sector_t::floor)) return true;

			// prevent 2s normals from bleeding through deep water
			if (mFrontSector->heightsec) return true;

			if (mBackSector->GetVisFlags(sector_t::floor) != mFrontSector->GetVisFlags(sector_t::floor)) return true;
			if (mBackSector->Colormap != mFrontSector->Colormap) return true;
			if (mFrontSector->e && mFrontSector->e->XFloor.lightlist.Size()) return true;
			if (mBackSector->e && mBackSector->e->XFloor.lightlist.Size()) return true;

			if (sidedef->GetTexture(side_t::mid).isValid() && ((ib_compatflags & BCOMPATF_CLIPMIDTEX) || (linedef->flags & (ML_CLIP_MIDTEX | ML_WRAP_MIDTEX)) || sidedef->Flags & (WALLF_CLIP_MIDTEX | WALLF_WRAP_MIDTEX))) return true;

			return false;
		}
	}

	bool SWRenderLine::ShouldMarkCeiling() const
	{
		if (!mCeilingPlane)
			return false;
		
		// deep water check
		if (mFrontSector->GetHeightSec() == nullptr && mFrontSector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			int planeside = mFrontSector->ceilingplane.PointOnSide(Thread->Viewport->viewpoint.Pos);
			if (mFrontSector->ceilingplane.fC() > 0) // 3D floors have the ceiling backwards
				planeside = -planeside;
			if (planeside <= 0) // below view plane
				return false;
		}

		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;

		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors))
		{
			return true;
		}
		else if (mBackSector == nullptr) // single sided line
		{
			return true;
		}
		else // two-sided line
		{
			if (linedef->isVisualPortal()) return true;

			// closed door
			if (mBackCeilingZ1 <= mFrontFloorZ1 && mBackCeilingZ2 <= mFrontFloorZ2) return true;
			if (mBackFloorZ1 >= mFrontCeilingZ1 && mBackFloorZ2 >= mFrontCeilingZ2) return true;

			if (mFrontSector->GetTexture(sector_t::ceiling) != skyflatnum || mBackSector->GetTexture(sector_t::ceiling) != skyflatnum)
			{
				if (mBackSector->ceilingplane != mFrontSector->ceilingplane) return true;
				if (mBackSector->lightlevel != mFrontSector->lightlevel) return true;
				if (mBackSector->GetTexture(sector_t::ceiling) != mFrontSector->GetTexture(sector_t::ceiling)) return true;

				// Add checks for (x,y) offsets
				if (mBackSector->planes[sector_t::ceiling].xform != mFrontSector->planes[sector_t::ceiling].xform) return true;
				if (mBackSector->GetAlpha(sector_t::ceiling) != mFrontSector->GetAlpha(sector_t::ceiling)) return true;

				// prevent 2s normals from bleeding through fake ceilings
				if (mFrontSector->heightsec && mFrontSector->GetTexture(sector_t::ceiling) != skyflatnum) return true;

				if (mBackSector->GetPlaneLight(sector_t::ceiling) != mFrontSector->GetPlaneLight(sector_t::ceiling)) return true;
				if (mBackSector->GetFlags(sector_t::ceiling) != mFrontSector->GetFlags(sector_t::ceiling)) return true;

				if (mBackSector->Colormap != mFrontSector->Colormap) return true;
				if (mFrontSector->e && mFrontSector->e->XFloor.lightlist.Size()) return true;
				if (mBackSector->e && mBackSector->e->XFloor.lightlist.Size()) return true;

				if (sidedef->GetTexture(side_t::mid).isValid())
				{
					if (ib_compatflags & BCOMPATF_CLIPMIDTEX) return true;
					if (linedef->flags & (ML_CLIP_MIDTEX | ML_WRAP_MIDTEX)) return true;
					if (sidedef->Flags & (WALLF_CLIP_MIDTEX | WALLF_WRAP_MIDTEX)) return true;
				}
			}
			return false;
		}
	}

	bool SWRenderLine::ShouldMarkPortal() const
	{
		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;

		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors))
		{
			return true;
		}
		else
		{
			return linedef->isVisualPortal();
		}
	}
	
	void SWRenderLine::SetWallVariables(bool needlights)
	{
		RenderPortal *renderportal = Thread->Portal.get();

		bool rw_havehigh = false;
		bool rw_havelow = false;
		if (mBackSector)
		{
			// Cannot make these walls solid, because it can result in
			// sprite clipping problems for sprites near the wall
			if (mFrontCeilingZ1 > mBackCeilingZ1 || mFrontCeilingZ2 > mBackCeilingZ2)
			{
				rw_havehigh = true;
				wallupper.Project(Thread->Viewport.get(), mBackSector->ceilingplane, &WallC, mLineSegment, renderportal->MirrorFlags & RF_XFLIP);
			}
			if (mFrontFloorZ1 < mBackFloorZ1 || mFrontFloorZ2 < mBackFloorZ2)
			{
				rw_havelow = true;
				walllower.Project(Thread->Viewport.get(), mBackSector->floorplane, &WallC, mLineSegment, renderportal->MirrorFlags & RF_XFLIP);
			}
		}

		if (mLineSegment->linedef->special == Line_Horizon)
		{
			// Be aware: Line_Horizon does not work properly with sloped planes
			fillshort(walltop.ScreenY + WallC.sx1, WallC.sx2 - WallC.sx1, Thread->Viewport->viewwindow.centery);
			fillshort(wallbottom.ScreenY + WallC.sx1, WallC.sx2 - WallC.sx1, Thread->Viewport->viewwindow.centery);
		}
		else
		{
			mCeilingClipped = walltop.Project(Thread->Viewport.get(), mFrontSector->ceilingplane, &WallC, mLineSegment, renderportal->MirrorFlags & RF_XFLIP);
			mFloorClipped = wallbottom.Project(Thread->Viewport.get(), mFrontSector->floorplane, &WallC, mLineSegment, renderportal->MirrorFlags & RF_XFLIP);
		}

		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;

		// mark the segment as visible for auto map
		if (!Thread->Scene->DontMapLines()) linedef->flags |= ML_MAPPED;

		markfloor = ShouldMarkFloor();
		markceiling = ShouldMarkCeiling();

		SetTopTexture();
		SetMiddleTexture();
		SetBottomTexture();

		if (mBackSector && !(sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors)))
		{
			// skyhack to allow height changes in outdoor areas
			if (mFrontSector->GetTexture(sector_t::ceiling) == skyflatnum &&
				mBackSector->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				if (rw_havehigh)
				{ // front ceiling is above back ceiling
					memcpy(&walltop.ScreenY[WallC.sx1], &wallupper.ScreenY[WallC.sx1], (WallC.sx2 - WallC.sx1) * sizeof(walltop.ScreenY[0]));
					rw_havehigh = false;
				}
				else if (rw_havelow && mFrontSector->ceilingplane != mBackSector->ceilingplane)
				{ // back ceiling is above front ceiling
				  // The check for rw_havelow is not Doom-compliant, but it avoids HoM that
				  // would otherwise occur because there is space made available for this
				  // wall but nothing to draw for it.
				  // Recalculate walltop so that the wall is clipped by the back sector's
				  // ceiling instead of the front sector's ceiling.
					walltop.Project(Thread->Viewport.get(), mBackSector->ceilingplane, &WallC, mLineSegment, Thread->Portal->MirrorFlags & RF_XFLIP);
				}
			}
		}

		FTexture *ftex = TexMan.GetPalettedTexture(sidedef->GetTexture(side_t::mid), true);
		FSoftwareTexture *midtex = ftex && ftex->isValid() ? ftex->GetSoftwareTexture() : nullptr;

		bool segtextured = ftex != NULL || mTopPart.Texture != NULL || mBottomPart.Texture != NULL;

		// calculate light table
		if (needlights && (segtextured || (mBackSector && IsFogBoundary(mFrontSector, mBackSector))))
		{
			lwallscale =
				ftex ? ((midtex? midtex->GetScale().X : 1.0) * sidedef->GetTextureXScale(side_t::mid)) :
				mTopPart.Texture ? (mTopPart.Texture->GetScale().X * sidedef->GetTextureXScale(side_t::top)) :
				mBottomPart.Texture ? (mBottomPart.Texture->GetScale().X * sidedef->GetTextureXScale(side_t::bottom)) :
				1.;

			walltexcoords.Project(Thread->Viewport.get(), sidedef->TexelLength * lwallscale, WallC.sx1, WallC.sx2, WallT);

			CameraLight *cameraLight = CameraLight::Instance();
			if (cameraLight->FixedColormap() == nullptr && cameraLight->FixedLightLevel() < 0)
			{
				lightlevel = mLineSegment->sidedef->GetLightLevel(foggy, mFrontSector->lightlevel);
				rw_lightleft = float(Thread->Light->WallVis(WallC.sz1, foggy));
				rw_lightstep = float((Thread->Light->WallVis(WallC.sz2, foggy) - rw_lightleft) / (WallC.sx2 - WallC.sx1));
			}
			else
			{
				rw_lightleft = 1;
				rw_lightstep = 0;
			}
		}
	}

	void SWRenderLine::SetTopTexture()
	{
		mTopPart.Texture = nullptr;

		if (!(mFrontCeilingZ1 > mBackCeilingZ1 || mFrontCeilingZ2 > mBackCeilingZ2)) return;

		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;
		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors)) return;
		if (!mBackSector) return;
		
		// No top texture for skyhack lines
		if (mFrontSector->GetTexture(sector_t::ceiling) == skyflatnum && mBackSector->GetTexture(sector_t::ceiling) == skyflatnum) return;
		
		FTexture *tex = TexMan.GetPalettedTexture(sidedef->GetTexture(side_t::top), true);
		mTopPart.Texture = tex && tex->isValid() ? tex->GetSoftwareTexture() : nullptr;
		if (mTopPart.Texture == nullptr) return;

		mTopPart.TextureOffsetU = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::top));
		double rowoffset = sidedef->GetTextureYOffset(side_t::top);
		mTopPart.TextureScaleU = sidedef->GetTextureXScale(side_t::top);
		mTopPart.TextureScaleV = sidedef->GetTextureYScale(side_t::top);
		double yrepeat = mTopPart.Texture->GetScale().Y * mTopPart.TextureScaleV;
		if (yrepeat >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGTOP)
			{ // top of texture at top
				mTopPart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
				if (rowoffset < 0 && mTopPart.Texture != NULL)
				{
					rowoffset += mTopPart.Texture->GetHeight();
				}
			}
			else
			{ // bottom of texture at bottom
				mTopPart.TextureMid = (mBackSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat + mTopPart.Texture->GetHeight();
			}
		}
		else
		{ // upside down
			rowoffset = -rowoffset;
			if (linedef->flags & ML_DONTPEGTOP)
			{ // bottom of texture at top
				mTopPart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat + mTopPart.Texture->GetHeight();
			}
			else
			{ // top of texture at bottom
				mTopPart.TextureMid = (mBackSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
			}
		}
		if (mTopPart.Texture->useWorldPanning())
		{
			mTopPart.TextureMid += rowoffset * yrepeat;
		}
		else
		{
			mTopPart.TextureMid += rowoffset;
		}
	}
	
	void SWRenderLine::SetMiddleTexture()
	{
		mMiddlePart.Texture = nullptr;
		
		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;
		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors)) return;
		if (mBackSector) return;
		
		// [RH] Horizon lines do not need to be textured
		if (linedef->isVisualPortal()) return;
		if (linedef->special == Line_Horizon) return;
			
		auto tex = TexMan.GetPalettedTexture(sidedef->GetTexture(side_t::mid), true);
		mMiddlePart.Texture = tex && tex->isValid() ? tex->GetSoftwareTexture() : nullptr;
		if (mMiddlePart.Texture == nullptr) return;
		mMiddlePart.TextureOffsetU = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::mid));
		double rowoffset = sidedef->GetTextureYOffset(side_t::mid);
		mMiddlePart.TextureScaleU = sidedef->GetTextureXScale(side_t::mid);
		mMiddlePart.TextureScaleV = sidedef->GetTextureYScale(side_t::mid);
		double yrepeat = mMiddlePart.Texture->GetScale().Y * mMiddlePart.TextureScaleV;
		if (yrepeat >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				mMiddlePart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::floor) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat + mMiddlePart.Texture->GetHeight();
			}
			else
			{ // top of texture at top
				mMiddlePart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
				if (rowoffset < 0 && mMiddlePart.Texture != NULL)
				{
					rowoffset += mMiddlePart.Texture->GetHeight();
				}
			}
		}
		else
		{ // upside down
			rowoffset = -rowoffset;
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // top of texture at bottom
				mMiddlePart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::floor) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
			}
			else
			{ // bottom of texture at top
				mMiddlePart.TextureMid = (mFrontSector->GetPlaneTexZ(sector_t::ceiling) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat + mMiddlePart.Texture->GetHeight();
			}
		}
		if (mMiddlePart.Texture->useWorldPanning())
		{
			mMiddlePart.TextureMid += rowoffset * yrepeat;
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			mMiddlePart.TextureMid += rowoffset;
		}
	}
	
	void SWRenderLine::SetBottomTexture()
	{
		mBottomPart.Texture = nullptr;
		
		if (!(mFrontFloorZ1 < mBackFloorZ1 || mFrontFloorZ2 < mBackFloorZ2)) return;
		
		side_t *sidedef = mLineSegment->sidedef;
		line_t *linedef = mLineSegment->linedef;
		if (sidedef == linedef->sidedef[0] && (linedef->special == Line_Mirror && r_drawmirrors)) return;
		if (!mBackSector) return;
		
		double frontlowertop = mFrontSector->GetPlaneTexZ(sector_t::ceiling);
		if (mFrontSector->GetTexture(sector_t::ceiling) == skyflatnum && mBackSector->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			// Putting sky ceilings on the front and back of a line alters the way unpegged
			// positioning works.
			frontlowertop = mBackSector->GetPlaneTexZ(sector_t::ceiling);
		}
		
		FTexture *tex = TexMan.GetPalettedTexture(sidedef->GetTexture(side_t::bottom), true);
		mBottomPart.Texture = tex && tex->isValid() ? tex->GetSoftwareTexture() : nullptr;
		if (!mBottomPart.Texture) return;

		mBottomPart.TextureOffsetU = FLOAT2FIXED(sidedef->GetTextureXOffset(side_t::bottom));
		double rowoffset = sidedef->GetTextureYOffset(side_t::bottom);
		mBottomPart.TextureScaleU = sidedef->GetTextureXScale(side_t::bottom);
		mBottomPart.TextureScaleV = sidedef->GetTextureYScale(side_t::bottom);
		double yrepeat = mBottomPart.Texture->GetScale().Y * mBottomPart.TextureScaleV;
		if (yrepeat >= 0)
		{ // normal orientation
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // bottom of texture at bottom
				mBottomPart.TextureMid = (frontlowertop - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
			}
			else
			{ // top of texture at top
				mBottomPart.TextureMid = (mBackSector->GetPlaneTexZ(sector_t::floor) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
				if (rowoffset < 0 && mBottomPart.Texture != NULL)
				{
					rowoffset += mBottomPart.Texture->GetHeight();
				}
			}
		}
		else
		{ // upside down
			rowoffset = -rowoffset;
			if (linedef->flags & ML_DONTPEGBOTTOM)
			{ // top of texture at bottom
				mBottomPart.TextureMid = (frontlowertop - Thread->Viewport->viewpoint.Pos.Z) * yrepeat;
			}
			else
			{ // bottom of texture at top
				mBottomPart.TextureMid = (mBackSector->GetPlaneTexZ(sector_t::floor) - Thread->Viewport->viewpoint.Pos.Z) * yrepeat + mBottomPart.Texture->GetHeight();
			}
		}
		if (mBottomPart.Texture->useWorldPanning())
		{
			mBottomPart.TextureMid += rowoffset * yrepeat;
		}
		else
		{
			mBottomPart.TextureMid += rowoffset;
		}
	}

	bool SWRenderLine::IsFogBoundary(sector_t *front, sector_t *back) const
	{
		return r_fogboundary && CameraLight::Instance()->FixedColormap() == nullptr && front->Colormap.FadeColor &&
			front->Colormap.FadeColor != back->Colormap.FadeColor &&
			(front->GetTexture(sector_t::ceiling) != skyflatnum || back->GetTexture(sector_t::ceiling) != skyflatnum);
	}
	
	void SWRenderLine::ClipSegmentTopBottom(int x1, int x2)
	{
		// clip wall to the floor and ceiling
		auto ceilingclip = Thread->OpaquePass->ceilingclip;
		auto floorclip = Thread->OpaquePass->floorclip;
		for (int x = x1; x < x2; ++x)
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
	}

	void SWRenderLine::MarkCeilingPlane(int x1, int x2)
	{
		// mark ceiling areas
		if (markceiling)
		{
			mCeilingPlane = Thread->PlaneList->GetRange(mCeilingPlane, x1, x2);

			auto ceilingclip = Thread->OpaquePass->ceilingclip;
			auto floorclip = Thread->OpaquePass->floorclip;
			Clip3DFloors *clip3d = Thread->Clip3D.get();

			for (int x = x1; x < x2; ++x)
			{
				short top = (clip3d->fakeFloor && m3DFloor.type == Fake3DOpaque::FakeCeiling) ? clip3d->fakeFloor->ceilingclip[x] : ceilingclip[x];
				short bottom = MIN(walltop.ScreenY[x], floorclip[x]);
				if (top < bottom)
				{
					mCeilingPlane->top[x] = top;
					mCeilingPlane->bottom[x] = bottom;
				}
			}
		}
	}

	void SWRenderLine::MarkFloorPlane(int x1, int x2)
	{
		if (markfloor)
		{
			mFloorPlane = Thread->PlaneList->GetRange(mFloorPlane, x1, x2);

			auto ceilingclip = Thread->OpaquePass->ceilingclip;
			auto floorclip = Thread->OpaquePass->floorclip;
			Clip3DFloors *clip3d = Thread->Clip3D.get();

			for (int x = x1; x < x2; ++x)
			{
				short top = MAX(wallbottom.ScreenY[x], ceilingclip[x]);
				short bottom = (clip3d->fakeFloor && m3DFloor.type == Fake3DOpaque::FakeFloor) ? clip3d->fakeFloor->floorclip[x] : floorclip[x];
				if (top < bottom)
				{
					assert(bottom <= viewheight);
					mFloorPlane->top[x] = top;
					mFloorPlane->bottom[x] = bottom;
				}
			}
		}
	}

	void SWRenderLine::Mark3DFloors(int x1, int x2)
	{
		Clip3DFloors *clip3d = Thread->Clip3D.get();

		// kg3D - fake planes clipping
		if (m3DFloor.type == Fake3DOpaque::FakeBack)
		{
			auto ceilingclip = Thread->OpaquePass->ceilingclip;
			auto floorclip = Thread->OpaquePass->floorclip;

			if (m3DFloor.clipBotFront)
			{
				memcpy(clip3d->fakeFloor->floorclip + x1, wallbottom.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else
			{
				for (int x = x1; x < x2; ++x)
				{
					walllower.ScreenY[x] = MIN(MAX(walllower.ScreenY[x], ceilingclip[x]), wallbottom.ScreenY[x]);
				}
				memcpy(clip3d->fakeFloor->floorclip + x1, walllower.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			if (m3DFloor.clipTopFront)
			{
				memcpy(clip3d->fakeFloor->ceilingclip + x1, walltop.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else
			{
				for (int x = x1; x < x2; ++x)
				{
					wallupper.ScreenY[x] = MAX(MIN(wallupper.ScreenY[x], floorclip[x]), walltop.ScreenY[x]);
				}
				memcpy(clip3d->fakeFloor->ceilingclip + x1, wallupper.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
		}
	}

	void SWRenderLine::MarkOpaquePassClip(int x1, int x2)
	{
		auto ceilingclip = Thread->OpaquePass->ceilingclip;
		auto floorclip = Thread->OpaquePass->floorclip;

		if (mMiddlePart.Texture) // one sided line
		{
			fillshort(ceilingclip + x1, x2 - x1, viewheight);
			fillshort(floorclip + x1, x2 - x1, 0xffff);
		}
		else
		{ // two sided line
			if (mTopPart.Texture != nullptr)
			{ // top wall
				for (int x = x1; x < x2; ++x)
				{
					wallupper.ScreenY[x] = MAX(MIN(wallupper.ScreenY[x], floorclip[x]), walltop.ScreenY[x]);
				}
				memcpy(ceilingclip + x1, wallupper.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else if (markceiling)
			{ // no top wall
				memcpy(ceilingclip + x1, walltop.ScreenY + x1, (x2 - x1) * sizeof(short));
			}

			if (mBottomPart.Texture != nullptr)
			{ // bottom wall
				for (int x = x1; x < x2; ++x)
				{
					walllower.ScreenY[x] = MIN(MAX(walllower.ScreenY[x], ceilingclip[x]), wallbottom.ScreenY[x]);
				}
				memcpy(floorclip + x1, walllower.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
			else if (markfloor)
			{ // no bottom wall
				memcpy(floorclip + x1, wallbottom.ScreenY + x1, (x2 - x1) * sizeof(short));
			}
		}
	}

	void SWRenderLine::RenderTopTexture(int x1, int x2)
	{
		if (mMiddlePart.Texture) return;
		if (!mTopPart.Texture) return;
		if (!viewactive) return;

		auto rw_pic = mTopPart.Texture;
		double xscale = rw_pic->GetScale().X * mTopPart.TextureScaleU;
		double yscale = rw_pic->GetScale().Y * mTopPart.TextureScaleV;
		if (xscale != lwallscale)
		{
			walltexcoords.ProjectPos(Thread->Viewport.get(), mLineSegment->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
			lwallscale = xscale;
		}
		fixed_t offset;
		if (mTopPart.Texture->useWorldPanning())
		{
			offset = xs_RoundToInt(mTopPart.TextureOffsetU * xscale);
		}
		else
		{
			offset = mTopPart.TextureOffsetU;
		}
		if (xscale < 0)
		{
			offset = -offset;
		}

		float rw_light = rw_lightleft + rw_lightstep * (x1 - WallC.sx1);

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(mFrontSector, mLineSegment, WallC, rw_pic, x1, x2, walltop.ScreenY, wallupper.ScreenY, mTopPart.TextureMid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(mFrontCeilingZ1, mFrontCeilingZ2), MIN(mBackCeilingZ1, mBackCeilingZ2), false, false, OPAQUE, lightlevel, offset, rw_light, rw_lightstep, GetLightList(), foggy, basecolormap);
	}

	void SWRenderLine::RenderMiddleTexture(int x1, int x2)
	{
		if (!mMiddlePart.Texture) return;
		if (!viewactive) return;

		auto rw_pic = mMiddlePart.Texture;
		double xscale = rw_pic->GetScale().X * mMiddlePart.TextureScaleU;
		double yscale = rw_pic->GetScale().Y * mMiddlePart.TextureScaleV;
		if (xscale != lwallscale)
		{
			walltexcoords.ProjectPos(Thread->Viewport.get(), mLineSegment->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
			lwallscale = xscale;
		}
		fixed_t offset;
		if (mMiddlePart.Texture->useWorldPanning())
		{
			offset = xs_RoundToInt(mMiddlePart.TextureOffsetU * xscale);
		}
		else
		{
			offset = mMiddlePart.TextureOffsetU;
		}
		if (xscale < 0)
		{
			offset = -offset;
		}

		float rw_light = rw_lightleft + rw_lightstep * (x1 - WallC.sx1);

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(mFrontSector, mLineSegment, WallC, rw_pic, x1, x2, walltop.ScreenY, wallbottom.ScreenY, mMiddlePart.TextureMid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(mFrontCeilingZ1, mFrontCeilingZ2), MIN(mFrontFloorZ1, mFrontFloorZ2), false, false, OPAQUE, lightlevel, offset, rw_light, rw_lightstep, GetLightList(), foggy, basecolormap);
	}

	void SWRenderLine::RenderBottomTexture(int x1, int x2)
	{
		if (mMiddlePart.Texture) return;
		if (!mBottomPart.Texture) return;
		if (!viewactive) return;

		auto rw_pic = mBottomPart.Texture;
		double xscale = rw_pic->GetScale().X * mBottomPart.TextureScaleU;
		double yscale = rw_pic->GetScale().Y * mBottomPart.TextureScaleV;
		if (xscale != lwallscale)
		{
			walltexcoords.ProjectPos(Thread->Viewport.get(), mLineSegment->sidedef->TexelLength*xscale, WallC.sx1, WallC.sx2, WallT);
			lwallscale = xscale;
		}
		fixed_t offset;
		if (mBottomPart.Texture->useWorldPanning())
		{
			offset = xs_RoundToInt(mBottomPart.TextureOffsetU * xscale);
		}
		else
		{
			offset = mBottomPart.TextureOffsetU;
		}
		if (xscale < 0)
		{
			offset = -offset;
		}

		float rw_light = rw_lightleft + rw_lightstep * (x1 - WallC.sx1);

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(mFrontSector, mLineSegment, WallC, rw_pic, x1, x2, walllower.ScreenY, wallbottom.ScreenY, mBottomPart.TextureMid, walltexcoords.VStep, walltexcoords.UPos, yscale, MAX(mBackFloorZ1, mBackFloorZ2), MIN(mFrontFloorZ1, mFrontFloorZ2), false, false, OPAQUE, lightlevel, offset, rw_light, rw_lightstep, GetLightList(), foggy, basecolormap);
	}

	FLightNode *SWRenderLine::GetLightList()
	{
		CameraLight *cameraLight = CameraLight::Instance();
		if ((cameraLight->FixedLightLevel() >= 0) || cameraLight->FixedColormap())
			return nullptr; // [SP] Don't draw dynlights if invul/lightamp active
		else if (mLineSegment && mLineSegment->sidedef)
			return mLineSegment->sidedef->lighthead;
		else
			return nullptr;
	}

	////////////////////////////////////////////////////////////////////////////

	// Transform and clip coordinates. Returns true if it was clipped away
	bool FWallCoords::Init(RenderThread *thread, const DVector2 &pt1, const DVector2 &pt2, double too_close)
	{
		auto viewport = thread->Viewport.get();
		RenderPortal *renderportal = thread->Portal.get();

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
		}
		else
		{
			if (tright.X < -tright.Y) return true;	// wall is off the left side
			float den = tleft.X - tright.X - tright.Y + tleft.Y;
			if (den == 0) return true;
			fsx1 = 0;
			fsz1 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X + tleft.Y) / den;
		}

		if (fsz1 < too_close)
			return true;

		if (tright.X <= tright.Y)
		{
			if (tright.X < -tright.Y) return true;	// right edge is off the left side
			if (tright.Y == 0) return true;
			fsx2 = viewport->CenterX + tright.X * viewport->CenterX / tright.Y;
			fsz2 = tright.Y;
		}
		else
		{
			if (tleft.X > tleft.Y) return true;	// wall is off the right side
			float den = tright.Y - tleft.Y - tright.X + tleft.X;
			if (den == 0) return true;
			fsx2 = viewwidth;
			fsz2 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X - tleft.Y) / den;
		}

		if (fsz2 < too_close)
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

		return sx2 <= sx1;
	}

	/////////////////////////////////////////////////////////////////////////

	void FWallTmapVals::InitFromWallCoords(RenderThread *thread, const FWallCoords *wallc)
	{
		const FVector2 *left = &wallc->tleft;
		const FVector2 *right = &wallc->tright;
		
		RenderPortal *renderportal = thread->Portal.get();

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			swapvalues(left, right);
		}
		UoverZorg = left->X * thread->Viewport->CenterX;
		UoverZstep = -left->Y;
		InvZorg = (left->X - right->X) * thread->Viewport->CenterX;
		InvZstep = right->Y - left->Y;
	}

	void FWallTmapVals::InitFromLine(RenderThread *thread, const DVector2 &left, const DVector2 &right)
	{
		// Coordinates should have already had viewx,viewy subtracted

		auto viewport = thread->Viewport.get();

		double fullx1 = left.X * viewport->viewpoint.Sin - left.Y * viewport->viewpoint.Cos;
		double fullx2 = right.X * viewport->viewpoint.Sin - right.Y * viewport->viewpoint.Cos;
		double fully1 = left.X * viewport->viewpoint.TanCos + left.Y * viewport->viewpoint.TanSin;
		double fully2 = right.X * viewport->viewpoint.TanCos + right.Y * viewport->viewpoint.TanSin;
		
		RenderPortal *renderportal = thread->Portal.get();

		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			fullx1 = -fullx1;
			fullx2 = -fullx2;
		}

		UoverZorg = float(fullx1 * viewport->CenterX);
		UoverZstep = float(-fully1);
		InvZorg = float((fullx1 - fullx2) * viewport->CenterX);
		InvZstep = float(fully2 - fully1);
	}
}
