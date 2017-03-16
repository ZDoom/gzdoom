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
//		Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------


#ifndef __R_STATE_H__
#define __R_STATE_H__

// Need data structure definitions.
#include "doomtype.h"
#include "r_defs.h"
#include "r_data/sprites.h"

//
// Refresh internal data structures,
//	for rendering.
//

extern int				viewwindowx;
extern int				viewwindowy;
extern int				viewwidth;
extern int				viewheight;

//
// Lookup tables for map data.
//
extern TArray<spritedef_t> sprites;
extern uint32_t NumStdSprites;

extern TArray<vertexdata_t> vertexdatas;

extern int				numnodes;
extern node_t*			nodes;

extern TArray<zone_t>	Zones;

extern node_t * 		gamenodes;
extern int 				numgamenodes;


//
// POV data.
//

int R_FindSkin (const char *name, int pclass);	// [RH] Find a skin

#endif // __R_STATE_H__
