// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
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
// $Log:$
//
// DESCRIPTION:
//		Here is a core component: drawing the floors and ceilings,
//		 while maintaining a per column clipping list only.
//		Moreover, the sky areas have to be determined.
//
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// Lee Killough
//
// [RH] Further modified to significantly increase accuracy and add slopes.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "r_sky.h"
#include "stats.h"

#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "gl/dynlights/gl_dynlight.h"
#include "r_walldraw.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "r_portal.h"
#include "swrenderer/plane/r_skyplane.h"
#include "swrenderer/plane/r_flatplane.h"
#include "swrenderer/plane/r_slopeplane.h"
#include "swrenderer/r_memory.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

CVAR(Bool, tilt, false, 0);

namespace swrenderer
{
	using namespace drawerargs;

extern subsector_t *InSubsector;

visplane_t 				*floorplane;
visplane_t 				*ceilingplane;

// These are copies of the main parameters used when drawing stacked sectors.
// When you change the main parameters, you should copy them here too *unless*
// you are changing them to draw a stacked sector. Otherwise, stacked sectors
// won't draw in skyboxes properly.
int stacked_extralight;
double stacked_visibility;
DVector3 stacked_viewpos;
DAngle stacked_angle;

//
// Clip values are the solid pixel bounding the range.
//	floorclip starts out SCREENHEIGHT and is just outside the range
//	ceilingclip starts out 0 and is just inside the range
//
short					floorclip[MAXWIDTH];
short					ceilingclip[MAXWIDTH];

//
// texture mapping
//

short					spanend[MAXHEIGHT];

void					R_DrawSinglePlane (visplane_t *, fixed_t alpha, bool additive, bool masked);


//==========================================================================


//==========================================================================

namespace
{
	enum { max_plane_lights = 32 * 1024 };
	visplane_light plane_lights[max_plane_lights];
	int next_plane_light = 0;
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

//==========================================================================
//
// R_ClearPlanes
//
// Called at the beginning of each frame.
//
//==========================================================================

void R_ClearPlanes (bool fullclear)
{
	int i;

	// Don't clear fake planes if not doing a full clear.
	if (!fullclear)
	{
		for (i = 0; i <= MAXVISPLANES-1; i++)	// new code -- killough
		{
			for (visplane_t **probe = &visplanes[i]; *probe != NULL; )
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
					vis->next = NULL;
					freehead = &vis->next;
				}
			}
		}
	}
	else
	{
		for (i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
		{
			for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			{
				freehead = &(*freehead)->next;
			}
		}

		// opening / clipping determination
		fillshort (floorclip, viewwidth, viewheight);
		// [RH] clip ceiling to console bottom
		fillshort (ceilingclip, viewwidth,
			!screen->Accel2D && ConBottom > viewwindowy && !bRenderingToCanvas
			? (ConBottom - viewwindowy) : 0);

		R_FreeOpenings();

		next_plane_light = 0;
	}
}

//==========================================================================
//
// R_FindPlane
//
// killough 2/28/98: Add offsets
//==========================================================================

visplane_t *R_FindPlane (const secplane_t &height, FTextureID picnum, int lightlevel, double Alpha, bool additive,
						const FTransform &xxform,
						 int sky, FSectorPortal *portal)
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
		isskybox = portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX);
	}
	else if (portal != NULL && !(portal->mFlags & PORTSF_INSKYBOX))
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
		if (fake3D & (FAKE3D_FAKEFLOOR|FAKE3D_FAKECEILING)) sky = 0x80000000 | fakeAlpha;
		else sky = 0;	// not skyflatnum so it can't be a sky
		portal = NULL;
		alpha = OPAQUE;
	}

	// New visplane algorithm uses hash table -- killough
	hash = isskybox ? MAXVISPLANES : visplane_hash (picnum.GetIndex(), lightlevel, height);

	for (check = visplanes[hash]; check; check = check->next)	// killough
	{
		if (isskybox)
		{
			if (portal == check->portal && plane == check->height)
			{
				if (portal->mType != PORTS_SKYVIEWPOINT)
				{ // This skybox is really a stacked sector, so we need to
				  // check even more.
					if (check->extralight == stacked_extralight &&
						check->visibility == stacked_visibility &&
						check->viewpos == stacked_viewpos &&
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
								check->viewangle == stacked_angle
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
			CurrentPortalUniq == check->CurrentPortalUniq &&
			MirrorFlags == check->MirrorFlags &&
			CurrentSkybox == check->CurrentSkybox &&
			ViewPos == check->viewpos
			)
		{
		  return check;
		}
	}

	check = new_visplane (hash);		// killough

	check->height = plane;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->xform = *xform;
	check->colormap = basecolormap;		// [RH] Save colormap
	check->sky = sky;
	check->portal = portal;
	check->left = viewwidth;			// Was SCREENWIDTH -- killough 11/98
	check->right = 0;
	check->extralight = stacked_extralight;
	check->visibility = stacked_visibility;
	check->viewpos = stacked_viewpos;
	check->viewangle = stacked_angle;
	check->Alpha = alpha;
	check->Additive = additive;
	check->CurrentPortalUniq = CurrentPortalUniq;
	check->MirrorFlags = MirrorFlags;
	check->CurrentSkybox = CurrentSkybox;

	fillshort (check->top, viewwidth, 0x7fff);

	return check;
}

//==========================================================================
//
// R_CheckPlane
//
//==========================================================================

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop)
{
	int intrl, intrh;
	int unionl, unionh;
	int x;

	assert (start >= 0 && start < viewwidth);
	assert (stop > start && stop <= viewwidth);

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

		if (pl->portal != NULL && !(pl->portal->mFlags & PORTSF_INSKYBOX) && viewactive)
		{
			hash = MAXVISPLANES;
		}
		else
		{
			hash = visplane_hash (pl->picnum.GetIndex(), pl->lightlevel, pl->height);
		}
		visplane_t *new_pl = new_visplane (hash);

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
		fillshort (pl->top, viewwidth, 0x7fff);
	}
	return pl;
}

//==========================================================================
//
// R_DrawPlanes
//
// At the end of each frame.
//
//==========================================================================

int R_DrawPlanes ()
{
	visplane_t *pl;
	int i;
	int vpcount = 0;

	ds_color = 3;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentPortalUniq != CurrentPortalUniq || pl->CurrentSkybox != CurrentSkybox)
				continue;
			// kg3D - draw only real planes now
			if(pl->sky >= 0) {
				vpcount++;
				R_DrawSinglePlane (pl, OPAQUE, false, false);
			}
		}
	}
	return vpcount;
}

// kg3D - draw all visplanes with "height"
void R_DrawHeightPlanes(double height)
{
	visplane_t *pl;
	int i;

	ds_color = 3;

	DVector3 oViewPos = ViewPos;
	DAngle oViewAngle = ViewAngle;

	for (i = 0; i < MAXVISPLANES; i++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			// kg3D - draw only correct planes
			if(pl->CurrentSkybox != CurrentSkybox || pl->CurrentPortalUniq != CurrentPortalUniq)
				continue;
			if(pl->sky < 0 && pl->height.Zat0() == height) {
				ViewPos = pl->viewpos;
				ViewAngle = pl->viewangle;
				MirrorFlags = pl->MirrorFlags;
				R_DrawSinglePlane (pl, pl->sky & 0x7FFFFFFF, pl->Additive, true);
			}
		}
	}
	ViewPos = oViewPos;
	ViewAngle = oViewAngle;
}


//==========================================================================
//
// R_DrawSinglePlane
//
// Draws a single visplane.
//
//==========================================================================

void R_DrawSinglePlane (visplane_t *pl, fixed_t alpha, bool additive, bool masked)
{
	if (pl->left >= pl->right)
		return;

	if (r_drawflat)
	{ // [RH] no texture mapping
		ds_color += 4;
		R_DrawColoredPlane(pl);
	}
	else if (pl->picnum == skyflatnum)
	{ // sky flat
		R_DrawSkyPlane (pl);
	}
	else
	{ // regular flat
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

		if (r_drawflat || (!pl->height.isSlope() && !tilt))
		{
			R_DrawNormalPlane(pl, xscale, yscale, alpha, additive, masked);
		}
		else
		{
			R_DrawTiltedPlane(pl, xscale, yscale, alpha, additive, masked);
		}
	}
	NetUpdate ();
}

//==========================================================================
//
// R_MapVisPlane
//
// t1/b1 are at x
// t2/b2 are at x+1
// spanend[y] is at the right edge
//
//==========================================================================

void R_MapVisPlane (visplane_t *pl, void (*mapfunc)(int y, int x1, int x2), void(*stepfunc)())
{
	int x = pl->right - 1;
	int t2 = pl->top[x];
	int b2 = pl->bottom[x];

	ds_light_list = pl->lights;

	if (b2 > t2)
	{
		fillshort (spanend+t2, b2-t2, x);
	}

	for (--x; x >= pl->left; --x)
	{
		int t1 = pl->top[x];
		int b1 = pl->bottom[x];
		const int xr = x+1;
		int stop;

		// Draw any spans that have just closed
		stop = MIN (t1, b2);
		while (t2 < stop)
		{
			int y = t2++;
			int x2 = spanend[y];
			mapfunc (y, xr, x2);
		}
		stop = MAX (b1, t2);
		while (b2 > stop)
		{
			int y = --b2;
			int x2 = spanend[y];
			mapfunc (y, xr, x2);
		}

		// Mark any spans that have just opened
		stop = MIN (t2, b1);
		while (t1 < stop)
		{
			spanend[t1++] = x;
		}
		stop = MAX (b2, t2);
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
		mapfunc (y, pl->left, x2);
	}

	ds_light_list = nullptr;
}

}
