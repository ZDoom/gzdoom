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

#include <memory>
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
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
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
	if (buffer.size() < 4) return nullptr;

	std::unique_ptr<anim_t> anim = std::make_unique<anim_t>(); // note that this struct is very large and should not go onto the stack!
	if (ANIM_LoadAnim(anim.get(), buffer.bytes(), buffer.size() - 1) < 0)
	{
		return nullptr;
	}
	int numframes = ANIM_NumFrames(anim.get());
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
	auto lump = fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes(); 

	std::unique_ptr<anim_t> anim = std::make_unique<anim_t>(); // note that this struct is very large and should not go onto the stack!
	if (ANIM_LoadAnim(anim.get(), source, (int)lump.size()) >= 0)
	{
		int numframes = ANIM_NumFrames(anim.get());
		if (numframes >= 1)
		{
			memcpy(palette, ANIM_GetPalette(anim.get()), 768);
			memcpy(pixels, ANIM_DrawFrame(anim.get(), 1), Width * Height);
			return;
		}
	}
	memset(pixels, 0, Width*Height);
	memset(palette, 0, 768);
}

struct workbuf
{
	uint8_t buffer[64000];
	uint8_t palette[768];
};

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FAnmTexture::CreatePalettedPixels(int conversion, int frame)
{
	PalettedPixels pixels(Width*Height);
	uint8_t remap[256];
	std::unique_ptr<workbuf> w = std::make_unique<workbuf>();

	ReadFrame(w->buffer, w->palette);
	for(int i=0;i<256;i++)
	{
		remap[i] = ColorMatcher.Pick(w->palette[i*3], w->palette[i*3+1], w->palette[i*3+2]);
	}
	ImageHelpers::FlipNonSquareBlockRemap (pixels.Data(), w->buffer, Width, Height, Width, remap); 
	return pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FAnmTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	std::unique_ptr<workbuf> w = std::make_unique<workbuf>();
	ReadFrame(w->buffer, w->palette);

    auto dpix = bmp->GetPixels();
	for (int i = 0; i < Width * Height; i++)
	{
		int p = i * 4;
		int index = w->buffer[i];
		dpix[p + 0] = w->palette[index * 3 + 2];
		dpix[p + 1] = w->palette[index * 3 + 1];
		dpix[p + 2] = w->palette[index * 3];
		dpix[p + 3] = 255;
	}

	return -1;
}

 