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

#include "r_data.h"


// killough 10/98: special mask indicates sky flat comes from sidedef
#define PL_SKYFLAT (0x80000000)

// Visplane related.
extern	short*			lastopening;


typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t	floorfunc;
extern planefunction_t	ceilingfunc_t;

extern short			*floorclip;
extern short			*ceilingclip;

extern fixed_t			*yslope;
extern fixed_t			*distscale;

void R_InitPlanes (void);
void R_ClearPlanes (void);

void R_MapPlane (int y, int x1, int x2);
void R_MakeSpans (int x, int t1, int b1, int t2, int b2);
void R_DrawPlanes (void);

visplane_t *R_FindPlane
( fixed_t		height,
  int			picnum,
  int			lightlevel,
  fixed_t		xoffs,		// killough 2/28/98: add x-y offsets
  fixed_t		yoffs,
  fixed_t		xscale,
  fixed_t		yscale,
  angle_t		angle);

visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop);

BOOL R_AlignFlat (int linenum, int side, int fc);

// [RH] Added for multires support
BOOL R_PlaneInitData (void);


#endif // __R_PLANE_H__
