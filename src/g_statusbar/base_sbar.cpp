/*
** base_sbar.cpp
** Base status bar implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2017-2020 Christoph Oelckers
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
#include "base_sbar.h"
#include "printf.h"
#include "v_draw.h"
#include "cmdlib.h"
#include "texturemanager.h"
#include "c_cvars.h"

FGameTexture* CrosshairImage;
static int CrosshairNum;


IMPLEMENT_CLASS(DStatusBarCore, false, false)


CVAR(Color, crosshaircolor, 0xff0000, CVAR_ARCHIVE);
CVAR(Int, crosshairhealth, 1, CVAR_ARCHIVE);
CVAR(Float, crosshairscale, 1.0, CVAR_ARCHIVE);
CVAR(Bool, crosshairgrow, false, CVAR_ARCHIVE);



void ST_LoadCrosshair(int num, bool alwaysload)
{
	char name[16];
	char size;

	if (!alwaysload && CrosshairNum == num && CrosshairImage != NULL)
	{ // No change.
		return;
	}

	if (num == 0)
	{
		CrosshairNum = 0;
		CrosshairImage = NULL;
		return;
	}
	if (num < 0)
	{
		num = -num;
	}
	size = (twod->GetWidth() < 640) ? 'S' : 'B';

	mysnprintf(name, countof(name), "XHAIR%c%d", size, num);
	FTextureID texid = TexMan.CheckForTexture(name, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
	if (!texid.isValid())
	{
		mysnprintf(name, countof(name), "XHAIR%c1", size);
		texid = TexMan.CheckForTexture(name, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
		if (!texid.isValid())
		{
			texid = TexMan.CheckForTexture("XHAIRS1", ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
		}
	}
	CrosshairNum = num;
	CrosshairImage = TexMan.GetGameTexture(texid);
}

void ST_UnloadCrosshair()
{
	CrosshairImage = NULL;
	CrosshairNum = 0;
}


//---------------------------------------------------------------------------
//
// DrawCrosshair
//
//---------------------------------------------------------------------------

void ST_DrawCrosshair(int phealth, double xpos, double ypos, double scale)
{
	uint32_t color;
	double size;
	int w, h;

	// Don't draw the crosshair if there is none
	if (CrosshairImage == NULL)
	{
		return;
	}

	if (crosshairscale > 0.0f)
	{
		size = twod->GetHeight() * crosshairscale * 0.005;
	}
	else
	{
		size = 1.;
	}

	if (crosshairgrow)
	{
		size *= scale;
	}

	w = int(CrosshairImage->GetDisplayWidth() * size);
	h = int(CrosshairImage->GetDisplayHeight() * size);

	if (crosshairhealth == 1)
	{
		// "Standard" crosshair health (green-red)
		int health = phealth;

		if (health >= 85)
		{
			color = 0x00ff00;
		}
		else
		{
			int red, green;
			health -= 25;
			if (health < 0)
			{
				health = 0;
			}
			if (health < 30)
			{
				red = 255;
				green = health * 255 / 30;
			}
			else
			{
				red = (60 - health) * 255 / 30;
				green = 255;
			}
			color = (red << 16) | (green << 8);
		}
	}
	else if (crosshairhealth == 2)
	{
		// "Enhanced" crosshair health (blue-green-yellow-red)
		int health = clamp(phealth, 0, 200);
		float rr, gg, bb;

		float saturation = health < 150 ? 1.f : 1.f - (health - 150) / 100.f;

		HSVtoRGB(&rr, &gg, &bb, health * 1.2f, saturation, 1);
		int red = int(rr * 255);
		int green = int(gg * 255);
		int blue = int(bb * 255);

		color = (red << 16) | (green << 8) | blue;
	}
	else
	{
		color = crosshaircolor;
	}

	DrawTexture(twod, CrosshairImage,
		xpos, ypos,
		DTA_DestWidth, w,
		DTA_DestHeight, h,
		DTA_AlphaChannel, true,
		DTA_FillColor, color & 0xFFFFFF,
		TAG_DONE);
}


