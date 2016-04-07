/*
** playerdisplay.cpp
** The player display for the player setup and class selection screen
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "templates.h"
#include "menu/menu.h"
#include "colormatcher.h"
#include "textures/textures.h"
#include "w_wad.h"
#include "v_font.h"
#include "v_video.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "r_state.h"
#include "r_data/r_translate.h"


//=============================================================================
//
// Used by the player display
//
//=============================================================================

struct FBackdropTexture : public FTexture
{
	enum
	{
		COS_SIZE = 256,
		ANGLESHIFT = 24
	};

	static constexpr uint32_t DEGREES(double v)
	{
		return uint32_t((v)*(0x40000000 / 90.0));
	}

	static double TORAD(uint32_t x)
	{
		return x*(M_PI / 0x80000000);
	}

public:
	FBackdropTexture();

	const BYTE *GetColumn(unsigned int column, const Span **spans_out);
	const BYTE *GetPixels();
	void Unload();
	bool CheckModified();

protected:
	uint32_t costab[COS_SIZE];
	BYTE Pixels[144*160];
	static const Span DummySpan[2];
	int LastRenderTic;

	uint32_t time1, time2, time3, time4;
	uint32_t t1ang, t2ang, z1ang, z2ang;

	void Render();
};



// A 32x32 cloud rendered with Photoshop, plus some other filters
static BYTE pattern1[1024] =
{
	 5, 9, 7,10, 9,15, 9, 7, 8,10, 5, 3, 5, 7, 9, 8,14, 8, 4, 7, 8, 9, 5, 7,14, 7, 0, 7,13,13, 9, 6,
	 2, 7, 9, 7, 7,10, 8, 8,11,10, 6, 7,10, 7, 5, 6, 6, 4, 7,13,15,16,11,15,11, 8, 0, 4,13,22,17,11,
	 5, 9, 9, 7, 9,10, 4, 3, 6, 7, 8, 6, 5, 4, 2, 2, 1, 4, 6,11,15,15,14,13,17, 9, 5, 9,11,12,17,20,
	 9,16, 9, 8,12,13, 7, 3, 7, 9, 5, 4, 2, 5, 5, 5, 7,11, 6, 7, 6,13,17,10,10, 9,12,17,14,12,16,15,
	15,13, 5, 3, 9,10, 4,10,12,12, 7, 9, 8, 8, 8,10, 7, 6, 5, 5, 5, 6,11, 9, 3,13,16,18,21,16,23,18,
	23,13, 0, 0, 0, 0, 0,12,18,14,15,16,13, 7, 7, 5, 9, 6, 6, 8, 4, 0, 0, 0, 0,14,19,17,14,20,21,25,
	19,20,14,13, 7, 5,13,19,14,13,17,15,14, 7, 3, 5, 6,11, 7, 7, 8, 8,10, 9, 9,18,17,15,14,15,18,16,
	16,29,24,23,18, 9,17,20,11, 5,12,15,15,12, 6, 3, 4, 6, 7,10,13,18,18,19,16,12,17,19,23,16,14,14,
	 9,18,20,26,19, 5,18,18,10, 5,12,15,14,17,11, 6,11, 9,10,13,10,20,24,20,21,20,14,18,15,22,20,19,
	 0, 6,16,18, 8, 7,15,18,10,13,17,17,13,11,15,11,19,12,13,10, 4,15,19,21,21,24,14, 9,17,20,24,17,
	18,17, 7, 7,16,21,22,15, 5,14,20,14,13,21,13, 8,12,14, 7, 8,11,15,13,11,16,17, 7, 5,12,17,19,14,
	25,23,17,16,23,18,15, 7, 0, 6,11, 6,11,15,11, 7,12, 7, 4,10,16,13, 7, 7,15,13, 9,15,21,14, 5, 0,
	18,22,21,21,21,22,12, 6,14,20,15, 6,10,19,13, 8, 7, 3, 7,12,14,16, 9,12,22,15,12,18,24,19,17, 9,
	 0,15,18,21,17,25,14,13,19,21,21,11, 6,13,16,16,12,10,12,11,13,20,14,13,18,13, 9,15,16,25,31,20,
	 5,20,24,16, 7,14,14,11,18,19,19, 6, 0, 5,11,14,17,16,19,14,15,21,19,15,14,14, 8, 0, 7,24,18,16,
	 9,17,15, 7, 6,14,12, 7,14,16,11, 4, 7, 6,13,16,15,13,12,20,21,20,21,17,18,26,14, 0,13,23,21,11,
	 9,12,18,11,15,21,13, 8,13,13,10, 7,13, 8, 8,19,13, 7, 4,15,19,18,14,12,14,15, 8, 6,16,22,22,15,
	 9,17,14,19,15,14,15, 9,11, 9, 6, 8,14,13,13,12, 5, 0, 0, 6,12,13, 7, 7, 9, 7, 0,12,21,16,15,18,
	15,16,18,11, 6, 8,15, 9, 2, 0, 5,10,10,16, 9, 0, 4,12,15, 9,12, 9, 7, 7,12, 7, 0, 6,12, 6, 9,13,
	12,19,15,14,11, 7, 8, 9,12,10, 5, 5, 7,12,12,10,14,16,16,11, 8,12,10,12,10, 8,10,10,14,12,16,16,
	16,17,20,22,12,15,12,14,19,11, 6, 5,10,13,17,17,21,19,15, 9, 6, 9,15,18,10,10,18,14,20,15,16,17,
	11,19,19,18,19,14,17,13,12,12, 7,11,18,17,16,15,19,19,10, 2, 0, 8,15,12, 8,11,12,10,19,20,19,19,
	 6,14,18,13,13,16,16,12, 5, 8,10,12,10,13,18,12, 9,10, 7, 6, 5,11, 8, 6, 7,13,16,13,10,15,20,14,
	 0, 5,12,12, 4, 0, 9,16, 9,10,12, 8, 0, 9,13, 9, 0, 2, 4, 7,10, 6, 7, 3, 4,11,16,18,10,11,21,21,
	16,13,11,15, 8, 0, 5, 9, 8, 7, 6, 3, 0, 9,17, 9, 0, 0, 0, 3, 5, 4, 3, 5, 7,15,16,16,17,14,22,22,
	24,14,15,12, 9, 0, 5,10, 8, 4, 7,12,10,11,12, 7, 6, 8, 6, 5, 7, 8, 8,11,13,10,15,14,12,18,20,16,
	16,17,17,18,12, 9,12,16,10, 5, 6,20,13,15, 8, 4, 8, 9, 8, 7, 9,11,12,17,16,16,11,10, 9,10, 5, 0,
	 0,14,18,18,15,16,14, 9,10, 9, 9,15,14,10, 4, 6,10, 8, 8, 7,10, 9,10,16,18,10, 0, 0, 7,12,10, 8,
	 0,14,19,14, 9,11,11, 8, 8,10,15, 9,10, 7, 4,10,13, 9, 7, 5, 5, 7, 7, 7,13,13, 5, 5,14,22,18,16,
	 0,10,14,10, 3, 6, 5, 6, 8, 9, 8, 9, 5, 9, 8, 9, 6, 8, 8, 8, 1, 0, 0, 0, 9,17,12,12,17,19,20,13,
	 6,11,17,11, 5, 5, 8,10, 6, 5, 6, 6, 3, 7, 9, 7, 6, 8,12,10, 4, 8, 6, 6,11,16,16,15,16,17,17,16,
	11, 9,10,10, 5, 6,12,10, 5, 1, 6,10, 5, 3, 3, 5, 4, 7,15,10, 7,13, 7, 8,15,11,15,15,15, 8,11,15,
};

// Just a 32x32 cloud rendered with the standard Photoshop filter
static BYTE pattern2[1024] =
{
	  9, 9, 8, 8, 8, 8, 6, 6,13,13,11,21,19,21,23,18,23,24,19,19,24,17,18,12, 9,14, 8,12,12, 5, 8, 6,
	 11,10, 6, 7, 8, 8, 9,13,10,11,17,15,23,22,23,22,20,26,27,26,17,21,20,14,12, 8,11, 8,11, 7, 8, 7,
	  6, 9,13,13,10, 9,13, 7,12,13,16,19,16,20,22,25,22,25,27,22,21,23,15,10,14,14,15,13,12, 8,12, 6,
	  6, 7,12,12,12,16, 9,12,12,15,16,11,21,24,19,24,23,26,28,27,26,21,14,15, 7, 7,10,15,12,11,10, 9,
	  7,14,11,16,12,18,16,14,16,14,11,14,15,21,23,17,20,18,26,24,27,18,20,11,11,14,10,17,17,10, 6,10,
	 13, 9,14,10,13,11,14,15,18,15,15,12,19,19,20,18,22,20,19,22,19,19,19,20,17,15,15,11,16,14,10, 8,
	 13,16,12,16,17,19,17,18,15,19,14,18,15,14,15,17,21,19,23,18,23,22,18,18,17,15,15,16,12,12,15,10,
	 10,12,14,10,16,11,18,15,21,20,20,17,18,19,16,19,14,20,19,14,19,25,22,21,22,24,18,12, 9, 9, 8, 6,
	 10,10,13, 9,15,13,20,19,22,18,18,17,17,21,21,13,13,12,19,18,16,17,27,26,22,23,20,17,12,11, 8, 9,
	  7,13,14,15,11,13,18,22,19,23,23,20,22,24,21,14,12,16,17,19,18,18,22,18,24,23,19,17,16,14, 8, 7,
	 12,12, 8, 8,16,20,26,25,28,28,22,29,23,22,21,18,13,16,15,15,20,17,25,24,19,17,17,17,15,10, 8, 9,
	  7,12,15,11,17,20,25,25,25,29,30,31,28,26,18,16,17,18,20,21,22,20,23,19,18,19,10,16,16,11,11, 8,
	  5, 6, 8,14,14,17,17,21,27,23,27,31,27,22,23,21,19,19,21,19,20,19,17,22,13,17,12,15,10,10,12, 6,
	  8, 9, 8,14,15,16,15,18,27,26,23,25,23,22,18,21,20,17,19,20,20,16,20,14,15,13,12, 8, 8, 7,11,13,
	  7, 6,11,11,11,13,15,22,25,24,26,22,24,26,23,18,24,24,20,18,20,16,17,12,12,12,10, 8,11, 9, 6, 8,
	  9,10, 9, 6, 5,14,16,19,17,21,26,20,23,19,19,17,20,21,26,25,23,21,17,13,12, 5,13,11, 7,12,10,12,
	  6, 5, 4,10,11, 9,10,13,17,20,20,18,23,26,27,20,21,24,20,19,24,20,18,10,11, 3, 6,13, 9, 6, 8, 8,
	  1, 2, 2,11,13,13,11,16,16,16,19,21,20,23,22,28,21,20,19,18,23,16,18, 7, 5, 9, 7, 6, 5,10, 8, 8,
	  0, 0, 6, 9,11,15,12,12,19,18,19,26,22,24,26,30,23,22,22,16,20,19,12,12, 3, 4, 6, 5, 4, 7, 2, 4,
	  2, 0, 0, 7,11, 8,14,13,15,21,26,28,25,24,27,26,23,24,22,22,15,17,12, 8,10, 7, 7, 4, 0, 5, 0, 1,
	  1, 2, 0, 1, 9,14,13,10,19,24,22,29,30,28,30,30,31,23,24,19,17,14,13, 8, 8, 8, 1, 4, 0, 0, 0, 3,
	  5, 2, 4, 2, 9, 8, 8, 8,18,23,20,27,30,27,31,25,28,30,28,24,24,15,11,14,10, 3, 4, 3, 0, 0, 1, 3,
	  9, 3, 4, 3, 5, 6, 8,13,14,23,21,27,28,27,28,27,27,29,30,24,22,23,13,15, 8, 6, 2, 0, 4, 3, 4, 1,
	  6, 5, 5, 3, 9, 3, 6,14,13,16,23,26,28,23,30,31,28,29,26,27,21,20,15,15,13, 9, 1, 0, 2, 0, 5, 8,
	  8, 4, 3, 7, 2, 0,10, 7,10,14,21,21,29,28,25,27,30,28,25,24,27,22,19,13,10, 5, 0, 0, 0, 0, 0, 7,
	  7, 6, 7, 0, 2, 2, 5, 6,15,11,19,24,22,29,27,31,30,30,31,28,23,18,14,14, 7, 5, 0, 0, 1, 0, 1, 0,
	  5, 5, 5, 0, 0, 4, 5,11, 7,10,13,20,21,21,28,31,28,30,26,28,25,21, 9,12, 3, 3, 0, 2, 2, 2, 0, 1,
	  3, 3, 0, 2, 0, 3, 5, 3,11,11,16,19,19,27,26,26,30,27,28,26,23,22,16, 6, 2, 2, 3, 2, 0, 2, 4, 0,
	  0, 0, 0, 3, 3, 1, 0, 4, 5, 9,11,16,24,20,28,26,28,24,28,25,22,21,16, 5, 7, 5, 7, 3, 2, 3, 3, 6,
	  0, 0, 2, 0, 2, 0, 4, 3, 8,12, 9,17,16,23,23,27,27,22,26,22,21,21,13,14, 5, 3, 7, 3, 2, 4, 6, 1,
	  2, 5, 6, 4, 0, 1, 5, 8, 7, 6,15,17,22,20,24,28,23,25,20,21,18,16,13,15,13,10, 8, 5, 5, 9, 3, 7,
	  7, 7, 0, 5, 1, 6, 7, 9,12, 9,12,21,22,25,24,22,23,25,24,18,24,22,17,13,10, 9,10, 9, 6,11, 6, 5,
};

const FTexture::Span FBackdropTexture::DummySpan[2] = { { 0, 160 }, { 0, 0 } };

//=============================================================================
//
//
//
//=============================================================================

FBackdropTexture::FBackdropTexture()
{
	Width = 144;
	Height = 160;
	WidthBits = 8;
	HeightBits = 8;
	WidthMask = 255;
	LastRenderTic = 0;

	for (int i = 0; i < COS_SIZE; ++i)
	{
		costab[i] = uint32_t(cos(i * (M_PI / (COS_SIZE / 2))) * 65536);
	}

	time1 = DEGREES(180);
	time2 = DEGREES(56);
	time3 = DEGREES(99);
	time4 = DEGREES(1);
	t1ang = DEGREES(90);
	t2ang = 0;
	z1ang = 0;
	z2ang = DEGREES(45);
}

//=============================================================================
//
//
//
//=============================================================================

bool FBackdropTexture::CheckModified()
{
	return LastRenderTic != gametic;
}

void FBackdropTexture::Unload()
{
}

//=============================================================================
//
//
//
//=============================================================================

const BYTE *FBackdropTexture::GetColumn(unsigned int column, const Span **spans_out)
{
	if (LastRenderTic != gametic)
	{
		Render();
	}
	column = clamp(column, 0u, 143u);
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + column*160;
}

//=============================================================================
//
//
//
//=============================================================================

const BYTE *FBackdropTexture::GetPixels()
{
	if (LastRenderTic != gametic)
	{
		Render();
	}
	return Pixels;
}

//=============================================================================
//
// This is one plasma and two rotozoomers. I think it turned out quite awesome.
//
//=============================================================================

void FBackdropTexture::Render()
{
	BYTE *from;
	int width, height, pitch;

	width = 160;
	height = 144;
	pitch = width;

	int x, y;

	const DWORD a1add = DEGREES(0.5);
	const DWORD a2add = DEGREES(359);
	const DWORD a3add = DEGREES(5 / 7.f);
	const DWORD a4add = DEGREES(358.66666);

	const DWORD t1add = DEGREES(358);
	const DWORD t2add = DEGREES(357.16666);
	const DWORD t3add = DEGREES(2.285);
	const DWORD t4add = DEGREES(359.33333);
	const DWORD x1add = 5 * 524288;
	const DWORD x2add = 0u - 13 * 524288;
	const DWORD z1add = 3 * 524288;
	const DWORD z2add = 4 * 524288;


	DWORD a1, a2, a3, a4;
	SDWORD c1, c2, c3, c4;
	DWORD tx, ty, tc, ts;
	DWORD ux, uy, uc, us;
	DWORD ltx, lty, lux, luy;

	from = Pixels;

	a3 = time3;
	a4 = time4;

	double z1 = (cos(TORAD(z2ang)) / 4 + 0.5) * (0x8000000);
	double z2 = (cos(TORAD(z1ang)) / 4 + 0.75) * (0x8000000);

	tc = SDWORD(cos(TORAD(t1ang)) * z1);
	ts = SDWORD(sin(TORAD(t1ang)) * z1);
	uc = SDWORD(cos(TORAD(t2ang)) * z2);
	us = SDWORD(sin(TORAD(t2ang)) * z2);
	
	ltx = -width / 2 * tc;
	lty = -width / 2 * ts;
	lux = -width / 2 * uc;
	luy = -width / 2 * us;

	for (y = 0; y < height; ++y)
	{
		a1 = time1;
		a2 = time2;
		c3 = SDWORD(cos(TORAD(a3)) * 65536.0);
		c4 = SDWORD(cos(TORAD(a4)) * 65536.0);
		tx = ltx - (y - height / 2)*ts;
		ty = lty + (y - height / 2)*tc;
		ux = lux - (y - height / 2)*us;
		uy = luy + (y - height / 2)*uc;
		for (x = 0; x < width; ++x)
		{
			c1 = costab[a1 >> ANGLESHIFT];
			c2 = costab[a2 >> ANGLESHIFT];
			from[x] = ((c1 + c2 + c3 + c4) >> (16 + 3 - 7)) + 128		// plasma
				+ pattern1[(tx >> 27) + ((ty >> 22) & 992)]				// rotozoomer 1
				+ pattern2[(ux >> 27) + ((uy >> 22) & 992)];			// rotozoomer 2
			tx += tc;
			ty += ts;
			ux += uc;
			uy += us;
			a1 += a1add;
			a2 += a2add;
		}
		a3 += a3add;
		a4 += a4add;
		from += pitch;
	}

	time1 += t1add;
	time2 += t2add;
	time3 += t3add;
	time4 += t4add;
	t1ang += x1add;
	t2ang += x2add;
	z1ang += z1add;
	z2ang += z2add;

	LastRenderTic = gametic;
}


//=============================================================================
//
//
//
//=============================================================================

FListMenuItemPlayerDisplay::FListMenuItemPlayerDisplay(FListMenuDescriptor *menu, int x, int y, PalEntry c1, PalEntry c2, bool np, FName action)
: FListMenuItem(x, y, action)
{
	mOwner = menu;

	for (int i = 0; i < 256; i++)
	{
		int r = c1.r + c2.r * i / 255;
		int g = c1.g + c2.g * i / 255;
		int b = c1.b + c2.b * i / 255;
		mRemap.Remap[i] = ColorMatcher.Pick (r, g, b);
		mRemap.Palette[i] = PalEntry(255, r, g, b);
	}
	mBackdrop = new FBackdropTexture;
	mPlayerClass = NULL;
	mPlayerState = NULL;
	mNoportrait = np;
	mMode = 0;
	mRotation = 0;
	mTranslate = false;
	mSkin = 0;
	mRandomClass = 0;
	mRandomTimer = 0;
	mClassNum = -1;
}


//=============================================================================
//
//
//
//=============================================================================

FListMenuItemPlayerDisplay::~FListMenuItemPlayerDisplay()
{
	delete mBackdrop;
}

//=============================================================================
//
//
//
//=============================================================================

void FListMenuItemPlayerDisplay::UpdateRandomClass()
{
	if (--mRandomTimer < 0)
	{
		if (++mRandomClass >= (int)PlayerClasses.Size ()) mRandomClass = 0;
		mPlayerClass = &PlayerClasses[mRandomClass];
		mPlayerState = GetDefaultByType (mPlayerClass->Type)->SeeState;
		if (mPlayerState == NULL)
		{ // No see state, so try spawn state.
			mPlayerState = GetDefaultByType (mPlayerClass->Type)->SpawnState;
		}
		mPlayerTics = mPlayerState != NULL ? mPlayerState->GetTics() : -1;
		mRandomTimer = 6;

		// Since the newly displayed class may used a different translation
		// range than the old one, we need to update the translation, too.
		UpdateTranslation();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FListMenuItemPlayerDisplay::UpdateTranslation()
{
	int PlayerColor = players[consoleplayer].userinfo.GetColor();
	int	PlayerSkin = players[consoleplayer].userinfo.GetSkin();
	int PlayerColorset = players[consoleplayer].userinfo.GetColorSet();

	if (mPlayerClass != NULL)
	{
		PlayerSkin = R_FindSkin (skins[PlayerSkin].name, int(mPlayerClass - &PlayerClasses[0]));
		R_GetPlayerTranslation(PlayerColor, mPlayerClass->Type->GetColorSet(PlayerColorset),
			&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FListMenuItemPlayerDisplay::SetPlayerClass(int classnum, bool force)
{
	if (classnum < 0 || classnum >= (int)PlayerClasses.Size ())
	{
		if (mClassNum != -1)
		{
			mClassNum = -1;
			mRandomTimer = 0;
			UpdateRandomClass();
		}
	}
	else if (mPlayerClass != &PlayerClasses[classnum] || force)
	{
		mPlayerClass = &PlayerClasses[classnum];
		mPlayerState = GetDefaultByType (mPlayerClass->Type)->SeeState;
		if (mPlayerState == NULL)
		{ // No see state, so try spawn state.
			mPlayerState = GetDefaultByType (mPlayerClass->Type)->SpawnState;
		}
		mPlayerTics = mPlayerState != NULL ? mPlayerState->GetTics() : -1;
		mClassNum = classnum;
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool FListMenuItemPlayerDisplay::UpdatePlayerClass()
{
	if (mOwner->mSelectedItem >= 0)
	{
		int classnum;
		FName seltype = mOwner->mItems[mOwner->mSelectedItem]->GetAction(&classnum);

		if (seltype != NAME_Episodemenu) return false;
		if (PlayerClasses.Size() == 0) return false;

		SetPlayerClass(classnum);
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool FListMenuItemPlayerDisplay::SetValue(int i, int value)
{
	switch (i)
	{
	case PDF_MODE:
		mMode = value;
		return true;

	case PDF_ROTATION:
		mRotation = value;
		return true;

	case PDF_TRANSLATE:
		mTranslate = value;

	case PDF_CLASS:
		SetPlayerClass(value, true);
		break;

	case PDF_SKIN:
		mSkin = value;
		break;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void FListMenuItemPlayerDisplay::Ticker()
{
	if (mClassNum < 0) UpdateRandomClass();

	if (mPlayerState != NULL && mPlayerState->GetTics () != -1 && mPlayerState->GetNextState () != NULL)
	{
		if (--mPlayerTics <= 0)
		{
			mPlayerState = mPlayerState->GetNextState();
			mPlayerTics = mPlayerState->GetTics();
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FListMenuItemPlayerDisplay::Drawer(bool selected)
{
	if (mMode == 0 && !UpdatePlayerClass())
	{
		return;
	}

	FString portrait = mPlayerClass->Type->Portrait;

	if (portrait.IsNotEmpty() && !mNoportrait)
	{
		FTextureID texid = TexMan.CheckForTexture(portrait, FTexture::TEX_MiscPatch);
		if (texid.isValid())
		{
			FTexture *tex = TexMan(texid);
			if (tex != NULL)
			{
				screen->DrawTexture (tex, mXpos, mYpos, DTA_Clean, true, TAG_DONE);
				return;
			}
		}
	}
	int x = (mXpos - 160) * CleanXfac + (SCREENWIDTH>>1);
	int y = (mYpos - 100) * CleanYfac + (SCREENHEIGHT>>1);

	screen->DrawTexture (mBackdrop, x, y - 1,
		DTA_DestWidth, 72 * CleanXfac,
		DTA_DestHeight, 80 * CleanYfac,
		DTA_Translation, &mRemap,
		DTA_Masked, true,
		TAG_DONE);

	V_DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);

	spriteframe_t *sprframe = NULL;
	DVector2 Scale;

	if (mPlayerState != NULL)
	{
		if (mSkin == 0)
		{
			sprframe = &SpriteFrames[sprites[mPlayerState->sprite].spriteframes + mPlayerState->GetFrame()];
			Scale = GetDefaultByType(mPlayerClass->Type)->Scale;
		}
		else
		{
			sprframe = &SpriteFrames[sprites[skins[mSkin].sprite].spriteframes + mPlayerState->GetFrame()];
			Scale = skins[mSkin].Scale;
		}
	}

	if (sprframe != NULL)
	{
		FTexture *tex = TexMan(sprframe->Texture[mRotation]);
		if (tex != NULL && tex->UseType != FTexture::TEX_Null)
		{
			FRemapTable *trans = NULL;
			if (mTranslate) trans = translationtables[TRANSLATION_Players](MAXPLAYERS);
			screen->DrawTexture (tex,
				x + 36*CleanXfac, y + 71*CleanYfac,
				DTA_DestWidthF, tex->GetScaledWidthDouble() * CleanXfac * Scale.X,
				DTA_DestHeightF, tex->GetScaledHeightDouble() * CleanYfac * Scale.Y,
				DTA_Translation, trans,
				DTA_FlipX, sprframe->Flip & (1 << mRotation),
				TAG_DONE);
		}
	}
}

