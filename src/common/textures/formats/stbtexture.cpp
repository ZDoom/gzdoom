/*
** stbtexture.cpp
** Texture class for reading textures with stb_image
**
**---------------------------------------------------------------------------
** Copyright 2019 Christoph Oelckers
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

#define STB_IMAGE_IMPLEMENTATION    
#define STBI_NO_STDIO
// Undefine formats we do not want to support here.
#define STBI_NO_JPEG
#define STBI_NO_PNG
#define STBI_NO_TGA
#define STBI_NO_PSD
#define STBI_NO_HDR
#define STBI_NO_PNM
#include "stb_image.h"
 

#include "files.h"
#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"

//==========================================================================
//
// A texture backed by stb_image
// This will load GIF, BMP and PICT images.
// PNG, JPG and TGA are still being handled by the existing dedicated implementations.
// PSD and HDR are impractical for reading texture data and thus are disabled.
// PnM could be enabled, if its identification semantics were stronger.
// stb_image only checks the first two characters which simply would falsely identify several flats with the right colors in the first two bytes.
//
//==========================================================================

class FStbTexture : public FImageSource
{

public:
	FStbTexture (int lumpnum, int w, int h);
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap *bmp, int conversion) override;
};


static stbi_io_callbacks callbacks = {
	[](void* user,char* data,int size) -> int { return (int)reinterpret_cast<FileReader*>(user)->Read(data, size); },
	[](void* user,int n) { reinterpret_cast<FileReader*>(user)->Seek(n, FileReader::SeekCur); },
	[](void* user) -> int { return reinterpret_cast<FileReader*>(user)->Tell() >= reinterpret_cast<FileReader*>(user)->GetLength(); }
};
//==========================================================================
//
//
//
//==========================================================================

FImageSource *StbImage_TryCreate(FileReader & file, int lumpnum)
{
	int x, y, comp;
	file.Seek(0, FileReader::SeekSet);
	int result = stbi_info_from_callbacks(&callbacks, &file, &x, &y, &comp);
	if (result == 1)
	{
		return new FStbTexture(lumpnum, x, y);
	}
	
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FStbTexture::FStbTexture (int lumpnum, int w, int h)
	: FImageSource(lumpnum)
{
	Width = w;
	Height = h;
	LeftOffset = 0;
	TopOffset = 0;
}

//==========================================================================
//
//
//
//==========================================================================

TArray<uint8_t> FStbTexture::CreatePalettedPixels(int conversion)
{
	FBitmap bitmap;
	bitmap.Create(Width, Height);
	CopyPixels(&bitmap, conversion);
	const uint8_t *data = bitmap.GetPixels();

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	TArray<uint8_t> Pixels(Width*Height, true);
	dest_p = Pixels.Data();

	bool doalpha = conversion == luminance; 
	// Convert the source image from row-major to column-major format and remap it
	for (int y = Height; y != 0; --y)
	{
		for (int x = Width; x != 0; --x)
		{
			int b = *data++;
			int g = *data++;
			int r = *data++;
			int a = *data++;
			if (a < 128) *dest_p = 0;
			else *dest_p = ImageHelpers::RGBToPalette(doalpha, r, g, b); 
			dest_p += dest_adv;
		}
		dest_p -= dest_rew;
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FStbTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	auto lump = fileSystem.OpenFileReader (SourceLump); 
	int x, y, chan;
	auto image = stbi_load_from_callbacks(&callbacks, &lump, &x, &y, &chan, STBI_rgb_alpha); 	
	if (image)
		bmp->CopyPixelDataRGB(0, 0, image, x, y, 4, x*4, 0, CF_RGBA); 	
	stbi_image_free(image);	
	return -1;
}

 