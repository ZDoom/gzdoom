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
//	Refresh module, data I/O, caching, retrieval of graphics
//	by name.
//
//-----------------------------------------------------------------------------


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

// Retrieve column data for span blitting.
byte *R_GetColumn (int tex, int col);


// I/O, setting up the stuff.
void R_InitData (void);
void R_PrecacheLevel (void);


// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName (const char *name);
inline int R_FlatNumForName (const byte *name) { return R_FlatNumForName ((const char *)name); }


// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName (const char *name);
int R_CheckTextureNumForName (const char *name);

inline int R_TextureNumForName (const byte *name) { return R_TextureNumForName ((const char *)name); }
inline int R_CheckTextureNumForName (const byte *name) { return R_CheckTextureNumForName ((const char *)name); }

int R_ColormapNumForName(const char *name);		// killough 4/4/98
void R_SetDefaultColormap (const char *name);	// [RH] change normal fadetable
unsigned int R_BlendForColormap (int map);		// [RH] return calculated blend for a colormap
extern byte *realcolormaps;						// [RH] make the colormaps externally visible
extern size_t numfakecmaps;

int R_FindSkin (const char *name);	// [RH] Find a skin

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
