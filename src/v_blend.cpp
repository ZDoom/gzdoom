/*
** v_blend.cpp
** Screen blending stuff
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <assert.h>

#include "templates.h"
#include "sbar.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "s_sound.h"
#include "gi.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "vm.h"

CVAR( Float, blood_fade_scalar, 1.0f, CVAR_ARCHIVE )	// [SP] Pulled from Skulltag - changed default from 0.5 to 1.0
CVAR( Float, pickup_fade_scalar, 1.0f, CVAR_ARCHIVE )	// [SP] Uses same logic as blood_fade_scalar except for pickups

// [RH] Amount of red flash for up to 114 damage points. Calculated by hand
//		using a logarithmic scale and my trusty HP48G.
static uint8_t DamageToAlpha[114] =
{
	  0,   8,  16,  23,  30,  36,  42,  47,  53,  58,  62,  67,  71,  75,  79,
	 83,  87,  90,  94,  97, 100, 103, 107, 109, 112, 115, 118, 120, 123, 125,
	128, 130, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157,
	159, 160, 162, 164, 165, 167, 169, 170, 172, 173, 175, 176, 178, 179, 181,
	182, 183, 185, 186, 187, 189, 190, 191, 192, 194, 195, 196, 197, 198, 200,
	201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216,
	217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 226, 227, 228, 229, 229,
	230, 231, 232, 233, 234, 235, 235, 236, 237
};


/*
=============
SV_AddBlend
[RH] This is from Q2.
=============
*/
void V_AddBlend (float r, float g, float b, float a, float v_blend[4])
{
	float a2, a3;

	if (a <= 0)
		return;
	a2 = v_blend[3] + (1-v_blend[3])*a;	// new total alpha
	a3 = v_blend[3]/a2;		// fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*(1-a3);
	v_blend[1] = v_blend[1]*a3 + g*(1-a3);
	v_blend[2] = v_blend[2]*a3 + b*(1-a3);
	v_blend[3] = a2;
}

//---------------------------------------------------------------------------
//
// BlendView
//
//---------------------------------------------------------------------------

void V_AddPlayerBlend (player_t *CPlayer, float blend[4], float maxinvalpha, int maxpainblend)
{
	int cnt;

	// [RH] All powerups can affect the screen blending now
	for (AActor *item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
	{
		PalEntry color = 0;

		IFVIRTUALPTRNAME(item, NAME_Inventory, GetBlend)
		{
			VMValue params[1] = { item };
			VMReturn ret((int*)&color.d);
			VMCall(func, params, 1, &ret, 1);
		}


		if (color.a != 0)
		{
			V_AddBlend (color.r/255.f, color.g/255.f, color.b/255.f, color.a/255.f, blend);
			if (color.a/255.f > maxinvalpha) maxinvalpha = color.a/255.f;
		}
	}
	if (CPlayer->bonuscount)
	{
		cnt = CPlayer->bonuscount << 3;

		// [SP] Allow player to tone down intensity of pickup flash.
		cnt = (int)( cnt * pickup_fade_scalar );
		
		V_AddBlend (RPART(gameinfo.pickupcolor)/255.f, GPART(gameinfo.pickupcolor)/255.f, 
					BPART(gameinfo.pickupcolor)/255.f, cnt > 128 ? 0.5f : cnt / 255.f, blend);
	}

	PalEntry painFlash = CPlayer->mo->DamageFade;
	CPlayer->GetPainFlash(CPlayer->mo->DamageTypeReceived, &painFlash);

	if (painFlash.a != 0)
	{
		cnt = DamageToAlpha[MIN (113, CPlayer->damagecount * painFlash.a / 255)];

		// [BC] Allow users to tone down the intensity of the blood on the screen.
		cnt = (int)( cnt * blood_fade_scalar );

		if (cnt)
		{
			if (cnt > maxpainblend)
				cnt = maxpainblend;

			V_AddBlend (painFlash.r / 255.f, painFlash.g / 255.f, painFlash.b / 255.f, cnt / 255.f, blend);
		}
	}

	// Unlike Doom, I did not have any utility source to look at to find the
	// exact numbers to use here, so I've had to guess by looking at how they
	// affect the white color in Hexen's palette and picking an alpha value
	// that seems reasonable.
	// [Gez] The exact values could be obtained by looking how they affect
	// each color channel in Hexen's palette.

	if (CPlayer->poisoncount)
	{
		cnt = MIN (CPlayer->poisoncount, 64);
		if (paletteflash & PF_POISON)
		{
			V_AddBlend(44/255.f, 92/255.f, 36/255.f, ((cnt + 7) >> 3) * 0.1f, blend);
		}
		else
		{
			V_AddBlend (0.04f, 0.2571f, 0.f, cnt/93.2571428571f, blend);
		}

	}

	if (CPlayer->hazardcount)
	{
		if (paletteflash & PF_HAZARD)
		{
			if (CPlayer->hazardcount > 16*TICRATE || (CPlayer->hazardcount & 8))
			{
				float r = ((level.hazardflash & 0xff0000) >> 16) / 255.f;
				float g = ((level.hazardflash & 0xff00) >> 8) / 255.f;
				float b = ((level.hazardflash & 0xff)) / 255.f;
				V_AddBlend (r, g, b, 0.125f, blend);
			}
		}
		else
		{
			cnt= MIN(CPlayer->hazardcount/8, 64);
			float r = ((level.hazardcolor & 0xff0000) >> 16) / 255.f;
			float g = ((level.hazardcolor & 0xff00) >> 8) / 255.f;
			float b = ((level.hazardcolor & 0xff)) / 255.f;
			V_AddBlend (r, g, b, cnt/93.2571428571f, blend);
		}
	}

	if (CPlayer->mo->DamageType == NAME_Ice)
	{
		if (paletteflash & PF_ICE)
		{
			V_AddBlend(0.f, 0.f, 224/255.f, 0.5f, blend);
		}
		else
		{
			V_AddBlend (0.25f, 0.25f, 0.853f, 0.4f, blend);
		}		
	}

	// cap opacity if desired
	if (blend[3] > maxinvalpha) blend[3] = maxinvalpha;
}
