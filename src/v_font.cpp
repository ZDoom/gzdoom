/*
** v_font.cpp
** Font management
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
#include "sc_man.h"

// This structure is used by BuildTranslations() to hold color information.
struct TranslationParm
{
	short RangeStart;	// First level for this range
	short RangeEnd;		// Last level for this range
	BYTE Start[3];		// Start color for this range
	BYTE End[3];		// End color for this range
};

// This is a font character that loads a texture and recolors it.
class FFontChar1 : public FTexture
{
public:
   FFontChar1 (int sourcelump, const BYTE *sourceremap);
   const BYTE *GetColumn (unsigned int column, const Span **spans_out);
   const BYTE *GetPixels ();
   void Unload ();
   ~FFontChar1 ();

protected:
   void MakeTexture ();

   FTexture * BaseTexture;
   BYTE *Pixels;
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
		lump = Wads.CheckNumForName (buffer, ns_graphics);
		if (doomtemplate && lump >= 0 && i + start == 121)
		{ // HACKHACK: Don't load STCFN121 in doom(2), because
		  // it's not really a lower-case 'y' but an upper-case 'I'.
		  // Because a lot of wads with their own font seem to foolishly
		  // copy STCFN121 and make it an 'I' themselves, wads must
		  // provide STCFN120 (x) and STCFN122 (z) for STCFN121 to load.
			if (Wads.CheckNumForName ("STCFN120", ns_graphics) == -1 ||
				Wads.CheckNumForName ("STCFN122", ns_graphics) == -1)
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

	if ('N'-first>=0 && 'N'-first<count && Chars['N' - first].Pic)
	{
		SpaceWidth = (Chars['N' - first].Pic->GetWidth() + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}
	BuildTranslations (luminosity, identity);

	delete[] luminosity;
	delete[] charlumps;
}

FFont::~FFont ()
{
	if (Chars)
	{
		int count = LastChar - FirstChar + 1;

		for (int i = 0; i < count; ++i)
		{
			if (Chars[i].Pic != NULL && Chars[i].Pic->Name[0] == 0)
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
	if (Name)
	{
		delete[] Name;
		Name = NULL;
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
	if (name == NULL)
	{
		return NULL;
	}
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
	for (i = 1, j = 1; i < 256; i++)
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

// Build translations for most text in the game. The console font uses
// BuildTranslations2, which fades to white.

void FFont::BuildTranslations (const double *luminosity, const BYTE *identity)
{
	static const TranslationParm transParm[NUM_TEXT_COLORS][3] =
	{
		{ {   0, 256, {  71,   0,   0 }, { 255, 184, 184 } } },		// CR_BRICK
		{ {   0, 256, {  51,  43,  19 }, { 255, 235, 223 } } },		// CR_TAN
		{ {   0, 256, {  39,  39,  39 }, { 239, 239, 239 } } },		// CR_GRAY
		{ {   0, 256, {  11,  23,   7 }, { 119, 255, 111 } } },		// CR_GREEN
		{ {   0, 256, {  83,  63,  47 }, { 191, 167, 143 } } },		// CR_BROWN
		{ {   0, 256, { 115,  43,   0 }, { 255, 255, 115 } } },		// CR_GOLD
		{ {   0, 256, {  63,   0,   0 }, { 255,   0,   0 } } },		// CR_RED
		{ {   0, 256, {   0,   0,  39 }, {   0,   0, 255 } } },		// CR_BLUE
		{ {   0, 256, {  32,   0,   0 }, { 255, 128,   0 } } },		// CR_ORANGE
		{ {   0, 256, {  36,  36,  36 }, { 255, 255, 255 } } },		// CR_WHITE
		{ {   0,  64, {  39,  39,  39 }, {  81,  81,  81 } },		// CR_YELLOW
		  {  80, 192, { 134,  83,  24 }, { 235, 159,  24 } },
		  { 208, 256, { 243, 168,  42 }, { 252, 208,  67 } } },
		{},															// CR_UNTRANSLATED
		{ {   0, 256, {  19,  19,  19 }, {  80,  80,  80 } } },		// CR_BLACK
		{ {   0, 256, {   0,   0, 115 }, { 180, 180, 255 } } },		// CR_LIGHTBLUE
		{ {   0, 256, { 207, 131,  83 }, { 255, 215, 187 } } },		// CR_CREAM
		{ {   0, 256, {  47,  55,  31 }, { 123, 127,  80 } } },		// CR_OLIVE
		{ {   0, 256, {  11,  23,   7 }, {  67, 147,  55 } } },		// CR_DARKGREEN
		{ {   0, 256, {  43,   0,   0 }, { 175,  43,  43 } } },		// CR_DARKRED
		{ {   0, 256, {  31,  23,  11 }, { 163, 107,  63 } } },		// CR_DARKBROWN
		{ {   0, 256, {  35,   0,  35 }, { 207,   0, 207 } } },		// CR_PURPLE
		{ {   0, 256, {  35,  35,  35 }, { 139, 139, 139 } } },		// CR_DARKGRAY
	};

	int i, j, k;
	BYTE *range;

	range = Ranges = new byte[NUM_TEXT_COLORS * ActiveColors];

	// Create different translations for different color ranges
	for (i = 0; i < NUM_TEXT_COLORS; i++)
	{
		if (i == CR_UNTRANSLATED)
		{
			memcpy (range, identity, ActiveColors);
			range += ActiveColors;
			continue;
		}

		*range++ = 0;

		for (j = 1; j < ActiveColors; j++)
		{
			int v = int(luminosity[j] * 256.0);

			// Find the color range that this luminosity value lies within.
			const TranslationParm *parms = &transParm[i][0];
			for (k = 0; k < 2; ++k, ++parms)
			{
				if (parms->RangeStart <= v && parms->RangeEnd >= v)
					break;
			}
			// Linearly interpolate to find out which color this luminosity level gets.
			int rangev = ((v - parms->RangeStart) << 8) / (parms->RangeEnd - parms->RangeStart);
			int r = ((parms->Start[0] << 8) + rangev * (parms->End[0] - parms->Start[0])) >> 8; // red
			int g = ((parms->Start[1] << 8) + rangev * (parms->End[1] - parms->Start[1])) >> 8; // green
			int b = ((parms->Start[2] << 8) + rangev * (parms->End[2] - parms->Start[2])) >> 8; // blue
			*range++ = ColorMatcher.Pick (r, g, b);
		}
	}
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
	Name = NULL;
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
		GlobalKerning = LittleShort(*(SWORD *)&data[12]);
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
		totalwidth = LittleShort(widths[0]);
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
			widths2[i] = LittleShort(widths[i]);
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
	delete[] widths2;
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
	// These translations are used by the console font.

	// The bottom half of the character's palette uses a darker range.
	static const BYTE transParmLo[NUM_TEXT_COLORS][2][3] =
	{
		{ {  71,   0,   0 }, { 163,  92,  92 } },	// CR_BRICK
		{ {  51,  43,  19 }, { 153, 139, 121 } },	// CR_TAN
		{ {  39,  39,  39 }, { 139, 139, 139 } },	// CR_GRAY
		{ {   0,   0,   0 }, {   0, 127,   0 } },	// CR_GREEN
		{ {   0,   0,   0 }, { 127,  64,   0 } },	// CR_BROWN
		{ {   0,   0,   0 }, { 127, 192,  64 } },	// CR_GOLD
		{ {   0,   0,   0 }, { 127,   0,   0 } },	// CR_RED
		{ {   0,   0,   0 }, {   0,   0, 127 } },	// CR_BLUE
		{ {  32,   0,   0 }, { 144,  64,   0 } },	// CR_ORANGE
		{ {   0,   0,   0 }, { 127, 127, 127 } },	// CR_WHITE
		{ {   0,   0,   0 }, { 127, 127,   0 } },	// CR_YELLOW
		{},											// CR_UNTRANSLATED
		{ {   0,   0,   0 }, {  50,  50,  50 } },	// CR_BLACK
		{ {   0,   0,  60 }, {  80,  80, 255 } },	// CR_LIGHTBLUE
		{ {  43,  35,  15 }, { 191, 123,  75 } },	// CR_CREAM
		{ {  55,  63,  39 }, { 123, 127,  99 } },	// CR_OLIVE
		{ {   0,   0,   0 }, {   0,  88,   0 } },	// CR_DARKGREEN
		{ {   0,   0,   0 }, { 115,   0,   0 } },	// CR_DARKRED
		{ {  43,  35,  15 }, { 119,  48,   0 } },	// CR_DARKBROWN
		{ {   0,   0,   0 }, { 159,   0, 155 } },	// CR_PURPLE
		{ {   0,   0,   0 }, { 100, 100, 100 } },	// CR_DARKGRAY
	};

	// And the top half of the character's palette uses a lighter range that
	// generally fades to white.
	static const BYTE transParmHi[NUM_TEXT_COLORS][2][3] =
	{
		{ { 128,   0,   0 }, { 255, 254, 254 } },	// CR_BRICK
		{ { 153, 139, 121 }, { 255, 255, 255 } },	// CR_TAN
		{ {  80,  80,  80 }, { 255, 255, 255 } },	// CR_GRAY
		{ {   0, 255,   0 }, { 254, 255, 254 } },	// CR_GREEN
		{ {  67,  47,  31 }, { 255, 231, 207 } },	// CR_BROWN
		{ { 223, 191,   0 }, { 223, 255, 254 } },	// CR_GOLD
		{ { 255,   0,   0 }, { 255, 254, 254 } },	// CR_RED
		{ {  64,  64, 255 }, { 222, 222, 255 } },	// CR_BLUE
		{ { 255, 127,   0 }, { 255, 254, 254 } },	// CR_ORANGE
		{ { 128, 128, 128 }, { 255, 255, 255 } },	// CR_WHITE
		{ { 255, 255,   0 }, { 255, 255, 255 } },	// CR_YELLOW
		{},											// CR_UNTRANSLATED
		{ {  10,  10,  10 }, {  80,  80,  80 } },	// CR_BLACK
		{ { 128, 128, 255 }, { 255, 255, 255 } },	// CR_LIGHTBLUE
		{ { 255, 179, 131 }, { 255, 255, 255 } },	// CR_CREAM
		{ { 103, 107,  79 }, { 209, 216, 168 } },	// CR_OLIVE
		{ {   0, 140,   0 }, { 220, 255, 220 } },	// CR_DARKGREEN
		{ { 128,   0,   0 }, { 255, 220, 220 } },	// CR_DARKRED
		{ { 115,  87,  67 }, { 247, 189,  88 } },	// CR_DARKBROWN
		{ { 255,   0, 255 }, { 255, 255, 255 } },	// CR_PURPLE
		{ {  64,  64,  64 }, { 180, 180, 180 } },	// CR_DARKGRAY
	};

	assert (ActiveColors == 255);

	byte *range;
	range = Ranges = new byte[NUM_TEXT_COLORS * ActiveColors];
	int i, j, r, g, b;

	for (i = 0; i < NUM_TEXT_COLORS; i++)
	{
		const BYTE (*parm)[3];

		if (i == CR_UNTRANSLATED)
		{
			memcpy (range, Ranges + CR_WHITE * ActiveColors, ActiveColors);
			range += ActiveColors;
			continue;
		}

		parm = &transParmLo[i][0];
		for (j = 0; j < 127; j++)
		{
			r = parm[0][0] + (parm[1][0] - parm[0][0]) * j / 126;
			g = parm[0][1] + (parm[1][1] - parm[0][1]) * j / 126;
			b = parm[0][2] + (parm[1][2] - parm[0][2]) * j / 126;
			*range++ = ColorMatcher.Pick (r, g, b);
		}

		parm = &transParmHi[i][0];
		for (j = 0; j < 128; j++)
		{
			r = parm[0][0] + (parm[1][0] - parm[0][0]) * j / 127;
			g = parm[0][1] + (parm[1][1] - parm[0][1]) * j / 127;
			b = parm[0][2] + (parm[1][2] - parm[0][2]) * j / 127;
			*range++ = ColorMatcher.Pick (r, g, b);
		}
	}
}

FFontChar1::FFontChar1 (int sourcelump, const BYTE *sourceremap)
: SourceRemap (sourceremap)
{
	UseType = FTexture::TEX_FontChar;
	Wads.GetLumpName(Name, sourcelump);
	Name[8] = 0;
	BaseTexture = TexMan[Name];		// it has already been added!
	Name[0] = 0;					// Make this texture unnamed

	// now copy all the properties from the base texture
	Width = BaseTexture->GetWidth();
	Height = BaseTexture->GetHeight();
	TopOffset = BaseTexture->TopOffset;
	LeftOffset = BaseTexture->LeftOffset;
	WidthBits = BaseTexture->WidthBits;
	HeightBits = BaseTexture->HeightBits;
	ScaleX = BaseTexture->ScaleX;
	ScaleY = BaseTexture->ScaleY;
	WidthMask = (1 << WidthBits) - 1;
	Pixels = NULL;
}

const BYTE *FFontChar1::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FFontChar1::MakeTexture ()
{
	// Make the texture as normal, then remap it so that all the colors
	// are at the low end of the palette
	Pixels = new BYTE[Width*Height];
	const BYTE *pix = BaseTexture->GetPixels();

	for (int x = 0; x < Width*Height; ++x)
	{
		Pixels[x]=SourceRemap[pix[x]];
	}
}

const BYTE *FFontChar1::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}

	BaseTexture->GetColumn(column, spans_out);
	return Pixels + column*Height;
}

void FFontChar1::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

FFontChar1::~FFontChar1 ()
{
	Unload ();
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
		Spans = NULL;
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

	if (Spans == NULL)
	{
		Spans = CreateSpans (Pixels);
	}
}

//===========================================================================
// 
// Essentially a normal multilump font but 
// with an explicit list of character patches
//
//===========================================================================
class FSpecialFont : public FFont
{
public:
	FSpecialFont (const char *name, int first, int count, int *lumplist, const bool *notranslate);
};


FSpecialFont::FSpecialFont (const char *name, int first, int count, int *lumplist, const bool *notranslate)
{
	int i, j, lump;
	char buffer[12];
	int *charlumps;
	byte usedcolors[256], identity[256];
	double *luminosity;
	int maxyoffs;
	int TotalColors;

	Name=copystring(name);
	Chars = new CharData[count];
	charlumps = new int[count];
	PatchRemap = new BYTE[256];
	Ranges = NULL;
	FirstChar = first;
	LastChar = first + count - 1;
	FontHeight = 0;
	GlobalKerning = false;
	memset (usedcolors, 0, 256);
	Next = FirstFont;
	FirstFont = this;

	maxyoffs = 0;

	for (i = 0; i < count; i++)
	{
		lump = charlumps[i] = lumplist[i];
		if (lump >= 0)
		{
			Wads.GetLumpName(buffer, lump);
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

	// exclude the non-translated colors from the translation calculation
	if (notranslate != NULL)
	{
		for (i = 0; i < 256; i++)
			if (notranslate[i])
				usedcolors[i] = false;
	}

	TotalColors = ActiveColors = SimpleTranslation (usedcolors, PatchRemap, identity, &luminosity);

	// Map all untranslated colors into the table of used colors
	if (notranslate != NULL)
	{
		for (i = 0; i < 256; i++) 
		{
			if (notranslate[i]) 
			{
				PatchRemap[i] = TotalColors;
				identity[TotalColors] = i;
				TotalColors++;
			}
		}
	}

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

	// Special fonts normally don't have all characters so be careful here!
	if ('N'-first>=0 && 'N'-first<count && Chars['N' - first].Pic) 
	{
		SpaceWidth = (Chars['N' - first].Pic->GetWidth() + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}

	BuildTranslations (luminosity, identity);

	// add the untranslated colors to the Ranges table
	if (ActiveColors < TotalColors)
	{
		int factor = 1;
		byte *oldranges = Ranges;
		Ranges = new byte[NUM_TEXT_COLORS * TotalColors * factor];

		for (i = 0; i < CR_UNTRANSLATED; i++)
		{
			memcpy (&Ranges[i * TotalColors * factor], &oldranges[i * ActiveColors * factor], ActiveColors * factor);

			for (j = ActiveColors; j < TotalColors; j++)
			{
				Ranges[TotalColors*i + j] = identity[j];
			}
		}
		delete[] oldranges;
	}
	ActiveColors=TotalColors;

	delete[] luminosity;
	delete[] charlumps;
}


//===========================================================================
// 
// Initialize a list of custom multipatch fonts
//
//===========================================================================

void V_InitCustomFonts()
{
	int lumplist[256];
	bool notranslate[256];
	char namebuffer[16], templatebuf[16];
	int adder=0;
	int i;
	int llump,lastlump=0;
	int format;
	int start;
	int first;
	int count;


	while ((llump = Wads.FindLump ("FONTDEFS", &lastlump)) != -1)
	{
		SC_OpenLumpNum (llump, "FONTDEFS");
		while (SC_GetString())
		{
			memset (lumplist, -1, sizeof(lumplist));
			memset (notranslate, 0, sizeof(notranslate));
			strncpy (namebuffer, sc_String, 15);
			namebuffer[15] = 0;
			format = 0;
			start = 33;
			first = 33;
			count = 223;

			SC_MustGetStringName ("{");
			while (!SC_CheckString ("}"))
			{
				SC_MustGetString();
				if (SC_Compare ("TEMPLATE"))
				{
					if (format == 2) goto wrong;
					SC_MustGetString();
					strncpy (templatebuf, sc_String, 16);
					templatebuf[15] = 0;
					format = 1;
				}
				else if (SC_Compare ("BASE"))
				{
					if (format == 2) goto wrong;
					SC_MustGetNumber();
					start = sc_Number;
					format = 1;
				}
				else if (SC_Compare ("FIRST"))
				{
					if (format == 2) goto wrong;
					SC_MustGetNumber();
					first = sc_Number;
					format = 1;
				}
				else if (SC_Compare ("COUNT"))
				{
					if (format == 2) goto wrong;
					SC_MustGetNumber();
					count = sc_Number;
					format = 1;
				}
				else if (SC_Compare ("NOTRANSLATION"))
				{
					if (format == 1) goto wrong;
					while (SC_CheckNumber() && !sc_Crossed)
					{
						if (sc_Number >= 0 && sc_Number < 256)
							notranslate[sc_Number] = true;
					}
					format=2;
				}
				else
				{
					if (format == 1) goto wrong;
					int *p = &lumplist[*(unsigned char*)sc_String];
					SC_MustGetString();
					*p = Wads.CheckNumForName (sc_String);
					format=2;
				}
			}
			if (format==1)
			{
				new FFont (namebuffer, templatebuf, first, count, start);
			}
			else if (format==2)
			{
				for (i = 0; i < 256; i++)
				{
					if (lumplist[i] != -1)
					{
						first = i;
						break;
					}
				}
				for (i = 255; i >= 0; i--)
				{
					if (lumplist[i] != -1)
					{
						count = i - first + 1;
						break;
					}
				}
				if (count>0)
				{
					new FSpecialFont (namebuffer, first, count, &lumplist[first], notranslate);
				}
			}
			else goto wrong;
		}
		SC_Close ();
	}
	return;

wrong:
	SC_ScriptError ("Invalid combination of properties in font '%s'", namebuffer);
}
