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
// $Log:$
//
// DESCRIPTION:
//	Sky rendering. The DOOM sky is a texture map like any
//	wall, wrapping around. A 1024 columns equal 360 degrees.
//	The default sky map is 256 columns and repeats 4 times
//	on a 320 screen?
//	
//
//-----------------------------------------------------------------------------



// Needed for FRACUNIT.
#include "m_fixed.h"

// Needed for Flat retrieval.
#include "r_data.h"

#include "c_cvars.h"


#include "r_sky.h"

extern int dmflags;

//
// sky mapping
//
int 					skyflatnum;
int 					sky1texture,	sky2texture;
int 					sky1texturemid,	sky2texturemid;
int						sky1stretch,	sky2stretch;
fixed_t					sky1scale,		sky2scale;
fixed_t					sky1height,		sky2height;

fixed_t					sky1pos=0,		sky1speed=0;
fixed_t					sky2pos=0,		sky2speed=0;

// [RH] Stretch sky texture if not taller than 128 pixels?
cvar_t					*r_stretchsky;


//
// R_InitSkyMap
// Called whenever the view size changes.
//
extern int detailxshift, detailyshift;
extern fixed_t freelookviewheight;

void R_InitSkyMap (cvar_t *var)
{
	// [RH] We are also the callback for r_stretchsky.
	r_stretchsky->u.callback = R_InitSkyMap;

	if (textureheight[sky1texture] <= (128 << FRACBITS)) {
		sky1texturemid = 200/2*FRACUNIT;
		sky1stretch = (var->value && !(dmflags & DF_NO_FREELOOK)) ? 1 : 0;
	} else {
		sky1texturemid = 240/2*FRACUNIT;
		sky1stretch = 0;
	}
	sky1height = textureheight[sky1texture] << sky1stretch;

	if (textureheight[sky1texture] <= (128 << FRACBITS)) {
		sky2texturemid = 200/2*FRACUNIT;
		sky2stretch = (var->value && !(dmflags & DF_NO_FREELOOK)) ? 1 : 0;
	} else {
		sky2texturemid = 240/2*FRACUNIT;
		sky2stretch = 0;
	}
	sky2height = textureheight[sky2texture] << sky2stretch;
	if (viewwidth && viewheight) {
		sky1iscale = (sky1texturemid << 1) / (((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift));
		sky1scale = ((((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift)) << FRACBITS) /
					(sky1texturemid>>(FRACBITS-1));

		sky2iscale = (sky2texturemid << FRACBITS) / (((viewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift));
		sky2scale = ((((viewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift)) << FRACBITS) /
					(sky2texturemid>>(FRACBITS-1));
	}
}

