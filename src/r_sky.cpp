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
#include "c_cvars.h"
#include "g_level.h"
#include "r_sky.h"
#include "r_utility.h"
#include "v_text.h"
#include "gi.h"

//
// sky mapping
//
FTextureID	skyflatnum;
FTextureID	sky1texture,	sky2texture;
double		skytexturemid;
double		skyscale;
float		skyiscale;
bool		skystretch;

fixed_t		sky1cyl,		sky2cyl;
double		sky1pos,		sky2pos;

// [RH] Stretch sky texture if not taller than 128 pixels?
CUSTOM_CVAR (Bool, r_stretchsky, true, CVAR_ARCHIVE)
{
	R_InitSkyMap ();
}

int			freelookviewheight;

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

	skytex1 = TexMan(sky1texture, true);
	skytex2 = TexMan(sky2texture, true);

	if (skytex1 == NULL)
		return;

	if ((level.flags & LEVEL_DOUBLESKY) && skytex1->GetHeight() != skytex2->GetHeight())
	{
		Printf (TEXTCOLOR_BOLD "Both sky textures must be the same height." TEXTCOLOR_NORMAL "\n");
		sky2texture = sky1texture;
	}

	// There are various combinations for sky rendering depending on how tall the sky is:
	//        h <  128: Unstretched and tiled, centered on horizon
	// 128 <= h <  200: Can possibly be stretched. When unstretched, the baseline is
	//                  28 rows below the horizon so that the top of the texture
	//                  aligns with the top of the screen when looking straight ahead.
	//                  When stretched, it is scaled to 228 pixels with the baseline
	//                  in the same location as an unstretched 128-tall sky, so the top
	//					of the texture aligns with the top of the screen when looking
	//                  fully up.
	//        h == 200: Unstretched, baseline is on horizon, and top is at the top of
	//                  the screen when looking fully up.
	//        h >  200: Unstretched, but the baseline is shifted down so that the top
	//                  of the texture is at the top of the screen when looking fully up.
	skyheight = skytex1->GetScaledHeight();
	skystretch = false;
	skytexturemid = 0;
	if (skyheight >= 128 && skyheight < 200)
	{
		skystretch = (r_stretchsky
					  && skyheight >= 128
					  && level.IsFreelookAllowed()
					  && !(level.flags & LEVEL_FORCENOSKYSTRETCH)) ? 1 : 0;
		skytexturemid = -28;
	}
	else if (skyheight > 200)
	{
		skytexturemid = (200 - skyheight) * skytex1->Scale.Y;
	}

	if (viewwidth != 0 && viewheight != 0)
	{
		skyiscale = float(r_Yaspect / freelookviewheight);
		skyscale = freelookviewheight / r_Yaspect;

		skyiscale *= float(FieldOfView.Degrees / 90.);
		skyscale *= float(90. / FieldOfView.Degrees);
	}

	if (skystretch)
	{
		skyscale *= (double)SKYSTRETCH_HEIGHT / skyheight;
		skyiscale *= skyheight / (float)SKYSTRETCH_HEIGHT;
		skytexturemid *= skyheight / (double)SKYSTRETCH_HEIGHT;
	}

	// The standard Doom sky texture is 256 pixels wide, repeated 4 times over 360 degrees,
	// giving a total sky width of 1024 pixels. So if the sky texture is no wider than 1024,
	// we map it to a cylinder with circumfrence 1024. For larger ones, we use the width of
	// the texture as the cylinder's circumfrence.
	sky1cyl = MAX(skytex1->GetWidth(), fixed_t(skytex1->Scale.X * 1024));
	sky2cyl = MAX(skytex2->GetWidth(), fixed_t(skytex2->Scale.Y * 1024));
}


//==========================================================================
//
// R_UpdateSky
//
// Performs sky scrolling
//
//==========================================================================

void R_UpdateSky (DWORD mstime)
{
	// Scroll the sky
	double ms = (double)mstime * FRACUNIT;
	sky1pos = ms * level.skyspeed1;
	sky2pos = ms * level.skyspeed2;
}

