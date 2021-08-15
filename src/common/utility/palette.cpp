/*
** palette.cpp
** Palette and color utility functions
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

#include <algorithm>
#include "palutil.h"
#include "palentry.h"
#include "sc_man.h"
#include "files.h"
#include "filesystem.h"
#include "printf.h"
#include "templates.h"
#include "m_png.h"

/****************************/
/* Palette management stuff */
/****************************/

int BestColor (const uint32_t *pal_in, int r, int g, int b, int first, int num, const uint8_t* indexmap)
{
	const PalEntry *pal = (const PalEntry *)pal_in;
	int bestcolor = first;
	int bestdist = 257 * 257 + 257 * 257 + 257 * 257;

	for (int color = first; color < num; color++)
	{
		int co = indexmap ? indexmap[color] : color;
		int x = r - pal[co].r;
		int y = g - pal[co].g;
		int z = b - pal[co].b;
		int dist = x*x + y*y + z*z;
		if (dist < bestdist)
		{
			if (dist == 0)
				return co;

			bestdist = dist;
			bestcolor = co;
		}
	}
	return bestcolor;
}


// [SP] Re-implemented BestColor for more precision rather than speed. This function is only ever called once until the game palette is changed.

int PTM_BestColor (const uint32_t *pal_in, int r, int g, int b, bool reverselookup, float powtable_val, int first, int num)
{
	const PalEntry *pal = (const PalEntry *)pal_in;
	static double powtable[256];
	static bool firstTime = true;
	static float trackpowtable = 0.;

	double fbestdist = DBL_MAX, fdist;
	int bestcolor = 0;

	if (firstTime || trackpowtable != powtable_val)
	{
		auto pt = powtable_val;
		trackpowtable = pt;
		firstTime = false;
		for (int x = 0; x < 256; x++) powtable[x] = pow((double)x/255, (double)pt);
	}

	for (int color = first; color < num; color++)
	{
		double x = powtable[abs(r-pal[color].r)];
		double y = powtable[abs(g-pal[color].g)];
		double z = powtable[abs(b-pal[color].b)];
		fdist = x + y + z;
		if (color == first || (reverselookup?(fdist <= fbestdist):(fdist < fbestdist)))
		{
			if (fdist == 0 && !reverselookup)
				return color;

			fbestdist = fdist;
			bestcolor = color;
		}
	}
	return bestcolor;
}

#if defined(_M_X64) || defined(_M_IX86) || defined(__i386__) || defined(__amd64__)

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <emmintrin.h>

static void DoBlending_SSE2(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	__m128i blendcolor;
	__m128i blendalpha;
	__m128i zero;
	__m128i blending256;
	__m128i color1;
	__m128i color2;
	size_t unaligned;

	unaligned = ((size_t)from | (size_t)to) & 0xF;

#if defined(__amd64__) || defined(_M_X64)
	int64_t color;

	blending256 = _mm_set_epi64x(0x10001000100ll, 0x10001000100ll);

	color = ((int64_t)r << 32) | (g << 16) | b;
	blendcolor = _mm_set_epi64x(color, color);

	color = ((int64_t)a << 32) | (a << 16) | a;
	blendalpha = _mm_set_epi64x(color, color);
#else
	int color;

	blending256 = _mm_set_epi32(0x100, 0x1000100, 0x100, 0x1000100);

	color = (g << 16) | b;
	blendcolor = _mm_set_epi32(r, color, r, color);

	color = (a << 16) | a;
	blendalpha = _mm_set_epi32(a, color, a, color);
#endif

	blendcolor = _mm_mullo_epi16(blendcolor, blendalpha);	// premultiply blend by alpha
	blendalpha = _mm_subs_epu16(blending256, blendalpha);	// one minus alpha

	zero = _mm_setzero_si128();

	if (unaligned)
	{
		for (count >>= 2; count > 0; --count)
		{
			color1 = _mm_loadu_si128((__m128i *)from);
			from += 4;
			color2 = _mm_unpackhi_epi8(color1, zero);
			color1 = _mm_unpacklo_epi8(color1, zero);
			color1 = _mm_mullo_epi16(blendalpha, color1);
			color2 = _mm_mullo_epi16(blendalpha, color2);
			color1 = _mm_adds_epu16(blendcolor, color1);
			color2 = _mm_adds_epu16(blendcolor, color2);
			color1 = _mm_srli_epi16(color1, 8);
			color2 = _mm_srli_epi16(color2, 8);
			_mm_storeu_si128((__m128i *)to, _mm_packus_epi16(color1, color2));
			to += 4;
		}
	}
	else
	{
		for (count >>= 2; count > 0; --count)
		{
			color1 = _mm_load_si128((__m128i *)from);
			from += 4;
			color2 = _mm_unpackhi_epi8(color1, zero);
			color1 = _mm_unpacklo_epi8(color1, zero);
			color1 = _mm_mullo_epi16(blendalpha, color1);
			color2 = _mm_mullo_epi16(blendalpha, color2);
			color1 = _mm_adds_epu16(blendcolor, color1);
			color2 = _mm_adds_epu16(blendcolor, color2);
			color1 = _mm_srli_epi16(color1, 8);
			color2 = _mm_srli_epi16(color2, 8);
			_mm_store_si128((__m128i *)to, _mm_packus_epi16(color1, color2));
			to += 4;
		}
	}
}
#endif

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	if (a == 0)
	{
		if (from != to)
		{
			memcpy (to, from, count * sizeof(uint32_t));
		}
		return;
	}
	else if (a == 256)
	{
		uint32_t t = MAKERGB(r,g,b);
		int i;

		for (i = 0; i < count; i++)
		{
			to[i] = t;
		}
		return;
	}
#if defined(_M_X64) || defined(_M_IX86) || defined(__i386__) || defined(__amd64__)
	else if (count >= 4)
	{
		int not3count = count & ~3;
		DoBlending_SSE2 (from, to, not3count, r, g, b, a);
		count &= 3;
		if (count <= 0)
		{
			return;
		}
		from += not3count;
		to += not3count;
	}
#endif
	int i, ia;

	ia = 256 - a;
	r *= a;
	g *= a;
	b *= a;

	for (i = count; i > 0; i--, to++, from++)
	{
		to->r = (r + from->r * ia) >> 8;
		to->g = (g + from->g * ia) >> 8;
		to->b = (b + from->b * ia) >> 8;
	}
}

/****** Colorspace Conversion Functions ******/

// Code from http://www.cs.rit.edu/~yxv4997/t_convert.html

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//				if s == 0, then h = -1 (undefined)

// Green Doom guy colors:
// RGB - 0: {    .46  1 .429 } 7: {    .254 .571 .206 } 15: {    .0317 .0794 .0159 }
// HSV - 0: { 116.743 .571 1 } 7: { 112.110 .639 .571 } 15: { 105.071  .800 .0794 }
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta, foo;

	if (r == g && g == b)
	{
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}

void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0)
	{ // achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor (h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:		*r = v; *g = t; *b = p; break;
	case 1:		*r = q; *g = v; *b = p; break;
	case 2:		*r = p; *g = v; *b = t; break;
	case 3:		*r = p; *g = q; *b = v; break;
	case 4:		*r = t; *g = p; *b = v; break;
	default:	*r = v; *g = p; *b = q; break;
	}
}

struct RemappingWork
{
	uint32_t Color;
	uint8_t Foreign;	// 0 = local palette, 1 = foreign palette
	uint8_t PalEntry;	// Entry # in the palette
	uint8_t Pad[2];
};

static int sortforremap(const void* a, const void* b)
{
	return (*(const uint32_t*)a & 0xFFFFFF) - (*(const uint32_t*)b & 0xFFFFFF);
}

static int sortforremap2(const void* a, const void* b)
{
	const RemappingWork* ap = (const RemappingWork*)a;
	const RemappingWork* bp = (const RemappingWork*)b;

	if (ap->Color == bp->Color)
	{
		return bp->Foreign - ap->Foreign;
	}
	else
	{
		return ap->Color - bp->Color;
	}
}


void MakeRemap(uint32_t* BaseColors, const uint32_t* colors, uint8_t* remap, const uint8_t* useful, int numcolors) 
{
	RemappingWork workspace[255 + 256];
	int i, j, k;

	// Fill in workspace with the colors from the passed palette and this palette.
	// By sorting this array, we can quickly find exact matches so that we can
	// minimize the time spent calling BestColor for near matches.

	for (i = 1; i < 256; ++i)
	{
		workspace[i - 1].Color = uint32_t(BaseColors[i]) & 0xFFFFFF;
		workspace[i - 1].Foreign = 0;
		workspace[i - 1].PalEntry = i;
	}
	for (i = k = 0, j = 255; i < numcolors; ++i)
	{
		if (useful == NULL || useful[i] != 0)
		{
			workspace[j].Color = colors[i] & 0xFFFFFF;
			workspace[j].Foreign = 1;
			workspace[j].PalEntry = i;
			++j;
			++k;
		}
		else
		{
			remap[i] = 0;
		}
	}
	qsort(workspace, j, sizeof(RemappingWork), sortforremap2);

	// Find exact matches
	--j;
	for (i = 0; i < j; ++i)
	{
		if (workspace[i].Foreign)
		{
			if (!workspace[i + 1].Foreign && workspace[i].Color == workspace[i + 1].Color)
			{
				remap[workspace[i].PalEntry] = workspace[i + 1].PalEntry;
				workspace[i].Foreign = 2;
				++i;
				--k;
			}
		}
	}

	// Find near matches
	if (k > 0)
	{
		for (i = 0; i <= j; ++i)
		{
			if (workspace[i].Foreign == 1)
			{
				remap[workspace[i].PalEntry] = BestColor((uint32_t*)BaseColors,
					RPART(workspace[i].Color), GPART(workspace[i].Color), BPART(workspace[i].Color),
					1, 255);
			}
		}
	}
}

// In ZDoom's new texture system, color 0 is used as the transparent color.
// But color 0 is also a valid color for Doom engine graphics. What to do?
// Simple. The default palette for every game has at least one duplicate
// color, so find a duplicate pair of palette entries, make one of them a
// duplicate of color 0, and remap every graphic so that it uses that entry
// instead of entry 0.
void MakeGoodRemap(uint32_t* BaseColors, uint8_t* Remap)
{
	for (int i = 0; i < 256; i++) Remap[i] = i;
	PalEntry color0 = BaseColors[0];
	int i;

	// First try for an exact match of color 0. Only Hexen does not have one.
	for (i = 1; i < 256; ++i)
	{
		if (BaseColors[i] == color0)
		{
			Remap[0] = i;
			break;
		}
	}

	// If there is no duplicate of color 0, find the first set of duplicate
	// colors and make one of them a duplicate of color 0. In Hexen's PLAYPAL
	// colors 209 and 229 are the only duplicates, but we cannot assume
	// anything because the player might be using a custom PLAYPAL where those
	// entries are not duplicates.
	if (Remap[0] == 0)
	{
		PalEntry sortcopy[256];

		for (i = 0; i < 256; ++i)
		{
			sortcopy[i] = (BaseColors[i] & 0xffffff) | (i << 24);
		}
		qsort(sortcopy, 256, 4, sortforremap);
		for (i = 255; i > 0; --i)
		{
			if ((sortcopy[i] & 0xFFFFFF) == (sortcopy[i - 1] & 0xFFFFFF))
			{
				int new0 = sortcopy[i].a;
				int dup = sortcopy[i - 1].a;
				if (new0 > dup)
				{
					// Make the lower-numbered entry a copy of color 0. (Just because.)
					std::swap(new0, dup);
				}
				Remap[0] = new0;
				Remap[new0] = dup;
				BaseColors[new0] = color0;
				break;
			}
		}
	}

	// If there were no duplicates, InitPalette() will remap color 0 to the
	// closest matching color. Hopefully nobody will use a palette where all
	// 256 entries are different. :-)
}

//===========================================================================
// 
//	Gets the average color of a texture for use as a sky cap color
//
//===========================================================================

PalEntry averageColor(const uint32_t* data, int size, int maxout)
{
	int				i;
	unsigned int	r, g, b;

	// First clear them.
	r = g = b = 0;
	if (size == 0)
	{
		return PalEntry(255, 255, 255);
	}
	for (i = 0; i < size; i++)
	{
		b += BPART(data[i]);
		g += GPART(data[i]);
		r += RPART(data[i]);
	}

	r = r / size;
	g = g / size;
	b = b / size;

	int maxv = MAX(MAX(r, g), b);

	if (maxv && maxout)
	{
		r = ::Scale(r, maxout, maxv);
		g = ::Scale(g, maxout, maxv);
		b = ::Scale(b, maxout, maxv);
	}
	return PalEntry(255, r, g, b);
}



//==========================================================================
//
// V_GetColorFromString
//
// Passed a string of the form "#RGB", "#RRGGBB", "R G B", or "RR GG BB",
// returns a number representing that color. If palette is non-NULL, the
// index of the best match in the palette is returned, otherwise the
// RRGGBB value is returned directly.
//
//==========================================================================

int V_GetColorFromString(const char* cstr, FScriptPosition* sc)
{
	int c[3], i, p;
	char val[3];

	val[2] = '\0';

	// Check for HTML-style #RRGGBB or #RGB color string
	if (cstr[0] == '#')
	{
		size_t len = strlen(cstr);

		if (len == 7)
		{
			// Extract each eight-bit component into c[].
			for (i = 0; i < 3; ++i)
			{
				val[0] = cstr[1 + i * 2];
				val[1] = cstr[2 + i * 2];
				c[i] = ParseHex(val, sc);
			}
		}
		else if (len == 4)
		{
			// Extract each four-bit component into c[], expanding to eight bits.
			for (i = 0; i < 3; ++i)
			{
				val[1] = val[0] = cstr[1 + i];
				c[i] = ParseHex(val, sc);
			}
		}
		else
		{
			// Bad HTML-style; pretend it's black.
			c[2] = c[1] = c[0] = 0;
		}
	}
	else
	{
		if (strlen(cstr) == 6)
		{
			char* p;
			int color = strtol(cstr, &p, 16);
			if (*p == 0)
			{
				// RRGGBB string
				c[0] = (color & 0xff0000) >> 16;
				c[1] = (color & 0xff00) >> 8;
				c[2] = (color & 0xff);
			}
			else goto normal;
		}
		else
		{
		normal:
			// Treat it as a space-delimited hexadecimal string
			for (i = 0; i < 3; ++i)
			{
				// Skip leading whitespace
				while (*cstr <= ' ' && *cstr != '\0')
				{
					cstr++;
				}
				// Extract a component and convert it to eight-bit
				for (p = 0; *cstr > ' '; ++p, ++cstr)
				{
					if (p < 2)
					{
						val[p] = *cstr;
					}
				}
				if (p == 0)
				{
					c[i] = 0;
				}
				else
				{
					if (p == 1)
					{
						val[1] = val[0];
					}
					c[i] = ParseHex(val, sc);
				}
			}
		}
	}
	return MAKERGB(c[0], c[1], c[2]);
}

//==========================================================================
//
// V_GetColorStringByName
//
// Searches for the given color name in x11r6rgb.txt and returns an
// HTML-ish "#RRGGBB" string for it if found or the empty string if not.
//
//==========================================================================

FString V_GetColorStringByName(const char* name, FScriptPosition* sc)
{
	FileData rgbNames;
	char* rgbEnd;
	char* rgb, * endp;
	int rgblump;
	int c[3], step;
	size_t namelen;

	if (fileSystem.GetNumEntries() == 0) return FString();

	rgblump = fileSystem.CheckNumForName("X11R6RGB");
	if (rgblump == -1)
	{
		if (!sc) Printf("X11R6RGB lump not found\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump not found");
		return FString();
	}

	rgbNames = fileSystem.ReadFile(rgblump);
	rgb = (char*)rgbNames.GetMem();
	rgbEnd = rgb + fileSystem.FileLength(rgblump);
	step = 0;
	namelen = strlen(name);

	while (rgb < rgbEnd)
	{
		// Skip white space
		if (*rgb <= ' ')
		{
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb <= ' ');
		}
		else if (step == 0 && *rgb == '!')
		{ // skip comment lines
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb != '\n');
		}
		else if (step < 3)
		{ // collect RGB values
			c[step++] = strtoul(rgb, &endp, 10);
			if (endp == rgb)
			{
				break;
			}
			rgb = endp;
		}
		else
		{ // Check color name
			endp = rgb;
			// Find the end of the line
			while (endp < rgbEnd && *endp != '\n')
				endp++;
			// Back up over any whitespace
			while (endp > rgb && *endp <= ' ')
				endp--;
			if (endp == rgb)
			{
				break;
			}
			size_t checklen = ++endp - rgb;
			if (checklen == namelen && strnicmp(rgb, name, checklen) == 0)
			{
				FString descr;
				descr.Format("#%02x%02x%02x", c[0], c[1], c[2]);
				return descr;
			}
			rgb = endp;
			step = 0;
		}
	}
	if (rgb < rgbEnd)
	{
		if (!sc) Printf("X11R6RGB lump is corrupt\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump is corrupt");
	}
	return FString();
}

//==========================================================================
//
// V_GetColor
//
// Works like V_GetColorFromString(), but also understands X11 color names.
//
//==========================================================================

int V_GetColor(const char* str, FScriptPosition* sc)
{
	FString string = V_GetColorStringByName(str, sc);
	int res;

	if (!string.IsEmpty())
	{
		res = V_GetColorFromString(string, sc);
	}
	else
	{
		res = V_GetColorFromString(str, sc);
	}
	return res;
}

int V_GetColor(FScanner& sc)
{
	FScriptPosition scc = sc;
	return V_GetColor(sc.String, &scc);
}

//==========================================================================
//
// Special colormaps
//
//==========================================================================


TArray<FSpecialColormap> SpecialColormaps;
uint8_t DesaturateColormap[31][256];

// These default tables are needed for texture composition.
static FSpecialColormapParameters SpecialColormapParms[] =
{
	// Doom invulnerability is an inverted grayscale.
	// Strife uses it when firing the Sigil
	{ { 1, 1, 1 }, {    0,    0,   0 } },

	// Heretic invulnerability is a golden shade.
	{ { 0, 0, 0 }, {  1.5, 0.75,   0 }, },

	// [BC] Build the Doomsphere colormap. It is red!
	{ { 0, 0, 0 }, {  1.5,    0,   0 } },

	// [BC] Build the Guardsphere colormap. It's a greenish-white kind of thing.
	{ { 0, 0, 0 }, { 1.25,  1.5,   1 } },

	// Build a blue colormap.
	{ { 0, 0, 0 }, {    0,    0, 1.5 } },

	// Repeated to get around the overridability of the other one
	{ { 1, 1, 1 }, {    0,    0,   0 } },

};

//==========================================================================
//
//
//
//==========================================================================

void UpdateSpecialColormap(PalEntry* BaseColors, unsigned int index, float r1, float g1, float b1, float r2, float g2, float b2)
{
	assert(index < SpecialColormaps.Size());

	FSpecialColormap* cm = &SpecialColormaps[index];
	cm->ColorizeStart[0] = float(r1);
	cm->ColorizeStart[1] = float(g1);
	cm->ColorizeStart[2] = float(b1);
	cm->ColorizeEnd[0] = float(r2);
	cm->ColorizeEnd[1] = float(g2);
	cm->ColorizeEnd[2] = float(b2);

	r2 -= r1;
	g2 -= g1;
	b2 -= b1;
	r1 *= 255;
	g1 *= 255;
	b1 *= 255;

	if (BaseColors)	// only create this table if needed
	{
		for (int c = 0; c < 256; c++)
		{
			double intensity = (BaseColors[c].r * 77 +
				BaseColors[c].g * 143 +
				BaseColors[c].b * 37) / 256.0;

			PalEntry pe = PalEntry(std::min(255, int(r1 + intensity * r2)),
				std::min(255, int(g1 + intensity * g2)),
				std::min(255, int(b1 + intensity * b2)));

			cm->Colormap[c] = BestColor((uint32_t*)BaseColors, pe.r, pe.g, pe.b);
		}
	}

	// This table is used by the texture composition code
	for (int i = 0; i < 256; i++)
	{
		cm->GrayscaleToColor[i] = PalEntry(std::min(255, int(r1 + i * r2)),
			std::min(255, int(g1 + i * g2)),
			std::min(255, int(b1 + i * b2)));
	}
}

//==========================================================================
//
//
//
//==========================================================================

int AddSpecialColormap(PalEntry *BaseColors, float r1, float g1, float b1, float r2, float g2, float b2)
{
	// Clamp these in range for the hardware shader.
	r1 = clamp(r1, 0.0f, 2.0f);
	g1 = clamp(g1, 0.0f, 2.0f);
	b1 = clamp(b1, 0.0f, 2.0f);
	r2 = clamp(r2, 0.0f, 2.0f);
	g2 = clamp(g2, 0.0f, 2.0f);
	b2 = clamp(b2, 0.0f, 2.0f);

	for (unsigned i = 1; i < SpecialColormaps.Size(); i++)
	{
		// Avoid precision issues here when trying to find a proper match.
		if (fabs(SpecialColormaps[i].ColorizeStart[0] - r1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeStart[1] - g1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeStart[2] - b1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[0] - r2) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[1] - g2) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[2] - b2) < FLT_EPSILON)
		{
			return i;	// The map already exists
		}
	}

	UpdateSpecialColormap(BaseColors, SpecialColormaps.Reserve(1), r1, g1, b1, r2, g2, b2);
	return SpecialColormaps.Size() - 1;
}

void InitSpecialColormaps(PalEntry *pe)
{
	for (unsigned i = 0; i < countof(SpecialColormapParms); ++i)
	{
		AddSpecialColormap(pe, SpecialColormapParms[i].Start[0], SpecialColormapParms[i].Start[1],
			SpecialColormapParms[i].Start[2], SpecialColormapParms[i].End[0],
			SpecialColormapParms[i].End[1], SpecialColormapParms[i].End[2]);
	}

	// desaturated colormaps. These are used for texture composition
	for (int m = 0; m < 31; m++)
	{
		uint8_t* shade = DesaturateColormap[m];
		for (int c = 0; c < 256; c++)
		{
			int intensity = pe[c].Luminance();

			int r = (pe[c].r * (31 - m) + intensity * m) / 31;
			int g = (pe[c].g * (31 - m) + intensity * m) / 31;
			int b = (pe[c].b * (31 - m) + intensity * m) / 31;
			shade[c] = BestColor((uint32_t*)pe, r, g, b);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

int ReadPalette(int lumpnum, uint8_t* buffer)
{
	if (lumpnum < 0)
	{
		return 0;
	}
	FileData lump = fileSystem.ReadFile(lumpnum);
	uint8_t* lumpmem = (uint8_t*)lump.GetMem();
	memset(buffer, 0, 768);

	FileReader fr;
	fr.OpenMemory(lumpmem, lump.GetSize());
	auto png = M_VerifyPNG(fr);
	if (png)
	{
		uint32_t id, len;
		fr.Seek(33, FileReader::SeekSet);
		fr.Read(&len, 4);
		fr.Read(&id, 4);
		bool succeeded = false;
		while (id != MAKE_ID('I', 'D', 'A', 'T') && id != MAKE_ID('I', 'E', 'N', 'D'))
		{
			len = BigLong((unsigned int)len);
			if (id != MAKE_ID('P', 'L', 'T', 'E'))
				fr.Seek(len, FileReader::SeekCur);
			else
			{
				int PaletteSize = MIN<int>(len, 768);
				fr.Read(buffer, PaletteSize);
				return PaletteSize / 3;
			}
			fr.Seek(4, FileReader::SeekCur);	// Skip CRC
			fr.Read(&len, 4);
			id = MAKE_ID('I', 'E', 'N', 'D');
			fr.Read(&id, 4);
		}
		I_Error("%s contains no palette", fileSystem.GetFileFullName(lumpnum));
	}
	if (memcmp(lumpmem, "JASC-PAL", 8) == 0)
	{
		FScanner sc;

		sc.OpenMem(fileSystem.GetFileFullName(lumpnum), (char*)lumpmem, int(lump.GetSize()));
		sc.MustGetString();
		sc.MustGetNumber();	// version - ignore
		sc.MustGetNumber();
		int colors = MIN(256, sc.Number) * 3;
		for (int i = 0; i < colors; i++)
		{
			sc.MustGetNumber();
			if (sc.Number < 0 || sc.Number > 255)
			{
				sc.ScriptError("Color %d value out of range.", sc.Number);
			}
			buffer[i] = sc.Number;
		}
		return colors / 3;
	}
	else
	{
		memcpy(buffer, lumpmem, MIN<size_t>(768, lump.GetSize()));
		return 256;
	}
}

