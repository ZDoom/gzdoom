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
//	Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#include "doomdef.h"
#include "d_player.h"

// Called by main loop, animate the intermission.
void WI_Ticker ();

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer ();

// Setup for an intermission screen.
void WI_Start (wbstartstruct_t *wbstartstruct);

#endif
