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
#include "r_defs.h"

EXTERN_CVAR (Bool, r_drawflat)		// [RH] Don't texture segs?

namespace swrenderer
{

struct visplane_t;

// The 3072 below is just an arbitrary value picked to avoid
// drawing lines the player is too close to that would overflow
// the texture calculations.
#define TOO_CLOSE_Z (3072.0 / (1<<12))

enum
{
	FAKED_Center,
	FAKED_BelowFloor,
	FAKED_AboveCeiling
};

void R_RenderScene();
void R_RenderBSPNode (void *node);

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, seg_t *backline, int backx1, int backx2, double frontcz1, double frontcz2);

extern short floorclip[MAXWIDTH];
extern short ceilingclip[MAXWIDTH];

}

#endif
