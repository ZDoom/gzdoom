/*
** startscreentexture.cpp
** Texture class to create a texture from the start screen's imagé
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2019 Christoph Oelckers
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
**
*/

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "gi.h"
#include "bitmap.h"
#include "textures/textures.h"
#include "imagehelpers.h"
#include "image.h"
#include "st_start.h"


//==========================================================================
//
//
//
//==========================================================================

class FStartScreenTexture : public FImageSource
{
	BitmapInfo *info; // This must remain constant for the lifetime of this texture

public:
	FStartScreenTexture (BitmapInfo *srcdata);
	int CopyPixels(FBitmap *bmp, int conversion) override;
};

//==========================================================================
//
//
//
//==========================================================================

FImageSource *CreateStartScreenTexture(BitmapInfo *srcdata)
{
	return new FStartScreenTexture(srcdata);
}


//==========================================================================
//
//
//
//==========================================================================

FStartScreenTexture::FStartScreenTexture (BitmapInfo *srcdata)
: FImageSource(-1)
{
	Width = srcdata->bmiHeader.biWidth;
	Height = srcdata->bmiHeader.biHeight;
	info = srcdata;
	bUseGamePalette = false;

}

//==========================================================================
//
//
//
//==========================================================================

int FStartScreenTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	const RgbQuad *psource = info->bmiColors;
	PalEntry paldata[256] = {};
	auto pixels = ST_Util_BitsForBitmap(info);
	for (uint32_t i = 0; i < info->bmiHeader.biClrUsed; i++)
	{
		PalEntry &pe = paldata[i];
		pe.r = psource[i].rgbRed;
		pe.g = psource[i].rgbGreen;
		pe.b = psource[i].rgbBlue;
		pe.a = 255;
	}
	bmp->CopyPixelData(0, 0, pixels, Width, Height, 1, (Width + 3) & ~3, 0, paldata);
	
	return 0;
}
