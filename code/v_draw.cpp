#include "doomtype.h"
#include "v_video.h"
#include "m_swap.h"

#include "i_system.h"
#include "i_video.h"

// [RH] Stretch values for V_DrawPatchClean()
int CleanXfac, CleanYfac;
// [RH] Virtual screen sizes as perpetuated by V_DrawPatchClean()
int CleanWidth, CleanHeight;

fixed_t V_Fade = 0x8000;

// The current set of column drawers (set in V_SetResolution)
DCanvas::vdrawfunc *DCanvas::m_Drawfuncs;
DCanvas::vdrawsfunc *DCanvas::m_Drawsfuncs;


// Palettized versions of the column drawers
DCanvas::vdrawfunc DCanvas::Pfuncs[6] =
{
	DCanvas::DrawPatchP,
	DCanvas::DrawLucentPatchP,
	DCanvas::DrawTranslatedPatchP,
	DCanvas::DrawTlatedLucentPatchP,
	DCanvas::DrawColoredPatchP,
	DCanvas::DrawColorLucentPatchP,
};
DCanvas::vdrawsfunc DCanvas::Psfuncs[6] =
{
	DCanvas::DrawPatchSP,
	DCanvas::DrawLucentPatchSP,
	DCanvas::DrawTranslatedPatchSP,
	DCanvas::DrawTlatedLucentPatchSP,
	DCanvas::DrawColoredPatchSP,
	DCanvas::DrawColorLucentPatchSP
};

byte *V_ColorMap;
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

	TransArea += count + 1;
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

	TransArea += count + 1;
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
	byte *colormap = V_ColorMap;

	TransArea += count + 1;
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
	byte *colormap = V_ColorMap;

	TransArea += count + 1;
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

	TransArea += count + 1;
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

	TransArea += count + 1;
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


/******************************/
/*							  */
/* The patch drawing wrappers */
/*							  */
/******************************/

//
// V_DrawWrapper
// Masks a column based masked pic to the screen. 
//
void DCanvas::DrawWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const
{
	int 		col;
	int			colstep;
	column_t*	column;
	byte*		desttop;
	int 		w;
	vdrawfunc	drawfunc;

	y -= SHORT(patch->topoffset);
	x -= SHORT(patch->leftoffset);
#ifdef RANGECHECK 
	if (x<0
		||x+SHORT(patch->width) > Width
		|| y<0
		|| y+SHORT(patch->height)>Height)
	{
	  // Printf (PRINT_HIGH, "Patch at %d,%d exceeds LFB\n", x,y );
	  // No I_Error abort - what is up with TNT.WAD?
	  DPrintf ("DCanvas::DrawWrapper: bad patch (ignored)\n");
	  return;
	}
#endif

	drawfunc = Pfuncs[drawer];
	colstep = 1;

	col = 0;
	desttop = Buffer + y*Pitch + x * colstep;

	w = SHORT(patch->width);

	for ( ; col<w ; x++, col++, desttop += colstep)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

		// step through the posts in a column
		while (column->topdelta != 0xff )
		{
			drawfunc ((byte *)column + 3,
					  desttop + column->topdelta * Pitch,
					  column->length,
					  Pitch);

			column = (column_t *)(	(byte *)column + column->length
									+ 4 );
		}
	}
}

//
// V_DrawSWrapper
// Masks a column based masked pic to the screen
// stretching it to fit the given dimensions.
//
extern void F_DrawPatchCol (int, const patch_t *, int, const DCanvas *);

void DCanvas::DrawSWrapper (EWrapperCode drawer, const patch_t *patch, int x0, int y0, int destwidth, int destheight) const
{
	column_t*	column; 
	byte*		desttop;
	vdrawsfunc	drawfunc;
	int			colstep;

	int			xinc, yinc, col, w, ymul, xmul;

	if (destwidth == SHORT(patch->width) && destheight == SHORT(patch->height))
	{
		DrawWrapper (drawer, patch, x0, y0);
		return;
	}

	if (destwidth == Width && destheight == Height &&
		SHORT(patch->width) == 320 && SHORT(patch->height) == 200
		&& drawer == EWrapper_Normal)
	{
		// Special case: Drawing a full-screen patch, so use
		// F_DrawPatchCol in f_finale.c, since it's faster.
		for (w = 0; w < 320; w++)
			F_DrawPatchCol (w, patch, w, this);
		return;
	}

	xinc = (SHORT(patch->width) << 16) / destwidth;
	yinc = (SHORT(patch->height) << 16) / destheight;
	xmul = (destwidth << 16) / SHORT(patch->width);
	ymul = (destheight << 16) / SHORT(patch->height);

	y0 -= (SHORT(patch->topoffset) * ymul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * xmul) >> 16;

#ifdef RANGECHECK 
	if (x0<0
		|| x0+destwidth > Width
		|| y0<0
		|| y0+destheight> Height)
	{
		//Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
		DPrintf ("DCanvas::DrawSWrapper: bad patch (ignored)\n");
		return;
	}
#endif

	drawfunc = Psfuncs[drawer];
	colstep = 1;

	col = 0;
	desttop = Buffer + y0*Pitch + x0 * colstep;

	w = destwidth * xinc;

	for ( ; col<w ; col += xinc, desttop += colstep)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> 16]));

		// step through the posts in a column
		while (column->topdelta != 0xff)
		{
			int top = column->topdelta * ymul;
			int bot = top + column->length * ymul;
			bot = (bot - top + 0x8000) >> 16;
			if (bot > 0)
			{
				top = (top) >> 16;
				drawfunc ((byte *)column + 3, desttop + top * Pitch,
					bot, Pitch, yinc);
				column = (column_t *)((byte *)column + column->length + 4);
			}
		}
	}
}

//
// V_DrawIWrapper
// Like V_DrawWrapper except it will stretch the patches as
// needed for non-320x200 screens.
//
void DCanvas::DrawIWrapper (EWrapperCode drawer, const patch_t *patch, int x0, int y0) const
{
	if (Width == 320 && Height == 200)
		DrawWrapper (drawer, patch, x0, y0);
	else
		DrawSWrapper (drawer, patch,
			 (Width * x0) / 320, (Height * y0) / 200,
			 (Width * SHORT(patch->width)) / 320, (Height * SHORT(patch->height)) / 200);
}

//
// V_DrawCWrapper
// Like V_DrawIWrapper, except it only uses integral multipliers.
//
void DCanvas::DrawCWrapper (EWrapperCode drawer, const patch_t *patch, int x0, int y0) const
{
	if (CleanXfac == 1 && CleanYfac == 1)
		DrawWrapper (drawer, patch, (x0-160) + (Width/2), (y0-100) + (Height/2));
	else
		DrawSWrapper (drawer, patch,
			(x0-160)*CleanXfac+(Width/2), (y0-100)*CleanYfac+(Height/2),
			SHORT(patch->width) * CleanXfac, SHORT(patch->height) * CleanYfac);
}

//
// V_DrawCNMWrapper
// Like V_DrawCWrapper, except it doesn't adjust the x and y coordinates.
//
void DCanvas::DrawCNMWrapper (EWrapperCode drawer, const patch_t *patch, int x0, int y0) const
{
	if (CleanXfac == 1 && CleanYfac == 1)
		DrawWrapper (drawer, patch, x0, y0);
	else
		DrawSWrapper (drawer, patch, x0, y0,
						SHORT(patch->width) * CleanXfac,
						SHORT(patch->height) * CleanYfac);
}


/********************************/
/*								*/
/* Other miscellaneous routines */
/*								*/
/********************************/

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
// Like V_DrawIWrapper except it only uses one drawing function and draws
// the patch flipped horizontally.
//
void DCanvas::DrawPatchFlipped (const patch_t *patch, int x0, int y0) const
{
	column_t*	column; 
	byte*		desttop;
	vdrawsfunc	drawfunc;
	int			colstep;
	int			destwidth, destheight;

	int			xinc, yinc, col, w, ymul, xmul;

	x0 = (Width * x0) / 320;
	y0 = (Height * y0) / 200;
	destwidth = (Width * SHORT(patch->width)) / 320;
	destheight = (Height * SHORT(patch->height)) / 200;

	xinc = (SHORT(patch->width) << 16) / destwidth;
	yinc = (SHORT(patch->height) << 16) / destheight;
	xmul = (destwidth << 16) / SHORT(patch->width);
	ymul = (destheight << 16) / SHORT(patch->height);

	y0 -= (SHORT(patch->topoffset) * ymul) >> 16;
	x0 -= (SHORT(patch->leftoffset) * xmul) >> 16;

#ifdef RANGECHECK 
	if (x0<0
		|| x0+destwidth > Width
		|| y0<0
		|| y0+destheight> Height)
	{
		//Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
		DPrintf ("DCanvas::DrawPatchFlipped: bad patch (ignored)\n");
		return;
	}
#endif

	drawfunc = Psfuncs[EWrapper_Normal];
	colstep = 1;

	w = destwidth * xinc;
	col = w - xinc;
	desttop = Buffer + y0*Pitch + x0 * colstep;

	for ( ; col >= 0 ; col -= xinc, desttop += colstep)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> 16]));

		// step through the posts in a column
		while (column->topdelta != 0xff )
		{
			drawfunc ((byte *)column + 3,
					  desttop + (((column->topdelta * ymul)) >> 16) * Pitch,
					  (column->length * ymul) >> 16,
					  Pitch,
					  yinc);
			column = (column_t *)(	(byte *)column + column->length
									+ 4 );
		}
	}
}


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

typedef void (STACK_ARGS *DrawRemapFn)   (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, int w, int h);
typedef void (STACK_ARGS *DrawNomapFn)	 (const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h);
typedef void (STACK_ARGS *DrawTRemapFn)  (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, int w, int h, DWORD *fg2rgb, DWORD *bg2rgb);
typedef void (STACK_ARGS *DrawTNomapFn)  (const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h, DWORD *fg2rgb, DWORD *bg2rgb);
typedef void (STACK_ARGS *DrawSRemapFn)  (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, int w, int h, DWORD fg, DWORD *bg2rgb);
typedef void (STACK_ARGS *DrawSNomapFn)  (const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h, DWORD fg, DWORD *bg2rgb);
typedef void (STACK_ARGS *DrawAlphaFn)   (const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h, DWORD *fgstart);

typedef void (STACK_ARGS *ScaleRemapFn)  (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h);
typedef void (STACK_ARGS *ScaleNomapFn)  (const byte *src, byte *dest, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h);
typedef void (STACK_ARGS *ScaleTRemapFn) (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h, DWORD *fg2rgb, DWORD *bg2rgb);
typedef void (STACK_ARGS *ScaleTNomapFn) (const byte *src, byte *dest, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h, DWORD *fg2rgb, DWORD *bg2rgb);
typedef void (STACK_ARGS *ScaleSRemapFn) (const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h, DWORD fg, DWORD *bg2rgb);
typedef void (STACK_ARGS *ScaleSNomapFn) (const byte *src, byte *dest, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h, DWORD fg, DWORD *bg2rgb);
typedef void (STACK_ARGS *ScaleAlphaFn)  (const byte *src, byte *dest, int srcpitch, int destpitch, fixed_t xinc, fixed_t yinc, fixed_t xstart, fixed_t err, int w, int h, DWORD *fgstart);

// Key to naming (because this fixed linker probs at first):
// 1st char: D = Draw, S = Scale
// 2nd char: M = Masked
// 3rd char: P = Plain, T = Translucent, S = Shadowed, A = Alpha
// 4th char: R = Remapped, U = Unmapped
// 5th char: P = Palettized output

extern "C" struct
{
	DrawRemapFn DMPRP;
	DrawNomapFn DMPUP;
	ScaleRemapFn SMPRP;
	ScaleNomapFn SMPUP;

	DrawTRemapFn DMTRP;
	DrawTNomapFn DMTUP;
	ScaleTRemapFn SMTRP;
	ScaleTNomapFn SMTUP;

	DrawSRemapFn DMSRP;
	DrawSNomapFn DMSUP;
	ScaleSRemapFn SMSRP;
	ScaleSNomapFn SMSUP;

	DrawAlphaFn DMAUP;
	ScaleAlphaFn SMAUP;
} MaskedBlockFunctions;

//
// 
void DCanvas::DrawMaskedBlock (int x, int y, int _width, int _height,
	const byte *src, const byte *colors) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.DMPRP (src, dest, colors, srcpitch, destpitch, _width, _height);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val != 0)
				{
					dest[j] = colors[val];
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.DMPUP (src, dest, srcpitch, destpitch, _width, _height);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val != 0)
				{
					dest[j] = val;
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
}

void DCanvas::ScaleMaskedBlock (int x, int y, int _width, int _height,
	int dwidth, int dheight, const byte *src, const byte *colors) const
{
	byte *dest;
	int srcpitch;
	int destpitch;
	fixed_t err;
	fixed_t xinc, yinc;
	fixed_t xstart;

	if (dwidth == _width && dheight == _height)
	{
		DrawMaskedBlock (x, y, _width, _height, src, colors);
		return;
	}

	srcpitch = _width;

	if (ClipScaleBox (x, y, _width, _height, dwidth, dheight, src, srcpitch, xinc, yinc, xstart, err))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.SMPRP (src, dest, colors, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					dest[i] = colors[in];
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += srcpitch;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.SMPUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					dest[i] = in;
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += srcpitch;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
}

void DCanvas::DrawTranslucentMaskedBlock (int x, int y, int _width, int _height,
	const byte *src, const byte *colors, fixed_t fade) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	DWORD *fg2rgb, *bg2rgb;

	{
		fixed_t fglevel, bglevel;

		fglevel = fade & ~0x3ff;
		bglevel = FRACUNIT - fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;
	if (this == screen)
		TransArea += _width * _height;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.DMTRP (src, dest, colors, srcpitch, destpitch, _width, _height, fg2rgb, bg2rgb);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val)
				{
					DWORD fg;
					fg = (fg2rgb[colors[val]] + bg2rgb[dest[j]]) | 0x1f07c1f;
					dest[j] = RGB32k[0][0][fg & (fg>>15)];
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.DMTUP (src, dest, srcpitch, destpitch, _width, _height, fg2rgb, bg2rgb);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val)
				{
					DWORD fg;
					fg = (fg2rgb[val] + bg2rgb[dest[j]]) | 0x1f07c1f;
					dest[j] = RGB32k[0][0][fg & (fg>>15)];
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
}

void DCanvas::ScaleTranslucentMaskedBlock (int x, int y, int _width, int _height,
	int dwidth, int dheight, const byte *src, const byte *colors, fixed_t fade) const
{
	byte *dest;
	int srcpitch;
	int destpitch;
	fixed_t err;
	fixed_t xinc, yinc;
	fixed_t xstart;

	if (dwidth == _width && dheight == _height)
	{
		DrawTranslucentMaskedBlock (x, y, _width, _height, src, colors, fade);
		return;
	}

	srcpitch = _width;

	if (ClipScaleBox (x, y, _width, _height, dwidth, dheight, src, srcpitch, xinc, yinc, xstart, err))
	{
		return;		// Nothing to draw
	}

	DWORD *fg2rgb, *bg2rgb;

	{
		fixed_t fglevel, bglevel;

		fglevel = fade & ~0x3ff;
		bglevel = FRACUNIT - fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;
	if (this == screen)
		TransArea += dwidth * dheight;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.SMTRP (src, dest, colors, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg2rgb, bg2rgb);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					DWORD fg;
					fg = (fg2rgb[colors[in]] + bg2rgb[dest[i]]) | 0x1f07c1f;
					dest[i] = RGB32k[0][0][fg & (fg>>15)];
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += srcpitch;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.SMTUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg2rgb, bg2rgb);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					DWORD fg;
					fg = (fg2rgb[in] + bg2rgb[dest[i]]) | 0x1f07c1f;
					dest[i] = RGB32k[0][0][fg & (fg>>15)];
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += srcpitch;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
}

void DCanvas::DrawShadowedMaskedBlock (int x, int y, int _width, int _height,
	const byte *src, const byte *colors, fixed_t shade) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	DWORD fg, *bg2rgb;

	{
		fixed_t fglevel, bglevel;

		fglevel = shade & ~0x3ff;
		bglevel = FRACUNIT - fglevel;
		fg = Col2RGB8[fglevel>>10][0];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	destpitch = Pitch;
	dest = Buffer + y*Pitch + x;
	if (this == screen)
		TransArea += _width * _height;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.DMSRP (src, dest, colors, srcpitch, destpitch, _width, _height, fg, bg2rgb);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val)
				{
					DWORD bg = bg2rgb[dest[j+destpitch*2+2]];
					bg = (fg + bg) | 0x1f07c1f;
					dest[j+destpitch*2+2] = RGB32k[0][0][bg & (bg>>15)];
					dest[j] = colors[val];
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.DMSUP (src, dest, srcpitch, destpitch, _width, _height, fg, bg2rgb);
#else
		do
		{
			int i, j;

			i = _width;
			j = 0;

			do
			{
				byte val = src[j];
				if (val)
				{
					DWORD bg = bg2rgb[dest[j+destpitch*2+2]];
					bg = (fg + bg) | 0x1f07c1f;
					dest[j+destpitch*2+2] = RGB32k[0][0][bg & (bg>>15)];
					dest[j] = val;
				}
			} while (++j, --i);
			dest += destpitch;
			src += srcpitch;
		}
		while (--_height);
#endif
	}
}

void DCanvas::ScaleShadowedMaskedBlock (int x, int y, int _width, int _height,
	int dwidth, int dheight, const byte *src, const byte *colors, fixed_t shade) const
{
	byte *dest;
	int srcpitch;
	int destpitch;
	fixed_t err;
	fixed_t xinc, yinc;
	fixed_t xstart;

	if (dwidth == _width && dheight == _height)
	{
		DrawShadowedMaskedBlock (x, y, _width, _height, src, colors, shade);
		return;
	}

	srcpitch = _width;

	if (ClipScaleBox (x, y, _width, _height, dwidth, dheight, src, srcpitch, xinc, yinc, xstart, err))
	{
		return;		// Nothing to draw
	}

	DWORD fg, *bg2rgb;

	{
		fixed_t fglevel, bglevel;

		fglevel = shade & ~0x3ff;
		bglevel = FRACUNIT - fglevel;
		fg = Col2RGB8[fglevel>>10][0];
		bg2rgb = Col2RGB8[bglevel>>10];
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;
	if (this == screen)
		TransArea += dwidth * dheight;

	if (colors != NULL)
	{
#ifdef USEASM
		MaskedBlockFunctions.SMSRP (src, dest, colors, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg, bg2rgb);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					DWORD bg;
					bg = (fg + bg2rgb[dest[i+destpitch*2+2]]) | 0x1f07c1f;
					dest[i+destpitch*2+2] = RGB32k[0][0][bg & (bg>>15)];
					dest[i] = colors[in];
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += _width;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
	else
	{
#ifdef USEASM
		MaskedBlockFunctions.SMSUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg, bg2rgb);
#else
		do
		{
			int i, x;

			i = 0;
			x = xstart;

			do
			{
				byte in = src[x >> FRACBITS];
				if (in)
				{
					DWORD bg;
					bg = (fg + bg2rgb[dest[i+destpitch*2+2]]) | 0x1f07c1f;
					dest[i+destpitch*2+2] = RGB32k[0][0][bg & (bg>>15)];
					dest[i] = in;
				}
				x += xinc;
			} while (++i < dwidth);
			dest += destpitch;
			err += yinc;
			while (err >= FRACUNIT)
			{
				src += _width;
				err -= FRACUNIT;
			}
		}
		while (--dheight);
#endif
	}
}

void DCanvas::DrawAlphaMaskedBlock (int x, int y, int _width, int _height,
	const byte *src, int color) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;
	DWORD *fgstart = &Col2RGB8[0][color];
	if (this == screen)
		TransArea += _width * _height;

#ifdef USEASM
	MaskedBlockFunctions.DMAUP (src, dest, srcpitch, destpitch, _width, _height, fgstart);
#else
	do
	{
		int i, j;

		i = _width;
		j = 0;

		do
		{
			DWORD val = src[j];
			if (val)
			{
				val = (val + 1) >> 2;
				DWORD fg = fgstart[val<<8];
				val = Col2RGB8[64-val][dest[j]];
				val = (val + fg) | 0x1f07c1f;
				dest[j] = RGB32k[0][0][val & (val>>15)];
			}
		} while (++j, --i);
		src += srcpitch;
		dest += destpitch;
	}
	while (--_height);
#endif
}

void DCanvas::ScaleAlphaMaskedBlock (int x, int y, int _width, int _height,
	int dwidth, int dheight, const byte *src, int color) const
{
	byte *dest;
	int srcpitch;
	int destpitch;
	fixed_t err;
	fixed_t xinc, yinc;
	fixed_t xstart;

	if (dwidth == _width && dheight == _height)
	{
		DrawAlphaMaskedBlock (x, y, _width, _height, src, color);
		return;
	}

	srcpitch = _width;

	if (ClipScaleBox (x, y, _width, _height, dwidth, dheight, src, srcpitch, xinc, yinc, xstart, err))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*destpitch + x;
	if (this == screen)
		TransArea += dwidth * dheight;

	DWORD *fgstart = &Col2RGB8[0][color];

#ifdef USEASM
	MaskedBlockFunctions.SMAUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fgstart);
#else
	do
	{
		int i, x;

		i = 0;
		x = xstart;

		do
		{
			DWORD val = src[x >> FRACBITS];
			if (val)
			{
				val = (val + 2) >> 2;
				DWORD fg = fgstart[val<<8];
				val = Col2RGB8[64-val][dest[i]];
				val = (fg + val) | 0x1f07c1f;
				dest[i] = RGB32k[0][0][val & (val>>15)];
			}
			x += xinc;
		} while (++i < dwidth);
		dest += destpitch;
		err += yinc;
		while (err >= FRACUNIT)
		{
			src += srcpitch;
			err -= FRACUNIT;
		}
	}
	while (--dheight);
#endif
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
