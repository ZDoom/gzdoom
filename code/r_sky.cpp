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
//	wall, wrapping around. 1024 columns equal 360 degrees.
//	The default sky map is 256 columns and repeats 4 times
//	on a 320 screen.
//	
//
//-----------------------------------------------------------------------------



// Needed for FRACUNIT.
#include "m_fixed.h"
#include "r_data.h"
#include "c_cvars.h"
#include "g_level.h"
#include "r_sky.h"

extern int dmflags;
extern int *texturewidthmask;

//
// sky mapping
//
int 		skyflatnum;
int 		sky1texture,	sky2texture;
fixed_t		skytexturemid;
fixed_t		skyscale;
int			skystretch;
fixed_t		skyheight;					
fixed_t		skyiscale;

int			sky1shift,		sky2shift;
fixed_t		sky1pos=0,		sky1speed=0;
fixed_t		sky2pos=0,		sky2speed=0;

// [RH] Stretch sky texture if not taller than 128 pixels?
BEGIN_CUSTOM_CVAR (r_stretchsky, "1", CVAR_ARCHIVE)
{
	R_InitSkyMap ();
}
END_CUSTOM_CVAR (r_stretchsky)

char SKYFLATNAME[8] = "F_SKY1";

extern "C" int detailxshift, detailyshift;
extern fixed_t freelookviewheight;

//==========================================================================
//
// R_InitSkyMap
//
// Called whenever the view size changes.
//
//==========================================================================

void R_InitSkyMap ()
{
	if (textureheight == NULL)
		return;

	if (textureheight[sky1texture] != textureheight[sky2texture])
	{
		Printf (PRINT_HIGH, "\x8a+Both sky textures must be the same height.\x8a-\n");
		sky2texture = sky1texture;
	}

	if (textureheight[sky1texture] <= (128 << FRACBITS))
	{
		skytexturemid = 200/2*FRACUNIT;
		skystretch = (r_stretchsky.value
					  && !(dmflags & DF_NO_FREELOOK)
					  && !(level.flags & LEVEL_FORCENOSKYSTRETCH)) ? 1 : 0;
	}
	else
	{
		skytexturemid = 200*FRACUNIT;
		skystretch = 0;
	}
	skyheight = textureheight[sky1texture] << skystretch;

	if (viewwidth && viewheight)
	{
		skyiscale = (200*FRACUNIT) / (((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift));
		skyscale = ((((freelookviewheight<<detailxshift) * viewwidth) / (viewwidth<<detailxshift)) << FRACBITS) /
					(200);

		skyiscale = FixedMul (skyiscale, FixedDiv (clipangle, ANGLE_45));
		skyscale = FixedMul (skyscale, FixedDiv (ANGLE_45, clipangle));
	}

	// The sky map is 256*128*4 maps.
	sky1shift = 22+skystretch-16;
	sky2shift = 22+skystretch-16;
	if (texturewidthmask[sky1texture] >= 256*2-1)
		sky1shift -= skystretch;
	if (texturewidthmask[sky2texture] >= 256*2-1)
		sky2shift -= skystretch;
}

