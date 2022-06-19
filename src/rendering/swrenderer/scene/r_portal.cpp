//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2006-2016 Christoph Oelckers
// Copyright 2015-2016 ZZYZX
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

#include <stdlib.h>
#include <math.h>


#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_bbox.h"
#include "r_portal.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "stats.h"
#include "i_video.h"

#include "a_sharedglobal.h"
#include "r_data/r_translate.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "v_palette.h"
#include "po_man.h"
#include "p_effect.h"
#include "v_font.h"
#include "r_data/colormaps.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "r_utility.h"
#include "r_3dfloors.h"
#include "g_levellocals.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "r_memory.h"
#include "swrenderer/r_renderthread.h"

CVAR(Int, r_portal_recursions, 4, CVAR_ARCHIVE)
#if 0
CVAR(Bool, r_highlight_portals, false, 0)
#endif
CVAR(Bool, r_skyboxes, true, 0)

// Avoid infinite recursion with stacked sectors by limiting them.
#define MAX_SKYBOX_PLANES 1000

namespace swrenderer
{
	RenderPortal::RenderPortal(RenderThread *thread)
	{
		Thread = thread;
	}

	// Draws any recorded sky boxes and then frees them.
	//
	// The process:
	//   1. Move the camera to coincide with the SkyViewpoint.
	//   2. Clear out the old planes. (They have already been drawn.)
	//   3. Clear a window out of the ClipSegs just large enough for the plane.
	//   4. Pretend the existing vissprites and drawsegs aren't there.
	//   5. Create a drawseg at 0 distance to clip sprites to the visplane. It
	//      doesn't need to be associated with a line in the map, since there
	//      will never be any sprites in front of it.
	//   6. Render the BSP, then planes, then masked stuff.
	//   7. Restore the previous vissprites and drawsegs.
	//   8. Repeat for any other sky boxes.
	//   9. Put the camera back where it was to begin with.
	//
	void RenderPortal::RenderPlanePortals()
	{
		numskyboxes = 0;

		VisiblePlaneList *planes = Thread->PlaneList.get();
		DrawSegmentList *drawseglist = Thread->DrawSegments.get();

		if (!planes->HasPortalPlanes())
			return;

		Thread->Clip3D->EnterSkybox();
		CurrentPortalInSkybox = true;

		int savedextralight = Thread->Viewport->viewpoint.extralight;
		DVector3 savedpos = Thread->Viewport->viewpoint.Pos;
		DRotator savedangles = Thread->Viewport->viewpoint.Angles;
		double savedvisibility = Thread->Light->GetVisibility();
		AActor *savedcamera = Thread->Viewport->viewpoint.camera;
		sector_t *savedsector = Thread->Viewport->viewpoint.sector;
		auto Level = Thread->Viewport->Level();

		for (VisiblePlane *pl = planes->PopFirstPortalPlane(); pl != nullptr; pl = planes->PopFirstPortalPlane())
		{
			if (pl->right < pl->left || !r_skyboxes || numskyboxes == MAX_SKYBOX_PLANES || pl->portal == nullptr)
			{
				pl->Render(Thread, OPAQUE, false, false);
				continue;
			}

			numskyboxes++;

			FSectorPortal *port = pl->portal;
			switch (port->mType)
			{
			case PORTS_SKYVIEWPOINT:
			{
				// Don't let gun flashes brighten the sky box
				AActor *sky = port->mSkybox;
				Thread->Viewport->viewpoint.extralight = 0;
				Thread->Light->SetVisibility(Thread->Viewport.get(), sky->args[0] * 0.25f, !!(Level->flags3 & LEVEL3_NOLIGHTFADE));

				Thread->Viewport->viewpoint.Pos = sky->InterpolatedPosition(Thread->Viewport->viewpoint.TicFrac);
				Thread->Viewport->viewpoint.Angles.Yaw = savedangles.Yaw + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * Thread->Viewport->viewpoint.TicFrac);

				CopyStackedViewParameters();
				break;
			}

			case PORTS_STACKEDSECTORTHING:
			case PORTS_PORTAL:
			case PORTS_LINKEDPORTAL:
				Thread->Viewport->viewpoint.extralight = pl->extralight;
				Thread->Light->SetVisibility(Thread->Viewport.get(), pl->visibility, !!(Level->flags3 & LEVEL3_NOLIGHTFADE));
				Thread->Viewport->viewpoint.Pos.X = pl->viewpos.X + port->mDisplacement.X;
				Thread->Viewport->viewpoint.Pos.Y = pl->viewpos.Y + port->mDisplacement.Y;
				Thread->Viewport->viewpoint.Pos.Z = pl->viewpos.Z;
				Thread->Viewport->viewpoint.Angles.Yaw = pl->viewangle;
				break;

			case PORTS_HORIZON:
			case PORTS_PLANE:
				// not implemented yet

			default:
				pl->Render(Thread, OPAQUE, false, false);
				numskyboxes--;
				continue;
			}

			SetInSkyBox(port);
			if (port->mPartner > 0) SetInSkyBox(&Level->sectorPortals[port->mPartner]);
			Thread->Viewport->viewpoint.camera = nullptr;
			Thread->Viewport->viewpoint.sector = port->mDestination;
			assert(Thread->Viewport->viewpoint.sector != nullptr);
			Thread->Viewport->viewpoint.SetViewAngle(Thread->Viewport->viewwindow);
			Thread->Viewport->SetupPolyViewport(Thread);
			Thread->OpaquePass->ClearSeenSprites();
			Thread->Clip3D->ClearFakeFloors();

			planes->ClearKeepFakePlanes();
			Thread->ClipSegments->Clear(pl->left, pl->right);
			WindowLeft = pl->left;
			WindowRight = pl->right;

			auto ceilingclip = Thread->OpaquePass->ceilingclip;
			auto floorclip = Thread->OpaquePass->floorclip;
			for (int i = pl->left; i < pl->right; i++)
			{
				if (pl->top[i] == 0x7fff)
				{
					ceilingclip[i] = viewheight;
					floorclip[i] = -1;
				}
				else
				{
					ceilingclip[i] = pl->top[i];
					floorclip[i] = pl->bottom[i];
				}
			}

			drawseglist->PushPortal();
			Thread->SpriteList->PushPortal();
			viewposStack.Push(Thread->Viewport->viewpoint.Pos);
			visplaneStack.Push(pl);

			// Create a drawseg to clip sprites to the sky plane
			DrawSegment *draw_segment = Thread->FrameMemory->NewObject<DrawSegment>();
			draw_segment->drawsegclip.CurrentPortalUniq = CurrentPortalUniq;
			draw_segment->WallC.sz1 = 0;
			draw_segment->WallC.sz2 = 0;
			draw_segment->x1 = pl->left;
			draw_segment->x2 = pl->right;
			draw_segment->drawsegclip.silhouette = SIL_BOTH;
			draw_segment->drawsegclip.SetTopClip(Thread, pl->left, pl->right, ceilingclip);
			draw_segment->drawsegclip.SetBottomClip(Thread, pl->left, pl->right, floorclip);
			draw_segment->curline = nullptr;
			drawseglist->Push(draw_segment);

			Thread->OpaquePass->RenderScene(Thread->Viewport->Level());
			Thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)
			planes->Render();

			ClearInSkyBox(port);
			if (port->mPartner > 0) SetInSkyBox(&Level->sectorPortals[port->mPartner]);
		}

		// Draw all the masked textures in a second pass, in the reverse order they
		// were added. This must be done separately from the previous step for the
		// sake of nested skyboxes.
		while (viewposStack.Size() > 0)
		{
			// Masked textures and planes need the view coordinates restored for proper positioning.
			viewposStack.Pop(Thread->Viewport->viewpoint.Pos);
			
			Thread->Viewport->SetupPolyViewport(Thread);
			Thread->TranslucentPass->Render();

			VisiblePlane *pl = nullptr;	// quiet, GCC!
			visplaneStack.Pop(pl);
			if (pl->Alpha > 0 && pl->picnum != skyflatnum)
			{
				pl->Render(Thread, pl->Alpha, pl->Additive, true);
			}
			
			Thread->SpriteList->PopPortal();
			drawseglist->PopPortal();
		}

		Thread->Viewport->viewpoint.camera = savedcamera;
		Thread->Viewport->viewpoint.sector = savedsector;
		Thread->Viewport->viewpoint.Pos = savedpos;
		Thread->Light->SetVisibility(Thread->Viewport.get(), savedvisibility, !!(Level->flags3 & LEVEL3_NOLIGHTFADE));
		Thread->Viewport->viewpoint.extralight = savedextralight;
		Thread->Viewport->viewpoint.Angles = savedangles;
		Thread->Viewport->viewpoint.SetViewAngle(Thread->Viewport->viewwindow);
		Thread->Viewport->SetupPolyViewport(Thread);

		CurrentPortalInSkybox = false;
		Thread->Clip3D->LeaveSkybox();

		if (Thread->Clip3D->fakeActive) return;

		planes->ClearPortalPlanes();
	}

	void RenderPortal::RenderLinePortals()
	{
		// [RH] Walk through mirrors
		// [ZZ] Merged with portals
		size_t lastportal = WallPortals.Size();
		for (unsigned int i = 0; i < lastportal; i++)
		{
			RenderLinePortal(WallPortals[i], 0);
		}

		CurrentPortal = nullptr;
		CurrentPortalUniq = 0;
	}

	void RenderPortal::RenderLinePortal(PortalDrawseg* pds, int depth)
	{
		auto viewport = Thread->Viewport.get();
		auto &viewpoint = viewport->viewpoint;
		
		// [ZZ] check depth. fill portal with black if it's exceeding the visual recursion limit, and continue like nothing happened.
		if (depth >= r_portal_recursions)
		{
			uint8_t color = (uint8_t)BestColor((uint32_t *)GPalette.BaseColors, 0, 0, 0, 0, 255);
			int spacing = viewport->RenderTarget->GetPitch();
			for (int x = pds->x1; x < pds->x2; x++)
			{
				if (x < 0 || x >= viewport->RenderTarget->GetWidth())
					continue;

				int Ytop = pds->ceilingclip[x - pds->x1];
				int Ybottom = pds->floorclip[x - pds->x1];

				if (viewport->RenderTarget->IsBgra())
				{
					uint32_t *dest = (uint32_t*)viewport->RenderTarget->GetPixels() + x + Ytop * spacing;

					uint32_t c = GPalette.BaseColors[color].d;
					for (int y = Ytop; y <= Ybottom; y++)
					{
						*dest = c;
						dest += spacing;
					}
				}
				else
				{
					uint8_t *dest = viewport->RenderTarget->GetPixels() + x + Ytop * spacing;

					for (int y = Ytop; y <= Ybottom; y++)
					{
						*dest = color;
						dest += spacing;
					}
				}
			}

#if 0
			if (r_highlight_portals)
				RenderLinePortalHighlight(pds);
#endif

			return;
		}

		DAngle startang = viewpoint.Angles.Yaw;
		DVector3 startpos = viewpoint.Pos;
		DVector3 savedpath[2] = { viewpoint.Path[0], viewpoint.Path[1] };
		ActorRenderFlags savedvisibility = viewpoint.camera ? viewpoint.camera->renderflags & RF_INVISIBLE : ActorRenderFlags::FromInt(0);
		
		viewpoint.camera->renderflags &= ~RF_INVISIBLE;

		CurrentPortalUniq++;

		unsigned int portalsAtStart = WallPortals.Size();

		if (pds->mirror)
		{
			IsInMirrorRecursively = true;

			//vertex_t *v1 = ds->curline->v1;
			vertex_t *v1 = pds->src->v1;

			// Reflect the current view behind the mirror.
			if (pds->src->Delta().X == 0)
			{ // vertical mirror
				viewpoint.Pos.X = v1->fX() - startpos.X + v1->fX();
			}
			else if (pds->src->Delta().Y == 0)
			{ // horizontal mirror
				viewpoint.Pos.Y = v1->fY() - startpos.Y + v1->fY();
			}
			else
			{ // any mirror
				vertex_t *v2 = pds->src->v2;

				double dx = v2->fX() - v1->fX();
				double dy = v2->fY() - v1->fY();
				double x1 = v1->fX();
				double y1 = v1->fY();
				double x = startpos.X;
				double y = startpos.Y;

				// the above two cases catch len == 0
				double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

				viewpoint.Pos.X = (x1 + r * dx) * 2 - x;
				viewpoint.Pos.Y = (y1 + r * dy) * 2 - y;
			}
			viewpoint.Angles.Yaw = pds->src->Delta().Angle() * 2 - startang;
		}
		else
		{
			P_TranslatePortalXY(pds->src, viewpoint.Pos.X, viewpoint.Pos.Y);
			P_TranslatePortalZ(pds->src, viewpoint.Pos.Z);
			P_TranslatePortalAngle(pds->src, viewpoint.Angles.Yaw);
			P_TranslatePortalXY(pds->src, viewpoint.Path[0].X, viewpoint.Path[0].Y);
			P_TranslatePortalXY(pds->src, viewpoint.Path[1].X, viewpoint.Path[1].Y);

			if (!viewpoint.showviewer && viewpoint.camera && P_PointOnLineSidePrecise(viewpoint.Path[0], pds->dst) != P_PointOnLineSidePrecise(viewpoint.Path[1], pds->dst))
			{
				double distp = (viewpoint.Path[0] - viewpoint.Path[1]).Length();
				if (distp > EQUAL_EPSILON)
				{
					double dist1 = (viewpoint.Pos - viewpoint.Path[0]).Length();
					double dist2 = (viewpoint.Pos - viewpoint.Path[1]).Length();

					if (dist1 + dist2 < distp + 1)
					{
						viewpoint.camera->renderflags |= RF_MAYBEINVISIBLE;
					}
				}
			}
		}

		viewpoint.Sin = viewpoint.Angles.Yaw.Sin();
		viewpoint.Cos = viewpoint.Angles.Yaw.Cos();

		viewpoint.TanSin = Thread->Viewport->viewwindow.FocalTangent * viewpoint.Sin;
		viewpoint.TanCos = Thread->Viewport->viewwindow.FocalTangent * viewpoint.Cos;

		CopyStackedViewParameters();

		validcount++;
		PortalDrawseg* prevpds = CurrentPortal;
		CurrentPortal = pds;

		Thread->PlaneList->ClearKeepFakePlanes();
		Thread->ClipSegments->Clear(pds->x1, pds->x2);

		WindowLeft = pds->x1;
		WindowRight = pds->x2;

		// RF_XFLIP should be removed before calling the root function
		int prevmf = MirrorFlags;
		if (pds->mirror)
		{
			if (MirrorFlags & RF_XFLIP)
				MirrorFlags &= ~RF_XFLIP;
			else MirrorFlags |= RF_XFLIP;
		}

		Thread->Viewport->SetupPolyViewport(Thread);

		// some portals have height differences, account for this here
		Thread->Clip3D->EnterSkybox(); // push 3D floor height map
		CurrentPortalInSkybox = false; // first portal in a skybox should set this variable to false for proper clipping in skyboxes.

		// first pass, set clipping
		auto ceilingclip = Thread->OpaquePass->ceilingclip;
		auto floorclip = Thread->OpaquePass->floorclip;
		memcpy(ceilingclip + pds->x1, &pds->ceilingclip[0], pds->len * sizeof(*ceilingclip));
		memcpy(floorclip + pds->x1, &pds->floorclip[0], pds->len * sizeof(*floorclip));

		Thread->OpaquePass->RenderScene(Thread->Viewport->Level());
		Thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)
		if (!savedvisibility && viewpoint.camera) viewpoint.camera->renderflags &= ~RF_INVISIBLE;

		Thread->PlaneList->Render();
		RenderPlanePortals();

		double vzp = viewpoint.Pos.Z;

		int prevuniq = CurrentPortalUniq;
		// depth check is in another place right now
		unsigned int portalsAtEnd = WallPortals.Size();
		for (; portalsAtStart < portalsAtEnd; portalsAtStart++)
		{
			RenderLinePortal(WallPortals[portalsAtStart], depth + 1);
		}
		int prevuniq2 = CurrentPortalUniq;
		CurrentPortalUniq = prevuniq;

		Thread->TranslucentPass->Render();	  //      this is required since with portals there often will be cases when more than 80% of the view is inside a portal.

		Thread->Clip3D->LeaveSkybox(); // pop 3D floor height map
		CurrentPortalUniq = prevuniq2;

#if 0
		// draw a red line around a portal if it's being highlighted
		if (r_highlight_portals)
			RenderLinePortalHighlight(pds);
#endif

		CurrentPortal = prevpds;
		MirrorFlags = prevmf;
		IsInMirrorRecursively = false;
		viewpoint.Angles.Yaw = startang;
		viewpoint.Pos = startpos;
		viewpoint.Path[0] = savedpath[0];
		viewpoint.Path[1] = savedpath[1];

		viewport->SetupPolyViewport(Thread);
	}

#if 0
	void RenderPortal::RenderLinePortalHighlight(PortalDrawseg* pds)
	{
		// [ZZ] NO OVERFLOW CHECKS HERE
		//      I believe it won't break. if it does, blame me. :(

		auto viewport = Thread->Viewport.get();

		if (viewport->RenderTarget->IsBgra()) // Assuming this is just a debug function
			return;

		uint8_t color = (uint8_t)BestColor((uint32_t *)GPalette.BaseColors, 255, 0, 0, 0, 255);

		uint8_t* pixels = viewport->RenderTarget->GetBuffer();
		// top edge
		for (int x = pds->x1; x < pds->x2; x++)
		{
			if (x < 0 || x >= viewport->RenderTarget->GetWidth())
				continue;

			int p = x - pds->x1;
			int Ytop = pds->ceilingclip[p];
			int Ybottom = pds->floorclip[p];

			if (x == pds->x1 || x == pds->x2 - 1)
			{
				viewport->RenderTarget->DrawLine(x, Ytop, x, Ybottom + 1, color, 0);
				continue;
			}

			int YtopPrev = pds->ceilingclip[p - 1];
			int YbottomPrev = pds->floorclip[p - 1];

			if (abs(Ytop - YtopPrev) > 1)
				viewport->RenderTarget->DrawLine(x, YtopPrev, x, Ytop, color, 0);
			else *(pixels + Ytop * viewport->RenderTarget->GetPitch() + x) = color;

			if (abs(Ybottom - YbottomPrev) > 1)
				viewport->RenderTarget->DrawLine(x, YbottomPrev, x, Ybottom, color, 0);
			else *(pixels + Ybottom * viewport->RenderTarget->GetPitch() + x) = color;
		}
	}
#endif
	
	void RenderPortal::CopyStackedViewParameters()
	{
		stacked_viewpos = Thread->Viewport->viewpoint.Pos;
		stacked_angle = Thread->Viewport->viewpoint.Angles;
		stacked_extralight = Thread->Viewport->viewpoint.extralight;
		stacked_visibility = Thread->Light->GetVisibility();
	}
	
	void RenderPortal::SetMainPortal()
	{
		WindowLeft = Thread->X1;
		WindowRight = Thread->X2;
		MirrorFlags = 0;
		CurrentPortal = nullptr;
		CurrentPortalUniq = 0;
		WallPortals.Clear();
		SectorPortalsInSkyBox.clear();
	}

	void RenderPortal::AddLinePortal(line_t *linedef, int x1, int x2, const short *topclip, const short *bottomclip)
	{
		WallPortals.Push(Thread->FrameMemory->NewObject<PortalDrawseg>(Thread, linedef, x1, x2, topclip, bottomclip));
	}
}
/*
ADD_STAT(skyboxes)
{
	FString out;
	out.Format("%d skybox planes", swrenderer::RenderPortal::Instance()->numskyboxes);
	return out;
}
*/
