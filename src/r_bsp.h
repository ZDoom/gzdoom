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
//		Refresh module, BSP traversal and handling.
//
//-----------------------------------------------------------------------------


#ifndef __R_BSP__
#define __R_BSP__

#include "tarray.h"
#include <stddef.h>

struct drawseg_s
{
	seg_t*		curline;
	short 		x1, x2;
	fixed_t 	neardepth, fardepth;
	fixed_t		light, lightstep;
	fixed_t		iscale, iscalestep;
	short		sx1, sx2;
	fixed_t		sz1, sz2;
	fixed_t		cx, cy, cdx, cdy;
	BYTE 		silhouette;		// 0=none, 1=bottom, 2=top, 3=both
	BYTE		bFogBoundary;
// Pointers to lists for sprite clipping,
// all three adjusted so [x1] is first value.
	ptrdiff_t	sprtopclip; 		// type short
	ptrdiff_t	sprbottomclip;		// type short
	ptrdiff_t	maskedtexturecol;	// type short
	ptrdiff_t	swall;				// type fixed_t
};
typedef struct drawseg_s drawseg_t;


extern seg_t*		curline;
extern side_t*		sidedef;
extern line_t*		linedef;
extern sector_t*	frontsector;
extern sector_t*	backsector;

extern drawseg_t	*drawsegs;
extern drawseg_t	*firstdrawseg;
extern drawseg_t*	ds_p;

extern TArray<size_t>	InterestingDrawsegs;	// drawsegs that have something drawn on them
extern size_t			FirstInterestingDrawseg;

extern int			WindowLeft, WindowRight;
extern WORD			MirrorFlags;
extern seg_t*		ActiveWallMirror;

extern TArray<size_t>	WallMirrors;

typedef void (*drawfunc_t) (int start, int stop);

EXTERN_CVAR (Bool, r_drawflat)		// [RH] Don't texture segs?

// BSP?
void R_ClearClipSegs (short left, short right);
void R_ClearDrawSegs ();
void R_RenderBSPNode (int bspnum);
void R_RenderSubsectors ();

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *, sector_t *, int *, int *, BOOL);


#endif
