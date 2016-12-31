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
// DESCRIPTION:
//		Refresh, visplane stuff (floor, ceilings).
//
//-----------------------------------------------------------------------------


#ifndef __R_PLANE_H__
#define __R_PLANE_H__

#include "r_visible_plane.h"

namespace swrenderer
{

typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t	floorfunc;
extern planefunction_t	ceilingfunc_t;

extern short			floorclip[MAXWIDTH];
extern short			ceilingclip[MAXWIDTH];

void R_ClearPlanes (bool fullclear);

void R_AddPlaneLights(visplane_t *plane, FLightNode *light_head);

int R_DrawPlanes ();
void R_DrawSinglePlane(visplane_t *pl, fixed_t alpha, bool additive, bool masked);
void R_MapVisPlane (visplane_t *pl, void (*mapfunc)(int y, int x1, int x2), void (*stepfunc)());

void R_DrawFogBoundary(int x1, int x2, short *uclip, short *dclip);

visplane_t *R_FindPlane(const secplane_t &height, FTextureID picnum, int lightlevel, double alpha, bool additive, const FTransform &xform, int sky, FSectorPortal *portal);
visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop);

extern visplane_t*		floorplane;
extern visplane_t*		ceilingplane;

}

#endif // __R_PLANE_H__
