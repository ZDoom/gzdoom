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
#include "gl/renderer/gl_lightdata.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_bitmap.h"

//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires one of the previously defined conversion classes to work
//
//===========================================================================
template<class T>
void iCopyColors(unsigned char * pout, const unsigned char * pin, int cm, int count, int step)
{
	int i;
	int fac;

	if (cm == CM_DEFAULT)
	{
		for(i=0;i<count;i++)
		{
			if (T::A(pin) != 0)
			{
				pout[0]=T::R(pin);
				pout[1]=T::G(pin);
				pout[2]=T::B(pin);
				pout[3]=T::A(pin);
			}
			pout+=4;
			pin+=step;
		}
	}
	else if (cm == CM_SHADE)
	{
		// Alpha shade uses the red channel for true color pics
		for(i=0;i<count;i++)
		{
			if (T::A(pin) != 0)
			{
				pout[0] = pout[1] =	pout[2] = 255;
				pout[3] = T::R(pin);
			}
			pout+=4;
			pin+=step;
		}
	}
	else if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_FIRSTSPECIALCOLORMAP + int(SpecialColormaps.Size()))
	{
		for(i=0;i<count;i++) 
		{
			if (T::A(pin) != 0)
			{
				PalEntry pe = SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP].GrayscaleToColor[T::Gray(pin)];
				pout[0] = pe.r;
				pout[1] = pe.g;
				pout[2] = pe.b;
				pout[3] = T::A(pin);
			}
			pout+=4;
			pin+=step;
		}
	}
	else if (cm<=CM_DESAT31)
	{
		// Desaturated light settings.
		fac=cm-CM_DESAT0;
		for(i=0;i<count;i++)
		{
			if (T::A(pin) != 0)
			{
				gl_Desaturate(T::Gray(pin), T::R(pin), T::G(pin), T::B(pin), pout[0], pout[1], pout[2], fac);
				pout[3] = T::A(pin);
			}
			pout+=4;
			pin+=step;
		}
	}
}

typedef void (*CopyFunc)(unsigned char * pout, const unsigned char * pin, int cm, int count, int step);

static CopyFunc copyfuncs[]={
	iCopyColors<cRGB>,
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
								int rotate, int ct, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = GetPixels() + 4*originx + Pitch*originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], cm, srcwidth, step_x);
		}
	}
}

//===========================================================================
// 
// Creates one of the special palette translations for the given palette
//
//===========================================================================
void ModifyPalette(PalEntry * pout, PalEntry * pin, int cm, int count)
{
	int i;
	int fac;

	if (cm == CM_DEFAULT)
	{
		if (pin != pout)
			memcpy(pout, pin, count * sizeof(PalEntry));
	}
	else if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_FIRSTSPECIALCOLORMAP + int(SpecialColormaps.Size()))
	{
		for(i=0;i<count;i++)
		{
			int gray = (pin[i].r*77 + pin[i].g*143 + pin[i].b*37) >> 8;
			// This can be done in place so we cannot copy the color directly.
			PalEntry pe = SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP].GrayscaleToColor[gray];
			pout[i].r = pe.r;
			pout[i].g = pe.g;
			pout[i].b = pe.b;
			pout[i].a = pin[i].a;
		}
	}
	else if (cm<=CM_DESAT31)
	{
		// Desaturated light settings.
		fac=cm-CM_DESAT0;
		for(i=0;i<count;i++)
		{
			int gray=(pin[i].r*77 + pin[i].g*143 + pin[i].b*36)>>8;
			gl_Desaturate(gray, pin[i].r, pin[i].g, pin[i].b, pout[i].r, pout[i].g, pout[i].b, fac);
			pout[i].a = pin[i].a;
		}
	}
	else if (pin!=pout)
	{
		memcpy(pout, pin, count * sizeof(PalEntry));
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

		// CM_SHADE is an alpha map with 0==transparent and 1==opaque
		if (cm == CM_SHADE) 
		{
			for(int i=0;i<256;i++) 
			{
				if (palette[i].a != 0)
					penew[i]=PalEntry(i, 255,255,255);
				else
					penew[i]=PalEntry(0,255,255,255);	// If the palette contains transparent colors keep them.
			}
		}
		else
		{
			// apply any translation. 
			// The ice and blood color translations are done directly
			// because that yields better results.
			switch(translation)
			{
			default:
			{
				PalEntry *ptrans = GLTranslationPalette::GetPalette(translation);
				if (ptrans)
				{
					for(i = 0; i < 256; i++)
					{
						penew[i] = (ptrans[i]&0xffffff) | (palette[i]&0xff000000);
					}
					break;
				}
			}

			case 0:
				memcpy(penew, palette, 256*sizeof(PalEntry));
				break;
			}
			if (cm!=0)
			{
				// Apply color modifications like invulnerability, desaturation and Boom colormaps
				ModifyPalette(penew, penew, cm, 256);
			}
		}
		// Now penew contains the actual palette that is to be used for creating the image.

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
				/*
				else if (penew[v].a!=255)
				{
					buffer[pos  ] = (buffer[pos  ] * penew[v].a + penew[v].r * (1-penew[v].a)) / 255;
					buffer[pos+1] = (buffer[pos+1] * penew[v].a + penew[v].g * (1-penew[v].a)) / 255;
					buffer[pos+2] = (buffer[pos+2] * penew[v].a + penew[v].b * (1-penew[v].a)) / 255;
					buffer[pos+3] = clamp<int>(buffer[pos+3] + (( 255-buffer[pos+3]) * (255-penew[v].a))/255, 0, 255);
				}
				*/
			}
		}
	}
}
