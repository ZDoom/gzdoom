/*
** flattexture.cpp
** Texture class for standard Doom flats
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
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
#include "filesystem.h"
#include "imagehelpers.h"
#include "image.h"

//==========================================================================
//
// A texture defined between F_START and F_END markers
//
//==========================================================================

class FFlatTexture : public FImageSource
{
public:
	FFlatTexture (int lumpnum);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
};



//==========================================================================
//
// Since there is no way to detect the validity of a flat
// they can't be used anywhere else but between F_START and F_END
//
//==========================================================================

FImageSource *FlatImage_TryCreate(FileReader & file, int lumpnum)
{
	return new FFlatTexture(lumpnum);
}

//==========================================================================
//
//
//
//==========================================================================

FFlatTexture::FFlatTexture (int lumpnum)
: FImageSource(lumpnum)
{
	int bits;

	auto area = fileSystem.FileLength (lumpnum);

	switch (area)
	{
	default:
	case 64*64:		bits = 6;	break;
	case 8*8:		bits = 3;	break;
	case 16*16:		bits = 4;	break;
	case 32*32:		bits = 5;	break;
	case 128*128:	bits = 7;	break;
	case 256*256:	bits = 8;	break;
	}

	bUseGamePalette = true;
	bMasked = false;
	bTranslucent = false;
	Width = Height = 1 << bits;
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FFlatTexture::CreatePalettedPixels(int conversion, int frame)
{
	auto lump = fileSystem.OpenFileReader (SourceLump);
	PalettedPixels Pixels(Width*Height);
	auto numread = lump.Read (Pixels.Data(), Width*Height);
	if (numread < Width*Height)
	{
		memset (Pixels.Data() + numread, 0xBB, Width*Height - numread);
	}
	ImageHelpers::FlipSquareBlockRemap(Pixels.Data(), Width, ImageHelpers::GetRemap(conversion == luminance));
	return Pixels;
}

