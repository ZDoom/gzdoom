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
//		Rendering of moving objects, sprites.
//
//-----------------------------------------------------------------------------


#ifndef __R_THINGS__
#define __R_THINGS__

#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/scene/r_bsp.h"

struct particle_t;
struct FVoxel;

#define MINZ			double((2048*4) / double(1 << 20))
#define BASEXCENTER		(160)
#define BASEYCENTER 	(100)

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{

bool R_ClipSpriteColumnWithPortals(int x, vissprite_t* spr);


void R_CacheSprite (spritedef_t *sprite);
void R_SortVisSprites (int (*compare)(const void *, const void *), size_t first);
void R_AddSprites (sector_t *sec, int lightlevel, WaterFakeSide fakeside);
void R_DrawSprites ();
void R_ClearSprites ();
void R_DrawMasked ();

enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2, DVF_MIRRORED = 4 };

void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);


}

#endif
