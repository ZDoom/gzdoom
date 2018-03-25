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

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "templates.h"
#include "v_palette.h"
#include "v_video.h"
#include "bitmap.h"
#include "textures/textures.h"
#include "r_data/r_translate.h"


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

class FPatchTexture : public FWorldTexture
{
	bool badflag = false;
	bool isalpha = false;
public:
	FPatchTexture (int lumpnum, patch_t *header, bool isalphatex);
	uint8_t *MakeTexture (FRenderStyle style) override;
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf) override;
	void DetectBadPatches();

	bool UseBasePalette() override { return !isalpha; }
	FTextureFormat GetFormat() override { return isalpha ? TEX_RGB : TEX_Pal; } // should be TEX_Gray instead of TEX_RGB. Maybe later when all is working.
};

//==========================================================================
//
// Checks if the currently open lump can be a Doom patch
//
//==========================================================================

static bool CheckIfPatch(FileReader & file, bool &isalpha)
{
	if (file.GetLength() < 13) return false;	// minimum length of a valid Doom patch
	
	uint8_t *data = new uint8_t[file.GetLength()];
	file.Seek(0, FileReader::SeekSet);
	file.Read(data, file.GetLength());
	
	const patch_t *foo = (const patch_t *)data;
	
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
				delete [] data;
				return false;
			}
		}
		if (!gapAtStart)
		{
			// only check this if the texture passed validation.
			// Here is a good point because we already have a valid buffer of the lump's data.
			isalpha = checkPatchForAlpha(data, (uint32_t)file.GetLength());
		}
		delete[] data;
		return !gapAtStart;
	}
	delete [] data;
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *PatchTexture_TryCreate(FileReader & file, int lumpnum)
{
	patch_t header;
	bool isalpha;

	if (!CheckIfPatch(file, isalpha)) return NULL;
	file.Seek(0, FileReader::SeekSet);
	header.width = file.ReadUInt16();
	header.height = file.ReadUInt16();
	header.leftoffset = file.ReadInt16();
	header.topoffset = file.ReadInt16();
	return new FPatchTexture(lumpnum, &header, isalpha);
}

//==========================================================================
//
//
//
//==========================================================================

FPatchTexture::FPatchTexture (int lumpnum, patch_t * header, bool isalphatex)
: FWorldTexture(NULL, lumpnum)
{
	isalpha = isalphatex;
	Width = header->width;
	Height = header->height;
	LeftOffset = header->leftoffset;
	TopOffset = header->topoffset;
	DetectBadPatches();
	CalcBitSize ();
}

//==========================================================================
//
//
//
//==========================================================================

uint8_t *FPatchTexture::MakeTexture (FRenderStyle style)
{
	uint8_t *remap, remaptable[256];
	int numspans;
	const column_t *maxcol;
	int x;

	FMemLump lump = Wads.ReadLump (SourceLump);
	const patch_t *patch = (const patch_t *)lump.GetMem();

	maxcol = (const column_t *)((const uint8_t *)patch + Wads.LumpLength (SourceLump) - 3);

	remap = GetRemap(style, isalpha);
	// Special case for skies
	if (bNoRemap0 && remap == GPalette.Remap)
	{
		memcpy(remaptable, GPalette.Remap, 256);
		remaptable[0] = 0;
		remap = remaptable;
	}

	if (badflag)
	{
		auto Pixels = new uint8_t[Width * Height];
		uint8_t *out;

		// Draw the image to the buffer
		for (x = 0, out = Pixels; x < Width; ++x)
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

	// Add a little extra space at the end if the texture's height is not
	// a power of 2, in case somebody accidentally makes it repeat vertically.
	int numpix = Width * Height + (1 << HeightBits) - Height;

	numspans = Width;

	auto Pixels = new uint8_t[numpix];
	memset (Pixels, 0, numpix);

	// Draw the image to the buffer
	for (x = 0; x < Width; ++x)
	{
		uint8_t *outtop = Pixels + x*Height;
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

int FPatchTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	if (!isalpha) return FTexture::CopyTrueColorPixels(bmp, x, y, rotate, inf);
	else return CopyTrueColorTranslated(bmp, x, y, rotate, translationtables[TRANSLATION_Standard][STD_Grayscale]->Palette, inf);
}

//==========================================================================
//
// Fix for certain special patches on single-patch textures.
//
//==========================================================================

void FPatchTexture::DetectBadPatches ()
{
	// The patch must look like it is large enough for the rules to apply to avoid using this on truly empty patches.
	if (Wads.LumpLength(SourceLump) < Width * Height / 2) return;

	// Check if this patch is likely to be a problem.
	// It must be 256 pixels tall, and all its columns must have exactly
	// one post, where each post has a supposed length of 0.
	FMemLump lump = Wads.ReadLump (SourceLump);
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
