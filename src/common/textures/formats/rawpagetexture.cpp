/*
** rawpagetexture.cpp
** Texture class for Raven's raw fullscreen pages
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
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"


//==========================================================================
//
// A raw 320x200 graphic used by Heretic and Hexen fullscreen images
//
//==========================================================================

class FRawPageTexture : public FImageSource
{
	int mPaletteLump = -1;
public:
	FRawPageTexture (int lumpnum);
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap *bmp, int conversion) override;
};

//==========================================================================
//
// RAW textures must be exactly 64000 bytes long and not be identifiable
// as Doom patch
//
//==========================================================================

bool CheckIfRaw(FileReader & data, int desiredsize)
{
	if (data.GetLength() != desiredsize) return false;

	// This is probably a raw page graphic, but do some checking to be sure
	patch_t *foo;
	int height;
	int width;

	data.Seek(0, FileReader::SeekSet);
	auto bits = data.Read(data.GetLength());
	foo = (patch_t *)bits.Data();;

	height = LittleShort(foo->height);
	width = LittleShort(foo->width);

	if (height > 0 && height < 510 && width > 0 && width < 15997)
	{
		// The dimensions seem like they might be valid for a patch, so
		// check the column directory for extra security. At least one
		// column must begin exactly at the end of the column directory,
		// and none of them must point past the end of the patch.
		bool gapAtStart = true;
		int x;

		for (x = 0; x < width; ++x)
		{
			uint32_t ofs = LittleLong(foo->columnofs[x]);
			if (ofs == (uint32_t)width * 4 + 8)
			{
				gapAtStart = false;
			}
			else if (ofs >= 64000-1)	// Need one byte for an empty column
			{
				return true;
			}
			else
			{
				// Ensure this column does not extend beyond the end of the patch
				const uint8_t *foo2 = (const uint8_t *)foo;
				while (ofs < 64000)
				{
					if (foo2[ofs] == 255)
					{
						return true;
					}
					ofs += foo2[ofs+1] + 4;
				}
				if (ofs >= 64000)
				{
					return true;
				}
			}
		}
		if (gapAtStart || (x != width))
		{
			return true;
		}
		return false;
	}
	else
	{
		return true;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FImageSource *RawPageImage_TryCreate(FileReader & file, int lumpnum)
{
	if (!CheckIfRaw(file, 64000)) return nullptr;
	return new FRawPageTexture(lumpnum);
}


//==========================================================================
//
//
//
//==========================================================================

FRawPageTexture::FRawPageTexture (int lumpnum)
: FImageSource(lumpnum)
{
	Width = 320;
	Height = 200;

	// Special case hack for Heretic's E2 end pic. This is not going to be exposed as an editing feature because the implications would be horrible.
	FString Name;
	fileSystem.GetFileShortName(Name, lumpnum);
	if (Name.CompareNoCase("E2END") == 0)
	{
		mPaletteLump = fileSystem.CheckNumForName("E2PAL");
		if (fileSystem.FileLength(mPaletteLump) < 768) mPaletteLump = -1;
	}
	else bUseGamePalette = true;
}

//==========================================================================
//
//
//
//==========================================================================

TArray<uint8_t> FRawPageTexture::CreatePalettedPixels(int conversion)
{
	FileData lump = fileSystem.ReadFile (SourceLump);
	const uint8_t *source = (const uint8_t *)lump.GetMem();
	const uint8_t *source_p = source;
	uint8_t *dest_p;

	TArray<uint8_t> Pixels(Width*Height, true);
	dest_p = Pixels.Data();

	const uint8_t *remap = ImageHelpers::GetRemap(conversion == luminance);

	// This does not handle the custom palette. 
	// User maps are encouraged to use a real image format when replacing E2END and the original could never be used anywhere else.

	// Convert the source image from row-major to column-major format
	for (int y = 200; y != 0; --y)
	{
		for (int x = 320; x != 0; --x)
		{
			*dest_p = remap[*source_p];
			dest_p += 200;
			source_p++;
		}
		dest_p -= 200 * 320 - 1;
	}
	return Pixels;
}

int FRawPageTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	if (mPaletteLump < 0) return FImageSource::CopyPixels(bmp, conversion);
	else
	{
		FileData lump = fileSystem.ReadFile(SourceLump);
		FileData plump = fileSystem.ReadFile(mPaletteLump);
		const uint8_t *source = (const uint8_t *)lump.GetMem();
		const uint8_t *psource = (const uint8_t *)plump.GetMem();
		PalEntry paldata[256];
		for (auto & pe : paldata)
		{
			pe.r = *psource++;
			pe.g = *psource++;
			pe.b = *psource++;
			pe.a = 255;
		}
		bmp->CopyPixelData(0, 0, source, 320, 200, 1, 320, 0, paldata);
	}
	return 0;
}
