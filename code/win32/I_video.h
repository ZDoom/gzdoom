// Emacs style mode select   -*- C++ -*- 
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
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_VIDEO_H__
#define __I_VIDEO_H__

#include "doomtype.h"
#include "v_video.h"


// [RH] True if the display is not in a window
extern BOOL Fullscreen;


// Called by D_DoomMain,
// Sets up the video mode
void I_InitGraphics (void);

// [RH] Set the display mode
void I_SetMode (int width, int height, int Bpp);

void STACK_ARGS I_ShutdownGraphics(void);

// Takes full 8 bit values.
void I_SetPalette (unsigned int *palette);

void I_BeginUpdate (void);		// [RH] Locks screen[0]
void I_FinishUpdate (void);
void I_FinishUpdateNoBlit (void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_ReadScreen (byte *scr);

void I_BeginRead (void);
void I_EndRead (void);

BOOL I_CheckResolution (int width, int height, int bpp);
BOOL I_SetResolution (int width, int height, int bpp);

#endif // __I_VIDEO_H__
