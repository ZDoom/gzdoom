//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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
#include "g_levellocals.h"

//
// sky mapping
//
FTextureID	skyflatnum;
FTextureID	sky1texture,	sky2texture;
double		sky1pos,		sky2pos;
float		hw_sky1pos, hw_sky2pos;
bool		skystretch;

// [RH] Stretch sky texture if not taller than 128 pixels?
// Also now controls capped skies. 0 = normal, 1 = stretched, 2 = capped
CUSTOM_CVAR (Int, r_skymode, 2, CVAR_ARCHIVE)
{
	R_InitSkyMap ();
}

CVAR(Float, skyoffset, 0, 0)	// for testing

//==========================================================================
//
// R_InitSkyMap
//
// Called whenever the view size changes.
//
//==========================================================================

void R_InitSkyMap()
{
	int skyheight;
	FTexture *skytex1, *skytex2;

	// Do not allow the null texture which has no bitmap and will crash.
	if (sky1texture.isNull())
	{
		sky1texture = TexMan.CheckForTexture("-noflat-", ETextureType::Any);
	}
	if (sky2texture.isNull())
	{
		sky2texture = TexMan.CheckForTexture("-noflat-", ETextureType::Any);
	}

	skytex1 = TexMan.GetTexture(sky1texture, false);
	skytex2 = TexMan.GetTexture(sky2texture, false);

	if (skytex1 == nullptr)
		return;

	if ((level.flags & LEVEL_DOUBLESKY) && skytex1->GetDisplayHeight() != skytex2->GetDisplayHeight())
	{
		Printf(TEXTCOLOR_BOLD "Both sky textures must be the same height." TEXTCOLOR_NORMAL "\n");
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
	skyheight = skytex1->GetDisplayHeight();

	if (skyheight >= 128 && skyheight < 200)
	{
		skystretch = (r_skymode == 1
			&& skyheight >= 128
			&& level.IsFreelookAllowed()
			&& !(level.flags & LEVEL_FORCETILEDSKY)) ? 1 : 0;
	}
	else skystretch = false;
}

//==========================================================================
//
// R_UpdateSky
//
// Performs sky scrolling
//
//==========================================================================

void R_UpdateSky (uint64_t mstime)
{
	// Scroll the sky
	double ms = (double)mstime * FRACUNIT;
	sky1pos = ms * level.skyspeed1;
	sky2pos = ms * level.skyspeed2;

	// The hardware renderer uses a different value range and clamps it to a single rotation
	hw_sky1pos = (float)(fmod((double(mstime) * level.skyspeed1), 1024.) * (90. / 256.));
	hw_sky2pos = (float)(fmod((double(mstime) * level.skyspeed2), 1024.) * (90. / 256.));

}

