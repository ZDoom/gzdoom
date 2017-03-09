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
#include <math.h>

#include "templates.h"
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
#include "i_system.h"
#include "a_sharedglobal.h"
#include "r_data/r_translate.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "v_palette.h"
#include "po_man.h"
#include "p_effect.h"
#include "st_start.h"
#include "v_font.h"
#include "r_data/colormaps.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "version.h"
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
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

CVAR(Int, r_portal_recursions, 4, CVAR_ARCHIVE)
CVAR(Bool, r_highlight_portals, false, CVAR_ARCHIVE)
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

		int savedextralight = extralight;
		DVector3 savedpos = ViewPos;
		DAngle savedangle = ViewAngle;
		double savedvisibility = LightVisibility::Instance()->GetVisibility();
		AActor *savedcamera = camera;
		sector_t *savedsector = viewsector;

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
				extralight = 0;
				LightVisibility::Instance()->SetVisibility(sky->args[0] * 0.25f);

				ViewPos = sky->InterpolatedPosition(r_TicFracF);
				ViewAngle = savedangle + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * r_TicFracF);

				CopyStackedViewParameters();
				break;
			}

			case PORTS_STACKEDSECTORTHING:
			case PORTS_PORTAL:
			case PORTS_LINKEDPORTAL:
				extralight = pl->extralight;
				LightVisibility::Instance()->SetVisibility(pl->visibility);
				ViewPos.X = pl->viewpos.X + port->mDisplacement.X;
				ViewPos.Y = pl->viewpos.Y + port->mDisplacement.Y;
				ViewPos.Z = pl->viewpos.Z;
				ViewAngle = pl->viewangle;
				break;

			case PORTS_HORIZON:
			case PORTS_PLANE:
				// not implemented yet

			default:
				pl->Render(Thread, OPAQUE, false, false);
				numskyboxes--;
				continue;
			}

			port->mFlags |= PORTSF_INSKYBOX;
			if (port->mPartner > 0) level.sectorPortals[port->mPartner].mFlags |= PORTSF_INSKYBOX;
			camera = nullptr;
			viewsector = port->mDestination;
			assert(viewsector != nullptr);
			R_SetViewAngle();
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
			viewposStack.Push(ViewPos);
			visplaneStack.Push(pl);

			// Create a drawseg to clip sprites to the sky plane
			DrawSegment *draw_segment = Thread->FrameMemory->NewObject<DrawSegment>();
			draw_segment->CurrentPortalUniq = CurrentPortalUniq;
			draw_segment->siz1 = INT_MAX;
			draw_segment->siz2 = INT_MAX;
			draw_segment->sz1 = 0;
			draw_segment->sz2 = 0;
			draw_segment->x1 = pl->left;
			draw_segment->x2 = pl->right;
			draw_segment->silhouette = SIL_BOTH;
			draw_segment->sprbottomclip = Thread->FrameMemory->AllocMemory<short>(pl->right - pl->left);
			draw_segment->sprtopclip = Thread->FrameMemory->AllocMemory<short>(pl->right - pl->left);
			draw_segment->maskedtexturecol = nullptr;
			draw_segment->swall = nullptr;
			draw_segment->bFogBoundary = false;
			draw_segment->curline = nullptr;
			draw_segment->fake = 0;
			draw_segment->foggy = false;
			memcpy(draw_segment->sprbottomclip, floorclip + pl->left, (pl->right - pl->left) * sizeof(short));
			memcpy(draw_segment->sprtopclip, ceilingclip + pl->left, (pl->right - pl->left) * sizeof(short));
			drawseglist->Push(draw_segment);

			Thread->OpaquePass->RenderScene();
			Thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)
			planes->Render();

			port->mFlags &= ~PORTSF_INSKYBOX;
			if (port->mPartner > 0) level.sectorPortals[port->mPartner].mFlags &= ~PORTSF_INSKYBOX;
		}

		// Draw all the masked textures in a second pass, in the reverse order they
		// were added. This must be done separately from the previous step for the
		// sake of nested skyboxes.
		while (viewposStack.Size() > 0)
		{
			// Masked textures and planes need the view coordinates restored for proper positioning.
			viewposStack.Pop(ViewPos);
			
			Thread->TranslucentPass->Render();

			VisiblePlane *pl;
			visplaneStack.Pop(pl);
			if (pl->Alpha > 0 && pl->picnum != skyflatnum)
			{
				pl->Render(Thread, pl->Alpha, pl->Additive, true);
			}
			
			Thread->SpriteList->PopPortal();
			drawseglist->PopPortal();
		}

		camera = savedcamera;
		viewsector = savedsector;
		ViewPos = savedpos;
		LightVisibility::Instance()->SetVisibility(savedvisibility);
		extralight = savedextralight;
		ViewAngle = savedangle;
		R_SetViewAngle();

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
		auto viewport = RenderViewport::Instance();
		
		// [ZZ] check depth. fill portal with black if it's exceeding the visual recursion limit, and continue like nothing happened.
		if (depth >= r_portal_recursions)
		{
			uint8_t color = (uint8_t)BestColor((DWORD *)GPalette.BaseColors, 0, 0, 0, 0, 255);
			int spacing = viewport->RenderTarget->GetPitch();
			for (int x = pds->x1; x < pds->x2; x++)
			{
				if (x < 0 || x >= viewport->RenderTarget->GetWidth())
					continue;

				int Ytop = pds->ceilingclip[x - pds->x1];
				int Ybottom = pds->floorclip[x - pds->x1];

				if (viewport->RenderTarget->IsBgra())
				{
					uint32_t *dest = (uint32_t*)viewport->RenderTarget->GetBuffer() + x + Ytop * spacing;

					uint32_t c = GPalette.BaseColors[color].d;
					for (int y = Ytop; y <= Ybottom; y++)
					{
						*dest = c;
						dest += spacing;
					}
				}
				else
				{
					uint8_t *dest = viewport->RenderTarget->GetBuffer() + x + Ytop * spacing;

					for (int y = Ytop; y <= Ybottom; y++)
					{
						*dest = color;
						dest += spacing;
					}
				}
			}

			if (r_highlight_portals)
				RenderLinePortalHighlight(pds);

			return;
		}

		DAngle startang = ViewAngle;
		DVector3 startpos = ViewPos;
		DVector3 savedpath[2] = { ViewPath[0], ViewPath[1] };
		ActorRenderFlags savedvisibility = camera ? camera->renderflags & RF_INVISIBLE : ActorRenderFlags::FromInt(0);
		
		camera->renderflags &= ~RF_INVISIBLE;

		CurrentPortalUniq++;

		unsigned int portalsAtStart = WallPortals.Size();

		if (pds->mirror)
		{
			//vertex_t *v1 = ds->curline->v1;
			vertex_t *v1 = pds->src->v1;

			// Reflect the current view behind the mirror.
			if (pds->src->Delta().X == 0)
			{ // vertical mirror
				ViewPos.X = v1->fX() - startpos.X + v1->fX();
			}
			else if (pds->src->Delta().Y == 0)
			{ // horizontal mirror
				ViewPos.Y = v1->fY() - startpos.Y + v1->fY();
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

				ViewPos.X = (x1 + r * dx) * 2 - x;
				ViewPos.Y = (y1 + r * dy) * 2 - y;
			}
			ViewAngle = pds->src->Delta().Angle() * 2 - startang;
		}
		else
		{
			P_TranslatePortalXY(pds->src, ViewPos.X, ViewPos.Y);
			P_TranslatePortalZ(pds->src, ViewPos.Z);
			P_TranslatePortalAngle(pds->src, ViewAngle);
			P_TranslatePortalXY(pds->src, ViewPath[0].X, ViewPath[0].Y);
			P_TranslatePortalXY(pds->src, ViewPath[1].X, ViewPath[1].Y);

			if (!r_showviewer && camera && P_PointOnLineSidePrecise(ViewPath[0], pds->dst) != P_PointOnLineSidePrecise(ViewPath[1], pds->dst))
			{
				double distp = (ViewPath[0] - ViewPath[1]).Length();
				if (distp > EQUAL_EPSILON)
				{
					double dist1 = (ViewPos - ViewPath[0]).Length();
					double dist2 = (ViewPos - ViewPath[1]).Length();

					if (dist1 + dist2 < distp + 1)
					{
						camera->renderflags |= RF_INVISIBLE;
					}
				}
			}
		}

		ViewSin = ViewAngle.Sin();
		ViewCos = ViewAngle.Cos();

		ViewTanSin = FocalTangent * ViewSin;
		ViewTanCos = FocalTangent * ViewCos;

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

		// some portals have height differences, account for this here
		Thread->Clip3D->EnterSkybox(); // push 3D floor height map
		CurrentPortalInSkybox = false; // first portal in a skybox should set this variable to false for proper clipping in skyboxes.

		// first pass, set clipping
		auto ceilingclip = Thread->OpaquePass->ceilingclip;
		auto floorclip = Thread->OpaquePass->floorclip;
		memcpy(ceilingclip + pds->x1, &pds->ceilingclip[0], pds->len * sizeof(*ceilingclip));
		memcpy(floorclip + pds->x1, &pds->floorclip[0], pds->len * sizeof(*floorclip));

		Thread->OpaquePass->RenderScene();
		Thread->Clip3D->ResetClip(); // reset clips (floor/ceiling)
		if (!savedvisibility && camera) camera->renderflags &= ~RF_INVISIBLE;

		PlaneCycles.Clock();
		Thread->PlaneList->Render();
		RenderPlanePortals();
		PlaneCycles.Unclock();

		double vzp = ViewPos.Z;

		int prevuniq = CurrentPortalUniq;
		// depth check is in another place right now
		unsigned int portalsAtEnd = WallPortals.Size();
		for (; portalsAtStart < portalsAtEnd; portalsAtStart++)
		{
			RenderLinePortal(WallPortals[portalsAtStart], depth + 1);
		}
		int prevuniq2 = CurrentPortalUniq;
		CurrentPortalUniq = prevuniq;

		if (Thread->MainThread)
			NetUpdate();

		MaskedCycles.Clock(); // [ZZ] count sprites in portals/mirrors along with normal ones.
		Thread->TranslucentPass->Render();	  //      this is required since with portals there often will be cases when more than 80% of the view is inside a portal.
		MaskedCycles.Unclock();

		if (Thread->MainThread)
			NetUpdate();

		Thread->Clip3D->LeaveSkybox(); // pop 3D floor height map
		CurrentPortalUniq = prevuniq2;

		// draw a red line around a portal if it's being highlighted
		if (r_highlight_portals)
			RenderLinePortalHighlight(pds);

		CurrentPortal = prevpds;
		MirrorFlags = prevmf;
		ViewAngle = startang;
		ViewPos = startpos;
		ViewPath[0] = savedpath[0];
		ViewPath[1] = savedpath[1];
	}

	void RenderPortal::RenderLinePortalHighlight(PortalDrawseg* pds)
	{
		// [ZZ] NO OVERFLOW CHECKS HERE
		//      I believe it won't break. if it does, blame me. :(

		auto viewport = RenderViewport::Instance();

		if (viewport->RenderTarget->IsBgra()) // Assuming this is just a debug function
			return;

		uint8_t color = (uint8_t)BestColor((DWORD *)GPalette.BaseColors, 255, 0, 0, 0, 255);

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
	
	void RenderPortal::CopyStackedViewParameters()
	{
		stacked_viewpos = ViewPos;
		stacked_angle = ViewAngle;
		stacked_extralight = extralight;
		stacked_visibility = LightVisibility::Instance()->GetVisibility();
	}
	
	void RenderPortal::SetMainPortal()
	{
		WindowLeft = 0;
		WindowRight = viewwidth;
		MirrorFlags = 0;
		CurrentPortal = nullptr;
		CurrentPortalUniq = 0;
		WallPortals.Clear();
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
