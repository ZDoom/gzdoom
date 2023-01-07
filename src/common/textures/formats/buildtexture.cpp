/*
** buildtexture.cpp
** Handling Build textures (now as a usable editing feature!)
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2018 Christoph Oelckers
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

#include "files.h"
#include "bitmap.h"
#include "image.h"
#include "palettecontainer.h"


//==========================================================================
//
//
//
//==========================================================================

FBuildTexture::FBuildTexture(const FString &pathprefix, int tilenum, const uint8_t *pixels, FRemapTable *translation, int width, int height, int left, int top)
: RawPixels (pixels), Translation(translation)
{
	Width = width;
	Height = height;
	LeftOffset = left;
	TopOffset = top;
}

PalettedPixels FBuildTexture::CreatePalettedPixels(int conversion)
{
	PalettedPixels Pixels(Width * Height);
	FRemapTable *Remap = Translation;
	for (int i = 0; i < Width*Height; i++)
	{
		auto c = RawPixels[i];
		Pixels[i] = conversion == luminance ? Remap->Palette[c].Luminance() : Remap->Remap[c];
	}
	return Pixels;
}

int FBuildTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	PalEntry *Remap = Translation->Palette;
	bmp->CopyPixelData(0, 0, RawPixels, Width, Height, Height, 1, 0, Remap);
	return -1;

}

