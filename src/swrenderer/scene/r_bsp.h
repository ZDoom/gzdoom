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

extern subsector_t *InSubsector;

extern sector_t*	frontsector;
extern sector_t*	backsector;

extern int			WindowLeft, WindowRight;
extern WORD			MirrorFlags;

void R_RenderBSPNode (void *node);

// killough 4/13/98: fake floors/ceilings for deep water / fake ceilings:
sector_t *R_FakeFlat(sector_t *, sector_t *, int *, int *, bool);

}

#endif
