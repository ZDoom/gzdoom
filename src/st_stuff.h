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
//		Status bar code.
//		Does the face/direction indicator animatin.
//		Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

struct event_t;

extern int ST_X;
extern int ST_Y;

bool ST_Responder(event_t* ev);

// [RH] Base blending values (for e.g. underwater)
extern int BaseBlendR, BaseBlendG, BaseBlendB;
extern float BaseBlendA;

#endif
