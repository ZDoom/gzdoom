/*
** anmtexture.cpp
** Texture class for reading the first frame of Build ANM files
**
**---------------------------------------------------------------------------
** Copyright 2020 Christoph Oelckers
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
#include "animlib.h"

//==========================================================================
//
//
//==========================================================================

class FAnmTexture : public FImageSource
{

public:
	FAnmTexture (int lumpnum, int w, int h);
	void ReadFrame(uint8_t *buffer, uint8_t *palette);
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap *bmp, int conversion) override;
};


//==========================================================================
//
//
//
//==========================================================================

FImageSource *AnmImage_TryCreate(FileReader & file, int lumpnum)
{
	file.Seek(0, FileReader::SeekSet);
	char check[4];
	auto num = file.Read(check, 4);
	if (num < 4) return nullptr;
	if (memcmp(check, "LPF ", 4)) return nullptr;
	file.Seek(0, FileReader::SeekSet);
	auto buffer = file.ReadPadded(1);

	anim_t anim;
	if (ANIM_LoadAnim(&anim, buffer.Data(), buffer.Size() - 1) < 0)
	{
		return nullptr;
	}
	int numframes = ANIM_NumFrames(&anim);
	if (numframes >= 1)
	{
		return new FAnmTexture(lumpnum, 320, 200);
	}

	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FAnmTexture::FAnmTexture (int lumpnum, int w, int h)
	: FImageSource(lumpnum)
{
	Width = w;
	Height = h;
	LeftOffset = 0;
	TopOffset = 0;
}

void FAnmTexture::ReadFrame(uint8_t *pixels, uint8_t *palette)
{
	FileData lump = fileSystem.ReadFile (SourceLump);
	uint8_t *source = (uint8_t *)lump.GetMem(); 

	anim_t anim;
	if (ANIM_LoadAnim(&anim, source, (int)lump.GetSize()) >= 0)
	{
		int numframes = ANIM_NumFrames(&anim);
		if (numframes >= 1)
		{
			memcpy(palette, ANIM_GetPalette(&anim), 768);
			memcpy(pixels, ANIM_DrawFrame(&anim, 1), Width*Height);
			return;
		}
	}
	memset(pixels, 0, Width*Height);
	memset(palette, 0, 768);
}

//==========================================================================
//
//
//
//==========================================================================

TArray<uint8_t> FAnmTexture::CreatePalettedPixels(int conversion)
{
	TArray<uint8_t> pixels(Width*Height, true);
	uint8_t buffer[64000];
	uint8_t palette[768];
	uint8_t remap[256];

	ReadFrame(buffer, palette);
	for(int i=0;i<256;i++)
	{
		remap[i] = ColorMatcher.Pick(palette[i*3], palette[i*3+1], palette[i*3+2]);
	}
	ImageHelpers::FlipNonSquareBlockRemap (pixels.Data(), buffer, Width, Height, Width, remap); 
	return pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FAnmTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	uint8_t buffer[64000];
	uint8_t palette[768];
	ReadFrame(buffer, palette);

    auto dpix = bmp->GetPixels();
	for (int i = 0; i < Width * Height; i++)
	{
		int p = i * 4;
		int index = buffer[i];
		dpix[p + 0] = palette[index * 3 + 2];
		dpix[p + 1] = palette[index * 3 + 1];
		dpix[p + 2] = palette[index * 3];
		dpix[p + 3] = 255;
	}

	return -1;
}

 