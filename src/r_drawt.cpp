/* r_drawt.c [RH]
 *
 * Functions for drawing columns into a temporary buffer and then
 * copying them to the screen. On machines with a decent cache, this
 * is faster than drawing them directly to the screen. Will I be able
 * to even understand any of this if I come back to it later? Let's
 * hope so. :-)
 */

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"

byte dc_temp[1200*4];
unsigned int dc_tspans[4][256];
unsigned int *dc_ctspan[4];
unsigned int *horizspan[4];

#ifndef USEASM
// Copies one span at hx to the screen at sx.
void rt_copy1col_c (int hx, int sx, int yl, int yh)
{
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	if (count & 1) {
		*dest = *source;
		source += 4;
		dest += pitch;
	}
	if (count & 2) {
		dest[0] = source[0];
		dest[pitch] = source[4];
		source += 8;
		dest += pitch*2;
	}
	if (!(count >>= 2))
		return;

	do {
		dest[0] = source[0];
		dest[pitch] = source[4];
		dest[pitch*2] = source[8];
		dest[pitch*3] = source[12];
		source += 16;
		dest += pitch*4;
	} while (--count);
}

// Copies two spans at hx and hx+1 to the screen at sx and sx+1.
void rt_copy2cols_c (int hx, int sx, int yl, int yh)
{
	short *source;
	short *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = (short *)(ylookup[yl] + sx);
	source = (short *)(&dc_temp[yl*4 + hx]);
	pitch = dc_pitch/sizeof(short);

	if (count & 1) {
		*dest = *source;
		source += 4/sizeof(short);
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = source[0];
		dest[pitch] = source[4/sizeof(short)];
		source += 8/sizeof(short);
		dest += pitch*2;
	} while (--count);
}

// Copies all four spans to the screen starting at sx.
void rt_copy4cols_c (int sx, int yl, int yh)
{
	int *source;
	int *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	dest = (int *)(ylookup[yl] + sx);
	source = (int *)(&dc_temp[yl*4]);
	pitch = dc_pitch/sizeof(int);
	
	if (count & 1) {
		*dest = *source;
		source += 4/sizeof(int);
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = source[0];
		dest[pitch] = source[4/sizeof(int)];
		source += 8/sizeof(int);
		dest += pitch*2;
	} while (--count);
}

// Maps one span at hx to the screen at sx.
void rt_map1col_c (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	if (count & 1) {
		*dest = colormap[*source];
		source += 4;
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = colormap[source[0]];
		dest[pitch] = colormap[source[4]];
		source += 8;
		dest += pitch*2;
	} while (--count);
}

// Maps two spans at hx and hx+1 to the screen at sx and sx+1.
void rt_map2cols_c (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	if (count & 1) {
		dest[0] = colormap[source[0]];
		dest[1] = colormap[source[1]];
		source += 4;
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = colormap[source[0]];
		dest[1] = colormap[source[1]];
		dest[pitch] = colormap[source[4]];
		dest[pitch+1] = colormap[source[5]];
		source += 8;
		dest += pitch*2;
	} while (--count);
}

// Maps all four spans to the screen starting at sx.
void rt_map4cols_c (int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	
	if (count & 1) {
		dest[0] = colormap[source[0]];
		dest[1] = colormap[source[1]];
		dest[2] = colormap[source[2]];
		dest[3] = colormap[source[3]];
		source += 4;
		dest += pitch;
	}
	if (!(count >>= 1))
		return;

	do {
		dest[0] = colormap[source[0]];
		dest[1] = colormap[source[1]];
		dest[2] = colormap[source[2]];
		dest[3] = colormap[source[3]];
		dest[pitch] = colormap[source[4]];
		dest[pitch+1] = colormap[source[5]];
		dest[pitch+2] = colormap[source[6]];
		dest[pitch+3] = colormap[source[7]];
		source += 8;
		dest += pitch*2;
	} while (--count);
}
#endif	/* !USEASM */

// Translates one span at hx to the screen at sx.
void rt_tlate1col (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	translation = dc_translation;
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		*dest = colormap[translation[*source]];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates two spans at hx and hx+1 to the screen at sx and sx+1.
void rt_tlate2cols (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	translation = dc_translation;
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		dest[0] = colormap[translation[source[0]]];
		dest[1] = colormap[translation[source[1]]];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates all four spans to the screen starting at sx.
void rt_tlate4cols (int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	translation = dc_translation;
	count = yh-yl;
	if (count < 0)
		return;
	count++;

	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	
	do {
		dest[0] = colormap[translation[source[0]]];
		dest[1] = colormap[translation[source[1]]];
		dest[2] = colormap[translation[source[2]]];
		dest[3] = colormap[translation[source[3]]];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds one span at hx to the screen at sx without clamping.
void rt_add1col (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD fg = colormap[*source];
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds two spans at hx and hx+1 to the screen at sx and sx+1 without clamping.
void rt_add2cols (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD fg = colormap[source[0]];
		DWORD bg = dest[0];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[0] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[source[1]];
		bg = dest[1];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[1] = RGB32k[0][0][fg & (fg>>15)];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds all four spans to the screen starting at sx without clamping.
void rt_add4cols (int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD fg = colormap[source[0]];
		DWORD bg = dest[0];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[0] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[source[1]];
		bg = dest[1];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[1] = RGB32k[0][0][fg & (fg>>15)];


		fg = colormap[source[2]];
		bg = dest[2];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[2] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[source[3]];
		bg = dest[3];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[3] = RGB32k[0][0][fg & (fg>>15)];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds one span at hx to the screen at sx without clamping.
void rt_tlateadd1col (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	translation = dc_translation;
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		DWORD fg = colormap[translation[*source]];
		DWORD bg = *dest;

		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		*dest = RGB32k[0][0][fg & (fg>>15)];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds two spans at hx and hx+1 to the screen at sx and sx+1 without clamping.
void rt_tlateadd2cols (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	translation = dc_translation;
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		DWORD fg = colormap[translation[source[0]]];
		DWORD bg = dest[0];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[0] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[translation[source[1]]];
		bg = dest[1];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[1] = RGB32k[0][0][fg & (fg>>15)];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds all four spans to the screen starting at sx without clamping.
void rt_tlateadd4cols (int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	translation = dc_translation;
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	
	do {
		DWORD fg = colormap[translation[source[0]]];
		DWORD bg = dest[0];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[0] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[translation[source[1]]];
		bg = dest[1];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[1] = RGB32k[0][0][fg & (fg>>15)];


		fg = colormap[translation[source[2]]];
		bg = dest[2];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[2] = RGB32k[0][0][fg & (fg>>15)];

		fg = colormap[translation[source[3]]];
		bg = dest[3];
		fg = fg2rgb[fg];
		bg = bg2rgb[bg];
		fg = (fg+bg) | 0x1f07c1f;
		dest[3] = RGB32k[0][0][fg & (fg>>15)];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Shades one span at hx to the screen at sx.
void rt_shaded1col (int hx, int sx, int yl, int yh)
{
	DWORD *fgstart;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	fgstart = &Col2RGB8[0][dc_color];
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		DWORD val = colormap[*source];
		DWORD fg = fgstart[val<<8];
		val = (Col2RGB8[64-val][*dest] + fg) | 0x1f07c1f;
		*dest = RGB32k[0][0][val & (val>>15)];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Shade two spans at hx and hx+1 to the screen at sx and sx+1.
void rt_shaded2cols (int hx, int sx, int yl, int yh)
{
	DWORD *fgstart;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	fgstart = &Col2RGB8[0][dc_color];
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;

	do {
		DWORD val = colormap[source[0]];
		DWORD fg = fgstart[val<<8];
		val = (Col2RGB8[64-val][dest[0]] + fg) | 0x1f07c1f;
		dest[0] = RGB32k[0][0][val & (val>>15)];

		val = colormap[source[1]];
		fg = fgstart[val<<8];
		val = (Col2RGB8[64-val][dest[1]] + fg) | 0x1f07c1f;
		dest[1] = RGB32k[0][0][val & (val>>15)];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Shades all four spans to the screen starting at sx.
void rt_shaded4cols (int sx, int yl, int yh)
{
	BYTE fill;
	DWORD *fgstart;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	fgstart = &Col2RGB8[0][dc_color];
	colormap = dc_colormap;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	{
		DWORD val = fgstart[64] | 0x1f07c1f;
		fill = RGB32k[0][0][val & (val>>15)];
	}
	
	do {
		DWORD val = colormap[source[0]];
		DWORD fg;
		if (val < 64)
		{
			fg = fgstart[val<<8];
			val = (Col2RGB8[64-val][dest[0]] + fg) | 0x1f07c1f;
			dest[0] = RGB32k[0][0][val & (val>>15)];
		}
		else
		{
			dest[0] = fill;
		}

		val = colormap[source[1]];
		if (val < 64)
		{
			fg = fgstart[val<<8];
			val = (Col2RGB8[64-val][dest[1]] + fg) | 0x1f07c1f;
			dest[1] = RGB32k[0][0][val & (val>>15)];
		}
		else
		{
			dest[1] = fill;
		}

		val = colormap[source[2]];
		if (val < 64)
		{
			fg = fgstart[val<<8];
			val = (Col2RGB8[64-val][dest[2]] + fg) | 0x1f07c1f;
			dest[2] = RGB32k[0][0][val & (val>>15)];
		}
		else
		{
			dest[2] = fill;
		}

		val = colormap[source[3]];
		if (val < 64)
		{
			fg = fgstart[val<<8];
			val = (Col2RGB8[64-val][dest[3]] + fg) | 0x1f07c1f;
			dest[3] = RGB32k[0][0][val & (val>>15)];
		}
		else
		{
			dest[3] = fill;
		}

		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds one span at hx to the screen at sx with clamping.
void rt_addclamp1col (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD a = fg2rgb[colormap[*source]] + bg2rgb[*dest];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		*dest = RGB32k[0][0][(a>>15) & a];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds two spans at hx and hx+1 to the screen at sx and sx+1 with clamping.
void rt_addclamp2cols (int hx, int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD a = fg2rgb[colormap[source[0]]] + bg2rgb[dest[0]];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[0] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[source[1]]] + bg2rgb[dest[1]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[1] = RGB32k[0][0][(a>>15) & a];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Adds all four spans to the screen starting at sx with clamping.
void rt_addclamp4cols (int sx, int yl, int yh)
{
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;

	do {
		DWORD a = fg2rgb[colormap[source[0]]] + bg2rgb[dest[0]];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[0] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[source[1]]] + bg2rgb[dest[1]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[1] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[source[2]]] + bg2rgb[dest[2]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[2] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[source[3]]] + bg2rgb[dest[3]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[3] = RGB32k[0][0][(a>>15) & a];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds one span at hx to the screen at sx with clamping.
void rt_tlateaddclamp1col (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;
	translation = dc_translation;

	do {
		DWORD a = fg2rgb[colormap[translation[*source]]] + bg2rgb[*dest];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		*dest = RGB32k[0][0][(a>>15) & a];
		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds two spans at hx and hx+1 to the screen at sx and sx+1 with clamping.
void rt_tlateaddclamp2cols (int hx, int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4 + hx];
	pitch = dc_pitch;
	colormap = dc_colormap;
	translation = dc_translation;

	do {
		DWORD a = fg2rgb[colormap[translation[source[0]]]] + bg2rgb[dest[0]];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[0] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[translation[source[1]]]] + bg2rgb[dest[1]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[1] = RGB32k[0][0][(a>>15) & a];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Translates and adds all four spans to the screen starting at sx with clamping.
void rt_tlateaddclamp4cols (int sx, int yl, int yh)
{
	byte *translation;
	byte *colormap;
	byte *source;
	byte *dest;
	int count;
	int pitch;

	count = yh-yl;
	if (count < 0)
		return;
	count++;

	DWORD *fg2rgb = dc_srcblend;
	DWORD *bg2rgb = dc_destblend;
	dest = ylookup[yl] + sx;
	source = &dc_temp[yl*4];
	pitch = dc_pitch;
	colormap = dc_colormap;
	translation = dc_translation;

	do {
		DWORD a = fg2rgb[colormap[translation[source[0]]]] + bg2rgb[dest[0]];
		DWORD b = a;

		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[0] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[translation[source[1]]]] + bg2rgb[dest[1]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[1] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[translation[source[2]]]] + bg2rgb[dest[2]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[2] = RGB32k[0][0][(a>>15) & a];

		a = fg2rgb[colormap[translation[source[3]]]] + bg2rgb[dest[3]];
		b = a;
		a |= 0x01f07c1f;
		b &= 0x40100400;
		a &= 0x3fffffff;
		b = b - (b >> 5);
		a |= b;
		dest[3] = RGB32k[0][0][(a>>15) & a];

		source += 4;
		dest += pitch;
	} while (--count);
}

// Draws all spans at hx to the screen at sx.
void rt_draw1col (int hx, int sx)
{
	while (horizspan[hx] < dc_ctspan[hx]) {
		hcolfunc_post1 (hx, sx, horizspan[hx][0], horizspan[hx][1]);
		horizspan[hx] += 2;
	}
}

// Adjusts two columns so that they both start on the same row.
// Returns false if it succeeded, true if a column ran out.
static BOOL rt_nudgecols (int hx, int sx)
{
	if (horizspan[hx][0] < horizspan[hx+1][0]) {
spaghetti1:
		// first column starts before the second; it might also end before it
		if (horizspan[hx][1] < horizspan[hx+1][0]){
			while (horizspan[hx] < dc_ctspan[hx] && horizspan[hx][1] < horizspan[hx+1][0]) {
				hcolfunc_post1 (hx, sx, horizspan[hx][0], horizspan[hx][1]);
				horizspan[hx] += 2;
			}
			if (horizspan[hx] >= dc_ctspan[hx]) {
				// the first column ran out of spans
				rt_draw1col (hx+1, sx+1);
				return true;
			}
			if (horizspan[hx][0] > horizspan[hx+1][0])
				goto spaghetti2;	// second starts before first now
			else if (horizspan[hx][0] == horizspan[hx+1][0])
				return false;
		}
		hcolfunc_post1 (hx, sx, horizspan[hx][0], horizspan[hx+1][0] - 1);
		horizspan[hx][0] = horizspan[hx+1][0];
	}
	if (horizspan[hx][0] > horizspan[hx+1][0]) {
spaghetti2:
		// second column starts before the first; it might also end before it
		if (horizspan[hx+1][1] < horizspan[hx][0]) {
			while (horizspan[hx+1] < dc_ctspan[hx+1] && horizspan[hx+1][1] < horizspan[hx][0]) {
				hcolfunc_post1 (hx+1, sx+1, horizspan[hx+1][0], horizspan[hx+1][1]);
				horizspan[hx+1] += 2;
			}
			if (horizspan[hx+1] >= dc_ctspan[hx+1]) {
				// the second column ran out of spans
				rt_draw1col (hx, sx);
				return true;
			}
			if (horizspan[hx][0] < horizspan[hx+1][0])
				goto spaghetti1;	// first starts before second now
			else if (horizspan[hx][0] == horizspan[hx+1][0])
				return false;
		}
		hcolfunc_post1 (hx+1, sx+1, horizspan[hx+1][0], horizspan[hx][0] - 1);
		horizspan[hx+1][0] = horizspan[hx][0];
	}
	return false;
}

// Copies all spans at hx and hx+1 to the screen at sx and sx+1.
// hx and sx should be word-aligned.
void rt_draw2cols (int hx, int sx)
{
loop:
	if (horizspan[hx] >= dc_ctspan[hx]) {
		// no first column, do the second (if any)
		rt_draw1col (hx+1, sx+1);
		return;
	}
	if (horizspan[hx+1] >= dc_ctspan[hx+1]) {
		// no second column, do the first
		rt_draw1col (hx, sx);
		return;
	}

	// both columns have spans, align their tops
	if (rt_nudgecols (hx, sx))
		return;

	// now draw as much as possible as a series of words
	if (horizspan[hx][1] < horizspan[hx+1][1]) {
		// first column ends first, so draw down to its bottom
		hcolfunc_post2 (hx, sx, horizspan[hx][0], horizspan[hx][1]);
		horizspan[hx+1][0] = horizspan[hx][1] + 1;
		horizspan[hx] += 2;
	} else {
		// second column ends first, or they end at the same spot
		hcolfunc_post2 (hx, sx, horizspan[hx+1][0], horizspan[hx+1][1]);
		if (horizspan[hx][1] == horizspan[hx+1][1]) {
			horizspan[hx] += 2;
			horizspan[hx+1] += 2;
		} else {
			horizspan[hx][0] = horizspan[hx+1][1] + 1;
			horizspan[hx+1] += 2;
		}
	}

	goto loop;	// keep going until all columns have no more spans
}

// Copies all spans in all four columns to the screen starting at sx.
// sx should be longword-aligned.
void rt_draw4cols (int sx)
{
loop:
	if (horizspan[0] >= dc_ctspan[0]) {
		// no first column, do the second (if any)
		rt_draw1col (1, sx+1);
		rt_draw2cols (2, sx+2);
		return;
	}
	if (horizspan[1] >= dc_ctspan[1]) {
		// no second column, we already know there is a first one
		rt_draw1col (0, sx);
		rt_draw2cols (2, sx+2);
		return;
	}
	if (horizspan[2] >= dc_ctspan[2]) {
		// no third column, do the fourth (if any)
		rt_draw2cols (0, sx);
		rt_draw1col (3, sx+3);
		return;
	}
	if (horizspan[3] >= dc_ctspan[3]) {
		// no fourth column, but there is a third
		rt_draw2cols (0, sx);
		rt_draw1col (2, sx+2);
		return;
	}

	// if we get here, then we know all four columns have something,
	// make sure they all align at the top
	if (rt_nudgecols (0, sx)) {
		rt_draw2cols (2, sx+2);
		return;
	}
	if (rt_nudgecols (2, sx+2)) {
		rt_draw2cols (0, sx);
		return;
	}

	// first column is now aligned with second at top, and third is aligned
	// with fourth at top. now make sure both halves align at the top and
	// also have some shared space to their bottoms.
	{
		unsigned int bot1 = horizspan[0][1] < horizspan[1][1] ? horizspan[0][1] : horizspan[1][1];
		unsigned int bot2 = horizspan[2][1] < horizspan[3][1] ? horizspan[2][1] : horizspan[3][1];

		if (horizspan[0][0] < horizspan[2][0]) {
			// first half starts before second half
			if (bot1 >= horizspan[2][0]) {
				// first half ends after second begins
				hcolfunc_post2 (0, sx, horizspan[0][0], horizspan[2][0] - 1);
				horizspan[0][0] = horizspan[1][0] = horizspan[2][0];
			} else {
				// first half ends before second begins
				hcolfunc_post2 (0, sx, horizspan[0][0], bot1);
				if (horizspan[0][1] == bot1)
					horizspan[0] += 2;
				else
					horizspan[0][0] = bot1 + 1;
				if (horizspan[1][1] == bot1)
					horizspan[1] += 2;
				else
					horizspan[1][0] = bot1 + 1;
				goto loop;	// start over
			}
		} else if (horizspan[0][0] > horizspan[2][0]) {
			// second half starts before the first
			if (bot2 >= horizspan[0][0]) {
				// second half ends after first begins
				hcolfunc_post2 (2, sx+2, horizspan[2][0], horizspan[0][0] - 1);
				horizspan[2][0] = horizspan[3][0] = horizspan[0][0];
			} else {
				// second half ends before first begins
				hcolfunc_post2 (2, sx+2, horizspan[2][0], bot2);
				if (horizspan[2][1] == bot2)
					horizspan[2] += 2;
				else
					horizspan[2][0] = bot2 + 1;
				if (horizspan[3][1] == bot2)
					horizspan[3] += 2;
				else
					horizspan[3][0] = bot2 + 1;
				goto loop;	// start over
			}
		}

		// all four columns are now aligned at the top; draw all of them
		// until one ends.
		bot1 = bot1 < bot2 ? bot1 : bot2;

		hcolfunc_post4 (sx, horizspan[0][0], bot1);

		{
			int x;

			for (x = 3; x >= 0; x--)
				if (horizspan[x][1] == bot1)
					horizspan[x] += 2;
				else
					horizspan[x][0] = bot1 + 1;
		}
	}

	goto loop;	// keep going until all columns have no more spans
}

// Before each pass through a rendering loop that uses these routines,
// call this function to set up the span pointers.
void rt_initcols (void)
{
	int y;

	for (y = 3; y >= 0; y--)
		horizspan[y] = dc_ctspan[y] = &dc_tspans[y][0];
}

// Stretches a column into a temporary buffer which is later
// drawn to the screen along with up to three other columns.
void R_DrawColumnHorizP_C (void)
{
	int count = dc_yh - dc_yl;
	byte *dest;
	fixed_t fracstep;
	fixed_t frac;

	if (count++ < 0)
		return;

	{
		int x = dc_x & 3;
		unsigned int **span = &dc_ctspan[x];

		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;
		dest = &dc_temp[x + 4*dc_yl];
	}
	fracstep = dc_iscale;
	frac = dc_texturefrac;

	{
		int mask = dc_mask;
		byte *source = dc_source;

		if (count & 1) {
			*dest = source[(frac>>FRACBITS) & mask];
			dest += 4;
			frac += fracstep;
		}
		if (count & 2) {
			dest[0] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[4] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest += 8;
		}
		if (count & 4) {
			dest[0] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[4] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[8] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[12] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest += 16;
		}
		count >>= 3;
		if (!count) return;

		do
		{
			dest[0] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[4] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[8] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[12] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[16] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[20] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[24] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest[28] = source[(frac>>FRACBITS) & mask];
			frac += fracstep;
			dest += 32;
		} while (--count);
	}
}

// [RH] Just fills a column with a given color
void R_FillColumnHorizP (void)
{
	int count = dc_yh - dc_yl;
	byte color = dc_color;
	byte *dest;

	if (count++ < 0)
		return;

	count++;
	{
		int x = dc_x & 3;
		unsigned int **span = &dc_ctspan[x];

		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;
		dest = &dc_temp[x + 4*dc_yl];
	}

	if (count & 1) {
		*dest = color;
		dest += 4;
	}
	if (!(count >>= 1))
		return;
	do {
		dest[0] = color;
		dest[4] = color;
		dest += 8;
	} while (--count);
}

// Same as R_DrawMaskedColumn() except that it always uses
// R_DrawColumnHoriz().

void R_DrawMaskedColumnHoriz (column_t *column, int baseclip)
{
	for (; column->topdelta != 0xff; column = (column_t *)((byte *)column + column->length + 4))
	{
		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * column->topdelta) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (column->topdelta + column->length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swap (dc_yl, dc_yh);
		}

		if (dc_yh >= mfloorclip[dc_x])
		{
			dc_yh = mfloorclip[dc_x] - 1;
		}
		if (dc_yh > baseclip)
		{
			dc_yh = baseclip;
		}
		if (dc_yl < mceilingclip[dc_x])
		{
			dc_yl = mceilingclip[dc_x];
		}

		if (dc_yl <= dc_yh)
		{
			if (sprflipvert)
			{
				dc_texturefrac = (dc_yl*dc_iscale) - (column->topdelta << FRACBITS)
					- FixedMul (centeryfrac, dc_iscale) - dc_texturemid;
				if (dc_texturefrac >= column->length << FRACBITS)
				{
					if (++dc_yl > dc_yh)
						continue;
					dc_texturefrac += dc_iscale;
				}
			}
			else
			{
				dc_texturefrac = dc_texturemid - (column->topdelta << FRACBITS)
					+ (dc_yl*dc_iscale) - FixedMul (centeryfrac-FRACUNIT, dc_iscale);
				if (dc_texturefrac < 0)
				{
					if (++dc_yl > dc_yh)
						continue;
					dc_texturefrac += dc_iscale;
				}
			}
			dc_source = (byte *)column + 3;
			if (r_MarkTrans)
				TransArea += dc_yh - dc_yl + 1;
			hcolfunc_pre ();
		}
	}
}
