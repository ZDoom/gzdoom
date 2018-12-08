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


FWarpTexture::FWarpTexture (FTexture *source, int warptype)
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
	if (time != GenTime[2])
	{
		auto otherpix = FSoftwareTexture::GetPixelsBgra();
		WarpedPixelsRgba.Resize(GetWidth() * GetHeight());
		WarpBuffer(WarpedPixelsRgba.Data(), otherpix, GetWidth(), GetHeight(), WidthOffsetMultiplier, HeightOffsetMultiplier, time, mTexture->shaderspeed, bWarped);
		FreeAllSpans();
		GenTime[2] = time;
	}
	return WarpedPixelsRgba.Data();
}


const uint8_t *FWarpTexture::GetPixels(int index)
{
	uint64_t time = screen->FrameTime;
	if (time != GenTime[index])
	{
		const uint8_t *otherpix = FSoftwareTexture::GetPixels(index);
		WarpedPixels[index].Resize(GetWidth() * GetHeight());
		WarpBuffer(WarpedPixels[index].Data(), otherpix, GetWidth(), GetHeight(), WidthOffsetMultiplier, HeightOffsetMultiplier, time, mTexture->shaderspeed, bWarped);
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
