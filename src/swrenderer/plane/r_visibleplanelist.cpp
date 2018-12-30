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
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "a_dynlight.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_flatplane.h"
#include "swrenderer/plane/r_slopeplane.h"
#include "swrenderer/plane/r_skyplane.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	VisiblePlaneList::VisiblePlaneList(RenderThread *thread)
	{
		Thread = thread;
	}

	VisiblePlaneList::VisiblePlaneList()
	{
		for (auto &plane : visplanes)
			plane = nullptr;
	}

	VisiblePlane *VisiblePlaneList::Add(unsigned hash)
	{
		VisiblePlane *newplane = Thread->FrameMemory->NewObject<VisiblePlane>(Thread);
		newplane->next = visplanes[hash];
		visplanes[hash] = newplane;
		return newplane;
	}

	void VisiblePlaneList::Clear()
	{
		for (int i = 0; i <= MAXVISPLANES; i++)
			visplanes[i] = nullptr;
	}

	void VisiblePlaneList::ClearKeepFakePlanes()
	{
		for (int i = 0; i <= MAXVISPLANES - 1; i++)
		{
			for (VisiblePlane **probe = &visplanes[i]; *probe != nullptr; )
			{
				if ((*probe)->sky < 0)
				{ // fake: move past it
					probe = &(*probe)->next;
				}
				else
				{ // not fake: move from list
					VisiblePlane *vis = *probe;
					*probe = vis->next;
					vis->next = nullptr;
				}
			}
		}
	}

	VisiblePlane *VisiblePlaneList::FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, bool foggy, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal, FDynamicColormap *basecolormap, Fake3DOpaque::Type fakeFloorType, fixed_t fakeAlpha)
	{
		secplane_t plane;
		VisiblePlane *check;
		unsigned hash;						// killough
		bool isskybox;
		const FTransform *xform = &xxform;
		fixed_t alpha = FLOAT2FIXED(Alpha);
		//angle_t angle = (xform.Angle + xform.baseAngle).BAMs();

		FTransform nulltransform;

		RenderPortal *renderportal = Thread->Portal.get();

		lightlevel += LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get());

		if (picnum == skyflatnum)	// killough 10/98
		{ // most skies map together
			lightlevel = 0;
			xform = &nulltransform;
			nulltransform.xOffs = nulltransform.yOffs = nulltransform.baseyOffs = 0;
			nulltransform.xScale = nulltransform.yScale = 1;
			nulltransform.Angle = nulltransform.baseAngle = 0.0;
			additive = false;
			// [RH] Map floor skies and ceiling skies to separate visplanes. This isn't
			// always necessary, but it is needed if a floor and ceiling sky are in the
			// same column but separated by a wall. If they both try to reside in the
			// same visplane, then only the floor sky will be drawn.
			plane.set(0., 0., height.fC(), 0.);
			isskybox = portal != nullptr && !renderportal->InSkyBox(portal);
		}
		else if (portal != nullptr && !renderportal->InSkyBox(portal))
		{
			plane = height;
			isskybox = true;
		}
		else
		{
			plane = height;
			isskybox = false;
			// kg3D - hack, store alpha in sky
			// i know there is ->alpha, but this also allows to identify fake plane
			// and ->alpha is for stacked sectors
			if (fakeFloorType == Fake3DOpaque::FakeFloor || fakeFloorType == Fake3DOpaque::FakeCeiling) sky = 0x80000000 | fakeAlpha;
			else sky = 0;	// not skyflatnum so it can't be a sky
			portal = nullptr;
			alpha = OPAQUE;
		}

		// New visplane algorithm uses hash table -- killough
		hash = isskybox ? ((unsigned)MAXVISPLANES) : CalcHash(picnum.GetIndex(), lightlevel, height);
		
		for (check = visplanes[hash]; check; check = check->next)	// killough
		{
			if (isskybox)
			{
				if (portal == check->portal && plane == check->height)
				{
					if (portal->mType != PORTS_SKYVIEWPOINT)
					{ // This skybox is really a stacked sector, so we need to
					  // check even more.
						if (check->extralight == renderportal->stacked_extralight &&
							check->visibility == renderportal->stacked_visibility &&
							check->viewpos == renderportal->stacked_viewpos &&
							(
								// headache inducing logic... :(
								(portal->mType != PORTS_STACKEDSECTORTHING) ||
								(
									check->Alpha == alpha &&
									check->Additive == additive &&
									(alpha == 0 ||	// if alpha is > 0 everything needs to be checked
									(plane == check->height &&
										picnum == check->picnum &&
										lightlevel == check->lightlevel &&
										basecolormap == check->colormap &&	// [RH] Add more checks
										*xform == check->xform
										)
										) &&
									check->viewangle == renderportal->stacked_angle.Yaw
									)
								)
							)
						{
							return check;
						}
					}
					else
					{
						return check;
					}
				}
			}
			else
				if (plane == check->height &&
					picnum == check->picnum &&
					lightlevel == check->lightlevel &&
					basecolormap == check->colormap &&	// [RH] Add more checks
					*xform == check->xform &&
					sky == check->sky &&
					renderportal->CurrentPortalUniq == check->CurrentPortalUniq &&
					renderportal->MirrorFlags == check->MirrorFlags &&
					Thread->Clip3D->CurrentSkybox == check->CurrentSkybox &&
					Thread->Viewport->viewpoint.Pos == check->viewpos
					)
				{
					return check;
				}
		}

		check = Add(hash);		// killough

		check->height = plane;
		check->picnum = picnum;
		check->lightlevel = lightlevel;
		check->xform = *xform;
		check->colormap = basecolormap;		// [RH] Save colormap
		check->sky = sky;
		check->portal = portal;
		check->extralight = renderportal->stacked_extralight;
		check->visibility = renderportal->stacked_visibility;
		check->viewpos = renderportal->stacked_viewpos;
		check->viewangle = renderportal->stacked_angle.Yaw;
		check->Alpha = alpha;
		check->Additive = additive;
		check->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		check->MirrorFlags = renderportal->MirrorFlags;
		check->CurrentSkybox = Thread->Clip3D->CurrentSkybox;

		return check;
	}

	VisiblePlane *VisiblePlaneList::GetRange(VisiblePlane *pl, int start, int stop)
	{
		int intrl, intrh;
		int unionl, unionh;
		int x;

		assert(start >= 0 && start < viewwidth);
		assert(stop > start && stop <= viewwidth);

		if (start < pl->left)
		{
			intrl = pl->left;
			unionl = start;
		}
		else
		{
			unionl = pl->left;
			intrl = start;
		}

		if (stop > pl->right)
		{
			intrh = pl->right;
			unionh = stop;
		}
		else
		{
			unionh = pl->right;
			intrh = stop;
		}

		x = intrl;
		while (x < intrh && pl->top[x] == 0x7fff) x++;

		if (x >= intrh)
		{
			// use the same visplane
			pl->left = unionl;
			pl->right = unionh;
		}
		else
		{
			// make a new visplane
			unsigned hash;

			if (pl->portal != nullptr && !Thread->Portal->InSkyBox(pl->portal) && viewactive)
			{
				hash = MAXVISPLANES;
			}
			else
			{
				hash = CalcHash(pl->picnum.GetIndex(), pl->lightlevel, pl->height);
			}
			VisiblePlane *new_pl = Add(hash);

			new_pl->height = pl->height;
			new_pl->picnum = pl->picnum;
			new_pl->lightlevel = pl->lightlevel;
			new_pl->xform = pl->xform;
			new_pl->colormap = pl->colormap;
			new_pl->portal = pl->portal;
			new_pl->extralight = pl->extralight;
			new_pl->visibility = pl->visibility;
			new_pl->viewpos = pl->viewpos;
			new_pl->viewangle = pl->viewangle;
			new_pl->sky = pl->sky;
			new_pl->Alpha = pl->Alpha;
			new_pl->Additive = pl->Additive;
			new_pl->CurrentPortalUniq = pl->CurrentPortalUniq;
			new_pl->MirrorFlags = pl->MirrorFlags;
			new_pl->CurrentSkybox = pl->CurrentSkybox;
			new_pl->lights = pl->lights;
			pl = new_pl;
			pl->left = start;
			pl->right = stop;
		}
		return pl;
	}

	bool VisiblePlaneList::HasPortalPlanes() const
	{
		return visplanes[MAXVISPLANES] != nullptr;
	}

	VisiblePlane *VisiblePlaneList::PopFirstPortalPlane()
	{
		VisiblePlane *pl = visplanes[VisiblePlaneList::MAXVISPLANES];
		if (pl)
		{
			visplanes[VisiblePlaneList::MAXVISPLANES] = pl->next;
			pl->next = nullptr;
		}
		return pl;
	}

	void VisiblePlaneList::ClearPortalPlanes()
	{
		visplanes[VisiblePlaneList::MAXVISPLANES] = nullptr;
	}

	int VisiblePlaneList::Render()
	{
		if (Thread->MainThread)
			PlaneCycles.Clock();

		VisiblePlane *pl;
		int i;
		int vpcount = 0;

		RenderPortal *renderportal = Thread->Portal.get();

		for (i = 0; i < MAXVISPLANES; i++)
		{
			for (pl = visplanes[i]; pl; pl = pl->next)
			{
				// kg3D - draw only correct planes
				if (pl->CurrentPortalUniq != renderportal->CurrentPortalUniq || pl->CurrentSkybox != Thread->Clip3D->CurrentSkybox)
					continue;
				// kg3D - draw only real planes now
				if (pl->sky >= 0) {
					vpcount++;
					pl->Render(Thread, OPAQUE, false, false);
				}
			}
		}

		if (Thread->MainThread)
			PlaneCycles.Unclock();

		return vpcount;
	}

	void VisiblePlaneList::RenderHeight(double height)
	{
		VisiblePlane *pl;
		int i;

		DVector3 oViewPos = Thread->Viewport->viewpoint.Pos;
		DAngle oViewAngle = Thread->Viewport->viewpoint.Angles.Yaw;
		
		RenderPortal *renderportal = Thread->Portal.get();

		for (i = 0; i < MAXVISPLANES; i++)
		{
			for (pl = visplanes[i]; pl; pl = pl->next)
			{
				if (pl->CurrentSkybox != Thread->Clip3D->CurrentSkybox || pl->CurrentPortalUniq != renderportal->CurrentPortalUniq)
					continue;

				if (pl->sky < 0 && pl->height.Zat0() == height)
				{
					Thread->Viewport->viewpoint.Pos = pl->viewpos;
					Thread->Viewport->viewpoint.Angles.Yaw = pl->viewangle;
					renderportal->MirrorFlags = pl->MirrorFlags;

					pl->Render(Thread, pl->sky & 0x7FFFFFFF, pl->Additive, true);
				}
			}
		}
		Thread->Viewport->viewpoint.Pos = oViewPos;
		Thread->Viewport->viewpoint.Angles.Yaw = oViewAngle;
	}
}
