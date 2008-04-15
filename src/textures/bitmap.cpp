/*
** bitmap.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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

#include "bitmap.h"
#include "templates.h"


//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires one of the previously defined conversion classes to work
//
//===========================================================================
template<class T>
void iCopyColors(BYTE *pout, const BYTE *pin, int count, int step)
{
	for(int i=0;i<count;i++)
	{
		pout[0]=T::B(pin);
		pout[1]=T::G(pin);
		pout[2]=T::R(pin);
		pout[3]=T::A(pin);
		pout+=4;
		pin+=step;
	}
}

typedef void (*CopyFunc)(BYTE *pout, const BYTE *pin, int count, int step);

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
// Clips the copy area for CopyPixelData functions
//
//===========================================================================
bool ClipCopyPixelRect(int texwidth, int texheight, int &originx, int &originy,
									const BYTE *&patch, int &srcwidth, int &srcheight, 
									int &pstep_x, int &pstep_y, int rotate)
{
	int pixxoffset;
	int pixyoffset;

	int step_x;
	int step_y;

	// First adjust the settings for the intended rotation
	switch (rotate)
	{
	default:
	case 0:	// normal
		pixxoffset = 0;
		pixyoffset = 0;
		step_x = pstep_x;
		step_y = pstep_y;
		break;

	case 1: // rotate 90° right
		pixxoffset = 0;
		pixyoffset = srcheight - 1;
		step_x = -pstep_y;
		step_y = pstep_x;
		break;

	case 2:	// rotate 180°
		pixxoffset = srcwidth - 1;
		pixyoffset = srcheight - 1;
		step_x = -pstep_x;
		step_y = -pstep_y;
		break;

	case 3: // rotate 90° left
		pixxoffset = srcwidth - 1;
		pixyoffset = 0;
		step_x = pstep_y;
		step_y = -pstep_x;
		break;

	case 4:	// flip horizontally
		pixxoffset = srcwidth - 1;
		pixyoffset = 0;
		step_x = -pstep_x;
		step_y = pstep_y;
		break;

	case 5:	// flip horizontally and rotate 90° right
		pixxoffset = srcwidth - 1;
		pixyoffset = srcheight - 1;
		step_x = -pstep_y;
		step_y = -pstep_x;
		break;

	case 6:	// flip vertically
		pixxoffset = 0;
		pixyoffset = srcheight - 1;
		step_x = pstep_x;
		step_y = -pstep_y;
		break;

	case 7:	// flip horizontally and rotate 90° left
		pixxoffset = 0;
		pixyoffset = 0;
		step_x = pstep_y;
		step_y = pstep_x;
		break;
	}
	if (rotate&1)
	{
		int v = srcwidth;
		srcwidth = srcheight;
		srcheight = v;
	}

	patch += pixxoffset * pstep_x + pixyoffset * pstep_y;
	pstep_x = step_x;
	pstep_y = step_y;

	// clip source rectangle to destination
	if (originx<0)
	{
		srcwidth+=originx;
		patch-=originx*step_x;
		originx=0;
		if (srcwidth<=0) return false;
	}
	if (originx+srcwidth>texwidth)
	{
		srcwidth=texwidth-originx;
		if (srcwidth<=0) return false;
	}
		
	if (originy<0)
	{
		srcheight+=originy;
		patch-=originy*step_y;
		originy=0;
		if (srcheight<=0) return false;
	}
	if (originy+srcheight>texheight)
	{
		srcheight=texheight-originy;
		if (srcheight<=0) return false;
	}
	return true;
}


//===========================================================================
//
// True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
							   int srcheight, int step_x, int step_y, int rotate, int ct)
{
	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4 * originx + Pitch * originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x);
		}
	}
}

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette)
{
	int x,y,pos;
	
	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4*originx + Pitch*originy;

		for (y=0;y<srcheight;y++)
		{
			pos = y*Pitch;
			for (x=0;x<srcwidth;x++,pos+=4)
			{
				int v=(unsigned char)patch[y*step_y+x*step_x];
				if (palette[v].a==0)
				{
					buffer[pos]=palette[v].b;
					buffer[pos+1]=palette[v].g;
					buffer[pos+2]=palette[v].r;
					buffer[pos+3]=255;
				}
				else if (palette[v].a!=255)
				{
					// [RH] Err... This can't be right, can it?
					buffer[pos  ] = (buffer[pos  ] * palette[v].a + palette[v].b * (1-palette[v].a)) / 255;
					buffer[pos+1] = (buffer[pos+1] * palette[v].a + palette[v].g * (1-palette[v].a)) / 255;
					buffer[pos+2] = (buffer[pos+2] * palette[v].a + palette[v].r * (1-palette[v].a)) / 255;
					buffer[pos+3] = clamp<int>(buffer[pos+3] + (( 255-buffer[pos+3]) * (255-palette[v].a))/255, 0, 255);
				}
			}
		}
	}
}

