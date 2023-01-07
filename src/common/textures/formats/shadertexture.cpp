/*
** shadertexture.cpp
**
** simple shader gradient textures, used by the status bars.
** 
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
** Copyright 2017 Christoph Oelckers
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
*/

#include "filesystem.h"
#include "bitmap.h"
#include "imagehelpers.h"
#include "image.h"
#include "textures.h"


class FBarShader : public FImageSource
{
public:
	FBarShader(bool vertical, bool reverse)
	{
		int i;

		Width = vertical ? 2 : 256;
		Height = vertical ? 256 : 2;
		bMasked = false;
		bTranslucent = false;

		// Fill the column/row with shading values.
		// Vertical shaders have have minimum alpha at the top
		// and maximum alpha at the bottom, unless flipped by
		// setting reverse to true. Horizontal shaders are just
		// the opposite.
		if (vertical)
		{
			if (!reverse)
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i] = i;
					Pixels[256+i] = i;
				}
			}
			else
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i] = 255 - i;
					Pixels[256+i] = 255 -i;
				}
			}
		}
		else
		{
			if (!reverse)
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i*2] = 255 - i;
					Pixels[i*2+1] = 255 - i;
				}
			}
			else
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i*2] = i;
					Pixels[i*2+1] = i;
				}
			}
		}
	}

	PalettedPixels CreatePalettedPixels(int conversion) override
	{
		PalettedPixels Pix(512);
		if (conversion == luminance)
		{
			memcpy(Pix.Data(), Pixels, 512);
		}
		else
		{
			// Since this presents itself to the game as a regular named texture
			// it can easily be used on walls and flats and should work as such, 
			// even if it makes little sense.
			for (int i = 0; i < 512; i++)
			{
				Pix[i] = GPalette.GrayMap[Pixels[i]];
			}
		}
		return Pix;
	}

	int CopyPixels(FBitmap *bmp, int conversion) override
	{
		bmp->CopyPixelData(0, 0, Pixels, Width, Height, Height, 1, 0, GPalette.GrayRamp.Palette);
		return 0;
	}

private:
	uint8_t Pixels[512];
};


FGameTexture *CreateShaderTexture(bool vertical, bool reverse)
{
	FStringf name("BarShader%c%c", vertical ? 'v' : 'h', reverse ? 'r' : 'f');
	return MakeGameTexture(CreateImageTexture(new FBarShader(vertical, reverse)), name.GetChars(), ETextureType::Override);

}
