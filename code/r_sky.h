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


#ifndef __R_SKY__
#define __R_SKY__


#include "c_cvars.h"

// SKY, store the number for name.
extern char SKYFLATNAME[8];

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT 		22

extern int 				sky1texture,	sky2texture;
extern fixed_t			sky1pos,		sky2pos;
extern int				skytexturemid;
extern int				skystretch;
fixed_t					skyiscale;
int						skyscale;
fixed_t					skyheight;

extern cvar_t			*r_stretchsky;

// Called whenever the sky changes.
void R_InitSkyMap		(cvar_t *stretchsky);

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
