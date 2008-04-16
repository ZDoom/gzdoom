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
#include "r_translate.h"


//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires the previously defined conversion classes to work
//
//===========================================================================
template<class TSrc, class TDest, class TBlend>
void iCopyColors(BYTE *pout, const BYTE *pin, int count, int step, FCopyInfo *inf)
{
	int i;
	int fac;
	BYTE r,g,b;
	int gray;
	int a;

	switch(inf? inf->blend : BLEND_NONE)
	{
	case BLEND_NONE:
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				TBlend::OpC(pout[TDest::RED], TSrc::R(pin), a, inf);
				TBlend::OpC(pout[TDest::GREEN], TSrc::G(pin), a, inf);
				TBlend::OpC(pout[TDest::BLUE], TSrc::B(pin), a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_INVERSEMAP:
		// Doom's inverted invulnerability map
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				gray = clamp<int>(255 - TSrc::Gray(pin),0,255);

				TBlend::OpC(pout[TDest::RED], gray, a, inf);
				TBlend::OpC(pout[TDest::GREEN], gray, a, inf);
				TBlend::OpC(pout[TDest::BLUE], gray, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_GOLDMAP:
		// Heretic's golden invulnerability map
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				gray = TSrc::Gray(pin);
				r=clamp<int>(gray+(gray>>1),0,255);
				g=clamp<int>(gray-(gray>>2),0,255);

				TBlend::OpC(pout[TDest::RED], r, a, inf);
				TBlend::OpC(pout[TDest::GREEN], g, a, inf);
				TBlend::OpC(pout[TDest::BLUE], 0, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_REDMAP:
		// Skulltag's red Doomsphere map
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				gray = TSrc::Gray(pin);
				r=clamp<int>(gray+(gray>>1),0,255);

				TBlend::OpC(pout[TDest::RED], r, a, inf);
				TBlend::OpC(pout[TDest::GREEN], 0, a, inf);
				TBlend::OpC(pout[TDest::BLUE], 0, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_GREENMAP:
		// Skulltags's Guardsphere map
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				gray = TSrc::Gray(pin);
				r=clamp<int>(gray+(gray>>1),0,255);

				TBlend::OpC(pout[TDest::RED], r, a, inf);
				TBlend::OpC(pout[TDest::GREEN], r, a, inf);
				TBlend::OpC(pout[TDest::BLUE], gray, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_ICEMAP:
		// Create the ice translation table, based on Hexen's.
		// Since this is done in True Color the purplish tint is fully preserved - even in Doom!
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				int gray = TSrc::Gray(pin)>>4;
	
				TBlend::OpC(pout[TDest::RED],   IcePalette[gray][0], a, inf);
				TBlend::OpC(pout[TDest::GREEN], IcePalette[gray][1], a, inf);
				TBlend::OpC(pout[TDest::BLUE],  IcePalette[gray][2], a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	default:
		if (inf->blend >= BLEND_DESATURATE1 && inf->blend<=BLEND_DESATURATE31)
		{
			// Desaturated light settings.
			fac=inf->blend-BLEND_DESATURATE1+1;
			for(i=0;i<count;i++)
			{
				if ((a = TSrc::A(pin)))
				{
					gray = TSrc::Gray(pin);
					r = (TSrc::R(pin)*(31-fac) + gray*fac)/31;
					g = (TSrc::G(pin)*(31-fac) + gray*fac)/31;
					b = (TSrc::B(pin)*(31-fac) + gray*fac)/31;

					TBlend::OpC(pout[TDest::RED],   r, a, inf);
					TBlend::OpC(pout[TDest::GREEN], g, a, inf);
					TBlend::OpC(pout[TDest::BLUE],  b, a, inf);
					TBlend::OpA(pout[TDest::ALPHA], a, inf);
				}
				pout+=4;
				pin+=step;
			}
		}
		break;

	case BLEND_MODULATE:
		for(i=0;i<count;i++)
		{
			if ((a = TSrc::A(pin)))
			{
				r = (TSrc::R(pin)*inf->blendcolor[0])>>FRACBITS;
				g = (TSrc::G(pin)*inf->blendcolor[1])>>FRACBITS;
				b = (TSrc::B(pin)*inf->blendcolor[2])>>FRACBITS;

				TBlend::OpC(pout[TDest::RED],   r, a, inf);
				TBlend::OpC(pout[TDest::GREEN], g, a, inf);
				TBlend::OpC(pout[TDest::BLUE],  b, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	case BLEND_OVERLAY:
		for(i=0;i<count;i++)
		{
			// color blend
			if ((a = TSrc::A(pin)))
			{
				r = (TSrc::R(pin)*inf->blendcolor[3] + inf->blendcolor[0]) >> FRACBITS;
				g = (TSrc::G(pin)*inf->blendcolor[3] + inf->blendcolor[1]) >> FRACBITS;
				b = (TSrc::B(pin)*inf->blendcolor[3] + inf->blendcolor[2]) >> FRACBITS;

				TBlend::OpC(pout[TDest::RED],   r, a, inf);
				TBlend::OpC(pout[TDest::GREEN], g, a, inf);
				TBlend::OpC(pout[TDest::BLUE],  b, a, inf);
				TBlend::OpA(pout[TDest::ALPHA], a, inf);
			}
			pout+=4;
			pin+=step;
		}
		break;

	}
}

typedef void (*CopyFunc)(BYTE *pout, const BYTE *pin, int count, int step, FCopyInfo *inf);

static CopyFunc copyfuncs[9]=
	{
		iCopyColors<cRGB, cBGRA, bCopy>,
		iCopyColors<cRGBA, cBGRA, bCopy>,
		iCopyColors<cIA, cBGRA, bCopy>,
		iCopyColors<cCMYK, cBGRA, bCopy>,
		iCopyColors<cBGR, cBGRA, bCopy>,
		iCopyColors<cBGRA, cBGRA, bCopy>,
		iCopyColors<cI16, cBGRA, bCopy>,
		iCopyColors<cRGB555, cBGRA, bCopy>,
		iCopyColors<cPalEntry, cBGRA, bCopy>
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
							   int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4 * originx + Pitch * originy;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x, inf);
		}
	}
}

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	int x,y,pos;
	
	if (ClipCopyPixelRect(Width, Height, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4*originx + Pitch*originy;
		PalEntry penew[256];

		if (inf && inf->blend)
		{
			// The palette's alpha is inverted so in order to use the
			// True Color copy functions it has to be inverted temporarily.
			memcpy(penew, palette, 4*256);
			for(int i=0;i<256;i++) penew[i].a = 255-penew[i].a;
			copyfuncs[CF_PalEntry]((BYTE*)penew, (BYTE*)penew, 256, 4, inf);
			for(int i=0;i<256;i++) penew[i].a = 255-penew[i].a;
			palette = penew;
		}


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

