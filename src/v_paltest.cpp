/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
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

#include "v_video.h"

class FPaletteTester : public FTexture
{
public:
	FPaletteTester ();

	const uint8_t *GetPixels(FRenderStyle);
	bool CheckModified(FRenderStyle);
	void SetTranslation(int num);

protected:
	uint8_t Pixels[16*16];
	int CurTranslation;
	int WantTranslation;

	void MakeTexture();
};

//==========================================================================
//
// FPaleteTester Constructor
//
// This is just a 16x16 image with every possible color value.
//
//==========================================================================

FPaletteTester::FPaletteTester()
{
	Width = 16;
	Height = 16;
	WidthBits = 4;
	HeightBits = 4;
	WidthMask = 15;
	CurTranslation = 0;
	WantTranslation = 1;
	MakeTexture();
}

//==========================================================================
//
// FPaletteTester :: CheckModified
//
//==========================================================================

bool FPaletteTester::CheckModified(FRenderStyle)
{
	return CurTranslation != WantTranslation;
}

//==========================================================================
//
// FPaletteTester :: SetTranslation
//
//==========================================================================

void FPaletteTester::SetTranslation(int num)
{
	if (num >= 1 && num <= 9)
	{
		WantTranslation = num;
	}
}

//==========================================================================
//
// FPaletteTester :: GetPixels
//
//==========================================================================

const uint8_t *FPaletteTester::GetPixels (FRenderStyle)
{
	if (CurTranslation != WantTranslation)
	{
		MakeTexture();
	}
	return Pixels;
}

//==========================================================================
//
// FPaletteTester :: MakeTexture
//
//==========================================================================

void FPaletteTester::MakeTexture()
{
	int i, j, k, t;
	uint8_t *p;

	t = WantTranslation;
	p = Pixels;
	k = 0;
	for (i = 0; i < 16; ++i)
	{
		for (j = 0; j < 16; ++j)
		{
			*p++ = (t > 1) ? translationtables[TRANSLATION_Standard][t - 2]->Remap[k] : k;
			k += 16;
		}
		k -= 255;
	}
	CurTranslation = t;
}


//==========================================================================
//
// 
//
//==========================================================================

void V_DrawPaletteTester(int paletteno)
{
	// This used to just write the palette to the display buffer.
	// With hardware-accelerated 2D, that doesn't work anymore.
	// Drawing it as a texture does and continues to show how
	// well the PalTex shader is working.
	static FPaletteTester palette;
	int size = screen->GetHeight() < 800 ? 16 * 7 : 16 * 7 * 2;

	palette.SetTranslation(paletteno);
	screen->DrawTexture(&palette, 0, 0,
		DTA_DestWidth, size,
		DTA_DestHeight, size,
		DTA_Masked, false,
		TAG_DONE);
}
