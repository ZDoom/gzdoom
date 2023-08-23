/*
** webptexture.cpp
** Texture class for WebP images.
**
**---------------------------------------------------------------------------
** Copyright 2023 Cacodemon345
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
#include "webp/decode.h"
#include "webp/mux.h"

#include "files.h"
#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"
#include "printf.h"

class FWebPTexture : public FImageSource
{

public:
	FWebPTexture(int lumpnum, int w, int h, int xoff, int yoff);
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
};


FImageSource *WebPImage_TryCreate(FileReader &file, int lumpnum)
{
	int width = 0, height = 0;
	int xoff = 0, yoff = 0;
	file.Seek(0, FileReader::SeekSet);

	uint8_t header[12];
	if (file.Read(header, 12) != 12) return nullptr;
	if (memcmp(header, "RIFF", 4) || memcmp(header + 8, "WEBP", 4)) return nullptr;

	file.Seek(0, FileReader::SeekSet);
	auto bytes = file.Read();

	if (WebPGetInfo(bytes.data(), bytes.size(), &width, &height))
	{
		WebPData data{ bytes.data(), bytes.size() };
		WebPData chunk_data;
		auto mux = WebPMuxCreate(&data, 0);
		if (mux)
		{
			const char fourcc[4] = { 'g', 'r', 'A', 'b' };
			if (WebPMuxGetChunk(mux, fourcc, &chunk_data) == WEBP_MUX_OK && chunk_data.size >= 4)
			{
				xoff = chunk_data.bytes[0] | (chunk_data.bytes[1] << 8);
				yoff = chunk_data.bytes[2] | (chunk_data.bytes[3] << 8);
			}
			WebPMuxDelete(mux);
		}
		return new FWebPTexture(lumpnum, width, height, xoff, yoff);
	}
	return nullptr;
}

FWebPTexture::FWebPTexture(int lumpnum, int w, int h, int xoff, int yoff)
	: FImageSource(lumpnum)
{
	Width = w;
	Height = h;
	LeftOffset = xoff;
	TopOffset = yoff;
}

PalettedPixels FWebPTexture::CreatePalettedPixels(int conversion, int frame)
{
	FBitmap bitmap;
	bitmap.Create(Width, Height);
	CopyPixels(&bitmap, conversion);
	const uint8_t *data = bitmap.GetPixels();

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	PalettedPixels Pixels(Width*Height);
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

int FWebPTexture::CopyPixels(FBitmap *bmp, int conversion, int frame)
{
	WebPDecoderConfig config;
	auto bytes = fileSystem.ReadFile(SourceLump);

	if (WebPInitDecoderConfig(&config) == false)
		return 0;

	config.options.no_fancy_upsampling = 0;
	config.output.colorspace = MODE_BGRA;
	config.output.u.RGBA.rgba = (uint8_t*)bmp->GetPixels();
	config.output.u.RGBA.size = bmp->GetBufferSize();
	config.output.u.RGBA.stride = bmp->GetPitch();
	config.output.is_external_memory = 1;

	(void)WebPDecode(bytes.GetBytes(), bytes.GetSize(), &config);

	return 0;
}
