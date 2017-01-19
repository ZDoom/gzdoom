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
#include "gl/dynlights/gl_dynlight.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_flatplane.h"
#include "swrenderer/plane/r_slopeplane.h"
#include "swrenderer/plane/r_skyplane.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/drawers/r_draw.h"

namespace swrenderer
{
	VisiblePlaneList *VisiblePlaneList::Instance()
	{
		static VisiblePlaneList instance;
		return &instance;
	}

	VisiblePlaneList::VisiblePlaneList()
	{
		for (auto &plane : visplanes)
			plane = nullptr;
	}

	visplane_t *VisiblePlaneList::Add(unsigned hash)
	{
		visplane_t *newplane = RenderMemory::NewObject<visplane_t>();
		newplane->next = visplanes[hash];
		visplanes[hash] = newplane;
		return newplane;
	}

	void VisiblePlaneList::Clear(bool fullclear)
	{
		// Don't clear fake planes if not doing a full clear.
		if (!fullclear)
		{
			for (int i = 0; i <= MAXVISPLANES - 1; i++)
			{
				for (visplane_t **probe = &visplanes[i]; *probe != nullptr; )
				{
					if ((*probe)->sky < 0)
					{ // fake: move past it
						probe = &(*probe)->next;
					}
					else
					{ // not fake: move from list
						visplane_t *vis = *probe;
						*probe = vis->next;
						vis->next = nullptr;
					}
				}
			}
		}
		else
		{
			for (int i = 0; i <= MAXVISPLANES; i++)
				visplanes[i] = nullptr;
		}
	}

	visplane_t *VisiblePlaneList::FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal, FDynamicColormap *basecolormap)
	{
		secplane_t plane;
		visplane_t *check;
		unsigned hash;						// killough
		bool isskybox;
		const FTransform *xform = &xxform;
		fixed_t alpha = FLOAT2FIXED(Alpha);
		//angle_t angle = (xform.Angle + xform.baseAngle).BAMs();

		FTransform nulltransform;

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
			isskybox = portal != nullptr && !(portal->mFlags & PORTSF_INSKYBOX);
		}
		else if (portal != nullptr && !(portal->mFlags & PORTSF_INSKYBOX))
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
			Clip3DFloors *clip3d = Clip3DFloors::Instance();
			if (clip3d->fake3D & (FAKE3D_FAKEFLOOR | FAKE3D_FAKECEILING)) sky = 0x80000000 | clip3d->fakeAlpha;
			else sky = 0;	// not skyflatnum so it can't be a sky
			portal = nullptr;
			alpha = OPAQUE;
		}

		// New visplane algorithm uses hash table -- killough
		hash = isskybox ? MAXVISPLANES : CalcHash(picnum.GetIndex(), lightlevel, height);
		
		RenderPortal *renderportal = RenderPortal::Instance();

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
									check->viewangle == renderportal->stacked_angle
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
					Clip3DFloors::Instance()->CurrentSkybox == check->CurrentSkybox &&
					ViewPos == check->viewpos
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
		check->viewangle = renderportal->stacked_angle;
		check->Alpha = alpha;
		check->Additive = additive;
		check->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		check->MirrorFlags = renderportal->MirrorFlags;
		check->CurrentSkybox = Clip3DFloors::Instance()->CurrentSkybox;

		return check;
	}

	visplane_t *VisiblePlaneList::GetRange(visplane_t *pl, int start, int stop)
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

			if (pl->portal != nullptr && !(pl->portal->mFlags & PORTSF_INSKYBOX) && viewactive)
			{
				hash = MAXVISPLANES;
			}
			else
			{
				hash = CalcHash(pl->picnum.GetIndex(), pl->lightlevel, pl->height);
			}
			visplane_t *new_pl = Add(hash);

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

	int VisiblePlaneList::Render()
	{
		visplane_t *pl;
		int i;
		int vpcount = 0;

		drawerargs::ds_color = 3;
		
		RenderPortal *renderportal = RenderPortal::Instance();

		for (i = 0; i < MAXVISPLANES; i++)
		{
			for (pl = visplanes[i]; pl; pl = pl->next)
			{
				// kg3D - draw only correct planes
				if (pl->CurrentPortalUniq != renderportal->CurrentPortalUniq || pl->CurrentSkybox != Clip3DFloors::Instance()->CurrentSkybox)
					continue;
				// kg3D - draw only real planes now
				if (pl->sky >= 0) {
					vpcount++;
					pl->Render(OPAQUE, false, false);
				}
			}
		}
		return vpcount;
	}

	void VisiblePlaneList::RenderHeight(double height)
	{
		visplane_t *pl;
		int i;

		drawerargs::ds_color = 3;

		DVector3 oViewPos = ViewPos;
		DAngle oViewAngle = ViewAngle;
		
		RenderPortal *renderportal = RenderPortal::Instance();

		for (i = 0; i < MAXVISPLANES; i++)
		{
			for (pl = visplanes[i]; pl; pl = pl->next)
			{
				if (pl->CurrentSkybox != Clip3DFloors::Instance()->CurrentSkybox || pl->CurrentPortalUniq != renderportal->CurrentPortalUniq)
					continue;

				if (pl->sky < 0 && pl->height.Zat0() == height)
				{
					ViewPos = pl->viewpos;
					ViewAngle = pl->viewangle;
					renderportal->MirrorFlags = pl->MirrorFlags;

					pl->Render(pl->sky & 0x7FFFFFFF, pl->Additive, true);
				}
			}
		}
		ViewPos = oViewPos;
		ViewAngle = oViewAngle;
	}
}
