
/*
** startuptexture.cpp
** Texture class for Hexen's startup screen
**
**---------------------------------------------------------------------------
** Copyright 2022 Christoph Oelckers
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

#define ST_NOTCH_WIDTH			16
#define ST_NOTCH_HEIGHT			23

#define ST_NETNOTCH_WIDTH		4
#define ST_NETNOTCH_HEIGHT		16

struct StrifeStartupInfo
{
	char name[9];
	uint8_t width, height;
};

static StrifeStartupInfo StrifeRawPics[] =
{
	{ "STRTPA1", 32, 64},
	{ "STRTPB1", 32, 64},
	{ "STRTPC1", 32, 64},
	{ "STRTPD1", 32, 64},
	{ "STRTLZ1", 16, 16},
	{ "STRTLZ2", 16, 16},
	{ "STRTBOT", 48, 48}
};

// there is only one palette for all these images.
static uint8_t startuppalette8[16];
static uint32_t startuppalette32[16];

//==========================================================================
//
//
//
//==========================================================================

class FStartupTexture : public FImageSource
{
public:
	FStartupTexture (int lumpnum);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame) override;
};

class FNotchTexture : public FImageSource
{
public:
	FNotchTexture (int lumpnum, int width, int height);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame) override;
};

class FStrifeStartupTexture : public FImageSource
{
public:
	FStrifeStartupTexture (int lumpnum, int w, int h);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
};

class FStrifeStartupBackground : public FImageSource
{
public:
	FStrifeStartupBackground (int lumpnum);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
};

//==========================================================================
//
// Use the same function as raw textures to eliminate Doom patches
//
//==========================================================================

bool CheckIfRaw(FileReader & data, unsigned desiredsize);

//==========================================================================
//
// loads all raw images for Hexen's and Strife's startup screens
//
//==========================================================================

FImageSource *StartupPageImage_TryCreate(FileReader & file, int lumpnum)
{
	if (fileSystem.CheckFileName(lumpnum, "STARTUP"))
	{
		if (!CheckIfRaw(file, 153648)) return nullptr;
		return new FStartupTexture(lumpnum);
	}
	if (fileSystem.CheckFileName(lumpnum, "NOTCH"))
	{
		if (!CheckIfRaw(file, ST_NOTCH_WIDTH * ST_NOTCH_HEIGHT / 2)) return nullptr;
		return new FNotchTexture(lumpnum, ST_NOTCH_WIDTH, ST_NOTCH_HEIGHT);
	}
	if (fileSystem.CheckFileName(lumpnum, "NETNOTCH"))
	{
		if (!CheckIfRaw(file, ST_NETNOTCH_WIDTH * ST_NETNOTCH_HEIGHT / 2)) return nullptr;
		return new FNotchTexture(lumpnum, ST_NETNOTCH_WIDTH, ST_NETNOTCH_HEIGHT);
	}
	if (fileSystem.CheckFileName(lumpnum, "STARTUP0"))
	{
		if (!CheckIfRaw(file, 64000)) return nullptr;
		return new FStrifeStartupBackground(lumpnum);
	}
	for(auto& sst : StrifeRawPics)
	{
		if (fileSystem.CheckFileName(lumpnum, sst.name))
		{
			if (!CheckIfRaw(file, sst.width * sst.height)) return nullptr;
			return new FStrifeStartupTexture(lumpnum, sst.width, sst.height);
		}
	}
	return nullptr;
}


//==========================================================================
//
//
//
//==========================================================================

FStartupTexture::FStartupTexture (int lumpnum)
: FImageSource(lumpnum)
{
	Width = 640;
	Height = 480;
	bUseGamePalette = false;
	
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();

	// Initialize the bitmap palette.
	// the palette is static so that the notches can share it.
	// Note that if the STARTUP image gets replaced, the notches will be all black unless they get replaced as well!
	for (int i = 0; i < 16; ++i)
	{
		PalEntry pe;
		pe.r = source[i * 3 + 0];
		pe.g = source[i * 3 + 1];
		pe.b = source[i * 3 + 2];
		pe.a = 63;
		// Convert from 6-bit per component to 8-bit per component.
		pe.d= (pe.d << 2) | ((pe.d >> 4) & 0x03030303);
		startuppalette8[i] = ColorMatcher.Pick(pe);
		startuppalette32[i] = pe;
	}
}

//==========================================================================
//
// PlanarToChunky
//
// Convert a 4-bpp planar image to chunky pixels.
//
//==========================================================================

template<class T>
void PlanarToChunky(T* dest, const uint8_t* src, const T* remap, int width, int height)
{
	int y, x;
	const uint8_t* src1, * src2, * src3, * src4;
	size_t plane_size = width / 8 * height;

	src1 = src;
	src2 = src1 + plane_size;
	src3 = src2 + plane_size;
	src4 = src3 + plane_size;

	for (y = height; y > 0; --y)
	{
		for (x = width; x > 0; x -= 8)
		{
			dest[0] = remap[((*src4 & 0x80) | ((*src3 & 0x80) >> 1) | ((*src2 & 0x80) >> 2) | ((*src1 & 0x80) >> 3)) >> 4];
			dest[1] = remap[((*src4 & 0x40) >> 3) | ((*src3 & 0x40) >> 4) | ((*src2 & 0x40) >> 5) | ((*src1 & 0x40) >> 6)];
			dest[2] = remap[(((*src4 & 0x20) << 2) | ((*src3 & 0x20) << 1) | ((*src2 & 0x20)) | ((*src1 & 0x20) >> 1)) >> 4];
			dest[3] = remap[((*src4 & 0x10) >> 1) | ((*src3 & 0x10) >> 2) | ((*src2 & 0x10) >> 3) | ((*src1 & 0x10) >> 4)];
			dest[4] = remap[(((*src4 & 0x08) << 4) | ((*src3 & 0x08) << 3) | ((*src2 & 0x08) << 2) | ((*src1 & 0x08) << 1)) >> 4];
			dest[5] = remap[((*src4 & 0x04) << 1) | ((*src3 & 0x04)) | ((*src2 & 0x04) >> 1) | ((*src1 & 0x04) >> 2)];
			dest[6] = remap[(((*src4 & 0x02) << 6) | ((*src3 & 0x02) << 5) | ((*src2 & 0x02) << 4) | ((*src1 & 0x02) << 3)) >> 4];
			dest[7] = remap[((*src4 & 0x01) << 3) | ((*src3 & 0x01) << 2) | ((*src2 & 0x01) << 1) | ((*src1 & 0x01))];
			dest += 8;
			src1 += 1;
			src2 += 1;
			src3 += 1;
			src4 += 1;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FStartupTexture::CreatePalettedPixels(int conversion, int frame)
{
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();
	const uint8_t *remap = ImageHelpers::GetRemap(conversion == luminance);


	TArray<uint8_t> Work(Width*Height, true);
	PalettedPixels Pixels(Width*Height);
	PlanarToChunky(Work.Data(), source + 48, startuppalette8, Width, Height);
	ImageHelpers::FlipNonSquareBlockRemap(Pixels.Data(), Work.Data(), Width, Height, Width, remap);
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FStartupTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();
	PlanarToChunky((uint32_t*)bmp->GetPixels(), source + 48, startuppalette32, Width, Height);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

FNotchTexture::FNotchTexture (int lumpnum, int width, int height)
: FImageSource(lumpnum)
{
	Width = width;
	Height = height;
	bUseGamePalette = false;
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FNotchTexture::CreatePalettedPixels(int conversion, int frame)
{
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();
	const uint8_t *remap = ImageHelpers::GetRemap(conversion == luminance);

	TArray<uint8_t> Work(Width*Height, true);
	PalettedPixels Pixels(Width*Height);
	for(int i=0; i * Width * Height / 2; i++)
	{
		Work[i * 2] = startuppalette8[source[i] >> 4];
		Work[i * 2 + 1] = startuppalette8[source[i] & 15];
	}
	ImageHelpers::FlipNonSquareBlockRemap(Pixels.Data(), Work.Data(), Width, Height, Width, remap);
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FNotchTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();

	auto Work = (uint32_t*)bmp->GetPixels();
	for(int i = 0; i < Width * Height / 2; i++)
	{
		Work[i * 2] = startuppalette32[source[i] >> 4];
		Work[i * 2 + 1] = startuppalette32[source[i] & 15];
	}
	return 0;
}


//==========================================================================
//
//
//
//==========================================================================

FStrifeStartupTexture::FStrifeStartupTexture (int lumpnum, int w, int h)
: FImageSource(lumpnum)
{
	Width = w;
	Height = h;
}

//==========================================================================
//
//
//
//==========================================================================

PalettedPixels FStrifeStartupTexture::CreatePalettedPixels(int conversion, int frame)
{
	auto lump =  fileSystem.ReadFile (SourceLump);
	auto source = lump.bytes();
	PalettedPixels Pixels(Width*Height);
	const uint8_t *remap = ImageHelpers::GetRemap(conversion == luminance);
	ImageHelpers::FlipNonSquareBlockRemap(Pixels.Data(), source, Width, Height, Width, remap);
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

FStrifeStartupBackground::FStrifeStartupBackground (int lumpnum)
: FImageSource(lumpnum)
{
	Width = 320;
	Height = 200;
}

//==========================================================================
//
// this image is very messy but let's prepare it just like Strife does
// so that the screen can be replaced with a whole image.
//
//==========================================================================

PalettedPixels FStrifeStartupBackground::CreatePalettedPixels(int conversion, int frame)
{
	TArray<uint8_t> source(64000, true);
	memset(source.Data(), 0xF0, 64000);
	auto lumpr = fileSystem.OpenFileReader(SourceLump);
	lumpr.Seek(57 * 320, FileReader::SeekSet);
	lumpr.Read(source.Data() + 41 * 320, 95 * 320);

	PalettedPixels Pixels(Width*Height);
	const uint8_t *remap = ImageHelpers::GetRemap(conversion == luminance);
	ImageHelpers::FlipNonSquareBlockRemap(Pixels.Data(), source.Data(), Width, Height, Width, remap);
	return Pixels;
}
