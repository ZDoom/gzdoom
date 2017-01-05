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
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/plane/r_flatplane.h"
#include "swrenderer/plane/r_slopeplane.h"
#include "swrenderer/plane/r_skyplane.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/drawers/r_draw.h"

CVAR(Bool, tilt, false, 0);

namespace swrenderer
{
	// [RH] Allocate one extra for sky box planes.
	visplane_t *visplanes[MAXVISPLANES + 1];
	visplane_t *freetail;
	visplane_t **freehead = &freetail;

	namespace
	{
		enum { max_plane_lights = 32 * 1024 };
		visplane_light plane_lights[max_plane_lights];
		int next_plane_light = 0;

		short spanend[MAXHEIGHT];
	}

	void R_DeinitPlanes()
	{
		// do not use R_ClearPlanes because at this point the screen pointer is no longer valid.
		for (int i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
		{
			for (*freehead = visplanes[i], visplanes[i] = nullptr; *freehead; )
			{
				freehead = &(*freehead)->next;
			}
		}
		for (visplane_t *pl = freetail; pl != nullptr; )
		{
			visplane_t *next = pl->next;
			free(pl);
			pl = next;
		}
	}

	visplane_t *new_visplane(unsigned hash)
	{
		visplane_t *check = freetail;

		if (check == nullptr)
		{
			check = (visplane_t *)M_Malloc(sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH * 2));
			memset(check, 0, sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH * 2));
			check->bottom = check->top + MAXWIDTH + 2;
		}
		else if (nullptr == (freetail = freetail->next))
		{
			freehead = &freetail;
		}

		check->lights = nullptr;

		check->next = visplanes[hash];
		visplanes[hash] = check;
		return check;
	}

	void R_PlaneInitData()
	{
		int i;
		visplane_t *pl;

		// Free all visplanes and let them be re-allocated as needed.
		pl = freetail;

		while (pl)
		{
			visplane_t *next = pl->next;
			M_Free(pl);
			pl = next;
		}
		freetail = nullptr;
		freehead = &freetail;

		for (i = 0; i < MAXVISPLANES; i++)
		{
			pl = visplanes[i];
			visplanes[i] = nullptr;
			while (pl)
			{
				visplane_t *next = pl->next;
				M_Free(pl);
				pl = next;
			}
		}
	}

	void R_ClearPlanes(bool fullclear)
	{
		int i;

		// Don't clear fake planes if not doing a full clear.
		if (!fullclear)
		{
			for (i = 0; i <= MAXVISPLANES - 1; i++)	// new code -- killough
			{
				for (visplane_t **probe = &visplanes[i]; *probe != nullptr; )
				{
					if ((*probe)->sky < 0)
					{ // fake: move past it
						probe = &(*probe)->next;
					}
					else
					{ // not fake: move to freelist
						visplane_t *vis = *probe;
						*freehead = vis;
						*probe = vis->next;
						vis->next = nullptr;
						freehead = &vis->next;
					}
				}
			}
		}
		else
		{
			for (i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
			{
				for (*freehead = visplanes[i], visplanes[i] = nullptr; *freehead; )
				{
					freehead = &(*freehead)->next;
				}
			}

			next_plane_light = 0;
		}
	}

	void R_AddPlaneLights(visplane_t *plane, FLightNode *node)
	{
		if (!r_dynlights)
			return;

		while (node)
		{
			if (!(node->lightsource->flags2&MF2_DORMANT))
			{
				bool found = false;
				visplane_light *light_node = plane->lights;
				while (light_node)
				{
					if (light_node->lightsource == node->lightsource)
					{
						found = true;
						break;
					}
					light_node = light_node->next;
				}
				if (!found)
				{
					if (next_plane_light == max_plane_lights)
						return;

					visplane_light *newlight = &plane_lights[next_plane_light++];
					newlight->next = plane->lights;
					newlight->lightsource = node->lightsource;
					plane->lights = newlight;
				}
			}
			node = node->nextLight;
		}
	}

	visplane_t *R_FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive, const FTransform &xxform, int sky, FSectorPortal *portal)
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
		hash = isskybox ? MAXVISPLANES : visplane_hash(picnum.GetIndex(), lightlevel, height);
		
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

		check = new_visplane(hash);		// killough

		check->height = plane;
		check->picnum = picnum;
		check->lightlevel = lightlevel;
		check->xform = *xform;
		check->colormap = basecolormap;		// [RH] Save colormap
		check->sky = sky;
		check->portal = portal;
		check->left = viewwidth;			// Was SCREENWIDTH -- killough 11/98
		check->right = 0;
		check->extralight = renderportal->stacked_extralight;
		check->visibility = renderportal->stacked_visibility;
		check->viewpos = renderportal->stacked_viewpos;
		check->viewangle = renderportal->stacked_angle;
		check->Alpha = alpha;
		check->Additive = additive;
		check->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		check->MirrorFlags = renderportal->MirrorFlags;
		check->CurrentSkybox = Clip3DFloors::Instance()->CurrentSkybox;

		fillshort(check->top, viewwidth, 0x7fff);

		return check;
	}

	visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
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

		for (x = intrl; x < intrh && pl->top[x] == 0x7fff; x++)
			;

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
				hash = visplane_hash(pl->picnum.GetIndex(), pl->lightlevel, pl->height);
			}
			visplane_t *new_pl = new_visplane(hash);

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
			fillshort(pl->top, viewwidth, 0x7fff);
		}
		return pl;
	}

	int R_DrawPlanes()
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
					R_DrawSinglePlane(pl, OPAQUE, false, false);
				}
			}
		}
		return vpcount;
	}

	void R_DrawHeightPlanes(double height)
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

					R_DrawSinglePlane(pl, pl->sky & 0x7FFFFFFF, pl->Additive, true);
				}
			}
		}
		ViewPos = oViewPos;
		ViewAngle = oViewAngle;
	}

	void R_DrawSinglePlane(visplane_t *pl, fixed_t alpha, bool additive, bool masked)
	{
		if (pl->left >= pl->right)
			return;

		if (pl->picnum == skyflatnum) // sky flat
		{
			R_DrawSkyPlane(pl);
		}
		else // regular flat
		{
			FTexture *tex = TexMan(pl->picnum, true);

			if (tex->UseType == FTexture::TEX_Null)
			{
				return;
			}

			if (!masked && !additive)
			{ // If we're not supposed to see through this plane, draw it opaque.
				alpha = OPAQUE;
			}
			else if (!tex->bMasked)
			{ // Don't waste time on a masked texture if it isn't really masked.
				masked = false;
			}
			R_SetSpanTexture(tex);
			double xscale = pl->xform.xScale * tex->Scale.X;
			double yscale = pl->xform.yScale * tex->Scale.Y;

			basecolormap = pl->colormap;

			if (!pl->height.isSlope() && !tilt)
			{
				R_DrawNormalPlane(pl, xscale, yscale, alpha, additive, masked);
			}
			else
			{
				R_DrawTiltedPlane(pl, xscale, yscale, alpha, additive, masked);
			}
		}
		NetUpdate();
	}

	void R_MapVisPlane(visplane_t *pl, void(*mapfunc)(int y, int x1, int x2), void(*stepfunc)())
	{
		// t1/b1 are at x
		// t2/b2 are at x+1
		// spanend[y] is at the right edge

		int x = pl->right - 1;
		int t2 = pl->top[x];
		int b2 = pl->bottom[x];

		if (b2 > t2)
		{
			fillshort(spanend + t2, b2 - t2, x);
		}

		for (--x; x >= pl->left; --x)
		{
			int t1 = pl->top[x];
			int b1 = pl->bottom[x];
			const int xr = x + 1;
			int stop;

			// Draw any spans that have just closed
			stop = MIN(t1, b2);
			while (t2 < stop)
			{
				int y = t2++;
				int x2 = spanend[y];
				mapfunc(y, xr, x2);
			}
			stop = MAX(b1, t2);
			while (b2 > stop)
			{
				int y = --b2;
				int x2 = spanend[y];
				mapfunc(y, xr, x2);
			}

			// Mark any spans that have just opened
			stop = MIN(t2, b1);
			while (t1 < stop)
			{
				spanend[t1++] = x;
			}
			stop = MAX(b2, t2);
			while (b1 > stop)
			{
				spanend[--b1] = x;
			}

			t2 = pl->top[x];
			b2 = pl->bottom[x];

			if (stepfunc)
				stepfunc();
		}
		// Draw any spans that are still open
		while (t2 < b2)
		{
			int y = --b2;
			int x2 = spanend[y];
			mapfunc(y, pl->left, x2);
		}
	}
}
