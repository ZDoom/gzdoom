/*
** patchtexture.cpp
** Texture class for single Doom patches
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
#include "image.h"
#include "imagehelpers.h"
#include "m_swap.h"


// Doom patch format header
struct patch_t
{
	int16_t			width;			// bounding box size 
	int16_t			height;
	int16_t			leftoffset; 	// pixels to the left of origin 
	int16_t			topoffset;		// pixels below the origin 
	uint32_t 		columnofs[1];	// only [width] used
};


// posts are runs of non masked source pixels
struct column_t
{
	uint8_t		topdelta;		// -1 is the last post in a column
	uint8_t		length; 		// length data bytes follows
};

bool checkPatchForAlpha(const void *buffer, uint32_t length);

//==========================================================================
//
// A texture that is just a single Doom format patch
//
//==========================================================================

class FPatchTexture : public FImageSource
{
	bool badflag = false;
	bool isalpha = false;
public:
	FPatchTexture (int lumpnum, int w, int h, int lo, int to, bool isalphatex);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
	bool SupportRemap0() override { return !badflag; }
	void DetectBadPatches();
};

//==========================================================================
//
// Checks if the currently open lump can be a Doom patch
//
//==========================================================================

static bool CheckIfPatch(FileReader & file, bool &isalpha)
{
	if (file.GetLength() < 13) return false;	// minimum length of a valid Doom patch

	file.Seek(0, FileReader::SeekSet);
	auto data = file.Read(file.GetLength());

	const patch_t *foo = (const patch_t *)data.data();

	int height = LittleShort(foo->height);
	int width = LittleShort(foo->width);

	if (height > 0 && height <= 2048 && width > 0 && width <= 2048 && width < file.GetLength()/4)
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
			else if (ofs >= (uint32_t)(file.GetLength()))	// Need one byte for an empty column (but there's patches that don't know that!)
			{
				return false;
			}
		}
		if (!gapAtStart)
		{
			// only check this if the texture passed validation.
			// Here is a good point because we already have a valid buffer of the lump's data.
			isalpha = checkPatchForAlpha(data.data(), (uint32_t)file.GetLength());
		}
		return !gapAtStart;
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FImageSource *PatchImage_TryCreate(FileReader & file, int lumpnum)
{
	bool isalpha;

	file.Seek(0, FileReader::SeekSet);
	int width = file.ReadUInt16();
	int height = file.ReadUInt16();
	int leftoffset = file.ReadInt16();
	int topoffset = file.ReadInt16();
	// quickly reject any lump which cannot be a texture without reading in all the data.
	if (height > 0 && height <= 2048 && width > 0 && width <= 2048 && width < file.GetLength() / 4 && abs(leftoffset) < 4096 && abs(topoffset) < 4096)
	{
		if (!CheckIfPatch(file, isalpha)) return NULL;
		file.Seek(8, FileReader::SeekSet);
		return new FPatchTexture(lumpnum, width, height, leftoffset, topoffset, isalpha);
	}
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FPatchTexture::FPatchTexture (int lumpnum, int w, int h, int lo, int to, bool isalphatex)
: FImageSource(lumpnum)
{
	bUseGamePalette = !isalphatex;
	isalpha = isalphatex;
	Width = w;
	Height = h;
	LeftOffset = lo;
	TopOffset = to;
	DetectBadPatches();
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FPatchTexture::CreatePalettedPixels(int conversion, int frame)
{
	uint8_t *remap, remaptable[256];
	int numspans;
	const column_t *maxcol;
	int x;

	auto lump =  fileSystem.ReadFile (SourceLump);
	const patch_t *patch = (const patch_t *)lump.GetMem();

	maxcol = (const column_t *)((const uint8_t *)patch + fileSystem.FileLength (SourceLump) - 3);

	remap = ImageHelpers::GetRemap(conversion == luminance, isalpha);
	// Special case for skies
	if (conversion == noremap0 && remap == GPalette.Remap)
	{
		memcpy(remaptable, GPalette.Remap, 256);
		remaptable[0] = 0;
		remap = remaptable;
	}

	if (badflag)
	{
		PalettedPixels Pixels(Width * Height);
		uint8_t *out;

		// Draw the image to the buffer
		for (x = 0, out = Pixels.Data(); x < Width; ++x)
		{
			const uint8_t *in = (const uint8_t *)patch + LittleLong(patch->columnofs[x]) + 3;

			for (int y = Height; y > 0; --y)
			{
				*out = remap[*in];
				out++, in++;
			}
		}
		return Pixels;
	}

	int numpix = Width * Height;

	numspans = Width;

	PalettedPixels Pixels(numpix);
	memset (Pixels.Data(), 0, numpix);

	// Draw the image to the buffer
	for (x = 0; x < Width; ++x)
	{
		uint8_t *outtop = Pixels.Data() + x*Height;
		const column_t *column = (const column_t *)((const uint8_t *)patch + LittleLong(patch->columnofs[x]));
		int top = -1;

		while (column < maxcol && column->topdelta != 0xFF)
		{
			if (column->topdelta <= top)
			{
				top += column->topdelta;
			}
			else
			{
				top = column->topdelta;
			}

			int len = column->length;
			uint8_t *out = outtop + top;

			if (len != 0)
			{
				if (top + len > Height)	// Clip posts that extend past the bottom
				{
					len = Height - top;
				}
				if (len > 0)
				{
					numspans++;

					const uint8_t *in = (const uint8_t *)column + 3;
					for (int i = 0; i < len; ++i)
					{
						out[i] = remap[in[i]];
					}
				}
			}
			column = (const column_t *)((const uint8_t *)column + column->length + 4);
		}
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FPatchTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	if (!isalpha) return FImageSource::CopyPixels(bmp, conversion, frame);
	else return CopyTranslatedPixels(bmp, GPalette.GrayscaleMap.Palette, frame);
}

//==========================================================================
//
// Fix for certain special patches on single-patch textures.
//
//==========================================================================

void FPatchTexture::DetectBadPatches ()
{
	// The patch must look like it is large enough for the rules to apply to avoid using this on truly empty patches.
	if (fileSystem.FileLength(SourceLump) < Width * Height / 2) return;

	// Check if this patch is likely to be a problem.
	// It must be 256 pixels tall, and all its columns must have exactly
	// one post, where each post has a supposed length of 0.
	auto lump =  fileSystem.ReadFile (SourceLump);
	const patch_t *realpatch = (patch_t *)lump.GetMem();
	const uint32_t *cofs = realpatch->columnofs;
	int x, x2 = LittleShort(realpatch->width);

	if (LittleShort(realpatch->height) == 256)
	{
		for (x = 0; x < x2; ++x)
		{
			const column_t *col = (column_t*)((uint8_t*)realpatch+LittleLong(cofs[x]));
			if (col->topdelta != 0 || col->length != 0)
			{
				return;	// It's not bad!
			}
			col = (column_t *)((uint8_t *)col + 256 + 4);
			if (col->topdelta != 0xFF)
			{
				return;	// More than one post in a column!
			}
		}
		LeftOffset = 0;
		TopOffset = 0;
		badflag = true;
		bMasked = false;	// Hacked textures don't have transparent parts.
	}
}
