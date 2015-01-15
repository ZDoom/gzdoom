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

// The 3072 below is just an arbitrary value picked to avoid
// drawing lines the player is too close to that would overflow
// the texture calculations.
#define TOO_CLOSE_Z 3072

struct FWallCoords
{
	fixed_t		tx1, tx2;	// x coords at left, right of wall in view space	rx1,rx2
	fixed_t		ty1, ty2;	// y coords at left, right of wall in view space	ry1,ry2

	short		sx1, sx2;	// x coords at left, right of wall in screen space	xb1,xb2
	fixed_t		sz1, sz2;	// depth at left, right of wall in screen space		yb1,yb2

	bool Init(int x1, int y1, int x2, int y2, int too_close);
};

struct FWallTmapVals
{
	float		UoverZorg, UoverZstep;
	float		InvZorg, InvZstep;

	void InitFromWallCoords(const FWallCoords *wallc);
	void InitFromLine(int x1, int y1, int x2, int y2);
};

extern FWallCoords WallC;
extern FWallTmapVals WallT;

enum
{
	FAKED_Center,
	FAKED_BelowFloor,
	FAKED_AboveCeiling
};

struct drawseg_t
{
	seg_t*		curline;
	fixed_t		light, lightstep;
	fixed_t		iscale, iscalestep;
	short 		x1, x2;			// Same as sx1 and sx2, but clipped to the drawseg
	short		sx1, sx2;		// left, right of parent seg on screen
	fixed_t		sz1, sz2;		// z for left, right of parent seg on screen
	fixed_t		siz1, siz2;		// 1/z for left, right of parent seg on screen
	fixed_t		cx, cy, cdx, cdy;
	fixed_t		yrepeat;
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
	FWallTmapVals tmapvals;
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

typedef void (*drawfunc_t) (int start, int stop);

EXTERN_CVAR (Bool, r_drawflat)		// [RH] Don't texture segs?

// BSP?
void R_ClearClipSegs (short left, short right);
void R_ClearDrawSegs ();
void R_RenderBSPNode (void *node);

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *, sector_t *, int *, int *, bool);


#endif
