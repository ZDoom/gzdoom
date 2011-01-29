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

struct drawseg_t;

void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);

extern short *openings;
extern ptrdiff_t lastopening;
extern size_t maxopenings;

ptrdiff_t R_NewOpening (ptrdiff_t len);

void R_CheckDrawSegs ();

void R_RenderSegLoop ();

#endif
