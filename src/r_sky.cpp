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
int			skystretch;
fixed_t		skyheight;					
fixed_t		skyiscale;

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
	fixed_t fskyheight;
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

	fskyheight = skytex1->GetHeight() << FRACBITS;
	if (skytex1->GetScaledHeight() <= 128)
	{
		skytexturemid = r_Yaspect/2*FRACUNIT;
		skystretch = (r_stretchsky
					  && level.IsFreelookAllowed()
					  && !(level.flags & LEVEL_FORCENOSKYSTRETCH)) ? 1 : 0;
	}
	else
	{
		skytexturemid = 199 * skytex1->yScale;
		skystretch = 0;
		// At heights above 600 pixels, the sky is drawn slightly too low.
		if (SCREENHEIGHT > 600)
		{
			skytexturemid += FRACUNIT;
		}
	}
	skyheight = fskyheight << skystretch;

	if (viewwidth && viewheight)
	{
		skyiscale = (r_Yaspect*FRACUNIT) / ((freelookviewheight * viewwidth) / viewwidth);
		skyscale = (((freelookviewheight * viewwidth) / viewwidth) << FRACBITS) /
					(r_Yaspect);

		skyiscale = Scale (skyiscale, FieldOfView, 2048);
		skyscale = Scale (skyscale, 2048, FieldOfView);
	}

	// The standard Doom sky texture is 256 pixels wide, repeated 4 times over 360 degrees,
	// giving a total sky width of 1024 pixels. So if the sky texture is no wider than 1024,
	// we map it to a cylinder with circumfrence 1024. For larger ones, we use the width of
	// the texture as the cylinder's circumfrence.
	sky1cyl = MAX(skytex1->GetWidth(), skytex1->xScale >> (16 - 10));
	sky2cyl = MAX(skytex2->GetWidth(), skytex2->xScale >> (16 - 10));

	// If we are stretching short skies, we also stretch them horizontally by halving the
	// circumfrence of the sky cylinder, unless the texture is 512 or more pixels wide,
	// in which case we only stretch it vertically.
	if (skystretch)
	{
		if (skytex1->GetScaledWidth() < 512)
		{
			sky1cyl >>= 1;
		}
		if (skytex2->GetScaledWidth() < 512)
		{
			sky2cyl >>= 1;
		}
	}
}

