/*
** v_draw.cpp
** Draw patches and blocks to a canvas
**
**---------------------------------------------------------------------------
** Copyright 1998-2003 Randy Heit
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

#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"
#include "v_video.h"
#include "m_swap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_things.h"

#include "i_system.h"
#include "i_video.h"
#include "templates.h"

// [RH] Stretch values to make a 320x200 image best fit the screen
// without using fractional steppings
int CleanXfac, CleanYfac;

// [RH] Effective screen sizes that the above scale values give you
int CleanWidth, CleanHeight;

fixed_t V_Fade = 0x8000;

// The current set of column drawers (set in V_SetResolution)
DCanvas::vdrawfunc *DCanvas::m_Drawfuncs;
DCanvas::vdrawsfunc *DCanvas::m_Drawsfuncs;


// Palettized versions of the column drawers
DCanvas::vdrawfunc DCanvas::Pfuncs[DCanvas::EWRAPPER_NUM] =
{
	DCanvas::DrawPatchP,
	DCanvas::DrawLucentPatchP,
	DCanvas::DrawTranslatedPatchP,
	DCanvas::DrawTlatedLucentPatchP,
	DCanvas::DrawColoredPatchP,
	DCanvas::DrawColorLucentPatchP,
	DCanvas::DrawAlphaPatchP,
};
DCanvas::vdrawsfunc DCanvas::Psfuncs[DCanvas::EWRAPPER_NUM] =
{
	DCanvas::DrawPatchSP,
	DCanvas::DrawLucentPatchSP,
	DCanvas::DrawTranslatedPatchSP,
	DCanvas::DrawTlatedLucentPatchSP,
	DCanvas::DrawColoredPatchSP,
	DCanvas::DrawColorLucentPatchSP,
	DCanvas::DrawAlphaPatchSP,
};

const BYTE *V_ColorMap;
int V_ColorFill;

// Palette lookup table for direct modes
unsigned int *V_Palette;


/*********************************/
/*								 */
/* The palletized column drawers */
/*								 */
/*********************************/

// Normal patch drawers
void DCanvas::DrawPatchP (const byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*dest = *source++;
		dest += pitch;
	} while (--count);
}

void DCanvas::DrawPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*dest = source[c >> 16]; 
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translucent patch drawers (amount is in V_Fade)
void DCanvas::DrawLucentPatchP (const byte *source, byte *dest, int count, int pitch)
{
	DWORD *fg2rgb, *bg2rgb;

	{
		fixed_t fglevel, bglevel;

		fglevel = V_Fade & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	do
	{
		DWORD fg = *source++;
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	DWORD *fg2rgb, *bg2rgb;
	int c = 0;

	{
		fixed_t fglevel, bglevel;

		fglevel = V_Fade & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	do
	{
		DWORD fg = source[c >> 16];
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translated patch drawers
void DCanvas::DrawTranslatedPatchP (const byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*dest = V_ColorMap[*source++];
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawTranslatedPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*dest = V_ColorMap[source[c >> 16]];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translated, translucent patch drawers
void DCanvas::DrawTlatedLucentPatchP (const byte *source, byte *dest, int count, int pitch)
{
	DWORD *fg2rgb, *bg2rgb;
	const BYTE *colormap = V_ColorMap;

	{
		fixed_t fglevel, bglevel;

		fglevel = V_Fade & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	do
	{
		DWORD fg = colormap[*source++];
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawTlatedLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;
	DWORD *fg2rgb, *bg2rgb;
	const BYTE *colormap = V_ColorMap;

	{
		fixed_t fglevel, bglevel;

		fglevel = V_Fade & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	do
	{
		DWORD fg = colormap[source[c >> 16]];
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Colored patch drawer
//
void DCanvas::DrawColoredPatchP (const byte *source, byte *dest, int count, int pitch)
{
	byte fill = (byte)V_ColorFill;

	do
	{
		*dest = fill;
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawColoredPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	byte fill = (byte)V_ColorFill;

	do
	{
		*dest = fill;
		dest += pitch; 
	} while (--count);
}


// Colored, translucent patch drawer
//
void DCanvas::DrawColorLucentPatchP (const byte *source, byte *dest, int count, int pitch)
{
	DWORD *bg2rgb;
	DWORD fg;

	{
		fixed_t fglevel;

		fglevel = V_Fade & ~0x3ff;
		bg2rgb = Col2RGB8[(FRACUNIT-fglevel)>>10];
		fg = Col2RGB8[fglevel>>10][V_ColorFill];
	}

	do
	{
		unsigned int bg;
		bg = (fg + bg2rgb[*dest]) | 0x1f07c1f;
		*dest = RGB32k[0][0][bg & (bg>>15)];
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawColorLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	DWORD *bg2rgb;
	DWORD fg;

	{
		fixed_t fglevel;

		fglevel = V_Fade & ~0x3ff;
		bg2rgb = Col2RGB8[(FRACUNIT-fglevel)>>10];
		fg = Col2RGB8[fglevel>>10][V_ColorFill];
	}

	do
	{
		DWORD bg;
		bg = (fg + bg2rgb[*dest]) | 0x1f07c1f;
		*dest = RGB32k[0][0][bg & (bg>>15)];
		dest += pitch; 
	} while (--count);
}

// Alpha patch drawers
//
void DCanvas::DrawAlphaPatchP (const byte *source, byte *dest, int count, int pitch)
{
	DWORD *fgstart = &Col2RGB8[0][V_ColorFill];

	do
	{
		DWORD val = (*source++ + 2) >> 2;
		if (val != 0)
		{
			DWORD fg = fgstart[val<<8];
			val = Col2RGB8[64-val][*dest];
			val = (val + fg) | 0x1f07c1f;
			*dest = RGB32k[0][0][val & (val>>15)];
		}
		dest += pitch; 
	} while (--count);
}

void DCanvas::DrawAlphaPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc)
{
	DWORD *fgstart = &Col2RGB8[0][V_ColorFill];
	int c = 0;

	do
	{
		DWORD val = (source[c >> 16] + 2) >> 2;
		if (val != 0)
		{
			DWORD fg = fgstart[val<<8];
			val = Col2RGB8[64-val][*dest];
			val = (fg + val) | 0x1f07c1f;
			*dest = RGB32k[0][0][val & (val>>15)];
		}
		dest += pitch;
		c += yinc;
	} while (--count);
}


void DCanvas::DrawTexture (FTexture *img, int x0, int y0, DWORD tags_first, ...)
{
	static BYTE identitymap[256];
	static short bottomclipper[MAXWIDTH];
	va_list tags;
	DWORD tag;
	BOOL boolval;
	int intval;

	int destwidth = img->GetWidth() << FRACBITS;
	int destheight = img->GetHeight() << FRACBITS;
	int top = img->TopOffset;
	int left = img->LeftOffset;
	fixed_t alpha = FRACUNIT;
	int fillcolor = -1;
	const BYTE *translation = NULL;
	BOOL alphaChannel = false;
	BOOL flipX = false;
	EWrapperCode drawer;
	fixed_t shadowAlpha = 0;
	int shadowColor = 0;

	x0 <<= FRACBITS;
	y0 <<= FRACBITS;

	// Parse the tag list for attributes
	va_start (tags, tags_first);
	tag = tags_first;

	while (tag != TAG_DONE)
	{
		DWORD *more_p;
		DWORD data;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, DWORD *);
			va_end (tags);
			va_start (tags, *more_p);
			tag = *more_p;
			continue;

		case DTA_DestWidth:
			destwidth = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_DestHeight:
			destheight = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_Clean:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				x0 = (x0 - 160*FRACUNIT) * CleanXfac + (Width * (FRACUNIT/2));
				y0 = (y0 - 100*FRACUNIT) * CleanYfac + (Height * (FRACUNIT/2));
				destwidth = img->GetWidth() * CleanXfac * FRACUNIT;
				destheight = img->GetHeight() * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				destwidth = img->GetWidth() * CleanXfac * FRACUNIT;
				destheight = img->GetHeight() * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_320x200:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				x0 = Scale (Width, x0, 320);
				y0 = Scale (Height, y0, 200);
				destwidth = FixedDiv (Width * img->GetWidth(), 320);
				destheight = FixedDiv (Height * img->GetHeight(), 200);
			}
			break;

		case DTA_VirtualWidth:
			intval = va_arg (tags, int);
			x0 = Scale (Width, x0, intval);
			destwidth = FixedDiv (Width * img->GetWidth(), intval);
			break;
			
		case DTA_VirtualHeight:
			intval = va_arg (tags, int);
			y0 = Scale (Width, y0, intval);
			destheight = FixedDiv (Height * img->GetHeight(), intval);
			break;

		case DTA_Alpha:
			alpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_AlphaChannel:
			alphaChannel = va_arg (tags, BOOL);
			break;

		case DTA_FillColor:
			fillcolor = va_arg (tags, int);
			break;

		case DTA_Translation:
			translation = va_arg (tags, const BYTE *);
			break;

		case DTA_FlipX:
			flipX = va_arg (tags, BOOL);
			break;

		case DTA_TopOffset:
			top = va_arg (tags, int);
			break;

		case DTA_LeftOffset:
			left = va_arg (tags, int);
			break;

		case DTA_ShadowAlpha:
			shadowAlpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_ShadowColor:
			shadowColor = va_arg (tags, int);
			break;

		case DTA_Shadow:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				shadowAlpha = FRACUNIT/2;
				shadowColor = 0;
			}
			else
			{
				shadowAlpha = 0;
			}
			break;
		}
		tag = va_arg (tags, DWORD);
	}
	va_end (tags);

#if 0
	// Determine which routine to use for drawing
	if (fillcolor >= 0)
	{
		if (alphaChannel)
		{
			drawer = EWrapper_Alpha;
		}
		else if (alpha < FRACUNIT)
		{
			drawer = EWrapper_ColoredLucent;
		}
		else
		{
			drawer = EWrapper_Colored;
		}
	}
	else if (alpha < FRACUNIT)
	{
		if (translation != NULL)
		{
			drawer = EWrapper_TlatedLucent;
		}
		else
		{
			drawer = EWrapper_Lucent;
		}
	}
	else if (translation != NULL)
	{
		drawer = EWrapper_Translated;
	}
	else
	{
		drawer = EWrapper_Normal;
	}

	V_ColorMap = translation;

	if (destwidth == img->GetWidth() && destheight == img->GetHeight())
	{
		x0 -= left;
		y0 -= top;
		if (shadowAlpha > 0)
		{
			V_ColorFill = shadowColor;
			V_Fade = shadowAlpha;
			DrawWrapper (EWrapper_ColoredLucent, img, x0+2, y0+2, flipX);
		}
		V_ColorFill = fillcolor;
		V_Fade = alpha;
		DrawWrapper (drawer, img, x0, y0, flipX);
	}
	else
	{
		x0 -= left * destwidth / img->GetWidth();
		y0 -= top * destheight / img->GetHeight();
		if (shadowAlpha > 0)
		{
			V_ColorFill = shadowColor;
			V_Fade = shadowAlpha;
			DrawSWrapper (EWrapper_ColoredLucent, img, x0+2, y0+2, destwidth, destheight, flipX);
		}
		V_ColorFill = fillcolor;
		V_Fade = alpha;
		DrawSWrapper (drawer, img, x0, y0, destwidth, destheight, flipX);
	}
#endif

	ERenderStyle style;

	if (fillcolor >= 0)
	{
		if (alphaChannel)
		{
			style = STYLE_Shaded;
		}
		else if (alpha < FRACUNIT)
		{
			style = STYLE_TranslucentStencil;
		}
		else
		{
			style = STYLE_Stencil;
		}
	}
	else if (alpha < FRACUNIT)
	{
		style = STYLE_Translucent;
	}
	else
	{
		style = STYLE_Normal;
	}

	ESPSResult mode = R_SetPatchStyle (style, alpha, 0, fillcolor<<24);

	x0 -= Scale (left, destwidth, img->GetWidth());
	y0 -= Scale (top, destheight, img->GetHeight());

	if (style != STYLE_Shaded)
	{
		if (translation != NULL)
		{
			dc_colormap = (lighttable_t *)translation;
		}
		else
		{
			dc_colormap = identitymap;
		}
	}

	if (mode != DontDraw)
	{
		const BYTE *pixels;
		const FTexture::Span *spans;
		int stop4;

		fixed_t centeryback = 0;
		centeryfrac = 0;

		sprtopscreen = y0;
		spryscale = destheight / img->GetHeight();

		// Fix precision errors that are noticeable at some resolutions
		if ((spryscale * img->GetHeight()) >> FRACBITS != destheight >> FRACBITS)
			spryscale++;

		sprflipvert = false;
		dc_iscale = 0xffffffffu / (unsigned)spryscale;
		dc_texturemid = FixedMul (-y0, dc_iscale);
		fixed_t frac = 0;
		fixed_t xiscale = DivScale32 (img->GetWidth(), destwidth);
		int x2 = (x0 + destwidth) >> FRACBITS;

		if (bottomclipper[0] != screen->GetHeight())
		{
			clearbufshort (bottomclipper, screen->GetWidth(), (short)screen->GetHeight());
			if (identitymap[1] != 1)
			{
				for (int i = 1; i < 256; ++i)
				{
					identitymap[i] = i;
				}
			}
		}
		mceilingclip = negonearray;
		mfloorclip = bottomclipper;

		if (flipX)
		{
			frac = (img->GetWidth() << FRACBITS) - 1;
			xiscale = -xiscale;
		}

		dc_x = x0 >> FRACBITS;
		if (dc_x < 0)
		{
			frac += -dc_x * xiscale;
			dc_x = 0;
		}

		if (destheight < 32*FRACUNIT)
		{
			mode = DoDraw0;
		}

		if (mode == DoDraw0)
		{
			// One column at a time
			stop4 = dc_x;
		}
		else	 // DoDraw1
		{
			// Up to four columns at a time
			stop4 = (x2 + 1) & ~3;
		}

		if (dc_x < x2)
		{
			while ((dc_x < stop4) && (dc_x & 3))
			{
				pixels = img->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop4)
			{
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					pixels = img->GetColumn (frac >> FRACBITS, &spans);
					R_DrawMaskedColumnHoriz (pixels, spans);
					dc_x++;
					frac += xiscale;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				pixels = img->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}
		}

		centeryfrac = centeryback;
	}
	R_FinishSetPatchStyle ();

	if (ticdup != 0)
	{
		NetUpdate ();
	}
}

/******************************/
/*							  */
/* The patch drawing wrappers */
/*							  */
/******************************/

//
// V_DrawWrapper
// Masks a column based masked pic to the screen. 
//
void DCanvas::DrawWrapper (EWrapperCode drawer, FTexture *tex, int x, int y, BOOL flipX) const
{
	int 		col;
	const BYTE *column;
	const FTexture::Span *spans;
	byte*		desttop;
	int 		w;
	vdrawfunc	drawfunc;

	w = tex->GetWidth ();
#ifdef RANGECHECK 
	if (x < 0 || x + w > Width ||
		y < 0 || y + tex->GetHeight() > Height)
	{
	  // Printf ("Patch at %d,%d exceeds LFB\n", x,y );
	  // No I_Error abort - what is up with TNT.WAD?
	  DPrintf ("DCanvas::DrawWrapper: bad patch (ignored)\n");
	  return;
	}
#endif

	drawfunc = Pfuncs[drawer];

	desttop = Buffer + y*Pitch + x;
	w--;

	for (col = 0; col <= w; x++, col++, desttop++)
	{
		column = tex->GetColumn (flipX ? w - col : col, &spans);

		// step through the posts in a column
		while (spans->Length != 0)
		{
			drawfunc (column + spans->TopOffset, desttop + spans->TopOffset * Pitch, spans->Length, Pitch);
			spans++;
		}
	}
}

//
// V_DrawSWrapper
// Masks a column based masked pic to the screen
// stretching it to fit the given dimensions.
//
extern void F_DrawPatchCol (int, FTexture *, int, const DCanvas *);

void DCanvas::DrawSWrapper (EWrapperCode drawer, FTexture *tex, int x0, int y0, int destwidth, int destheight, BOOL flipX) const
{
	const BYTE *column; 
	const FTexture::Span *spans;
	byte*		desttop;
	vdrawsfunc	drawfunc;
	int			maxy;

	int			xinc, yinc, col, w, ymul, xmul;

	w = tex->GetWidth ();

	if (destwidth == Width && destheight == Height &&
		w == 320 && tex->GetHeight() == 200 &&
		drawer == EWrapper_Normal)
	{
		// Special case: Drawing a full-screen patch, so use
		// F_DrawPatchCol in f_finale.c, since it's faster.
		for (w = 0; w < 320; w++)
			F_DrawPatchCol (w, tex, w, this);
		return;
	}

	xinc = (w << 16) / destwidth;
	yinc = (tex->GetHeight() << 16) / destheight;
	xmul = (destwidth << 16) / w;
	ymul = (destheight << 16) / tex->GetHeight();

#ifdef RANGECHECK 
	if (x0 < 0 || x0 + destwidth > Width ||
		y0 < 0 || y0 + destheight > Height)
	{
		//Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
		DPrintf ("DCanvas::DrawSWrapper: bad patch (ignored)\n");
		return;
	}
#endif

	drawfunc = Psfuncs[drawer];

	desttop = Buffer + y0*Pitch + x0;

	w = destwidth * xinc;
	maxy = tex->GetHeight();

	if (flipX)
	{
		xinc = -xinc;
		col = w + xinc;
	}
	else
	{
		col = 0;
	}

	for (; flipX ? col >= 0 : col < w; col += xinc, desttop++)
	{
		column = tex->GetColumn (col >> 16, &spans);

		// step through the posts in a column
		while (spans->Length != 0)
		{
			int top = spans->TopOffset * ymul;
			int bot = top + spans->Length * ymul;
			bot = (bot - top + 0x8000) >> 16;
			if (bot > 0)
			{
				top >>= 16;
				drawfunc (column + spans->TopOffset, desttop + top * Pitch,
					bot, Pitch, yinc);
			}
			spans++;
		}
	}
}

/********************************/
/*								*/
/* Other miscellaneous routines */
/*								*/
/********************************/


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void DCanvas::DrawBlock (int x, int y, int _width, int _height, const byte *src) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*Pitch + x;

	do
	{
		memcpy (dest, src, _width);
		src += srcpitch;
		dest += destpitch;
	} while (--_height);
}

void DCanvas::DrawPageBlock (const byte *src) const
{
	if (Width == 320 && Height == 200)
	{
		DrawBlock (0, 0, 320, 200, src);
		return;
	}

	byte *dest;
	fixed_t acc;
	fixed_t xinc, yinc;
	int h;

	dest = Buffer;
	xinc = 320 * FRACUNIT / Width;
	yinc = 200 * FRACUNIT / Height;
	acc = 0;
	h = Height;

	const byte *copysrc = NULL;

	do
	{
		if (copysrc)
		{
			memcpy (dest, copysrc, Width);
			dest += Pitch;
		}
		else
		{
			int w = Width;
			fixed_t xfrac = 0;
			do
			{
				*dest++ = src[xfrac >> FRACBITS];
				xfrac += xinc;
			} while (--w);
			dest += Pitch - Width;
		}
		acc += yinc;
		if (acc >= FRACUNIT)
		{
			src += (acc >> FRACBITS) * 320;
			acc &= FRACUNIT-1;
			copysrc = NULL;
		}
		else
		{
			copysrc = dest - Pitch;
		}
	} while (--h);
}

//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
//
void DCanvas::GetBlock (int x, int y, int _width, int _height, byte *dest) const
{
	const byte *src;

#ifdef RANGECHECK 
	if (x<0
		||x+_width > Width
		|| y<0
		|| y+_height>Height)
	{
		I_Error ("Bad V_GetBlock");
	}
#endif

	src = Buffer + y*Pitch + x;

	while (_height--)
	{
		memcpy (dest, src, _width);
		src += Pitch;
		dest += _width;
	}
}

// Returns true if the box was completely clipped. False otherwise.
bool DCanvas::ClipBox (int &x, int &y, int &w, int &h, const byte *&src, const int srcpitch) const
{
	if (x >= Width || y >= Height || x+w <= 0 || y+h <= 0)
	{ // Completely clipped off screen
		return true;
	}
	if (x < 0)				// clip left edge
	{
		src -= x;
		w += x;
		x = 0;
	}
	if (x+w > Width)		// clip right edge
	{
		w = Width - x;
	}
	if (y < 0)				// clip top edge
	{
		src -= y*srcpitch;
		h += y;
		y = 0;
	}
	if (y+h > Height)		// clip bottom edge
	{
		h = Height - y;
	}
	return false;
}

bool DCanvas::ClipScaleBox (
	int &x, int &y,
	int &w, int &h,
	int &dw, int &dh,
	const byte *&src, const int srcpitch,
	fixed_t &xinc, fixed_t &yinc,
	fixed_t &xstart, fixed_t &ystart) const
{
	if (x >= Width || y >= Height || x+dw <= 0 || y+dh <= 0)
	{ // Completely clipped off screen
		return true;
	}

	xinc = (w << FRACBITS) / dw;
	yinc = (h << FRACBITS) / dh;
	xstart = ystart = 0;

	if (x < 0)				// clip left edge
	{
		dw += x;
		xstart = -x*xinc;
		x = 0;
	}
	if (x+dw > Width)		// clip right edge
	{
		dw = Width - x;
	}
	if (y < 0)				// clip top edge
	{
		dh += y;
		ystart = -y*yinc;
		src += (ystart>>FRACBITS)*srcpitch;
		ystart &= FRACUNIT-1;
		y = 0;
	}
	if (y+dh > Height)		// clip bottom edge
	{
		dh = Height - y;
	}
	return false;
}
