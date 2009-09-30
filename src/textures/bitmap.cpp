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
#include "v_palette.h"


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
			a = TSrc::A(pin);
			if (TBlend::ProcessAlpha0() || a)
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

	case BLEND_ICEMAP:
		// Create the ice translation table, based on Hexen's.
		// Since this is done in True Color the purplish tint is fully preserved - even in Doom!
		for(i=0;i<count;i++)
		{
			a = TSrc::A(pin);
			if (TBlend::ProcessAlpha0() || a)
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

		if (inf->blend >= BLEND_SPECIALCOLORMAP1)
		{
			FSpecialColormap *cm = &SpecialColormaps[inf->blend - BLEND_SPECIALCOLORMAP1];
			for(i=0;i<count;i++)
			{
				a = TSrc::A(pin);
				if (TBlend::ProcessAlpha0() || a)
				{
					gray = clamp<int>(255 - TSrc::Gray(pin),0,255);

					PalEntry pe = cm->GrayscaleToColor[gray];
					TBlend::OpC(pout[TDest::RED], pe.r , a, inf);
					TBlend::OpC(pout[TDest::GREEN], pe.g, a, inf);
					TBlend::OpC(pout[TDest::BLUE], pe.b, a, inf);
					TBlend::OpA(pout[TDest::ALPHA], a, inf);
				}
				pout+=4;
				pin+=step;
			}
		}
		else if (inf->blend >= BLEND_DESATURATE1 && inf->blend<=BLEND_DESATURATE31)
		{
			// Desaturated light settings.
			fac=inf->blend-BLEND_DESATURATE1+1;
			for(i=0;i<count;i++)
			{
			a = TSrc::A(pin);
			if (TBlend::ProcessAlpha0() || a)
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
			a = TSrc::A(pin);
			if (TBlend::ProcessAlpha0() || a)
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
			a = TSrc::A(pin);
			if (TBlend::ProcessAlpha0() || a)
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

static const CopyFunc copyfuncs[][9]={
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
	},
	{
		iCopyColors<cRGB, cBGRA, bBlend>,
		iCopyColors<cRGBA, cBGRA, bBlend>,
		iCopyColors<cIA, cBGRA, bBlend>,
		iCopyColors<cCMYK, cBGRA, bBlend>,
		iCopyColors<cBGR, cBGRA, bBlend>,
		iCopyColors<cBGRA, cBGRA, bBlend>,
		iCopyColors<cI16, cBGRA, bBlend>,
		iCopyColors<cRGB555, cBGRA, bBlend>,
		iCopyColors<cPalEntry, cBGRA, bBlend>
	},
	{
		iCopyColors<cRGB, cBGRA, bAdd>,
		iCopyColors<cRGBA, cBGRA, bAdd>,
		iCopyColors<cIA, cBGRA, bAdd>,
		iCopyColors<cCMYK, cBGRA, bAdd>,
		iCopyColors<cBGR, cBGRA, bAdd>,
		iCopyColors<cBGRA, cBGRA, bAdd>,
		iCopyColors<cI16, cBGRA, bAdd>,
		iCopyColors<cRGB555, cBGRA, bAdd>,
		iCopyColors<cPalEntry, cBGRA, bAdd>
	},
	{
		iCopyColors<cRGB, cBGRA, bSubtract>,
		iCopyColors<cRGBA, cBGRA, bSubtract>,
		iCopyColors<cIA, cBGRA, bSubtract>,
		iCopyColors<cCMYK, cBGRA, bSubtract>,
		iCopyColors<cBGR, cBGRA, bSubtract>,
		iCopyColors<cBGRA, cBGRA, bSubtract>,
		iCopyColors<cI16, cBGRA, bSubtract>,
		iCopyColors<cRGB555, cBGRA, bSubtract>,
		iCopyColors<cPalEntry, cBGRA, bSubtract>
	},
	{
		iCopyColors<cRGB, cBGRA, bReverseSubtract>,
		iCopyColors<cRGBA, cBGRA, bReverseSubtract>,
		iCopyColors<cIA, cBGRA, bReverseSubtract>,
		iCopyColors<cCMYK, cBGRA, bReverseSubtract>,
		iCopyColors<cBGR, cBGRA, bReverseSubtract>,
		iCopyColors<cBGRA, cBGRA, bReverseSubtract>,
		iCopyColors<cI16, cBGRA, bReverseSubtract>,
		iCopyColors<cRGB555, cBGRA, bReverseSubtract>,
		iCopyColors<cPalEntry, cBGRA, bReverseSubtract>
	},
	{
		iCopyColors<cRGB, cBGRA, bModulate>,
		iCopyColors<cRGBA, cBGRA, bModulate>,
		iCopyColors<cIA, cBGRA, bModulate>,
		iCopyColors<cCMYK, cBGRA, bModulate>,
		iCopyColors<cBGR, cBGRA, bModulate>,
		iCopyColors<cBGRA, cBGRA, bModulate>,
		iCopyColors<cI16, cBGRA, bModulate>,
		iCopyColors<cRGB555, cBGRA, bModulate>,
		iCopyColors<cPalEntry, cBGRA, bModulate>
	},
	{
		iCopyColors<cRGB, cBGRA, bCopyAlpha>,
		iCopyColors<cRGBA, cBGRA, bCopyAlpha>,
		iCopyColors<cIA, cBGRA, bCopyAlpha>,
		iCopyColors<cCMYK, cBGRA, bCopyAlpha>,
		iCopyColors<cBGR, cBGRA, bCopyAlpha>,
		iCopyColors<cBGRA, cBGRA, bCopyAlpha>,
		iCopyColors<cI16, cBGRA, bCopyAlpha>,
		iCopyColors<cRGB555, cBGRA, bCopyAlpha>,
		iCopyColors<cPalEntry, cBGRA, bCopyAlpha>
	},
	{
		iCopyColors<cRGB, cBGRA, bOverwrite>,
		iCopyColors<cRGBA, cBGRA, bOverwrite>,
		iCopyColors<cIA, cBGRA, bOverwrite>,
		iCopyColors<cCMYK, cBGRA, bOverwrite>,
		iCopyColors<cBGR, cBGRA, bOverwrite>,
		iCopyColors<cBGRA, cBGRA, bOverwrite>,
		iCopyColors<cI16, cBGRA, bOverwrite>,
		iCopyColors<cRGB555, cBGRA, bOverwrite>,
		iCopyColors<cPalEntry, cBGRA, bOverwrite>
	},
};

//===========================================================================
//
// Clips the copy area for CopyPixelData functions
//
//===========================================================================
bool ClipCopyPixelRect(const FClipRect *cr, int &originx, int &originy,
						const BYTE *&patch, int &srcwidth, int &srcheight, 
						int &pstep_x, int &pstep_y, int rotate)
{
	int pixxoffset;
	int pixyoffset;

	int step_x;
	int step_y;

	assert(cr != NULL);
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
	if (originx < cr->x)
	{
		int skip = cr->x - originx;

		srcwidth -= skip;
		patch +=skip * step_x;
		originx = cr->x;
		if (srcwidth<=0) return false;
	}
	if (originx + srcwidth > cr->x + cr->width)
	{
		srcwidth = cr->x + cr->width - originx;
		if (srcwidth<=0) return false;
	}
		
	if (originy < cr->y)
	{
		int skip = cr->y - originy;

		srcheight -= skip;
		patch += skip*step_y;
		originy = cr->y;
		if (srcheight <= 0) return false;
	}
	if (originy + srcheight > cr->y + cr->height)
	{
		srcheight = cr->y + cr->height - originy;
		if (srcheight <= 0) return false;
	}

	return true;
}


//===========================================================================
//
// 
//
//===========================================================================

bool FClipRect::Intersect(int ix, int iy, int iw, int ih)
{
	if (ix > x)
	{
		width -= (ix-x);
		x = ix;
	}
	else
	{
		iw -= (x-ix);
	}
	if (iy > y)
	{
		height -= (iy-y);
		y = iy;
	}
	else
	{
		ih -= (x-ih);
	}
	if (iw < width) width = iw;
	if (ih < height) height = ih;
	return width > 0 && height > 0;
}

//===========================================================================
//
// True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
							   int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4 * originx + Pitch * originy;
		int op = inf==NULL? OP_COPY : inf->op;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[op][ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x, inf);
		}
	}
}


template<class TDest, class TBlend> 
void iCopyPaletted(BYTE *buffer, const BYTE * patch, int srcwidth, int srcheight, int Pitch,
					int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	int x,y,pos;

	for (y=0;y<srcheight;y++)
	{
		pos = y*Pitch;
		for (x=0;x<srcwidth;x++,pos+=4)
		{
			int v=(unsigned char)patch[y*step_y+x*step_x];
			int a = palette[v].a;

			if (TBlend::ProcessAlpha0() || a)
			{
				TBlend::OpC(buffer[pos + TDest::RED], palette[v].r, a, inf);
				TBlend::OpC(buffer[pos + TDest::GREEN], palette[v].g, a, inf);
				TBlend::OpC(buffer[pos + TDest::BLUE], palette[v].b, a, inf);
				TBlend::OpA(buffer[pos + TDest::ALPHA], a, inf);
			}
		}
	}
}

typedef void (*CopyPalettedFunc)(BYTE *buffer, const BYTE * patch, int srcwidth, int srcheight, int Pitch,
					int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf);


static const CopyPalettedFunc copypalettedfuncs[]=
{
	iCopyPaletted<cBGRA, bCopy>,
	iCopyPaletted<cBGRA, bBlend>,
	iCopyPaletted<cBGRA, bAdd>,
	iCopyPaletted<cBGRA, bSubtract>,
	iCopyPaletted<cBGRA, bReverseSubtract>,
	iCopyPaletted<cBGRA, bModulate>,
	iCopyPaletted<cBGRA, bCopyAlpha>,
	iCopyPaletted<cBGRA, bOverwrite>
};

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		BYTE *buffer = data + 4*originx + Pitch*originy;
		PalEntry penew[256];

		memset(penew, 0, sizeof(penew));
		if (inf && inf->blend)
		{
			iCopyColors<cPalEntry, cBGRA, bCopy>((BYTE*)penew, (const BYTE*)palette, 256, 4, inf);
			palette = penew;
		}

		copypalettedfuncs[inf==NULL? OP_COPY : inf->op](buffer, patch, srcwidth, srcheight, Pitch, 
														step_x, step_y, rotate, palette, inf);
	}
}

//===========================================================================
//
// Clear buffer
//
//===========================================================================
void FBitmap::Zero()
{
	BYTE *buffer = data;
	for (int y = ClipRect.y; y < ClipRect.height; ++y)
	{
		memset(buffer + ClipRect.x, 0, ClipRect.width*4);
		buffer += Pitch;
	}
}
