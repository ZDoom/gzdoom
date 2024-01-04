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
#include "palutil.h"

uint8_t IcePalette[16][3] =
{
	{  10,  8, 18 },
	{  15, 15, 26 },
	{  20, 16, 36 },
	{  30, 26, 46 },
	{  40, 36, 57 },
	{  50, 46, 67 },
	{  59, 57, 78 },
	{  69, 67, 88 },
	{  79, 77, 99 },
	{  89, 87,109 },
	{  99, 97,120 },
	{ 109,107,130 },
	{ 118,118,141 },
	{ 128,128,151 },
	{ 138,138,162 },
	{ 148,148,172 }
};

//===========================================================================
// 
// multi-format pixel copy with colormap application
// requires the previously defined conversion classes to work
//
//===========================================================================
template<class TSrc, class TDest, class TBlend>
void iCopyColors(uint8_t *pout, const uint8_t *pin, int count, int step, FCopyInfo *inf,
	uint8_t tr, uint8_t tg, uint8_t tb)
{
	int i;
	int fac;
	uint8_t r,g,b;
	int a;

	switch(inf? inf->blend : BLEND_NONE)
	{
	case BLEND_NONE:
		for(i=0;i<count;i++)
		{
			a = TSrc::A(pin, tr, tg, tb);
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
			a = TSrc::A(pin, tr, tg, tb);
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
				a = TSrc::A(pin, tr, tg, tb);
				if (TBlend::ProcessAlpha0() || a)
				{
					int gray = std::clamp<int>(TSrc::Gray(pin),0,255);

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
			a = TSrc::A(pin, tr, tg, tb);
			if (TBlend::ProcessAlpha0() || a)
				{
					int gray = TSrc::Gray(pin);
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
			a = TSrc::A(pin, tr, tg, tb);
			if (TBlend::ProcessAlpha0() || a)
			{
				r = (TSrc::R(pin)*inf->blendcolor[0])>>BLENDBITS;
				g = (TSrc::G(pin)*inf->blendcolor[1])>>BLENDBITS;
				b = (TSrc::B(pin)*inf->blendcolor[2])>>BLENDBITS;

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
			a = TSrc::A(pin, tr, tg, tb);
			if (TBlend::ProcessAlpha0() || a)
			{
				r = (TSrc::R(pin)*inf->blendcolor[3] + inf->blendcolor[0]) >> BLENDBITS;
				g = (TSrc::G(pin)*inf->blendcolor[3] + inf->blendcolor[1]) >> BLENDBITS;
				b = (TSrc::B(pin)*inf->blendcolor[3] + inf->blendcolor[2]) >> BLENDBITS;

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

typedef void (*CopyFunc)(uint8_t *pout, const uint8_t *pin, int count, int step, FCopyInfo *inf, uint8_t r, uint8_t g, uint8_t b);

#define COPY_FUNCS(op) \
	{ \
		iCopyColors<cRGB, cBGRA, op>, \
		iCopyColors<cRGBT, cBGRA, op>, \
		iCopyColors<cRGBA, cBGRA, op>, \
		iCopyColors<cIA, cBGRA, op>, \
		iCopyColors<cCMYK, cBGRA, op>, \
		iCopyColors<cYCbCr, cBGRA, op>, \
		iCopyColors<cYCCK, cBGRA, op>, \
		iCopyColors<cBGR, cBGRA, op>, \
		iCopyColors<cBGRA, cBGRA, op>, \
		iCopyColors<cI16, cBGRA, op>, \
		iCopyColors<cRGB555, cBGRA, op>, \
		iCopyColors<cPalEntry, cBGRA, op> \
	}
static const CopyFunc copyfuncs[][12]={
	COPY_FUNCS(bCopy),
	COPY_FUNCS(bBlend),
	COPY_FUNCS(bAdd),
	COPY_FUNCS(bSubtract),
	COPY_FUNCS(bReverseSubtract),
	COPY_FUNCS(bModulate),
	COPY_FUNCS(bCopyAlpha),
	COPY_FUNCS(bCopyNewAlpha),
	COPY_FUNCS(bOverlay),
	COPY_FUNCS(bOverwrite)
};
#undef COPY_FUNCS

//===========================================================================
//
// Clips the copy area for CopyPixelData functions
//
//===========================================================================
bool ClipCopyPixelRect(const FClipRect *cr, int &originx, int &originy,
						const uint8_t *&patch, int &srcwidth, int &srcheight, 
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
		ih -= (y-iy);
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
void FBitmap::CopyPixelDataRGB(int originx, int originy, const uint8_t *patch, int srcwidth, 
							   int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf,
							   int r, int g, int b)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		uint8_t *buffer = data + 4 * originx + Pitch * originy;
		int op = inf==NULL? OP_COPY : inf->op;
		for (int y=0;y<srcheight;y++)
		{
			copyfuncs[op][ct](&buffer[y*Pitch], &patch[y*step_y], srcwidth, step_x, inf, r, g, b);
		}
	}
}

template<class TDest, class TBlend> 
void iCopyPaletted(uint8_t *buffer, const uint8_t * patch, int srcwidth, int srcheight, int Pitch,
					int step_x, int step_y, int rotate, const PalEntry * palette, FCopyInfo *inf)
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

typedef void (*CopyPalettedFunc)(uint8_t *buffer, const uint8_t * patch, int srcwidth, int srcheight, int Pitch,
					int step_x, int step_y, int rotate, const PalEntry * palette, FCopyInfo *inf);


static const CopyPalettedFunc copypalettedfuncs[]=
{
	iCopyPaletted<cBGRA, bCopy>,
	iCopyPaletted<cBGRA, bBlend>,
	iCopyPaletted<cBGRA, bAdd>,
	iCopyPaletted<cBGRA, bSubtract>,
	iCopyPaletted<cBGRA, bReverseSubtract>,
	iCopyPaletted<cBGRA, bModulate>,
	iCopyPaletted<cBGRA, bCopyAlpha>,
	iCopyPaletted<cBGRA, bCopyNewAlpha>,
	iCopyPaletted<cBGRA, bOverlay>,
	iCopyPaletted<cBGRA, bOverwrite>
};

//===========================================================================
//
// Paletted to True Color texture copy function
//
//===========================================================================
void FBitmap::CopyPixelData(int originx, int originy, const uint8_t * patch, int srcwidth, int srcheight, 
										int step_x, int step_y, int rotate, const PalEntry * palette, FCopyInfo *inf)
{
	if (ClipCopyPixelRect(&ClipRect, originx, originy, patch, srcwidth, srcheight, step_x, step_y, rotate))
	{
		uint8_t *buffer = data + 4*originx + Pitch*originy;
		PalEntry penew[256];

		memset(penew, 0, sizeof(penew));
		if (inf)
		{
			if (inf->blend)
			{
				iCopyColors<cPalEntry, cBGRA, bCopy>((uint8_t*)penew, (const uint8_t*)palette, 256, 4, inf, 0, 0, 0);
				palette = penew;
			}
			else if (inf->palette)
			{
				palette = inf->palette;
			}
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
	uint8_t *buffer = data;
	for (int y = ClipRect.y; y < ClipRect.height; ++y)
	{
		memset(buffer + ClipRect.x, 0, ClipRect.width*4);
		buffer += Pitch;
	}
}
