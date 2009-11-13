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
#include "r_main.h"
#include "v_text.h"
#include "gi.h"

//
// sky mapping
//
FTextureID	skyflatnum;
FTextureID	sky1texture,	sky2texture;
fixed_t		skytexturemid;
fixed_t		skyscale;
fixed_t		skyiscale;
bool		skystretch;

fixed_t		sky1cyl,		sky2cyl;
double		sky1pos,		sky2pos;

// [RH] Stretch sky texture if not taller than 128 pixels?
CUSTOM_CVAR (Bool, r_stretchsky, true, CVAR_ARCHIVE)
{
	R_InitSkyMap ();
}

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
	int skyheight;
	FTexture *skytex1, *skytex2;

	skytex1 = TexMan[sky1texture];
	skytex2 = TexMan[sky2texture];

	if (skytex1 == NULL)
		return;

	if ((level.flags & LEVEL_DOUBLESKY) && skytex1->GetHeight() != skytex2->GetHeight())
	{
		Printf (TEXTCOLOR_BOLD "Both sky textures must be the same height." TEXTCOLOR_NORMAL "\n");
		sky2texture = sky1texture;
	}

	// Skies between [128,200) are stretched to 200 pixels. Shorter skies do
	// not stretch because it is assumed they are meant to tile, and taller
	// skies do not stretch because they provide enough information for no
	// repetition when looking all the way up.
	skyheight = skytex1->GetScaledHeight();
	if (skyheight < 200)
	{
		skystretch = (r_stretchsky
					  && skyheight >= 128
					  && level.IsFreelookAllowed()
					  && !(level.flags & LEVEL_FORCENOSKYSTRETCH)) ? 1 : 0;
		// The sky is shifted down from center so that it is entirely visible
		// when looking straight ahead.
		skytexturemid = -28*FRACUNIT;
	}
	else
	{
		// The sky is directly centered so that it is entirely visible when
		// looking fully up.
		skytexturemid = 0;
		skystretch = false;
	}

	if (viewwidth != 0 && viewheight != 0)
	{
		skyiscale = (r_Yaspect*FRACUNIT) / ((freelookviewheight * viewwidth) / viewwidth);
		skyscale = (((freelookviewheight * viewwidth) / viewwidth) << FRACBITS) /
					(r_Yaspect);

		skyiscale = Scale (skyiscale, FieldOfView, 2048);
		skyscale = Scale (skyscale, 2048, FieldOfView);
	}

	if (skystretch)
	{
		skyscale = Scale(skyscale, SKYSTRETCH_HEIGHT, skyheight);
		skyiscale = Scale(skyiscale, skyheight, SKYSTRETCH_HEIGHT);
		skytexturemid = Scale(skytexturemid, skyheight, SKYSTRETCH_HEIGHT);
	}

	// The standard Doom sky texture is 256 pixels wide, repeated 4 times over 360 degrees,
	// giving a total sky width of 1024 pixels. So if the sky texture is no wider than 1024,
	// we map it to a cylinder with circumfrence 1024. For larger ones, we use the width of
	// the texture as the cylinder's circumfrence.
	sky1cyl = MAX(skytex1->GetWidth(), skytex1->xScale >> (16 - 10));
	sky2cyl = MAX(skytex2->GetWidth(), skytex2->xScale >> (16 - 10));
}

