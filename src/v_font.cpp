/*
** v_font.cpp
** Font management
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
#include "i_system.h"
#include "gi.h"
#include "cmdlib.h"

// This is a font character that loads a patch and recolors it.
class FFontChar1 : public FPatchTexture
{
public:
	FFontChar1 (int sourcelump, const BYTE *sourceremap);

protected:
	void MakeTexture ();

	const BYTE *SourceRemap;
};

// This is a font character that reads RLE compressed data.
class FFontChar2 : public FTexture
{
public:
	FFontChar2 (int sourcelump, int sourcepos, int width, int height);
	~FFontChar2 ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	int SourcePos;
	BYTE *Pixels;
	Span **Spans;

	void MakeTexture ();
};

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

FFont *FFont::FirstFont = NULL;

#if defined(_MSC_VER) && _MSC_VER < 1310
template<> FArchive &operator<< (FArchive &arc, FFont* &font)
#else
FArchive &SerializeFFontPtr (FArchive &arc, FFont* &font)
#endif
{
	if (arc.IsStoring ())
	{
		arc << font->Name;
	}
	else
	{
		char *name = NULL;

		arc << name;
		font = FFont::FindFont (name);
		if (font == NULL)
		{
			int lump = Wads.CheckNumForName (name);
			if (lump != -1)
			{
				char head[3];
				{
					FWadLump lumpy = Wads.OpenLumpNum (lump);
					lumpy.Read (head, 3);
				}
				if (head[0] == 'F' && head[1] == 'O' && head[2] == 'N')
				{
					font = new FSingleLumpFont (name, lump);
				}
			}
			if (font == NULL)
			{
				int picnum = TexMan.CheckForTexture (name, FTexture::TEX_Any);
				if (picnum <= 0)
				{
					picnum = TexMan.AddPatch (name);
				}
				if (picnum > 0)
				{
					font = new FSingleLumpFont (name, -1);
				}
			}
			if (font == NULL)
			{
				Printf ("Could not load font %s\n", name);
				font = SmallFont;
			}
		}
		delete[] name;
	}
	return arc;
}


FFont::FFont (const char *name, const char *nametemplate, int first, int count, int start)
{
	int i, lump;
	char buffer[12];
	int *charlumps;
	byte usedcolors[256], identity[256];
	double *luminosity;
	int maxyoffs;
	bool doomtemplate = gameinfo.gametype == GAME_Doom ? strncmp (nametemplate, "STCFN", 5) == 0 : false;

	Chars = new CharData[count];
	charlumps = new int[count];
	PatchRemap = new BYTE[256];
	Ranges = NULL;
	FirstChar = first;
	LastChar = first + count - 1;
	FontHeight = 0;
	GlobalKerning = false;
	memset (usedcolors, 0, 256);
	Name = copystring (name);
	Next = FirstFont;
	FirstFont = this;

	maxyoffs = 0;

	for (i = 0; i < count; i++)
	{
		sprintf (buffer, nametemplate, i + start);
		lump = Wads.CheckNumForName (buffer);
		if (doomtemplate && lump >= 0 && i + start == 121)
		{ // HACKHACK: Don't load STCFN121 in doom(2), because
		  // it's not really a lower-case 'y' but an upper-case 'I'.
		  // Because a lot of wads with their own font seem to foolishly
		  // copy STCFN121 and make it an 'I' themselves, wads must
		  // provide STCFN120 (x) and STCFN122 (z) for STCFN121 to load.
			if (Wads.CheckNumForName ("STCFN120") == -1 ||
				Wads.CheckNumForName ("STCFN122") == -1)
			{
				lump = -1;
			}
		}
		charlumps[i] = lump;
		if (lump >= 0)
		{
			FTexture *pic = TexMan[TexMan.AddPatch (buffer)];
			int height = pic->GetHeight();
			int yoffs = pic->TopOffset;

			if (yoffs > maxyoffs)
			{
				maxyoffs = yoffs;
			}
			height += abs (yoffs);
			if (height > FontHeight)
			{
				FontHeight = height;
			}
			RecordTextureColors (pic, usedcolors);
		}
	}

	ActiveColors = SimpleTranslation (usedcolors, PatchRemap, identity, &luminosity);

	for (i = 0; i < count; i++)
	{
		if (charlumps[i] >= 0)
		{
			Chars[i].Pic = new FFontChar1 (charlumps[i], PatchRemap);
		}
		else
		{
			Chars[i].Pic = NULL;
		}
	}

	SpaceWidth = (Chars['N' - first].Pic->GetWidth() + 1) / 2;
	BuildTranslations (luminosity, identity);

	delete[] luminosity;
}

FFont::~FFont ()
{
	if (Chars)
	{
		int count = LastChar - FirstChar + 1;

		for (int i = 0; i < count; ++i)
		{
			if (Chars[i].Pic != NULL && Chars[i].Pic->Name == 0)
			{
				delete Chars[i].Pic;
			}
		}
		delete[] Chars;
		Chars = NULL;
	}
	if (Ranges)
	{
		delete[] Ranges;
		Ranges = NULL;
	}
	if (PatchRemap)
	{
		delete[] PatchRemap;
		PatchRemap = NULL;
	}

	FFont **prev = &FirstFont;
	FFont *font = *prev;

	while (font != NULL && font != this)
	{
		prev = &font->Next;
		font = *prev;
	}

	if (font != NULL)
	{
		*prev = font->Next;
	}
}

FFont *FFont::FindFont (const char *name)
{
	FFont *font = FirstFont;

	while (font != NULL)
	{
		if (stricmp (font->Name, name) == 0)
			break;
		font = font->Next;
	}
	return font;
}

void RecordTextureColors (FTexture *pic, byte *usedcolors)
{
	int x;

	for (x = pic->GetWidth() - 1; x >= 0; x--)
	{
		const FTexture::Span *spans;
		const BYTE *column = pic->GetColumn (x, &spans);

		while (spans->Length != 0)
		{
			const BYTE *source = column + spans->TopOffset;
			int count = spans->Length;

			do
			{
				usedcolors[*source++] = 1;
			} while (--count);

			spans++;
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

	reverse[0] = 0;
	for (i = 0, j = 1; i < 256; i++)
	{
		if (colorsused[i])
		{
			reverse[j++] = i;
		}
	}

	qsort (reverse, j, 1, compare);

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
	for (i = 0; i < j; i++)
	{
		(*luminosity)[i] = ((*luminosity)[i] - min) * diver;
	}

	return j;
}

void FFont::BuildTranslations (const double *luminosity, const BYTE *identity)
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
				base[0] = toint(v * parms[0].mul + parms[0].add);	// red
				base[1] = toint(v * parms[1].mul + parms[1].add);	// green
				base[2] = toint(v * parms[2].mul + parms[2].add);	// blue
				base += 3;
			}
		}
		else
		{ // Hexen yellow
			for (v = 0.0, k = 17; k > 0; k--, v += 0.0625f)
			{
				if (v <= 0.25)
				{
					base[0] = toint((v * 168.0) + 39.0);
					base[1] = toint((v * 168.0) + 39.0);
					base[2] = toint((v * 168.0) + 39.0);
				}
				else if (v < 0.8125)
				{
					base[0] = toint((v * 230.0) + 61.9);
					base[1] = toint((v * 172.5) + 29.4);
					base[2] = 24;
				}
				else
				{
					base[0] = toint((v * 46.8) + 205.2);
					base[1] = toint((v * 210.6) - 2.6);
					base[2] = toint((v * 292.5) - 195.5);
				}
				base += 3;
			}
		}

		*range++ = 0;

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
				k = toint(index) * 3;
				r = toint(colors[k+0] * ifrac + colors[k+3] * frac);
				g = toint(colors[k+1] * ifrac + colors[k+4] * frac);
				b = toint(colors[k+2] * ifrac + colors[k+5] * frac);
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
	memcpy (range, identity, ActiveColors);
}

byte *FFont::GetColorTranslation (EColorRange range) const
{
	if (ActiveColors == 0)
		return NULL;
	else if (range < NUM_TEXT_COLORS)
		return Ranges + ActiveColors * range;
	else
		return Ranges + ActiveColors * CR_UNTRANSLATED;
}

FTexture *FFont::GetChar (int code, int *const width) const
{
	if (code < FirstChar ||
		code > LastChar ||
		Chars[code - FirstChar].Pic == NULL)
	{
		if (myislower[code])
		{
			code -= 32;
			if (code < FirstChar ||
				code > LastChar ||
				Chars[code - FirstChar].Pic == NULL)
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
	*width = Chars[code].Pic->GetWidth();
	return Chars[code].Pic;
}

int FFont::GetCharWidth (int code) const
{
	if (code < FirstChar ||
		code > LastChar ||
		Chars[code - FirstChar].Pic == NULL)
	{
		if (myislower[code])
		{
			code -= 32;
			if (code < FirstChar ||
				code > LastChar ||
				Chars[code - FirstChar].Pic == NULL)
			{
				return SpaceWidth;
			}
		}
		else
		{
			return SpaceWidth;
		}
	}

	return Chars[code - FirstChar].Pic->GetWidth();
}


FFont::FFont ()
{
	Chars = NULL;
	Ranges = NULL;
	PatchRemap = NULL;
}

FSingleLumpFont::FSingleLumpFont (const char *name, int lump)
{
	Name = copystring (name);

	// If lump is -1, then the font name is really a texture name, so
	// the font should be a redirect to the texture.
	// If lump is >= 0, then the font is really a font.
	if (lump < 0)
	{
		int picnum = TexMan.CheckForTexture (name, FTexture::TEX_Any);

		if (picnum > 0)
		{
			CreateFontFromPic (picnum);
		}
		else
		{
			I_FatalError ("%s is not a font or texture", name);
		}
	}
	else
	{
		FMemLump data1 = Wads.ReadLump (lump);
		const BYTE *data = (const BYTE *)data1.GetMem();

		if (data[0] != 'F' || data[1] != 'O' || data[2] != 'N' ||
			(data[3] != '1' && data[3] != '2'))
		{
			I_FatalError ("%s is not a recognizable font", name);
		}
		else
		{
			switch (data[3])
			{
			case '1':
				LoadFON1 (lump, data);
				break;

			case '2':
				LoadFON2 (lump, data);
				break;
			}
		}
	}

	Next = FirstFont;
	FirstFont = this;
}

void FSingleLumpFont::CreateFontFromPic (int picnum)
{
	FTexture *pic = TexMan[picnum];

	FontHeight = pic->GetHeight ();
	SpaceWidth = pic->GetWidth ();
	GlobalKerning = 0;

	FirstChar = LastChar = 'A';
	Chars = new CharData[1];
	Chars->Pic = pic;

	// Only one color range. Don't bother with the others.
	ActiveColors = 0;
}

void FSingleLumpFont::LoadFON1 (int lump, const BYTE *data)
{
	const BYTE *data_p;
	int w, h, i;

	Chars = new CharData[256];

	w = data[4] + data[5]*256;
	h = data[6] + data[7]*256;

	FontHeight = h;
	SpaceWidth = w;
	ActiveColors = 255;
	FirstChar = 0;
	LastChar = 255;
	GlobalKerning = 0;

	BuildTranslations2 ();

	data_p = data + 8;

	for (i = 0; i < 256; i++)
	{
		int destSize = w*h;

		Chars[i].Pic = new FFontChar2 (lump, data_p - data, w, h);

		// Advance to next char's data
		do
		{
			SBYTE code = *data_p++;
			if (code >= 0)
			{
				data_p += code+1;
				destSize -= code+1;
			}
			else if (code != -128)
			{
				data_p++;
				destSize -= (-code)+1;
			}
		} while (destSize > 0);
	}
}

void FSingleLumpFont::LoadFON2 (int lump, const BYTE *data)
{
	int count, i, totalwidth;
	int *widths2;
	BYTE identity[256];
	double luminosity[256];
	WORD *widths;
	const BYTE *palette;
	const BYTE *data_p;

	FontHeight = data[4] + data[5]*256;
	FirstChar = data[6];
	LastChar = data[7];
	ActiveColors = data[10];
	Ranges = NULL;
	
	count = LastChar - FirstChar + 1;
	Chars = new CharData[count];
	widths2 = new int[count];
	if (data[11] & 1)
	{
		GlobalKerning = SHORT(*(SWORD *)&data[12]);
		widths = (WORD *)(data + 14);
	}
	else
	{
		GlobalKerning = 0;
		widths = (WORD *)(data + 12);
	}
	totalwidth = 0;

	if (data[8])
	{
		totalwidth = SHORT(widths[0]);
		for (i = 0; i < count; ++i)
		{
			widths2[i] = totalwidth;
		}
		totalwidth *= count;
		palette = (BYTE *)&widths[1];
	}
	else
	{
		for (i = 0; i < count; ++i)
		{
			widths2[i] = SHORT(widths[i]);
			totalwidth += widths2[i];
		}
		palette = (BYTE *)(widths + i);
	}

	if (FirstChar <= ' ' && LastChar >= ' ')
	{
		SpaceWidth = widths2[' '-FirstChar];
	}
	else if (FirstChar <= 'N' && LastChar >= 'N')
	{
		SpaceWidth = (widths2['N' - FirstChar] + 1) / 2;
	}
	else
	{
		SpaceWidth = totalwidth * 2 / (3 * count);
	}

	FixupPalette (identity, luminosity, palette, data[9] == 0);

	data_p = palette + (ActiveColors+1)*3;

	for (i = 0; i < count; ++i)
	{
		int destSize = widths2[i] * FontHeight;
		if (destSize <= 0)
		{
			Chars[i].Pic = NULL;
		}
		else
		{
			Chars[i].Pic = new FFontChar2 (lump, data_p - data, widths2[i], FontHeight);
			do
			{
				SBYTE code = *data_p++;
				if (code >= 0)
				{
					data_p += code+1;
					destSize -= code+1;
				}
				else if (code != -128)
				{
					data_p++;
					destSize -= (-code)+1;
				}
			} while (destSize > 0);
		}
		if (destSize < 0)
		{
			i += FirstChar;
			I_FatalError ("Overflow decompressing char %d (%c) of %s", i, i, Name);
		}
	}

	BuildTranslations (luminosity, identity);
}

void FSingleLumpFont::FixupPalette (BYTE *identity, double *luminosity, const BYTE *palette, bool rescale)
{
	int i;
	double maxlum = 0.0;
	double minlum = 100000000.0;
	double diver;

	identity[0] = 0;
	palette += 3;	// Skip the transparent color

	for (i = 1; i <= ActiveColors; ++i)
	{
		int r = palette[0];
		int g = palette[1];
		int b = palette[2];
		double lum = r*0.299 + g*0.587 + b*0.114;
		palette += 3;
		identity[i] = ColorMatcher.Pick (r, g, b);
		luminosity[i] = lum;
		if (lum > maxlum)
			maxlum = lum;
		if (lum < minlum)
			minlum = lum;
	}

	if (rescale)
	{
		diver = 1.0 / (maxlum - minlum);
	}
	else
	{
		diver = 1.0 / 255.0;
	}
	for (i = 1; i <= ActiveColors; ++i)
	{
		luminosity[i] = (luminosity[i] - minlum) * diver;
	}
}

void FSingleLumpFont::BuildTranslations2 ()
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

FFontChar1::FFontChar1 (int sourcelump, const BYTE *sourceremap)
: FPatchTexture (sourcelump, FTexture::TEX_FontChar),
  SourceRemap (sourceremap)
{
	// Make this texture unnamed
	Name[0] = 0;
}

void FFontChar1::MakeTexture ()
{
	// Make the texture as normal, then remap it so that all the colors
	// are at the low end of the palette
	FPatchTexture::MakeTexture ();

	BYTE *col = Pixels;

	for (int x = 0; x < Width; ++x)
	{
		Span *span = Spans[x];

		while (span->Length != 0)
		{
			int bot = span->TopOffset + span->Length;
			for (int y = span->TopOffset; y < bot; ++y)
			{
				col[y] = SourceRemap[col[y]];
			}
			span++;
		}

		col += Height;
	}
}


FFontChar2::FFontChar2 (int sourcelump, int sourcepos, int width, int height)
: SourceLump (sourcelump), SourcePos (sourcepos), Pixels (0), Spans (0)
{
	UseType = TEX_FontChar;
	Width = width;
	Height = height;
	TopOffset = 0;
	LeftOffset = 0;
	CalcBitSize ();
}

FFontChar2::~FFontChar2 ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
	}
}

void FFontChar2::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

const BYTE *FFontChar2::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

const BYTE *FFontChar2::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	if (column >= Width)
	{
		column = WidthMask;
	}
	if (spans_out != NULL)
	{
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}

void FFontChar2::MakeTexture ()
{
	FWadLump lump = Wads.OpenLumpNum (SourceLump);
	int destSize = Width * Height;
	BYTE max = 255;

	// This is to "fix" bad fonts
	{
		BYTE buff[8];
		lump.Read (buff, 4);
		if (buff[3] == '2')
		{
			lump.Read (buff, 7);
			max = buff[6];
			lump.Seek (SourcePos - 11, SEEK_CUR);
		}
		else
		{
			lump.Seek (SourcePos - 4, SEEK_CUR);
		}
	}

	Pixels = new BYTE[destSize];

	int runlen = 0, setlen = 0;
	BYTE setval = 0;  // Shut up, GCC!
	BYTE *dest_p = Pixels;
	int dest_adv = Height;
	int dest_rew = destSize - 1;

	for (int y = Height; y != 0; --y)
	{
		for (int x = Width; x != 0; )
		{
			if (runlen != 0)
			{
				BYTE color;

				lump >> color;
				*dest_p = MIN (color, max);
				dest_p += dest_adv;
				x--;
				runlen--;
			}
			else if (setlen != 0)
			{
				*dest_p = setval;
				dest_p += dest_adv;
				x--;
				setlen--;
			}
			else
			{
				SBYTE code;

				lump >> code;
				if (code >= 0)
				{
					runlen = code + 1;
				}
				else if (code != -128)
				{
					BYTE color;

					lump >> color;
					setlen = (-code) + 1;
					setval = MIN (color, max);
				}
			}
		}
		dest_p -= dest_rew;
	}

	if (destSize < 0)
	{
		char name[9];
		Wads.GetLumpName (name, SourceLump);
		name[8] = 0;
		I_FatalError ("The font %s is corrupt", name);
	}

	Spans = CreateSpans (Pixels);
}
