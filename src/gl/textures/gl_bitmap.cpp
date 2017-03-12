// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_bitmap.cpp
** Bitmap class for texture composition
**
*/


#include "v_palette.h"
#include "templates.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_bitmap.h"
#include "gl/system/gl_interface.h"

//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires one of the previously defined conversion classes to work
//
//===========================================================================
template<class T>
void iCopyColors(unsigned char * pout, const unsigned char * pin, int count, int step, uint8_t tr, uint8_t tg, uint8_t tb)
{
	int i;
	unsigned char a;

	for(i=0;i<count;i++)
	{
		if ((a = T::A(pin, tr, tg, tb)) != 0)
		{
			pout[0]=T::R(pin);
			pout[1]=T::G(pin);
			pout[2]=T::B(pin);
			pout[3]=a;
		}
		pout+=4;
		pin+=step;
	}
}

typedef void (*CopyFunc)(unsigned char * pout, const unsigned char * pin, int count, int step, uint8_t tr, uint8_t tg, uint8_t tb);

static CopyFunc copyfuncs[]={
	iCopyColors<cRGB>,
	iCopyColors<cRGBT>,
	iCopyColors<cRGBA>,
	iCopyColors<cIA>,
	iCopyColors<cCMYK>,
	iCopyColors<cBGR>,
	iCopyColors<cBGRA>,
	iCopyColors<cI16>,
	iCopyColors<cRGB555>,
	iCopyColors<cPalEntry>
};

//===========================================================================
//
// True Color texture copy function
// This excludes all the cases that force downconversion to the
// base palette because they wouldn't be used anyway.
//
//===========================================================================
void FGLBitmap::CopyPixelDataRGB(int originx, int originy,
								const uint8_t * patch, int srcwidth, int srcheight, int step_x, int step_y,
								int rotate, int ct, FCopyInfo *inf,	int r, int g, int b)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		uint8_t *buffer = GetPixels() + 4*originx + Pitch*originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x, (uint8_t)r, (uint8_t)g, (uint8_t)b);
		}
	}
}

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FGLBitmap::CopyPixelData(int originx, int originy, const uint8_t * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	PalEntry penew[256];

	int x,y,pos,i;

	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		uint8_t *buffer = GetPixels() + 4*originx + Pitch*originy;

		if (translation > 0)
		{
			PalEntry *ptrans = GLTranslationPalette::GetPalette(translation);
			if (ptrans && !alphatrans)
			{
				for (i = 0; i < 256; i++)
				{
					penew[i] = (ptrans[i] & 0xffffff) | (palette[i] & 0xff000000);
				}
			}
			else if (ptrans)
			{
				memcpy(penew, ptrans, 256 * sizeof(PalEntry));
			}
		}
		else
		{
			memcpy(penew, palette, 256*sizeof(PalEntry));
		}

		// convert the image according to the translated palette.
		for (y=0;y<srcheight;y++)
		{
			pos=(y*Pitch);
			for (x=0;x<srcwidth;x++,pos+=4)
			{
				int v=(unsigned char)patch[y*step_y+x*step_x];
				if (penew[v].a!=0)
				{
					buffer[pos]   = penew[v].r;
					buffer[pos+1] = penew[v].g;
					buffer[pos+2] = penew[v].b;
					buffer[pos+3] = penew[v].a;
				}
			}
		}
	}
}
