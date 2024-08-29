/*
** pngtexture.cpp
** Texture class for PNG images
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2005-2019 Christoph Oelckers
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

#include "m_png.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"
#include "printf.h"
#include "texturemanager.h"
#include "filesystem.h"
#include "m_swap.h"

//==========================================================================
//
// A PNG texture
//
//==========================================================================

class FPNGTexture : public FImageSource
{
public:
	FPNGTexture (int lumpnum, int width, int height, int32_t ofs_x, int32_t ofs_y, bool isMasked);

	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
};

FImageSource* StbImage_TryCreate(FileReader& file, int lumpnum);

//==========================================================================
//
//
//
//==========================================================================

FImageSource *PNGImage_TryCreate(FileReader & data, int lumpnum)
{
	uint32_t width = 0;
	uint32_t height = 0;
	int32_t ofs_x = 0;
	int32_t ofs_y = 0;
	bool isMasked = false;

	data.Seek (0, FileReader::SeekSet);

	if(!M_GetPNGSize(data, width, height, &ofs_x, &ofs_y, &isMasked))
	{
		return nullptr;
	}
	
	return new FPNGTexture(lumpnum, width, height, ofs_x, ofs_y, isMasked);
}

//==========================================================================
//
//
//
//==========================================================================

FPNGTexture::FPNGTexture (int lumpnum, int width, int height, int32_t ofs_x, int32_t ofs_y, bool isMasked) : FImageSource(lumpnum)
{
	Width = width;
	Height = height;
	bTranslucent = bMasked = isMasked;

	if (ofs_x < -32768 || ofs_x > 32767)
	{
		Printf ("X-Offset for PNG texture %s is bad: %d (0x%08x)\n", fileSystem.GetFileFullName (lumpnum), ofs_x, ofs_x);
		ofs_x = 0;
	}
	if (ofs_y < -32768 || ofs_y > 32767)
	{
		Printf ("Y-Offset for PNG texture %s is bad: %d (0x%08x)\n", fileSystem.GetFileFullName (lumpnum), ofs_y, ofs_y);
		ofs_y = 0;
	}
	LeftOffset = ofs_x;
	TopOffset = ofs_y;
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FPNGTexture::CreatePalettedPixels(int conversion, int frame)
{
	PalettedPixels Pixels(Width * Height);

	FBitmap tmp;

	M_ReadPNG(fileSystem.OpenFileReader(SourceLump), &tmp);

	uint8_t *in = tmp.GetPixels();
	uint8_t *out = Pixels.Data();

	int x, y;
	int pitch = Width * 4;
	int backstep = Height * pitch - 4;
	bool alphatex = conversion == luminance;

	for (x = Width; x > 0; --x)
	{
		for (y = Height; y > 0; --y)
		{
			*out++ = ImageHelpers::RGBToPalette(alphatex, in[2], in[1], in[0], in[3]);
			in += pitch;
		}
		in -= backstep;
	}

	return Pixels;
}

//===========================================================================
//
// FPNGTexture::CopyPixels
//
//===========================================================================

int FPNGTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	M_ReadPNG(fileSystem.OpenFileReader(SourceLump), bmp);
	return false;
}


#include "textures.h"

//==========================================================================
//
// A savegame picture
// This is essentially a stripped down version of the PNG texture
// only supporting the features actually present in a savegame
// that does not use an image source, because image sources are not
// meant to be transient data like the savegame picture.
//
//==========================================================================

class FPNGFileTexture : public FTexture
{
public:
	FPNGFileTexture (FBitmap && tmp);
	virtual FBitmap GetBgraBitmap(const PalEntry *remap, int *trans) override;

protected:
	FBitmap bmp;
};


//==========================================================================
//
//
//
//==========================================================================

FGameTexture *PNGTexture_CreateFromFile(FileSys::FileReader &&fr, const FString &filename)
{
	FBitmap tmp;

	if(M_ReadPNG(std::move(fr), &tmp))
	{
		return MakeGameTexture(new FPNGFileTexture(std::move(tmp)), nullptr, ETextureType::Override);
	}
	else
	{
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FPNGFileTexture::FPNGFileTexture (FBitmap && tmp)
{
	bmp = std::move(tmp);
	Width = bmp.GetWidth();
	Height = bmp.GetHeight();
	Masked = false;
	bTranslucent = false;
}

//===========================================================================
//
// FPNGTexture::CopyPixels
//
//===========================================================================

FBitmap FPNGFileTexture::GetBgraBitmap(const PalEntry *remap, int *trans)
{
	FBitmap bmp2;
	bmp2.Copy(bmp, false);
	return bmp2;
} 