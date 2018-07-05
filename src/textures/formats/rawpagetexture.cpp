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

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "gi.h"
#include "bitmap.h"
#include "textures/textures.h"


//==========================================================================
//
// A raw 320x200 graphic used by Heretic and Hexen fullscreen images
//
//==========================================================================

class FRawPageTexture : public FWorldTexture
{
	int mPaletteLump = -1;
public:
	FRawPageTexture (int lumpnum);
	uint8_t *MakeTexture (FRenderStyle style) override;
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf) override;
};

//==========================================================================
//
// RAW textures must be exactly 64000 bytes long and not be identifiable
// as Doom patch
//
//==========================================================================

static bool CheckIfRaw(FileReader & data)
{
	if (data.GetLength() != 64000) return false;

	// This is probably a raw page graphic, but do some checking to be sure
	patch_t *foo;
	int height;
	int width;

	foo = (patch_t *)M_Malloc (data.GetLength());
	data.Seek (0, FileReader::SeekSet);
	data.Read (foo, data.GetLength());

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
				M_Free (foo);
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
						M_Free (foo);
						return true;
					}
					ofs += foo2[ofs+1] + 4;
				}
				if (ofs >= 64000)
				{
					M_Free (foo);
					return true;
				}
			}
		}
		if (gapAtStart || (x != width))
		{
			M_Free (foo);
			return true;
		}
		M_Free(foo);
		return false;
	}
	else
	{
		M_Free (foo);
		return true;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *RawPageTexture_TryCreate(FileReader & file, int lumpnum)
{
	if (!CheckIfRaw(file)) return NULL;
	return new FRawPageTexture(lumpnum);
}


//==========================================================================
//
//
//
//==========================================================================

FRawPageTexture::FRawPageTexture (int lumpnum)
: FWorldTexture(NULL, lumpnum)
{
	Width = 320;
	Height = 200;
	WidthBits = 8;
	HeightBits = 8;
	WidthMask = 255;

	// Special case hack for Heretic's E2 end pic. This is not going to be exposed as an editing feature because the implications would be horrible.
	if (Name.CompareNoCase("E2END") == 0 && gameinfo.gametype == GAME_Heretic)
	{
		mPaletteLump = Wads.CheckNumForName("E2PAL");
		if (Wads.LumpLength(mPaletteLump) < 768) mPaletteLump = -1;
	}
}

//==========================================================================
//
//
//
//==========================================================================

uint8_t *FRawPageTexture::MakeTexture (FRenderStyle style)
{
	FMemLump lump = Wads.ReadLump (SourceLump);
	const uint8_t *source = (const uint8_t *)lump.GetMem();
	const uint8_t *source_p = source;
	uint8_t *dest_p;

	auto Pixels = new uint8_t[Width*Height];
	dest_p = Pixels;

	const uint8_t *remap = GetRemap(style);

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

int FRawPageTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	if (mPaletteLump < 0) return FTexture::CopyTrueColorPixels(bmp, x, y, rotate, inf);
	else
	{
		FMemLump lump = Wads.ReadLump(SourceLump);
		FMemLump plump = Wads.ReadLump(mPaletteLump);
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
		bmp->CopyPixelData(x, y, source, 320, 200, 1, 320, 0, paldata, inf);
	}
	return 0;
}
