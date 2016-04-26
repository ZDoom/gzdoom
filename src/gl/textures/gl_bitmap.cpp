/*
** gl_bitmap.cpp
** Bitmap class for texture composition
**
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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
void iCopyColors(unsigned char * pout, const unsigned char * pin, int count, int step, BYTE tr, BYTE tg, BYTE tb)
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

typedef void (*CopyFunc)(unsigned char * pout, const unsigned char * pin, int count, int step, BYTE tr, BYTE tg, BYTE tb);

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
								const BYTE * patch, int srcwidth, int srcheight, int step_x, int step_y,
								int rotate, int ct, FCopyInfo *inf,	int r, int g, int b)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = GetPixels() + 4*originx + Pitch*originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x, (BYTE)r, (BYTE)g, (BYTE)b);
		}
	}
}

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FGLBitmap::CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	PalEntry penew[256];

	int x,y,pos,i;

	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = GetPixels() + 4*originx + Pitch*originy;

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
