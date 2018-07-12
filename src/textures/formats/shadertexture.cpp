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

#include "doomtype.h"
#include "d_player.h"
#include "menu/menu.h"
#include "w_wad.h"
#include "bitmap.h"


class FBarShader : public FWorldTexture
{
public:
	FBarShader(bool vertical, bool reverse)
	{
		int i;

		Name.Format("BarShader%c%c", vertical ? 'v' : 'h', reverse ? 'r' : 'f');
		Width = vertical ? 2 : 256;
		Height = vertical ? 256 : 2;
		CalcBitSize();
		bMasked = false;
		bTranslucent = false;
		PixelsAreStatic = 2;	// The alpha buffer is static, but if this gets used as a regular texture, a separate buffer needs to be made.

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

	uint8_t *MakeTexture(FRenderStyle style) override
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			return Pixels;
		}
		else
		{
			// Since this presents itself to the game as a regular named texture
			// it can easily be used on walls and flats and should work as such, 
			// even if it makes little sense.
			auto Pix = new uint8_t[512];
			for (int i = 0; i < 512; i++)
			{
				Pix[i] = GrayMap[Pixels[i]];
			}
			return Pix;
		}
	}

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf = NULL) override
	{
		bmp->CopyPixelData(x, y, Pixels, Width, Height, Height, 1, rotate, translationtables[TRANSLATION_Standard][8]->Palette, inf);
		return 0;
	}

	bool UseBasePalette() override
	{
		return false;
	}


private:
	uint8_t Pixels[512];
};


FTexture *CreateShaderTexture(bool vertical, bool reverse)
{
	return new FBarShader(vertical, reverse);	
}
