/*
** v_pfx.cpp
** Pixel format conversion routines
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "doomtype.h"
#include "i_system.h"
#include "v_pfx.h"

PfxUnion GPfxPal;
PfxState GPfx;

static bool AnalyzeMask (uint32_t mask, uint8_t *shift);

static void Palette16Generic (const PalEntry *pal);
static void Palette16R5G5B5 (const PalEntry *pal);
static void Palette16R5G6B5 (const PalEntry *pal);
static void Palette32Generic (const PalEntry *pal);
static void Palette32RGB (const PalEntry *pal);
static void Palette32BGR (const PalEntry *pal);

static void Scale8 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
static void Convert8 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
static void Convert16 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
static void Convert24 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
static void Convert32 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);

void PfxState::SetFormat (int bits, uint32_t redMask, uint32_t greenMask, uint32_t blueMask)
{
	switch (bits)
	{
	case -8:
		Convert = Scale8;
		SetPalette = NULL;
		break;

	case 8:
		Convert = Convert8;
		SetPalette = NULL;
		break;

	case 16:
		if (redMask == 0x7c00 && greenMask == 0x03e0 && blueMask == 0x001f)
		{
			SetPalette = Palette16R5G5B5;
		}
		else if (redMask == 0xf800 && greenMask == 0x07e0 && blueMask == 0x001f)
		{
			SetPalette = Palette16R5G6B5;
		}
		else
		{
			SetPalette = Palette16Generic;
		}
		Convert = Convert16;
		Masks.Bits16.Red = (uint16_t)redMask;
		Masks.Bits16.Green = (uint16_t)greenMask;
		Masks.Bits16.Blue = (uint16_t)blueMask;
		break;

	case 24:
		if (redMask == 0xff0000 && greenMask == 0x00ff00 && blueMask == 0x0000ff)
		{
			SetPalette = Palette32RGB;
			Convert = Convert24;
		}
		else if (redMask == 0x0000ff && greenMask == 0x00ff00 && blueMask == 0xff0000)
		{
			SetPalette = Palette32BGR;
			Convert = Convert24;
		}
		else
		{
			I_FatalError ("24-bit displays are only supported if they are RGB or BGR");
		};
		break;

	case 32:
		if (redMask == 0xff0000 && greenMask == 0x00ff00 && blueMask == 0x0000ff)
		{
			SetPalette = Palette32RGB;
		}
		else if (redMask == 0x0000ff && greenMask == 0x00ff00 && blueMask == 0xff0000)
		{
			SetPalette = Palette32BGR;
		}
		else
		{
			SetPalette = Palette32Generic;
		}
		Convert = Convert32;
		Masks.Bits32.Red = redMask;
		Masks.Bits32.Green = greenMask;
		Masks.Bits32.Blue = blueMask;
		break;

	default:
		I_FatalError ("Can't draw to %d-bit displays", bits);
	}
	if (bits != 8 && bits != -8)
	{
		RedLeft = AnalyzeMask (redMask, &RedShift);
		GreenLeft = AnalyzeMask (greenMask, &GreenShift);
		BlueLeft = AnalyzeMask (blueMask, &BlueShift);
	}
}

static bool AnalyzeMask (uint32_t mask, uint8_t *shiftout)
{
	uint8_t shift = 0;

	if (mask >= 0xff)
	{
		while (mask > 0xff)
		{
			shift++;
			mask >>= 1;
		}
		*shiftout = shift;
		return true;
	}
	else
	{
		while (mask < 0xff)
		{
			shift++;
			mask <<= 1;
		}
		*shiftout = shift;
		return false;
	}
}

// Palette converters ------------------------------------------------------

static void Palette16Generic (const PalEntry *pal)
{
	uint16_t *p16;
	int i;

	for (p16 = GPfxPal.Pal16, i = 256; i != 0; i--, pal++, p16++)
	{
		uint16_t rpart, gpart, bpart;

		if (GPfx.RedLeft)	rpart = pal->r << GPfx.RedShift;
		else				rpart = pal->r >> GPfx.RedShift;

		if (GPfx.GreenLeft)	gpart = pal->g << GPfx.GreenShift;
		else				gpart = pal->g >> GPfx.GreenShift;

		if (GPfx.BlueLeft)	bpart = pal->b << GPfx.BlueShift;
		else				bpart = pal->b >> GPfx.BlueShift;

		*p16 = (rpart & GPfx.Masks.Bits16.Red) |
			   (gpart & GPfx.Masks.Bits16.Green) |
			   (bpart & GPfx.Masks.Bits16.Blue);
	}
}

static void Palette16R5G5B5 (const PalEntry *pal)
{
	uint16_t *p16;
	int i;

	for (p16 = GPfxPal.Pal16, i = 256; i != 0; i--, pal++, p16++)
	{
		*p16 = ((pal->r << 7) & 0x7c00) |
			   ((pal->g << 2) & 0x03e0) |
			   ((pal->b >> 3) & 0x001f);
	}
}

static void Palette16R5G6B5 (const PalEntry *pal)
{
	uint16_t *p16;
	int i;

	for (p16 = GPfxPal.Pal16, i = 256; i != 0; i--, pal++, p16++)
	{
		*p16 = ((pal->r << 8) & 0xf800) |
			   ((pal->g << 3) & 0x07e0) |
			   ((pal->b >> 3) & 0x001f);
	}
}

static void Palette32Generic (const PalEntry *pal)
{
	uint32_t *p32;
	int i;

	for (p32 = GPfxPal.Pal32, i = 256; i != 0; i--, pal++, p32++)
	{
		uint32_t rpart, gpart, bpart;

		if (GPfx.RedLeft)	rpart = pal->r << GPfx.RedShift;
		else				rpart = pal->r >> GPfx.RedShift;

		if (GPfx.GreenLeft)	gpart = pal->g << GPfx.GreenShift;
		else				gpart = pal->g >> GPfx.GreenShift;

		if (GPfx.BlueLeft)	bpart = pal->b << GPfx.BlueShift;
		else				bpart = pal->b >> GPfx.BlueShift;

		*p32 = (rpart & GPfx.Masks.Bits32.Red) |
			   (gpart & GPfx.Masks.Bits32.Green) |
			   (bpart & GPfx.Masks.Bits32.Blue);
	}
}

static void Palette32RGB (const PalEntry *pal)
{
	memcpy (GPfxPal.Pal32, pal, 256*4);
}

static void Palette32BGR (const PalEntry *pal)
{
	uint32_t *p32;
	int i;

	for (p32 = GPfxPal.Pal32, i = 256; i != 0; i--, pal++, p32++)
	{
		*p32 = (pal->r) | (pal->g << 8) | (pal->b << 16);
	}
}

// Bitmap converters -------------------------------------------------------

static void Scale8 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac)
{
	if ((destwidth | destheight) == 0)
	{
		return;
	}

	int x, y, savedx;
	uint8_t *dest = (uint8_t *)destin;

	if (xstep == FRACUNIT && ystep == FRACUNIT)
	{
		for (y = destheight; y != 0; y--)
		{
			memcpy(dest, src, destwidth);
			dest += destpitch;
			src += srcpitch;
		}
	}
	else if (xstep == FRACUNIT/2 && ystep == FRACUNIT/2)
	{
		uint8_t *dest2 = dest + destpitch;
		destpitch = destpitch * 2 - destwidth;
		srcpitch -= destwidth / 2;
		for (y = destheight / 2; y != 0; --y)
		{
			for (x = destwidth / 2; x != 0; --x)
			{
				uint8_t foo = src[0];
				dest[0] = foo;
				dest[1] = foo;
				dest2[0] = foo;
				dest2[1] = foo;
				dest += 2;
				dest2 += 2;
				src += 1;
			}
			dest += destpitch;
			dest2 += destpitch;
			src += srcpitch;
		}
	}
	else if (xstep == FRACUNIT/4 && ystep == FRACUNIT/4)
	{
		int gap = destpitch * 4 - destwidth;
		srcpitch -= destwidth / 4;
		for (y = destheight / 4; y != 0; --y)
		{
			for (uint8_t *end = dest + destpitch; dest != end; dest += 4)
			{
				uint8_t foo = src[0];
				dest[0] = foo;
				dest[1] = foo;
				dest[2] = foo;
				dest[3] = foo;
				dest[0 + destpitch] = foo;
				dest[1 + destpitch] = foo;
				dest[2 + destpitch] = foo;
				dest[3 + destpitch] = foo;
				dest[0 + destpitch*2] = foo;
				dest[1 + destpitch*2] = foo;
				dest[2 + destpitch*2] = foo;
				dest[3 + destpitch*2] = foo;
				dest[0 + destpitch*3] = foo;
				dest[1 + destpitch*3] = foo;
				dest[2 + destpitch*3] = foo;
				dest[3 + destpitch*3] = foo;
				src += 1;
			}
			dest += gap;
			src += srcpitch;
		}
	}
	else
	{
		destpitch -= destwidth;
		for (y = destheight; y != 0; y--)
		{
			fixed_t xf = xfrac;
			x = destwidth;
			while (((size_t)dest & 3) && x != 0)
			{
				*dest++ = src[xf >> FRACBITS];
				xf += xstep;
				x--;
			}
			for (savedx = x, x >>= 2; x != 0; x--)
			{
				uint32_t work;

#ifdef __BIG_ENDIAN__
				work  = src[xf >> FRACBITS] << 24;	xf += xstep;
				work |= src[xf >> FRACBITS] << 16;	xf += xstep;
				work |= src[xf >> FRACBITS] << 8;	xf += xstep;
				work |= src[xf >> FRACBITS];		xf += xstep;
#else
				work  = src[xf >> FRACBITS];		xf += xstep;
				work |= src[xf >> FRACBITS] << 8;	xf += xstep;
				work |= src[xf >> FRACBITS] << 16;	xf += xstep;
				work |= src[xf >> FRACBITS] << 24;	xf += xstep;
#endif
				*(uint32_t *)dest = work;
				dest += 4;
			}
			for (savedx &= 3; savedx != 0; savedx--, xf += xstep)
			{
				*dest++ = src[xf >> FRACBITS];
			}
			yfrac += ystep;
			while (yfrac >= FRACUNIT)
			{
				yfrac -= FRACUNIT;
				src += srcpitch;
			}
			dest += destpitch;
		}
	}
}

static void Convert8 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac)
{
	if ((destwidth | destheight) == 0)
	{
		return;
	}

	int x, y, savedx;
	uint8_t *dest = (uint8_t *)destin;

	destpitch -= destwidth;
	if (xstep == FRACUNIT && ystep == FRACUNIT)
	{
		srcpitch -= destwidth;
		for (y = destheight; y != 0; y--)
		{
			x = destwidth;
			while (((size_t)dest & 3) && x != 0)
			{
				*dest++ = GPfxPal.Pal8[*src++];
				x--;
			}
			for (savedx = x, x >>= 2; x != 0; x--)
			{
				*(uint32_t *)dest =
#ifdef __BIG_ENDIAN__
					(GPfxPal.Pal8[src[0]] << 24) |
					(GPfxPal.Pal8[src[1]] << 16) |
					(GPfxPal.Pal8[src[2]] << 8) |
					(GPfxPal.Pal8[src[3]]);
#else
					(GPfxPal.Pal8[src[0]]) |
					(GPfxPal.Pal8[src[1]] << 8) |
					(GPfxPal.Pal8[src[2]] << 16) |
					(GPfxPal.Pal8[src[3]] << 24);
#endif
				dest += 4;
				src += 4;
			}
			for (savedx &= 3; savedx != 0; savedx--)
			{
				*dest++ = GPfxPal.Pal8[*src++];
			}
			dest += destpitch;
			src += srcpitch;
		}
	}
	else
	{
		for (y = destheight; y != 0; y--)
		{
			fixed_t xf = xfrac;
			x = destwidth;
			while (((size_t)dest & 3) && x != 0)
			{
				*dest++ = GPfxPal.Pal8[src[xf >> FRACBITS]];
				xf += xstep;
				x--;
			}
			for (savedx = x, x >>= 2; x != 0; x--)
			{
				uint32_t work;

#ifdef __BIG_ENDIAN__
				work  = GPfxPal.Pal8[src[xf >> FRACBITS]] << 24;	xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]] << 16;	xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]] << 8;		xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]];			xf += xstep;
#else
				work  = GPfxPal.Pal8[src[xf >> FRACBITS]];			xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]] << 8;		xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]] << 16;	xf += xstep;
				work |= GPfxPal.Pal8[src[xf >> FRACBITS]] << 24;	xf += xstep;
#endif
				*(uint32_t *)dest = work;
				dest += 4;
			}
			for (savedx &= 3; savedx != 0; savedx--, xf += xstep)
			{
				*dest++ = GPfxPal.Pal8[src[xf >> FRACBITS]];
			}
			yfrac += ystep;
			while (yfrac >= FRACUNIT)
			{
				yfrac -= FRACUNIT;
				src += srcpitch;
			}
			dest += destpitch;
		}
	}
}

static void Convert16 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac)
{
	if ((destwidth | destheight) == 0)
	{
		return;
	}

	int x, y, savedx;
	uint16_t *dest = (uint16_t *)destin;

	destpitch = (destpitch >> 1) - destwidth;
	if (xstep == FRACUNIT && ystep == FRACUNIT)
	{
		srcpitch -= destwidth;
		for (y = destheight; y != 0; y--)
		{
			x = destwidth;
			if ((size_t)dest & 1)
			{
				x--;
				*dest++ = GPfxPal.Pal16[*src++];
			}
			for (savedx = x, x >>= 1; x != 0; x--)
			{
				*(uint32_t *)dest =
#ifdef __BIG_ENDIAN__
					(GPfxPal.Pal16[src[0]] << 16) |
					(GPfxPal.Pal16[src[1]]);
#else
					(GPfxPal.Pal16[src[0]]) |
					(GPfxPal.Pal16[src[1]] << 16);
#endif
				dest += 2;
				src += 2;
			}
			if (savedx & 1)
			{
				*dest++ = GPfxPal.Pal16[*src++];
			}
			dest += destpitch;
			src += srcpitch;
		}
	}
	else
	{
		for (y = destheight; y != 0; y--)
		{
			fixed_t xf = xfrac;
			x = destwidth;
			if ((size_t)dest & 1)
			{
				*dest++ = GPfxPal.Pal16[src[xf >> FRACBITS]];
				xf += xstep;
				x--;
			}
			for (savedx = x, x >>= 1; x != 0; x--)
			{
				uint32_t work;

#ifdef __BIG_ENDIAN__
				work  = GPfxPal.Pal16[src[xf >> FRACBITS]] << 16;	xf += xstep;
				work |= GPfxPal.Pal16[src[xf >> FRACBITS]];			xf += xstep;
#else
				work  = GPfxPal.Pal16[src[xf >> FRACBITS]];			xf += xstep;
				work |= GPfxPal.Pal16[src[xf >> FRACBITS]] << 16;	xf += xstep;
#endif
				*(uint32_t *)dest = work;
				dest += 2;
			}
			if (savedx & 1)
			{
				*dest++ = GPfxPal.Pal16[src[xf >> FRACBITS]];
			}
			yfrac += ystep;
			while (yfrac >= FRACUNIT)
			{
				yfrac -= FRACUNIT;
				src += srcpitch;
			}
			dest += destpitch;
		}
	}
}

static void Convert24 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac)
{
	if ((destwidth | destheight) == 0)
	{
		return;
	}

	int x, y;
	uint8_t *dest = (uint8_t *)destin;

	destpitch = destpitch - destwidth*3;
	if (xstep == FRACUNIT && ystep == FRACUNIT)
	{
		srcpitch -= destwidth;
		for (y = destheight; y != 0; y--)
		{
			for (x = destwidth; x != 0; x--)
			{
				uint8_t *pe = GPfxPal.Pal24[src[0]];
				dest[0] = pe[0];
				dest[1] = pe[1];
				dest[2] = pe[2];
				dest += 3;
				src++;
			}
			dest += destpitch;
			src += srcpitch;
		}
	}
	else
	{
		for (y = destheight; y != 0; y--)
		{
			fixed_t xf = xfrac;
			for (x = destwidth; x != 0; x--)
			{
				uint8_t *pe = GPfxPal.Pal24[src[xf >> FRACBITS]];
				dest[0] = pe[0];
				dest[1] = pe[1];
				dest[2] = pe[2];
				xf += xstep;
				dest += 2;
			}
			yfrac += ystep;
			while (yfrac >= FRACUNIT)
			{
				yfrac -= FRACUNIT;
				src += srcpitch;
			}
			dest += destpitch;
		}
	}
}

static void Convert32 (uint8_t *src, int srcpitch,
	void *destin, int destpitch, int destwidth, int destheight,
	fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac)
{
	if ((destwidth | destheight) == 0)
	{
		return;
	}

	int x, y, savedx;
	uint32_t *dest = (uint32_t *)destin;

	destpitch = (destpitch >> 2) - destwidth;
	if (xstep == FRACUNIT && ystep == FRACUNIT)
	{
		srcpitch -= destwidth;
		for (y = destheight; y != 0; y--)
		{
			for (savedx = x = destwidth, x >>= 3; x != 0; x--)
			{
				dest[0] = GPfxPal.Pal32[src[0]];
				dest[1] = GPfxPal.Pal32[src[1]];
				dest[2] = GPfxPal.Pal32[src[2]];
				dest[3] = GPfxPal.Pal32[src[3]];
				dest[4] = GPfxPal.Pal32[src[4]];
				dest[5] = GPfxPal.Pal32[src[5]];
				dest[6] = GPfxPal.Pal32[src[6]];
				dest[7] = GPfxPal.Pal32[src[7]];
				dest += 8;
				src += 8;
			}
			for (x = savedx & 7; x != 0; x--)
			{
				*dest++ = GPfxPal.Pal32[*src++];
			}
			dest += destpitch;
			src += srcpitch;
		}
	}
	else
	{
		for (y = destheight; y != 0; y--)
		{
			fixed_t xf = xfrac;
			for (savedx = x = destwidth, x >>= 1; x != 0; x--)
			{
				dest[0] = GPfxPal.Pal32[src[xf >> FRACBITS]];		xf += xstep;
				dest[1] = GPfxPal.Pal32[src[xf >> FRACBITS]];		xf += xstep;
				dest += 2;
			}
			if (savedx & 1)
			{
				*dest++ = GPfxPal.Pal32[src[xf >> FRACBITS]];
			}
			yfrac += ystep;
			while (yfrac >= FRACUNIT)
			{
				yfrac -= FRACUNIT;
				src += srcpitch;
			}
			dest += destpitch;
		}
	}
}
