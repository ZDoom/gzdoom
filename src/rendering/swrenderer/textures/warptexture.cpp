/*
** warptexture.cpp
** Texture class for warped textures
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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
#include "r_utility.h"
#include "r_swtexture.h"
#include "warpbuffer.h"
#include "v_video.h"

EXTERN_CVAR(Int, gl_texture_hqresizemult)
EXTERN_CVAR(Int, gl_texture_hqresizemode)
EXTERN_CVAR(Int, gl_texture_hqresize_targets)

FWarpTexture::FWarpTexture (FGameTexture *source, int warptype)
	: FSoftwareTexture (source)
{
	if (warptype == 2) SetupMultipliers(256, 128); 
	SetupMultipliers(128, 128); // [mxd]
	bWarped = warptype;
}

bool FWarpTexture::CheckModified (int style)
{
	return screen->FrameTime != GenTime[style];
}

const uint32_t *FWarpTexture::GetPixelsBgra()
{
	uint64_t time = screen->FrameTime;
	uint64_t resizeMult = gl_texture_hqresizemult;

	if (time != GenTime[2])
	{
		if (gl_texture_hqresizemode == 0 || gl_texture_hqresizemult < 1 || !(gl_texture_hqresize_targets & 1))
			resizeMult = 1;

		auto otherpix = FSoftwareTexture::GetPixelsBgra();
		WarpedPixelsRgba.Resize(unsigned(GetWidth() * GetHeight() * resizeMult * resizeMult * 4 / 3 + 1));
		WarpBuffer(WarpedPixelsRgba.Data(), otherpix, int(GetWidth() * resizeMult), int(GetHeight() * resizeMult), WidthOffsetMultiplier, HeightOffsetMultiplier, time, mTexture->GetShaderSpeed(), bWarped);
		GenerateBgraMipmapsFast();
		FreeAllSpans();
		GenTime[2] = time;
	}
	return WarpedPixelsRgba.Data();
}


const uint8_t *FWarpTexture::GetPixels(int index)
{
	uint64_t time = screen->FrameTime;
	uint64_t resizeMult = gl_texture_hqresizemult;

	if (time != GenTime[index])
	{
		if (gl_texture_hqresizemode == 0 || gl_texture_hqresizemult < 1 || !(gl_texture_hqresize_targets & 1))
			resizeMult = 1;

		const uint8_t *otherpix = FSoftwareTexture::GetPixels(index);
		WarpedPixels[index].Resize(unsigned(GetWidth() * GetHeight() * resizeMult * resizeMult));
		WarpBuffer(WarpedPixels[index].Data(), otherpix, int(GetWidth() * resizeMult), int(GetHeight() * resizeMult), WidthOffsetMultiplier, HeightOffsetMultiplier, time, mTexture->GetShaderSpeed(), bWarped);
		FreeAllSpans();
		GenTime[index] = time;
	}
	return WarpedPixels[index].Data();
}

// [mxd] Non power of 2 textures need different offset multipliers, otherwise warp animation won't sync across texture
void FWarpTexture::SetupMultipliers (int width, int height)
{
	WidthOffsetMultiplier = width;
	HeightOffsetMultiplier = height;
	int widthpo2 = NextPo2(GetWidth());
	int heightpo2 = NextPo2(GetHeight());
	if(widthpo2 != GetWidth()) WidthOffsetMultiplier = (int)(WidthOffsetMultiplier * ((float)widthpo2 / GetWidth()));
	if(heightpo2 != GetHeight()) HeightOffsetMultiplier = (int)(HeightOffsetMultiplier * ((float)heightpo2 / GetHeight()));
}

int FWarpTexture::NextPo2 (int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return ++v;
}

void FWarpTexture::GenerateBgraMipmapsFast()
{
	uint32_t *src = WarpedPixelsRgba.Data();
	uint32_t *dest = src + GetPhysicalWidth() * GetPhysicalHeight();
	int levels = MipmapLevels();
	for (int i = 1; i < levels; i++)
	{
		int srcw = MAX(GetPhysicalWidth() >> (i - 1), 1);
		int srch = MAX(GetPhysicalHeight() >> (i - 1), 1);
		int w = MAX(GetPhysicalWidth() >> i, 1);
		int h = MAX(GetPhysicalHeight() >> i, 1);

		for (int x = 0; x < w; x++)
		{
			int sx0 = x * 2;
			int sx1 = MIN((x + 1) * 2, srcw - 1);

			for (int y = 0; y < h; y++)
			{
				int sy0 = y * 2;
				int sy1 = MIN((y + 1) * 2, srch - 1);

				uint32_t src00 = src[sy0 + sx0 * srch];
				uint32_t src01 = src[sy1 + sx0 * srch];
				uint32_t src10 = src[sy0 + sx1 * srch];
				uint32_t src11 = src[sy1 + sx1 * srch];

				uint32_t alpha = (APART(src00) + APART(src01) + APART(src10) + APART(src11) + 2) / 4;
				uint32_t red = (RPART(src00) + RPART(src01) + RPART(src10) + RPART(src11) + 2) / 4;
				uint32_t green = (GPART(src00) + GPART(src01) + GPART(src10) + GPART(src11) + 2) / 4;
				uint32_t blue = (BPART(src00) + BPART(src01) + BPART(src10) + BPART(src11) + 2) / 4;

				dest[y + x * h] = (alpha << 24) | (red << 16) | (green << 8) | blue;
			}
		}

		src = dest;
		dest += w * h;
	}
}
