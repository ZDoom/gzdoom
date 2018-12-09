/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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

#include "v_video.h"
#include "bitmap.h"
#include "image.h"
#include "w_wad.h"
#include "files.h"

FMemArena FImageSource::ImageArena(32768);
TArray<FImageSource *>FImageSource::ImageForLump;
int FImageSource::NextID;

//===========================================================================
// 
// the default just returns an empty texture.
//
//===========================================================================

TArray<uint8_t> FImageSource::GetPalettedPixels(int conversion)
{
	TArray<uint8_t> Pixels(Width * Height, true);
	memset(Pixels.Data(), 0, Width * Height);
	return Pixels;
}


//===========================================================================
//
// FImageSource::CopyPixels 
//
// this is the generic case that can handle
// any properly implemented texture for software rendering.
// Its drawback is that it is limited to the base palette which is
// why all classes that handle different palettes should subclass this
// method
//
//===========================================================================

int FImageSource::CopyPixels(FBitmap *bmp, int conversion)
{
	if (conversion == luminance) conversion = normal;	// luminance images have no use as an RGB source.
	PalEntry *palette = screen->GetPalette();
	for(int i=1;i<256;i++) palette[i].a = 255;	// set proper alpha values
	auto ppix = GetPalettedPixels(conversion);	// should use composition cache
	bmp->CopyPixelData(0, 0, ppix.Data(), Width, Height, Height, 1, 0, palette, nullptr);
	for(int i=1;i<256;i++) palette[i].a = 0;
	return 0;
}

int FImageSource::CopyTranslatedPixels(FBitmap *bmp, PalEntry *remap)
{
	auto ppix = GetPalettedPixels(false);	// should use composition cache
	bmp->CopyPixelData(0, 0, ppix.Data(), Width, Height, Height, 1, 0, remap, nullptr);
	return 0;
}


//==========================================================================
//
//
//
//==========================================================================

typedef FImageSource * (*CreateFunc)(FileReader & file, int lumpnum);

struct TexCreateInfo
{
	CreateFunc TryCreate;
	ETextureType usetype;
};

FImageSource *IMGZImage_TryCreate(FileReader &, int lumpnum);
FImageSource *PNGImage_TryCreate(FileReader &, int lumpnum);
FImageSource *JPEGImage_TryCreate(FileReader &, int lumpnum);
FImageSource *DDSImage_TryCreate(FileReader &, int lumpnum);
FImageSource *PCXImage_TryCreate(FileReader &, int lumpnum);
FImageSource *TGAImage_TryCreate(FileReader &, int lumpnum);
FImageSource *RawPageImage_TryCreate(FileReader &, int lumpnum);
FImageSource *FlatImage_TryCreate(FileReader &, int lumpnum);
FImageSource *PatchImage_TryCreate(FileReader &, int lumpnum);
FImageSource *EmptyImage_TryCreate(FileReader &, int lumpnum);
FImageSource *AutomapImage_TryCreate(FileReader &, int lumpnum);


// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
FImageSource * FImageSource::GetImage(int lumpnum, ETextureType usetype)
{
	static TexCreateInfo CreateInfo[] = {
		{ IMGZImage_TryCreate,			ETextureType::Any },
		{ PNGImage_TryCreate,			ETextureType::Any },
		{ JPEGImage_TryCreate,			ETextureType::Any },
		{ DDSImage_TryCreate,			ETextureType::Any },
		{ PCXImage_TryCreate,			ETextureType::Any },
		{ TGAImage_TryCreate,			ETextureType::Any },
		{ RawPageImage_TryCreate,		ETextureType::MiscPatch },
		{ FlatImage_TryCreate,			ETextureType::Flat },
		{ PatchImage_TryCreate,			ETextureType::Any },
		{ EmptyImage_TryCreate,			ETextureType::Any },
		{ AutomapImage_TryCreate,		ETextureType::MiscPatch },
	};

	if (lumpnum == -1) return nullptr;

	unsigned size = ImageForLump.Size();
	if (size <= (unsigned)lumpnum)
	{
		// Hires textures can be added dynamically to the end of the lump array, so this must be checked each time.
		ImageForLump.Resize(lumpnum + 1);
		for (; size < ImageForLump.Size(); size++) ImageForLump[size] = nullptr;
	}
	// An image for this lump already exists. We do not need another one.
	if (ImageForLump[lumpnum] != nullptr) return ImageForLump[lumpnum];

	auto data = Wads.OpenLumpReader(lumpnum);

	for (size_t i = 0; i < countof(CreateInfo); i++)
	{
		if ((CreateInfo[i].usetype == usetype || CreateInfo[i].usetype == ETextureType::Any))
		{
			auto image = CreateInfo[i].TryCreate(data, lumpnum);
			if (image != nullptr)
			{
				ImageForLump[lumpnum] = image;
				return image;
			}
		}
	}
	return nullptr;
}
