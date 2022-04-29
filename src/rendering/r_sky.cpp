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
#include "texturemanager.h"
#include "palentry.h"
#include "bitmap.h"

//
// sky mapping
//
FTextureID	skyflatnum;

// [RH] Stretch sky texture if not taller than 128 pixels?
// Also now controls capped skies. 0 = normal, 1 = stretched, 2 = capped
CUSTOM_CVAR (Int, r_skymode, 2, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	R_InitSkyMap ();
}

//==========================================================================
//
// R_InitSkyMap
//
// Called whenever the view size changes.
//
//==========================================================================

void InitSkyMap(FLevelLocals *Level)
{
	FGameTexture *skytex1, *skytex2;

	// Do not allow the null texture which has no bitmap and will crash.
	if (Level->skytexture1.isNull())
	{
		Level->skytexture1 = TexMan.CheckForTexture("-noflat-", ETextureType::Any);
	}
	if (Level->skytexture2.isNull())
	{
		Level->skytexture2 = TexMan.CheckForTexture("-noflat-", ETextureType::Any);
	}
	if (Level->flags & LEVEL_DOUBLESKY)
	{
		Level->skytexture1 = TexMan.GetFrontSkyLayer(Level->skytexture1);
	}

	skytex1 = TexMan.GetGameTexture(Level->skytexture1, false);
	skytex2 = TexMan.GetGameTexture(Level->skytexture2, false);

	if (skytex1 == nullptr || skytex2 == nullptr)
		return;

	if ((Level->flags & LEVEL_DOUBLESKY) && skytex1->GetDisplayHeight() != skytex2->GetDisplayHeight())
	{
		Printf(TEXTCOLOR_BOLD "Both sky textures must be the same height." TEXTCOLOR_NORMAL "\n");
		Level->flags &= ~LEVEL_DOUBLESKY;
		Level->skytexture1 = Level->skytexture2;
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
	auto skyheight = skytex1->GetDisplayHeight();

	Level->skystretch = (r_skymode == 1
		&& skyheight >= 128 && skyheight <= 256
		&& Level->IsFreelookAllowed()
		&& !(Level->flags & LEVEL_FORCETILEDSKY)) ? 1 : 0;
}

void R_InitSkyMap()
{
	for(auto Level : AllLevels())
	{
		InitSkyMap(Level);
	}
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
	double ms = (double)mstime * FRACUNIT;
	for(auto Level : AllLevels())
	{
		// Scroll the sky
		Level->sky1pos = ms * Level->skyspeed1;
		Level->sky2pos = ms * Level->skyspeed2;

		// The hardware renderer uses a different value range and clamps it to a single rotation
		Level->hw_sky1pos = (float)(fmod((double(mstime) * Level->skyspeed1), 1024.) * (90. / 256.));
		Level->hw_sky2pos = (float)(fmod((double(mstime) * Level->skyspeed2), 1024.) * (90. / 256.));
	}
}

