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

#include "c_cvars.h"

void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);

// [RH] There are two different RenderSegLoops.
extern void (*R_RenderSegLoop)(void);
EXTERN_CVAR (r_columnmethod)

void R_RenderSegLoop1 (void);
void R_RenderSegLoop2 (void);

#endif
