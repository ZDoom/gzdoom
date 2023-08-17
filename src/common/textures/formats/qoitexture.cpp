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

class FQOITexture : public FImageSource
{
public:
	FQOITexture(int lumpnum, QOIHeader& header);
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

        file.Seek(0, FileReader::SeekSet);
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

FQOITexture::FQOITexture(int lumpnum, QOIHeader& header)
	: FImageSource(lumpnum)
{
	LeftOffset = TopOffset = 0;
	Width = header.width;
	Height = header.height;
	if (header.channels == 3) bMasked = bTranslucent = false;
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
	enum
	{
		QOI_OP_INDEX = 0x00, /* 00xxxxxx */
		QOI_OP_DIFF = 0x40, /* 01xxxxxx */
		QOI_OP_LUMA = 0x80, /* 10xxxxxx */
		QOI_OP_RUN = 0xc0, /* 11xxxxxx */
		QOI_OP_RGB = 0xfe, /* 11111110 */
		QOI_OP_RGBA = 0xff, /* 11111111 */

		QOI_MASK_2 = 0xc0, /* 11000000 */
	};

	constexpr auto QOI_COLOR_HASH = [](PalEntry C) { return (C.r * 3 + C.g * 5 + C.b * 7 + C.a * 11); };

	auto lump = fileSystem.OpenFileReader(SourceLump);
	auto bytes = lump.Read();
	if (bytes.Size() < 22) return 0;	// error
	PalEntry index[64] = {};
	PalEntry pe = 0xff000000;

	size_t p = 14, run = 0;

	size_t chunks_len = bytes.Size() - 8;

	for (int h = 0; h < Height; h++)
	{
		auto pixels = bmp->GetPixels() + h * bmp->GetPitch();
		for (int w = 0; w < Width; w++)
		{
			if (run > 0)
			{
				run--;
			}
			else if (p < chunks_len)
			{
				int b1 = bytes[p++];

				if (b1 == QOI_OP_RGB)
				{
					pe.r = bytes[p++];
					pe.g = bytes[p++];
					pe.b = bytes[p++];
				}
				else if (b1 == QOI_OP_RGBA)
				{
					pe.r = bytes[p++];
					pe.g = bytes[p++];
					pe.b = bytes[p++];
					pe.a = bytes[p++];
				}
				else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX)
				{
					pe = index[b1];
				}
				else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF)
				{
					pe.r += ((b1 >> 4) & 0x03) - 2;
					pe.g += ((b1 >> 2) & 0x03) - 2;
					pe.b += (b1 & 0x03) - 2;
				}
				else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA)
				{
					int b2 = bytes[p++];
					int vg = (b1 & 0x3f) - 32;
					pe.r += vg - 8 + ((b2 >> 4) & 0x0f);
					pe.g += vg;
					pe.b += vg - 8 + (b2 & 0x0f);
				}
				else if ((b1 & QOI_MASK_2) == QOI_OP_RUN)
				{
					run = (b1 & 0x3f);
				}

				index[QOI_COLOR_HASH(pe) % 64] = pe;
			}
		}

		pixels[0] = pe.b;
		pixels[1] = pe.g;
		pixels[2] = pe.r;
		pixels[3] = pe.a;
		pixels += 4;
	}
	return bMasked? -1 : 0;
}
