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

#include "basictypes.h"

class DCanvas;

// [RH] Set the display mode
void I_SetMode (int &width, int &height, int &bits);

// Takes full 8 bit values.
void I_SetPalette (DWORD *palette);

void I_BeginUpdate (void);		// [RH] Locks screen[0]
void I_FinishUpdate (void);
void I_FinishUpdateNoBlit (void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_ReadScreen (BYTE *scr);

bool I_CheckResolution (int width, int height, int bits);
void I_ClosestResolution (int *width, int *height, int bits);
bool I_SetResolution (int width, int height, int bits);

bool I_AllocateScreen (DCanvas *canvas, int width, int height, int bits);
void I_FreeScreen (DCanvas *canvas);

void I_LockScreen (DCanvas *canvas);
void I_UnlockScreen (DCanvas *canvas);
void I_Blit (DCanvas *from, int srcx, int srcy, int srcwidth, int srcheight,
			 DCanvas *to, int destx, int desty, int destwidth, int destheight);

enum EDisplayType
{
	DISPLAY_WindowOnly,
	DISPLAY_FullscreenOnly,
	DISPLAY_Both
};

#endif // __I_VIDEO_H__
