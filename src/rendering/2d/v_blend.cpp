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


#include "sbar.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "s_sound.h"
#include "gi.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "vm.h"
#include "v_palette.h"
#include "r_utility.h"
#include "hw_cvars.h"
#include "d_main.h"
#include "v_draw.h"

CVAR(Float, underwater_fade_scalar, 1.0f, CVAR_ARCHIVE) // [Nash] user-settable underwater blend intensity
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
	auto Level = CPlayer->mo->Level;

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

	PalEntry painFlash = 0;
	IFVIRTUALPTRNAME(CPlayer->mo, NAME_PlayerPawn, GetPainFlash)
	{
		VMValue param = CPlayer->mo;
		VMReturn ret((int*)&painFlash.d);
		VMCall(func, &param, 1, &ret, 1);
	}

	if (painFlash.a != 0)
	{
		cnt = DamageToAlpha[min (CPlayer->damagecount * painFlash.a / 255, (uint32_t)113)];

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
		cnt = min (CPlayer->poisoncount, 64);
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
				float r = ((Level->hazardflash & 0xff0000) >> 16) / 255.f;
				float g = ((Level->hazardflash & 0xff00) >> 8) / 255.f;
				float b = ((Level->hazardflash & 0xff)) / 255.f;
				V_AddBlend (r, g, b, 0.125f, blend);
			}
		}
		else
		{
			cnt= min(CPlayer->hazardcount/8, 64);
			float r = ((Level->hazardcolor & 0xff0000) >> 16) / 255.f;
			float g = ((Level->hazardcolor & 0xff00) >> 8) / 255.f;
			float b = ((Level->hazardcolor & 0xff)) / 255.f;
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

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

FVector4 V_CalcBlend(sector_t* viewsector, PalEntry* modulateColor)
{
	float blend[4] = { 0,0,0,0 };
	PalEntry blendv = 0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t* player = nullptr;
	bool fullbright = false;

	if (modulateColor) *modulateColor = 0xffffffff;

	if (players[consoleplayer].camera != nullptr)
	{
		player = players[consoleplayer].camera->player;
		if (player)
			fullbright = (player->fixedcolormap != NOFIXEDCOLORMAP || player->extralight == INT_MIN || player->fixedlightlevel != -1);
	}

	// don't draw sector based blends when any fullbright screen effect is active.
	if (!fullbright)
	{
		const auto& vpp = r_viewpoint.Pos;
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->GetHeightSec())
			{
				auto s = viewsector->heightsec;
				blendv = s->floorplane.PointOnSide(vpp) < 0 ? s->bottommap : s->ceilingplane.PointOnSide(vpp) < 0 ? s->topmap : s->midmap;
			}
		}
		else
		{
			TArray<lightlist_t>& lightlist = viewsector->e->XFloor.lightlist;

			for (unsigned int i = 0; i < lightlist.Size(); i++)
			{
				double lightbottom;
				if (i < lightlist.Size() - 1)
					lightbottom = lightlist[i + 1].plane.ZatPoint(vpp);
				else
					lightbottom = viewsector->floorplane.ZatPoint(vpp);

				if (lightbottom < vpp.Z && (!lightlist[i].caster || !(lightlist[i].caster->flags & FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv = lightlist[i].blend;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->Colormap.FadeColor) blendv = 0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a == 0 && blendv != 0) blendv.a = 128;
					break;
				}
			}
		}

		if (blendv.a == 0 && V_IsTrueColor())	// The paletted software renderer uses the original colormap as this frame's palette, but in true color that isn't doable.
		{
			blendv = R_BlendForColormap(blendv);
		}

		if (blendv.a == 255)
		{

			extra_red = blendv.r / 255.0f;
			extra_green = blendv.g / 255.0f;
			extra_blue = blendv.b / 255.0f;

			// If this is a multiplicative blend do it separately and add the additive ones on top of it.

			// black multiplicative blends are ignored
			if (extra_red || extra_green || extra_blue)
			{
				if (modulateColor) *modulateColor = blendv;
			}
			blendv = 0;
		}
		else if (blendv.a)
		{
			// [Nash] allow user to set blend intensity
			int cnt = blendv.a;
			cnt = (int)(cnt * underwater_fade_scalar);

			V_AddBlend(blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, cnt / 255.0f, blend);
		}
	}
	else if (player && player->fixedlightlevel != -1 && player->fixedcolormap == NOFIXEDCOLORMAP)
	{
		// Draw fixedlightlevel effects as a 2D overlay. The hardware renderer just processes such a scene fullbright without any lighting.
		auto torchtype = PClass::FindActor(NAME_PowerTorch);
		auto litetype = PClass::FindActor(NAME_PowerLightAmp);
		PalEntry color = 0xffffffff;
		for (AActor* in = player->mo->Inventory; in; in = in->Inventory)
		{
			// Need special handling for light amplifiers 
			if (in->IsKindOf(torchtype))
			{
				// The software renderer already bakes the torch flickering into its output, so this must be omitted here.
				float r = vid_rendermode < 4 ? 1.f : (0.8f + (7 - player->fixedlightlevel) / 70.0f);
				if (r > 1.0f) r = 1.0f;
				int rr = (int)(r * 255);
				int b = rr;
				if (gl_enhanced_nightvision) b = b * 3 / 4;
				color = PalEntry(255, rr, rr, b);
			}
			else if (in->IsKindOf(litetype))
			{
				if (gl_enhanced_nightvision)
				{
					color = PalEntry(255, 104, 255, 104);
				}
			}
		}
		if (modulateColor)
		{
			*modulateColor = color;
		}
	}

	if (player)
	{
		V_AddPlayerBlend(player, blend, 0.5, 175);
	}

	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t* player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		V_AddBlend(player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	const float br = clamp(blend[0] * 255.f, 0.f, 255.f);
	const float bg = clamp(blend[1] * 255.f, 0.f, 255.f);
	const float bb = clamp(blend[2] * 255.f, 0.f, 255.f);
	return { br, bg, bb, blend[3] };
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

void V_DrawBlend(sector_t* viewsector)
{
	auto drawer = twod;
	PalEntry modulateColor;
	auto blend = V_CalcBlend(viewsector, &modulateColor);
	if (modulateColor != 0xffffffff)
	{
		Dim(twod, modulateColor, 1, 0, 0, drawer->GetWidth(), drawer->GetHeight(), &LegacyRenderStyles[STYLE_Multiply]);
	}

	const PalEntry bcolor(255, uint8_t(blend.X), uint8_t(blend.Y), uint8_t(blend.Z));
	Dim(drawer, bcolor, blend.W, 0, 0, drawer->GetWidth(), drawer->GetHeight());
}

