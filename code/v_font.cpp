#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "templates.h"
#include "doomtype.h"
#include "m_swap.h"
#include "v_font.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_data.h"
#include "z_zone.h"
#include "i_system.h"
#include "gi.h"

static const byte myislower[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0
};

FFont::FFont (const char *nametemplate, int first, int count, int start)
{
	int neededsize;
	int i, lump;
	char buffer[12];
	byte usedcolors[256], translated[256], identity[256];
	double *luminosity;

	Chars = new CharData[count];
	Bitmaps = NULL;
	Ranges = NULL;
	FirstChar = first;
	LastChar = first + count - 1;
	FontHeight = 0;
	memset (usedcolors, 0, 256);

	for (i = neededsize = 0; i < count; i++)
	{
		sprintf (buffer, nametemplate, i + start);
		lump = W_CheckNumForName (buffer);
		if (gameinfo.gametype == GAME_Doom && lump >= 0 &&
			i + start == 121 && lumpinfo[lump].wadnum == 1)
		{ // HACKHACK: Don't load STCFN121 in doom(2).wad, because
		  // it's not really a lower-case 'y'
			lump = -1;
		}
		if (lump >= 0)
		{
			patch_t *patch = (patch_t *)W_CacheLumpNum (lump, PU_CACHE);
			Chars[i].Width = SHORT(patch->width);
			Chars[i].Height = SHORT(patch->height);
			Chars[i].XOffs = SHORT(patch->leftoffset);
			Chars[i].YOffs = SHORT(patch->topoffset);
			if (Chars[i].Height > FontHeight)
				FontHeight = Chars[i].Height;
			neededsize += Chars[i].Width * Chars[i].Height;
			RecordPatchColors (patch, usedcolors);
		}
		else
		{
			Chars[i].Width = Chars[i].Height = 0;
			Chars[i].XOffs = Chars[i].YOffs = 0;
		}
	}

	SpaceWidth = (Chars['N' - first].Width + 1) / 2;
	ActiveColors = SimpleTranslation (usedcolors, translated, identity, &luminosity);
	Bitmaps = new byte[neededsize];
	memset (Bitmaps, 0, neededsize);

	for (i = neededsize = 0; i < count; i++)
	{
		sprintf (buffer, nametemplate, i + start);
		lump = W_CheckNumForName (buffer);
		if (gameinfo.gametype == GAME_Doom && lump >= 0 &&
			i + start == 121 && lumpinfo[lump].wadnum == 1)
		{ // HACKHACK: See above
			lump = -1;
		}
		if (lump >= 0)
		{
			Chars[i].Data = Bitmaps + neededsize;
			RawDrawPatch ((patch_t *)W_CacheLumpNum (lump, PU_CACHE),
				Chars[i].Data, translated);
			neededsize += Chars[i].Width * Chars[i].Height;
		}
		else
		{
			Chars[i].Data = NULL;
		}
	}

	BuildTranslations (usedcolors, translated, identity, luminosity);

	delete[] luminosity;
}

FFont::~FFont ()
{
	if (Chars)
	{
		delete[] Chars;
		Chars = NULL;
	}
	if (Ranges)
	{
		delete[] Ranges;
		Ranges = NULL;
	}
}

void RecordPatchColors (patch_t *patch, byte *usedcolors)
{
	int x;

	for (x = SHORT(patch->width) - 1; x >= 0; x--)
	{
		column_t *column = (column_t *)((byte *)patch + LONG(patch->columnofs[x]));

		while (column->topdelta != 0xff)
		{
			byte *source = (byte *)column + 3;
			int count = column->length;

			do
			{
				usedcolors[*source++] = 1;
			} while (--count);

			column = (column_t *)(source + 1);
		}
	}
}

void RawDrawPatch (patch_t *patch, byte *out, byte *tlate)
{
	int width = SHORT(patch->width);
	byte *desttop = out;
	int x;

	for (x = 0; x < width; x++, desttop++)
	{
		column_t *column = (column_t *)((byte *)patch + LONG(patch->columnofs[x]));

		while (column->topdelta != 0xff)
		{
			byte *source = (byte *)column + 3;
			byte *dest = desttop + column->topdelta * width;
			int count = column->length;

			do
			{
				*dest = tlate[*source++];
				dest += width;
			} while (--count);

			column = (column_t *)(source + 1);
		}
	}
}

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	if (RPART(GPalette.BaseColors[*((byte *)arg1)]) * 299 +
		GPART(GPalette.BaseColors[*((byte *)arg1)]) * 587 +
		BPART(GPalette.BaseColors[*((byte *)arg1)]) * 114  <
		RPART(GPalette.BaseColors[*((byte *)arg2)]) * 299 +
		GPART(GPalette.BaseColors[*((byte *)arg2)]) * 587 +
		BPART(GPalette.BaseColors[*((byte *)arg2)]) * 114)
		return -1;
	else
		return 1;
}

int FFont::SimpleTranslation (byte *colorsused, byte *translation, byte *reverse, double **luminosity)
{
	double min, max, diver;
	int i, j;

	memset (translation, 0, 256);

	for (i = 0, j = 1; i < 256; i++)
	{
		if (colorsused[i])
		{
			reverse[j++] = i;
		}
	}

	qsort (reverse+1, j-1, 1, compare);

	*luminosity = new double[j];
	max = 0.0;
	min = 100000000.0;
	for (i = 1; i < j; i++)
	{
		translation[reverse[i]] = i;

		(*luminosity)[i] = RPART(GPalette.BaseColors[reverse[i]]) * 0.299 +
						   GPART(GPalette.BaseColors[reverse[i]]) * 0.587 +
						   BPART(GPalette.BaseColors[reverse[i]]) * 0.114;
		if ((*luminosity)[i] > max)
			max = (*luminosity)[i];
		if ((*luminosity)[i] < min)
			min = (*luminosity)[i];
	}
	diver = 1.0 / (max - min);
	for (i = 1; i < j; i++)
	{
		(*luminosity)[i] = ((*luminosity)[i] - min) * diver;
	}

	return j;
}

void FFont::BuildTranslations (const byte *usedcolors, const byte *tlate, const byte *identity, const double *luminosity)
{
	static const struct tp { double mul, add; } transParm[CR_YELLOW][3] =
	{
		{{ 184.0,  71.0 }, { 184.0,  0.0 }, { 184.0,  0.0 }},	// CR_BRICK
		{{ 204.0,  51.0 }, { 192.0, 43.0 }, { 204.0, 19.0 }},	// CR_TAN
		{{ 200.0,  39.0 }, { 200.0, 39.0 }, { 200.0, 39.0 }},	// CR_GRAY
		{{ 108.0,  11.0 }, { 232.0, 23.0 }, { 104.0,  7.0 }},	// CR_GREEN
		{{ 108.0,  83.0 }, { 104.0, 63.0 }, {  96.0, 47.0 }},	// CR_BROWN
		{{ 140.0, 115.0 }, { 212.0, 43.0 }, { 115.0,  0.0 }},	// CR_GOLD
		{{ 192.0,  63.0 }, {   0.0,  0.0 }, {   0.0,  0.0 }},	// CR_RED
		{{   0.0,   0.0 }, {   0.0,  0.0 }, { 216.0, 39.0 }},	// CR_BLUE
		{{ 223.0,  32.0 }, { 128.0,  0.0 }, {   0.0,  0.0 }},	// CR_ORANGE
		{{ 219.0,  36.0 }, { 219.0, 36.0 }, { 219.0, 36.0 }}	// CR_WHITE
	};

	byte colors[3*17];
	int i, j, k;
	double v;
	byte *base;
	byte *range;

	range = Ranges = new byte[NUM_TEXT_COLORS * ActiveColors];

	// Create different translations for different color ranges
	for (i = 0; i < CR_UNTRANSLATED; i++)
	{
		const tp *parms = &transParm[i][0];
		base = colors;

		if (i != CR_YELLOW)
		{
			for (v = 0.0, k = 17; k > 0; k--, v += 0.0625f)
			{
				base[0] = (int)(v * parms[0].mul + parms[0].add);	// red
				base[1] = (int)(v * parms[1].mul + parms[1].add);	// green
				base[2] = (int)(v * parms[2].mul + parms[2].add);	// blue
				base += 3;
			}
		}
		else
		{ // Hexen yellow
			for (v = 0.0, k = 17; k > 0; k--, v += 0.0625f)
			{
				if (v <= 0.25)
				{
					base[0] = (int)((v * 168.0) + 39.0);
					base[1] = (int)((v * 168.0) + 39.0);
					base[2] = (int)((v * 168.0) + 39.0);
				}
				else if (v < 0.8125)
				{
					base[0] = (int)((v * 230.0) + 61.9);
					base[1] = (int)((v * 172.5) + 29.4);
					base[2] = 24;
				}
				else
				{
					base[0] = (int)((v * 46.8) + 205.2);
					base[1] = (int)((v * 210.6) - 2.6);
					base[2] = (int)((v * 292.5) - 195.5);
				}
				base += 3;
			}
		}

		range++;

		for (j = 1; j < ActiveColors; j++)
		{
			double p1 = luminosity[j];
			double i1 = p1 * 16.0;
			double index, frac;
			int r, g, b;

			frac = modf (i1, &index);

			if (p1 < 1.0)
			{
				double ifrac = 1.0 - frac;
				k = (int)index * 3;
				r = (int)(colors[k+0] * ifrac + colors[k+3] * frac);
				g = (int)(colors[k+1] * ifrac + colors[k+4] * frac);
				b = (int)(colors[k+2] * ifrac + colors[k+5] * frac);
			}
			else
			{
				r = colors[3*16+0];
				g = colors[3*16+1];
				b = colors[3*16+2];
			}
			*range++ = ColorMatcher.Pick (r, g, b);
		}
	}

	// Store the identity translation
	memcpy (range, identity, ActiveColors);
}

byte *FFont::GetColorTranslation (EColorRange range) const
{
	if (range < NUM_TEXT_COLORS)
		return Ranges + ActiveColors * range;
	else
		return Ranges + ActiveColors * CR_UNTRANSLATED;
}

byte *FFont::GetChar (int code, int *const width, int *const height, int *const xoffs, int *const yoffs) const
{
	if (code < FirstChar ||
		code > LastChar ||
		Chars[code - FirstChar].Data == NULL)
	{
		if (myislower[code])
		{
			code -= 32;
			if (code < FirstChar ||
				code > LastChar ||
				Chars[code - FirstChar].Data == NULL)
			{
				*width = SpaceWidth;
				return NULL;
			}
		}
		else
		{
			*width = SpaceWidth;
			return NULL;
		}
	}

	code -= FirstChar;
	*width = Chars[code].Width;
	*height = Chars[code].Height;
	*xoffs = Chars[code].XOffs;
	*yoffs = Chars[code].YOffs;
	return Chars[code].Data;
}

int FFont::GetCharWidth (int code) const
{
	if (code < FirstChar ||
		code > LastChar ||
		Chars[code - FirstChar].Data == NULL)
	{
		if (myislower[code])
		{
			code -= 32;
			if (code < FirstChar ||
				code > LastChar ||
				Chars[code - FirstChar].Data == NULL)
			{
				return SpaceWidth;
			}
		}
		else
		{
			return SpaceWidth;
		}
	}

	return Chars[code - FirstChar].Width;
}


FFont::FFont ()
{
	Bitmaps = NULL;
	Chars = NULL;
	Ranges = NULL;
}

FConsoleFont::FConsoleFont (int lump)
{
	byte *data = (byte *)W_CacheLumpNum (lump, PU_CACHE);
	byte *data_p;
	int w, h;
	int i;

	if (data[0] != 'F' || data[1] != 'O' || data[2] != 'N' || data[3] != '1')
	{
		I_FatalError ("Console font is not of type FON1");
	}

	Chars = new CharData[256];

	w = data[4] + data[5]*256;
	h = data[6] + data[7]*256;

	FontHeight = h;
	SpaceWidth = w;
	ActiveColors = 255;
	Bitmaps = new byte[w*h*256];
	FirstChar = 0;
	LastChar = 255;

	BuildTranslations ();

	data_p = data + 8;

	for (i = 0; i < 256; i++)
	{
		Chars[i].Width = w;
		Chars[i].Height = h;
		Chars[i].XOffs = 0;
		Chars[i].YOffs = 0;
		Chars[i].Data = Bitmaps + w*h*i;

		byte *dest = Chars[i].Data;
		int destSize = w*h;

		do
		{
			SBYTE code = *data_p++;
			if (code >= 0)
			{
				memcpy (dest, data_p, code+1);
				dest += code+1;
				data_p += code+1;
				destSize -= code+1;
			}
			else if (code != -128)
			{
				memset (dest, *data_p, (-code)+1);
				dest += (-code)+1;
				data_p++;
				destSize -= (-code)+1;
			}
		} while (destSize > 0);
	}
}

void FConsoleFont::BuildTranslations ()
{
	// Create different translations for different color ranges
	// These are not the same as FFont::BuildTranslations()
	static const struct tp { short mul, add; } transParmLo[CR_UNTRANSLATED][3] =
	{
		{{ 184, 71 }, { 184,  0 }, { 184,  0 }},	// CR_BRICK
		{{ 204, 51 }, { 192, 43 }, { 204, 19 }},	// CR_TAN
		{{ 200, 39 }, { 200, 39 }, { 200, 39 }},	// CR_GRAY
		{{   0,  0 }, { 255,  0 }, {   0,  0 }},	// CR_GREEN
		{{ 255,  0 }, { 128,  0 }, {   0,  0 }},	// CR_BROWN
		{{ 255,  0 }, { 383,  0 }, { 128,  0 }},	// CR_GOLD
		{{ 255,  0 }, {   0,  0 }, {   0,  0 }},	// CR_RED
		{{   0,  0 }, {   0,  0 }, { 255,  0 }},	// CR_BLUE
		{{ 223, 32 }, { 128,  0 }, {   0,  0 }},	// CR_ORANGE
		{{ 255,  0 }, { 255,  0 }, { 255,  0 }},	// CR_WHITE
		{{ 255,  0 }, { 255,  0 }, {   0,  0 }}		// CR_YELLOW
	};
	static const tp transParmHi[CR_UNTRANSLATED][3] =
	{
		{{ 127, 128 }, { 254,   0 }, { 254,   0 }},	// CR_BRICK
		{{ 102, 153 }, { 116, 139 }, { 134, 121 }},	// CR_TAN
		{{ 192,  63 }, { 192,  63 }, { 192,  63 }},	// CR_GRAY
		{{ 254,   0 }, {   0, 255 }, { 254,   0 }},	// CR_GREEN
		{{ 188,  67 }, { 184,  47 }, { 176,  31 }},	// CR_BROWN
		{{   0, 223 }, {  64, 191 }, { 254,   0 }},	// CR_GOLD
		{{   0, 255 }, { 254,   0 }, { 254,   0 }},	// CR_RED
		{{ 191,  64 }, { 191,  64 }, {   0, 255 }},	// CR_BLUE
		{{   0, 255 }, { 127, 127 }, { 254,   0 }},	// CR_ORANGE
		{{ 127, 128 }, { 127, 128 }, { 127, 128 }},	// CR_WHITE
		{{   0, 255 }, {   0, 255 }, { 254,   0 }}	// CR_YELLOW
	};

	byte *range;
	range = Ranges = new byte[NUM_TEXT_COLORS * ActiveColors];
	int i, j, r, g, b;

	for (i = 0; i < CR_UNTRANSLATED; i++)
	{
		const tp *parm;

		parm = &transParmLo[i][0];
		for (j = 0; j < 127; j++)
		{
			r = j * parm[0].mul / 255 + parm[0].add;
			g = j * parm[1].mul / 255 + parm[1].add;
			b = j * parm[2].mul / 255 + parm[2].add;
			*range++ = ColorMatcher.Pick (r, g, b);
		}

		parm = &transParmHi[i][0];
		for (j = 0; j < 128; j++)
		{
			r = j * parm[0].mul / 127 + parm[0].add;
			g = MIN (j * parm[1].mul / 127 + parm[1].add, 255);
			b = j * parm[2].mul / 127 + parm[2].add;
			*range++ = ColorMatcher.Pick (r, g, b);
		}
	}
}