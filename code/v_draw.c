#include "doomtype.h"
#include "v_video.h"
#include "m_swap.h"

#include "i_system.h"
#include "i_video.h"

// [RH] Stretch values for V_DrawPatchClean()
int CleanXfac, CleanYfac;
// [RH] Virtual screen sizes as perpetuated by V_DrawPatchClean()
int CleanWidth, CleanHeight;

void V_DrawPatchP (byte *source, byte *dest, int count, int pitch);
void V_DrawLucentPatchP (byte *source, byte *dest, int count, int pitch);
void V_DrawTranslatedPatchP (byte *source, byte *dest, int count, int pitch);
void V_DrawTlatedLucentPatchP (byte *source, byte *dest, int count, int pitch);
void V_DrawColoredPatchP (byte *source, byte *dest, int count, int pitch);
void V_DrawColorLucentPatchP (byte *source, byte *dest, int count, int pitch);

void V_DrawPatchSP (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawLucentPatchSP (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawTranslatedPatchSP (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawTlatedLucentPatchSP (byte *source, byte *dest, int count, int pitch, int yinc);

void V_DrawPatchD (byte *source, byte *dest, int count, int pitch);
void V_DrawLucentPatchD (byte *source, byte *dest, int count, int pitch);
void V_DrawTranslatedPatchD (byte *source, byte *dest, int count, int pitch);
void V_DrawTlatedLucentPatchD (byte *source, byte *dest, int count, int pitch);
void V_DrawColoredPatchD (byte *source, byte *dest, int count, int pitch);
void V_DrawColorLucentPatchD (byte *source, byte *dest, int count, int pitch);

void V_DrawPatchSD (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawLucentPatchSD (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawTranslatedPatchSD (byte *source, byte *dest, int count, int pitch, int yinc);
void V_DrawTlatedLucentPatchSD (byte *source, byte *dest, int count, int pitch, int yinc);


// The current set of column drawers
// (set in V_SetResolution)
vdrawfunc *vdrawfuncs;
vdrawsfunc *vdrawsfuncs;


// Palettized versions of the column drawers
vdrawfunc vdrawPfuncs[6] = {
	V_DrawPatchP,
	V_DrawLucentPatchP,
	V_DrawTranslatedPatchP,
	V_DrawTlatedLucentPatchP,
	V_DrawColoredPatchP,
	V_DrawColorLucentPatchP,
};
vdrawsfunc vdrawsPfuncs[6] = {
	V_DrawPatchSP,
	V_DrawLucentPatchSP,
	V_DrawTranslatedPatchSP,
	V_DrawTlatedLucentPatchSP,
	(vdrawsfunc)V_DrawColoredPatchP,
	(vdrawsfunc)V_DrawColorLucentPatchP
};

// Direct (true-color) versions of the column drawers
vdrawfunc vdrawDfuncs[6] = {
	V_DrawPatchD,
	V_DrawLucentPatchD,
	V_DrawTranslatedPatchD,
	V_DrawTlatedLucentPatchD,
	V_DrawColoredPatchD,
	V_DrawColorLucentPatchD,
};
vdrawsfunc vdrawsDfuncs[6] = {
	V_DrawPatchSD,
	V_DrawLucentPatchSD,
	V_DrawTranslatedPatchSD,
	V_DrawTlatedLucentPatchSD,
	(vdrawsfunc)V_DrawColoredPatchD,
	(vdrawsfunc)V_DrawColorLucentPatchD
};

byte *V_ColorMap;
int V_ColorFill;

// Palette lookup table for direct modes
int *V_Palette;


/*********************************/
/*								 */
/* The palletized column drawers */
/*								 */
/*********************************/

// Normal patch drawers
void V_DrawPatchP (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*dest = *source++;
		dest += pitch;
	} while (--count);
}

void V_DrawPatchSP (byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*dest = source[c >> 16]; 
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translucent patch drawers (always 50%)
void V_DrawLucentPatchP (byte *source, byte *dest, int count, int pitch)
{
	byte *map = TransTable + 65536 * 2;

	do
	{
		*dest = map[(*dest)|((*source++)<<8)];
		dest += pitch; 
	} while (--count);
}

void V_DrawLucentPatchSP (byte *source, byte *dest, int count, int pitch, int yinc)
{
	byte *map = TransTable + 65536 * 2;
	int c = 0;

	do
	{
		*dest = map[(*dest)|((source[c >> 16])<<8)];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translated patch drawers
void V_DrawTranslatedPatchP (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*dest = V_ColorMap[*source++];
		dest += pitch; 
	} while (--count);
}

void V_DrawTranslatedPatchSP (byte *source, byte *dest, int count, int pitch, int yinc)
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
void V_DrawTlatedLucentPatchP (byte *source, byte *dest, int count, int pitch)
{
	byte *map = TransTable + 65536 * 2;

	do
	{
		*dest = map[(*dest)|(V_ColorMap[*source++]<<8)];
		dest += pitch; 
	} while (--count);
}

void V_DrawTlatedLucentPatchSP (byte *source, byte *dest, int count, int pitch, int yinc)
{
	byte *map = TransTable + 65536 * 2;
	int c = 0;

	do
	{
		*dest = map[(*dest)|(V_ColorMap[source[c >> 16]]<<8)];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Colored patch drawer
//
// This routine is the same for the stretched version since we don't
// care about the patch's actual contents, just it's outline.
void V_DrawColoredPatchP (byte *source, byte *dest, int count, int pitch)
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
// This routine is the same for the stretched version since we don't
// care about the patch's actual contents, just it's outline.
void V_DrawColorLucentPatchP (byte *source, byte *dest, int count, int pitch)
{
	byte *map = TransTable + 65536 * 2;
	int fill = V_ColorFill << 8;

	do
	{
		*dest = map[(*dest)|fill];
		dest += pitch; 
	} while (--count);
}



/**************************/
/*						  */
/* The RGB column drawers */
/*						  */
/**************************/

// Normal patch drawers
void V_DrawPatchD (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*((int *)dest) = V_Palette[*source++];
		dest += pitch;
	} while (--count);
}

void V_DrawPatchSD (byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*((int *)dest) = V_Palette[source[c >> 16]];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translucent patch drawers (always 50%)
void V_DrawLucentPatchD (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*((int *)dest) = ((V_Palette[*source++] & 0xfefefe) >> 1) +
						 ((*((int *)dest) & 0xfefefe) >> 1);
		dest += pitch; 
	} while (--count);
}

void V_DrawLucentPatchSD (byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*((int *)dest) = ((V_Palette[source[c >> 16]] & 0xfefefe) >> 1) +
						 ((*((int *)dest) & 0xfefefe) >> 1);
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translated patch drawers
void V_DrawTranslatedPatchD (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*((int *)dest) = V_Palette[V_ColorMap[*source++]];
		dest += pitch; 
	} while (--count);
}

void V_DrawTranslatedPatchSD (byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*((int *)dest) = V_Palette[V_ColorMap[source[c >> 16]]];
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Translated, translucent patch drawers
void V_DrawTlatedLucentPatchD (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*((int *)dest) = ((V_Palette[V_ColorMap[*source++]] & 0xfefefe) >> 1) +
						 ((*((int *)dest) & 0xfefefe) >> 1);
		dest += pitch; 
	} while (--count);
}

void V_DrawTlatedLucentPatchSD (byte *source, byte *dest, int count, int pitch, int yinc)
{
	int c = 0;

	do
	{
		*((int *)dest) = ((V_Palette[V_ColorMap[source[c >> 16]]] & 0xfefefe) >> 1) +
						 ((*((int *)dest) & 0xfefefe) >> 1);
		dest += pitch;
		c += yinc;
	} while (--count);
}


// Colored patch drawer
//
// This routine is the same for the stretched version since we don't
// care about the patch's actual contents, just it's outline.
void V_DrawColoredPatchD (byte *source, byte *dest, int count, int pitch)
{
	do
	{
		*((int *)dest) = V_ColorFill;
		dest += pitch; 
	} while (--count);
}


// Colored, translucent patch drawer
//
// This routine is the same for the stretched version since we don't
// care about the patch's actual contents, just it's outline.
void V_DrawColorLucentPatchD (byte *source, byte *dest, int count, int pitch)
{
	int fill = (V_ColorFill & 0xfefefe) >> 1;

	do
	{
		*((int *)dest) = fill + ((*((int *)dest) & 0xfefefe) >> 1);
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
void V_DrawWrapper (int drawer, int x, int y, screen_t *scrn, patch_t *patch)
{
	int 		col;
	int			colstep;
	column_t*	column;
	byte*		desttop;
	int 		w;
	int			pitch;
	vdrawfunc	drawfunc;

	y -= SHORT(patch->topoffset);
	x -= SHORT(patch->leftoffset);
#ifdef RANGECHECK 
	if (x<0
		||x+SHORT(patch->width) > scrn->width
		|| y<0
		|| y+SHORT(patch->height)>scrn->height)
	{
	  // Printf ("Patch at %d,%d exceeds LFB\n", x,y );
	  // No I_Error abort - what is up with TNT.WAD?
	  DPrintf ("V_DrawWrapper: bad patch (ignored)\n");
	  return;
	}
#endif

	if (scrn->is8bit) {
		drawfunc = vdrawPfuncs[drawer];
		colstep = 1;
	} else {
		drawfunc = vdrawDfuncs[drawer];
		colstep = 4;
	}

	pitch = scrn->pitch;

	if (scrn == &screens[0])
		V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

	col = 0;
	desttop = scrn->buffer + y*pitch + x * colstep;

	w = SHORT(patch->width);

	for ( ; col<w ; x++, col++, desttop += colstep)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

		// step through the posts in a column
		while (column->topdelta != 0xff )
		{
			drawfunc ((byte *)column + 3,
					  desttop + column->topdelta * pitch,
					  column->length,
					  pitch);

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
extern void F_DrawPatchCol (int, patch_t *, int, screen_t *);

void V_DrawSWrapper (int drawer, int x0, int y0, screen_t *scrn, patch_t *patch, int destwidth, int destheight)
{
	column_t*	column; 
	byte*		desttop;
	int			pitch;
	vdrawsfunc	drawfunc;
	int			colstep;

	int			xinc, yinc, col, w, ymul, xmul;

	if (destwidth == SHORT(patch->width) && destheight == SHORT(patch->height)) {
		V_DrawWrapper (drawer, x0, y0, scrn, patch);
		return;
	}

	if (destwidth == scrn->width && destheight == scrn->height &&
		SHORT(patch->width) == 320 && SHORT(patch->height) == 200
		&& drawer == V_DRAWPATCH) {
		// Special case: Drawing a full-screen patch, so use
		// F_DrawPatchCol in f_finale.c, since it's faster.
		for (w = 0; w < 320; w++)
			F_DrawPatchCol (w, patch, w, scrn);
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
		|| x0+destwidth > scrn->width
		|| y0<0
		|| y0+destheight> scrn->height)
	{
		//Printf ("Patch at %d,%d exceeds LFB\n", x0,y0 );
		DPrintf ("V_DrawSWrapper: bad patch (ignored)\n");
		return;
	}
#endif

	if (scrn->is8bit) {
		drawfunc = vdrawsPfuncs[drawer];
		colstep = 1;
	} else {
		drawfunc = vdrawsDfuncs[drawer];
		colstep = 4;
	}

	if (scrn == &screens[0])
		V_MarkRect (x0, y0, destwidth, destheight);

	col = 0;
	pitch = scrn->pitch;
	desttop = scrn->buffer + y0*scrn->pitch + x0 * colstep;

	w = destwidth * xinc;

	for ( ; col<w ; col += xinc, desttop += colstep)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col >> 16]));

		// step through the posts in a column
		while (column->topdelta != 0xff )
		{
			drawfunc ((byte *)column + 3,
					  desttop + (((column->topdelta * ymul)) >> 16) * pitch,
					  (column->length * ymul) >> 16,
					  pitch,
					  yinc);
			column = (column_t *)(	(byte *)column + column->length
									+ 4 );
		}
	}
}

//
// V_DrawIWrapper
// Like V_DrawWrapper except it will stretch the patches as
// needed for non-320x200 screens.
//
void V_DrawIWrapper (int drawer, int x0, int y0, screen_t *scrn, patch_t *patch)
{
	if (scrn->width == 320 && scrn->height == 200)
		V_DrawWrapper (drawer, x0, y0, scrn, patch);
	else
		V_DrawSWrapper (drawer,
			 (scrn->width * x0) / 320, (scrn->height * y0) / 200, scrn, patch,
			 (scrn->width * SHORT(patch->width)) / 320, (scrn->height * SHORT(patch->height)) / 200);
}

//
// V_DrawCWrapper
// Like V_DrawIWrapper, except it only uses integral multipliers.
//
void V_DrawCWrapper (int drawer, int x0, int y0, screen_t *scrn, patch_t *patch)
{
	if (CleanXfac == 1 && CleanYfac == 1) {
		V_DrawWrapper (drawer, (x0-160) + (scrn->width/2), (y0-100) + (scrn->height/2), scrn, patch);
	} else {
		V_DrawSWrapper (drawer,(x0-160)*CleanXfac+(scrn->width/2),
							   (y0-100)*CleanYfac+(scrn->height/2),
							   scrn, patch, SHORT(patch->width) * CleanXfac,
										    SHORT(patch->height) * CleanYfac);
	}
}

//
// V_DrawCNMWrapper
// Like V_DrawCWrapper, except it doesn't adjust the x and y coordinates.
//
void V_DrawCNMWrapper (int drawer, int x0, int y0, screen_t *scrn, patch_t *patch)
{
	if (CleanXfac == 1 && CleanYfac == 1)
		V_DrawWrapper (drawer, x0, y0, scrn, patch);
	else
		V_DrawSWrapper (drawer, x0, y0, scrn, patch,
						SHORT(patch->width) * CleanXfac,
						SHORT(patch->height) * CleanYfac);
}


/********************************/
/*								*/
/* Other miscellaneous routines */
/*								*/
/********************************/

//
// V_CopyRect 
// 
void V_CopyRect (int srcx, int srcy, screen_t *srcscrn, int width, int height,
				 int destx, int desty, screen_t *destscrn)
{
#ifdef RANGECHECK
	if (srcx<0
		||srcx+width > srcscrn->width
		|| srcy<0
		|| srcy+height> srcscrn->height
		||destx<0||destx+width > destscrn->width
		|| desty<0
		|| desty+height > destscrn->height)
	{
		I_Error ("Bad V_CopyRect");
	}
#endif
	V_MarkRect (destx, desty, width, height);

	I_Blit (srcscrn, srcx, srcy, width, height,
			destscrn, destx, desty, width, height);
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
// [RH] This is only called in f_finale.c, so I changed it to behave
// much like V_DrawPatchIndirect() instead of adding yet another function
// solely to handle virtual stretching of this function.
//
void V_DrawPatchFlipped (int x0, int y0, screen_t *scrn, patch_t *patch)
{
	// I must be dumb or something...
	V_DrawPatchIndirect (x0, y0, scrn, patch);
}


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void V_DrawBlock (int x, int y, screen_t *scrn, int width, int height, byte *src)
{
	byte *dest;

#ifdef RANGECHECK
	if (x<0
		||x+width > scrn->width
		|| y<0
		|| y+height>scrn->height)
	{
		I_Error ("Bad V_DrawBlock");
	}
#endif

	V_MarkRect (x, y, width, height);

	x <<= (scrn->is8bit) ? 0 : 2;
	width <<= (scrn->is8bit) ? 0 : 2;

	dest = scrn->buffer + y*scrn->pitch + x;

	while (height--)
	{
		memcpy (dest, src, width);
		src += width;
		dest += scrn->pitch;
	};
}



//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
//
void V_GetBlock (int x, int y, screen_t *scrn, int width, int height, byte *dest)
{
	byte*		src;

#ifdef RANGECHECK 
	if (x<0
		||x+width > scrn->width
		|| y<0
		|| y+height>scrn->height)
	{
		I_Error ("Bad V_GetBlock");
	}
#endif

	x <<= (scrn->is8bit) ? 0 : 2;
	width <<= (scrn->is8bit) ? 0 : 2;

	src = scrn->buffer + y*scrn->pitch + x;

	while (height--)
	{
		memcpy (dest, src, width);
		src += scrn->pitch;
		dest += width;
	}
}
