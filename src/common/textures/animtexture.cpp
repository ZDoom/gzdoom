/*
** animtexture.cpp
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
#include "animtexture.h"
#include "bitmap.h"
#include "texturemanager.h"


//==========================================================================
//
//
//
//==========================================================================

void AnimTexture::SetFrameSize(int  format, int width, int height)
{
	pixelformat = format;
	FTexture::SetSize(width, height);
	Image.Resize(width * height * (format == Paletted ? 1 : 3));
	memset(Image.Data(), 0, Image.Size());
}

void AnimTexture::SetFrame(const uint8_t* palette, const void* data_)
{
	if (palette) memcpy(Palette, palette, 768);
	if (data_)
	{
		if (pixelformat == YUV)
		{
			auto spix = (const uint8_t*)data_;
			auto dpix = Image.Data();
			for (int i = 0; i < Width * Height; i++)
			 {
				int p = i * 4;
				int q = i * 3;
				float y = spix[p] * (1 / 255.f);
				float u = spix[p + 1] * (1 / 255.f) - 0.5f;
				float v = spix[p + 2] * (1 / 255.f) - 0.5f;

				y = 1.1643f * (y - 0.0625f);

				float r = y + 1.5958f * v;
				float g = y - 0.39173f * u - 0.81290f * v;
				float b = y + 2.017f * u;

				dpix[q + 0] = (uint8_t)(clamp(r, 0.f, 1.f) * 255);
				dpix[q + 1] = (uint8_t)(clamp(g, 0.f, 1.f) * 255);
				dpix[q + 2] = (uint8_t)(clamp(b, 0.f, 1.f) * 255);
			}
		}
		else memcpy(Image.Data(), data_, Width * Height * (pixelformat == Paletted ? 1 : 3));
	}
}

//===========================================================================
//
// FPNGTexture::CopyPixels
//
//===========================================================================

FBitmap AnimTexture::GetBgraBitmap(const PalEntry* remap, int* trans)
{
	FBitmap bmp;

	bmp.Create(Width, Height);

	auto spix = Image.Data();
	auto dpix = bmp.GetPixels();
	if (pixelformat == Paletted)
	{
		for (int i = 0; i < Width * Height; i++)
		{
			int p = i * 4;
			int index = spix[i];
			dpix[p + 0] = Palette[index * 3 + 2];
			dpix[p + 1] = Palette[index * 3 + 1];
			dpix[p + 2] = Palette[index * 3];
			dpix[p + 3] = 255;
		}
	}
	else if (pixelformat == RGB || pixelformat == YUV)
	{
		for (int i = 0; i < Width * Height; i++)
		{
			int p = i * 4;
			int q = i * 3;
			dpix[p + 0] = spix[q + 2];
			dpix[p + 1] = spix[q + 1];
			dpix[p + 2] = spix[q];
			dpix[p + 3] = 255;
		}
	}
	return bmp;
}

//==========================================================================
//
//
//
//==========================================================================

AnimTextures::AnimTextures()
{
	active = 1;
	tex[0] = TexMan.FindGameTexture("AnimTextureFrame1", ETextureType::Override);
	tex[1] = TexMan.FindGameTexture("AnimTextureFrame2", ETextureType::Override);
}

AnimTextures::~AnimTextures()
{
	Clean();
}

void AnimTextures::Clean()
{
	if (tex[0]) tex[0]->CleanHardwareData(true);
	if (tex[1]) tex[1]->CleanHardwareData(true);
	tex[0] = tex[1] = nullptr;
}

void AnimTextures::SetSize(int format, int width, int height)
{
	static_cast<AnimTexture*>(tex[0]->GetTexture())->SetFrameSize(format, width, height);
	static_cast<AnimTexture*>(tex[1]->GetTexture())->SetFrameSize(format, width, height);
	tex[0]->SetSize(width, height);
	tex[1]->SetSize(width, height);
	tex[0]->CleanHardwareData();
	tex[1]->CleanHardwareData();
}

void AnimTextures::SetFrame(const uint8_t* palette, const void* data)
{
	active ^= 1;
	static_cast<AnimTexture*>(tex[active]->GetTexture())->SetFrame(palette, data);
	tex[active]->CleanHardwareData();
}
