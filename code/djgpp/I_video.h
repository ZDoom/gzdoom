// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_video.h,v 1.4 1998/05/03 22:40:58 killough Exp $
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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"
#include "v_video.h"

// [RH] Always true since DOS doesn't go windowed.
extern BOOL Fullscreen;


// Called by D_DoomMain,
// Sets up the video mode
void I_InitGraphics (void);

// [RH] Set the display mode
void I_SetMode (int width, int height, int Bpp);

void I_ShutdownGraphics(void);

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

BOOL I_AllocateScreen (screen_t *scrn, int width, int height, int Bpp);
void I_FreeScreen (screen_t *scrn);
void I_LockScreen (screen_t *scrn);
void I_UnlockScreen (screen_t *scrn);
void I_Blit (screen_t *src, int srcx, int srcy, int srcwidth, int srcheight,
			 screen_t *dest, int destx, int desty, int destwidth, int destheight);

#endif
