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

int			sky1shift,		sky2shift;
fixed_t		sky1pos=0,		sky1speed=0;
fixed_t		sky2pos=0,		sky2speed=0;

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

	// The (standard Doom) sky map is 256*128*4 maps.
	int swidth = skytex1->GetScaledWidth();
	sky1shift = 22 - 16 + (skystretch && swidth < 512) - (swidth >= 1024 && skytex1->xScale >= 4*FRACUNIT);
	swidth = skytex2->GetScaledWidth();
	sky2shift = 22 - 16 + (skystretch && swidth < 512) - (swidth >= 1024 && skytex2->xScale >= 4*FRACUNIT);
}

