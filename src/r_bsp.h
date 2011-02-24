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

struct drawseg_t
{
	seg_t*		curline;
	fixed_t		light, lightstep;
	fixed_t		iscale, iscalestep;
	short 		x1, x2;			// Same as sx1 and sx2, but clipped to the drawseg
	fixed_t		yrepeat;
	fixed_t		siz1, siz2;		// 1/z for left, right of parent seg on screen
	FWallTexMapParm tmap;
	BYTE 		silhouette;		// 0=none, 1=bottom, 2=top, 3=both
	BYTE		bFogBoundary;
	BYTE		bFakeBoundary;		// for fake walls
	int			shade;
// Pointers to lists for sprite clipping,
// all three adjusted so [x1] is first value.
	ptrdiff_t	sprtopclip; 		// type short
	ptrdiff_t	sprbottomclip;		// type short
	ptrdiff_t	maskedtexturecol;	// type short
	ptrdiff_t	swall;				// type fixed_t
	int fake;	// ident fake drawseg, don't draw and clip sprites
// backups
	ptrdiff_t	bkup;	// sprtopclip backup, for mid and fake textures
};

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

extern bool Has3DFloors;

typedef void (*drawfunc_t) (int start, int stop);

EXTERN_CVAR (Bool, r_drawflat)		// [RH] Don't texture segs?

// BSP?
void R_ClearClipSegs (short left, short right);
void R_ClearDrawSegs ();
void R_BuildPolyBSP(subsector_t *sub);
void R_RenderBSPNode (void *node);

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec, int *flight, int *clight, bool back=false, int x1=0, int x2=0);

int WallMost (short *mostbuf, const FWallTexMapParm *tmap, const secplane_t &plane, vertex_t *v1, vertex_t *v2);
int OWallMost (short *mostbuf, const FWallTexMapParm *tmap, fixed_t z);
void PrepWall (fixed_t *swall, fixed_t *lwall, const FWallTexMapParm *tmap, fixed_t walxrepeat);
void PrepLWall (fixed_t *lwall, const FWallTexMapParm *tmap, fixed_t walxrepeat);

enum EWallVis
{
	WT_Front,		// Wall is front-facing and visible.
	WT_Back,		// Wall is back-facing and visible.
	WT_Gone			// Wall is either out of view or parellel to the line of sight.
};

EWallVis R_TransformWall(FWallTexMapParm *texmap, seg_t *seg, bool allow_back);
EWallVis R_ProjectWall(FWallTexMapParm *texmap, EWallVis vis, fixed_t too_close_z=32);
void R_SetFullTMapParms(FWallTexMapParm *texmap);
void R_SetPartialTMapParms(FWallTexMapParm *texmap, EWallVis vis, seg_t *seg);

#endif
