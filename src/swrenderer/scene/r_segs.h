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
//		Refresh module, drawing LineSegs from BSP.
//
//-----------------------------------------------------------------------------


#ifndef __R_SEGS_H__
#define __R_SEGS_H__

namespace swrenderer
{

struct drawseg_t;
struct visplane_t;

bool R_StoreWallRange(int start, int stop);
void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);



void R_RenderSegLoop(int x1, int x2);

extern float	rw_light;		// [RH] Scale lights with viewsize adjustments
extern float	rw_lightstep;
extern float	rw_lightleft;
extern fixed_t	rw_offset;
extern FTexture *rw_pic;

extern short floorclip[MAXWIDTH];
extern short ceilingclip[MAXWIDTH];
extern visplane_t *floorplane;
extern visplane_t *ceilingplane;

}

#endif
