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
//		Sky rendering.
//
//-----------------------------------------------------------------------------

#ifndef __R_SKY_H__
#define __R_SKY_H__

#include "textures/textures.h"

extern FTextureID	skyflatnum;
extern fixed_t		sky1cyl,		sky2cyl;
extern FTextureID	sky1texture,	sky2texture;
extern double		sky1pos,		sky2pos;
extern double	skytexturemid;
extern float	skyiscale;
extern double	skyscale;
extern bool		skystretch;
extern int		freelookviewheight;

#define SKYSTRETCH_HEIGHT 228

// Called whenever the sky changes.
void R_InitSkyMap		();
void R_UpdateSky (DWORD mstime);

#endif //__R_SKY_H__
