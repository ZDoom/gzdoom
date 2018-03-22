//-----------------------------------------------------------------------------
//
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
#include "swrenderer/line/r_farclip_line.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/viewport/r_skydrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	FarClipLine::FarClipLine(RenderThread *thread)
	{
		Thread = thread;
	}

	void FarClipLine::Render(seg_t *line, subsector_t *subsector, VisiblePlane *linefloorplane, VisiblePlane *lineceilingplane, Fake3DOpaque opaque3dfloor)
	{
		mSubsector = subsector;
		mFrontSector = mSubsector->sector;
		mLineSegment = line;
		mFloorPlane = linefloorplane;
		mCeilingPlane = lineceilingplane;
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
			return;

		// reject lines that aren't seen from the portal (if any)
		// [ZZ] 10.01.2016: lines inside a skybox shouldn't be clipped, although this imposes some limitations on portals in skyboxes.
		if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && P_ClipLineToPortal(line->linedef, renderportal->CurrentPortal->dst, Thread->Viewport->viewpoint.Pos))
			return;

		mFrontCeilingZ1 = mFrontSector->ceilingplane.ZatPoint(line->v1);
		mFrontFloorZ1 = mFrontSector->floorplane.ZatPoint(line->v1);
		mFrontCeilingZ2 = mFrontSector->ceilingplane.ZatPoint(line->v2);
		mFrontFloorZ2 = mFrontSector->floorplane.ZatPoint(line->v2);

		mPrepped = false;

		Thread->ClipSegments->Clip(WallC.sx1, WallC.sx2, true, this);
	}

	bool FarClipLine::RenderWallSegment(int x1, int x2)
	{
		if (!mPrepped)
		{
			mPrepped = true;

			//walltop.Project(Thread->Viewport.get(), mFrontSector->ceilingplane, &WallC, mLineSegment, Thread->Portal->MirrorFlags & RF_XFLIP);
			wallbottom.Project(Thread->Viewport.get(), mFrontSector->floorplane, &WallC, mLineSegment, Thread->Portal->MirrorFlags & RF_XFLIP);
			memcpy(walltop.ScreenY, wallbottom.ScreenY, sizeof(short) * Thread->Viewport->RenderTarget->GetWidth());
		}

		ClipSegmentTopBottom(x1, x2);
		MarkCeilingPlane(x1, x2);
		MarkFloorPlane(x1, x2);

		return true;
	}

	void FarClipLine::ClipSegmentTopBottom(int x1, int x2)
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

	void FarClipLine::MarkCeilingPlane(int x1, int x2)
	{
		if (mCeilingPlane)
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

	void FarClipLine::MarkFloorPlane(int x1, int x2)
	{
		if (mFloorPlane)
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
}
