/*
** qoitexture.cpp
** Texture class for QOI (Quite OK Image Format) images
**
**---------------------------------------------------------------------------
** Copyright 2023 Cacodemon345
** Copyright 2022 Dominic Szablewski
** All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**---------------------------------------------------------------------------
**
**
*/

#include "files.h"
#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"

#define QOI_OP_INDEX 0x00 /* 00xxxxxx */
#define QOI_OP_DIFF 0x40  /* 01xxxxxx */
#define QOI_OP_LUMA 0x80  /* 10xxxxxx */
#define QOI_OP_RUN 0xc0	  /* 11xxxxxx */
#define QOI_OP_RGB 0xfe	  /* 11111110 */
#define QOI_OP_RGBA 0xff  /* 11111111 */

#define QOI_MASK_2 0xc0 /* 11000000 */

#define QOI_COLOR_HASH(C) (C.rgba.r * 3 + C.rgba.g * 5 + C.rgba.b * 7 + C.rgba.a * 11)

#pragma pack(1)

struct QOIHeader
{
	char magic[4];
	uint32_t width;
	uint32_t height;
	uint8_t channels;
	uint8_t colorspace;
};
#pragma pack()

typedef union
{
	struct
	{
		unsigned char r, g, b, a;
	} rgba;
	unsigned int v;
} QOIRgba;

class FQOITexture : public FImageSource
{
	QOIHeader header;

public:
	FQOITexture(int lumpnum, QOIHeader header);
	PalettedPixels CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap *bmp, int conversion) override;
};

FImageSource *QOIImage_TryCreate(FileReader &file, int lumpnum)
{
	QOIHeader header;

	if (file.GetLength() < (sizeof(header) + 8))
	{
		return nullptr;
	}

	file.Read((void *)&header, sizeof(header));

	if (header.magic[0] != 'q' || header.magic[1] != 'o' || header.magic[2] != 'i' || header.magic[3] != 'f')
	{
		return nullptr;
	}

	if (header.width == 0 || header.height == 0 || header.channels < 3 || header.channels > 4 || header.colorspace > 1)
	{
		return nullptr;
	}

	return new FQOITexture(lumpnum, header);
}

FQOITexture::FQOITexture(int lumpnum, QOIHeader qoi_header)
	: FImageSource(lumpnum), header(qoi_header)
{
	LeftOffset = TopOffset = 0;
	Width = header.width;
	Height = header.height;
}

PalettedPixels FQOITexture::CreatePalettedPixels(int conversion)
{
	FBitmap bitmap;
	bitmap.Create(Width, Height);
	CopyPixels(&bitmap, conversion);
	const uint8_t *data = bitmap.GetPixels();

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	PalettedPixels Pixels(Width * Height);
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
			if (a < 128)
				*dest_p = 0;
			else
				*dest_p = ImageHelpers::RGBToPalette(doalpha, r, g, b);
			dest_p += dest_adv;
		}
		dest_p -= dest_rew;
	}
	return Pixels;
}

int FQOITexture::CopyPixels(FBitmap *bmp, int conversion)
{
	QOIRgba index[64];
	QOIRgba px;
	size_t px_len = 0, chunks_len = 0, px_pos = 0;
	int run = 0;
	auto file = fileSystem.OpenFileReader(SourceLump);

	memset((void *)index, 0x00, sizeof(index));

	uint8_t *Pixels = new uint8_t[Width * Height * 4];

	px.rgba.r = 0;
	px.rgba.g = 0;
	px.rgba.b = 0;
	px.rgba.a = 255;

	chunks_len = file.GetLength() - 8;

	for (px_pos = 0; px_pos < px_len; px_pos += 4)
	{
		if (run > 0)
		{
			run--;
		}
		else if (file.Tell() < (FileReader::Size)chunks_len)
		{
			int b1 = file.ReadUInt8();

			if (b1 == QOI_OP_RGB || b1 == QOI_OP_RGBA)
			{
				px.rgba.r = file.ReadUInt8();
				px.rgba.g = file.ReadUInt8();
				px.rgba.b = file.ReadUInt8();
				if (b1 == QOI_OP_RGBA)
					px.rgba.a = file.ReadUInt8();
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX)
			{
				px = index[b1];
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF)
			{
				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += (b1 & 0x03) - 2;
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA)
			{
				int b2 = file.ReadUInt8();
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 + (b2 & 0x0f);
			}
			else if ((b1 & QOI_MASK_2) == QOI_OP_RUN)
			{
				run = (b1 & 0x3f);
			}
			index[QOI_COLOR_HASH(px) % 64] = px;
		}

		Pixels[px_pos + 0] = px.rgba.r;
		Pixels[px_pos + 1] = px.rgba.g;
		Pixels[px_pos + 2] = px.rgba.b;
		Pixels[px_pos + 3] = px.rgba.a;
	}

	bmp->CopyPixelDataRGB(0, 0, Pixels, Width, Height, 4, Width * 4, 0, CF_RGBA);
	delete[] Pixels;

	return -1;
}