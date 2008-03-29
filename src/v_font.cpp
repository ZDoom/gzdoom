/*
** v_font.cpp
** Font management
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

/* Special file formats handled here:

FON1 "console" fonts have the following header:
	char  Magic[4];			-- The characters "FON1"
	uword CharWidth;		-- Character cell width
	uword CharHeight;		-- Character cell height

The FON1 header is followed by RLE character data for all 256
8-bit ASCII characters.


FON2 "standard" fonts have the following header:
	char  Magic[4];			-- The characters "FON2"
	uword FontHeight;		-- Every character in a font has the same height
	ubyte FirstChar;		-- First character defined by this font.
	ubyte LastChar;			-- Last character definde by this font.
	ubyte bConstantWidth;
	ubyte ShadingType;
	ubyte PaletteSize;		-- size of palette in entries (not bytes!)
	ubyte Flags;

There is presently only one flag for FON2:
	FOF_WHOLEFONTKERNING 1	-- The Kerning field is present in the file

The FON2 header is followed by variable length data:
	word  Kerning;
		-- only present if FOF_WHOLEFONTKERNING is set

	ubyte Palette[PaletteSize+1][3];
		-- The last entry is the delimiter color. The delimiter is not used
		-- by the font but is used my imagetool when converting the font
		-- back to an image. Color 0 is the transparent color and is also
		-- used only for converting the font back to an image. The other
		-- entries are all presorted in increasing order of brightness.

	ubyte CharacterData[...];
		-- RLE character data, in order
*/

// HEADER FILES ------------------------------------------------------------

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
#include "hu_stuff.h"
#include "r_draw.h"
#include "r_translate.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_LOG_COLOR	PalEntry(223,223,223)

// TYPES -------------------------------------------------------------------

// This structure is used by BuildTranslations() to hold color information.
struct TranslationParm
{
	short RangeStart;	// First level for this range
	short RangeEnd;		// Last level for this range
	BYTE Start[3];		// Start color for this range
	BYTE End[3];		// End color for this range
};

struct TranslationMap
{
	FName Name;
	int Number;
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

   FTexture *BaseTexture;
   BYTE *Pixels;
   const BYTE *SourceRemap;
};

// This is a font character that reads RLE compressed data.
class FFontChar2 : public FTexture
{
public:
	FFontChar2 (int sourcelump, const BYTE *sourceremap, int sourcepos, int width, int height);
	~FFontChar2 ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	int SourcePos;
	BYTE *Pixels;
	Span **Spans;
	const BYTE *SourceRemap;

	void MakeTexture ();
};

struct TempParmInfo
{
	unsigned int StartParm[2];
	unsigned int ParmLen[2];
	int Index;
};
struct TempColorInfo
{
	FName Name;
	unsigned int ParmInfo;
	PalEntry LogColor;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int STACK_ARGS TranslationMapCompare (const void *a, const void *b);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FFont *FFont::FirstFont = NULL;
int NumTextColors;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const BYTE myislower[256] =
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

static TArray<TranslationParm> TranslationParms[2];
static TArray<TranslationMap> TranslationLookup;
static TArray<PalEntry> TranslationColors;

// CODE --------------------------------------------------------------------

FFont *V_GetFont(const char *name)
{
	FFont *font = FFont::FindFont (name);
	if (font == NULL)
	{
		int lump = -1;

		lump = Wads.CheckNumForFullName(name, true);
		if (lump < 0 && strlen(name) > 8)
		{
			FString fullname;
			fullname.Format("%s.fon", name);
			lump = Wads.CheckNumForFullName(fullname);
		}
		
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
			if (picnum > 0)
			{
				font = new FSinglePicFont (name);
			}
		}
	}
	return font;
}
//==========================================================================
//
// SerializeFFontPtr
//
//==========================================================================

FArchive &SerializeFFontPtr (FArchive &arc, FFont* &font)
{
	if (arc.IsStoring ())
	{
		arc << font->Name;
	}
	else
	{
		char *name = NULL;

		arc << name;
		font = V_GetFont(name);
		if (font == NULL)
		{
			Printf ("Could not load font %s\n", name);
			font = SmallFont;
		}
		delete[] name;
	}
	return arc;
}

//==========================================================================
//
// FFont :: FFont
//
// Loads a multi-texture font.
//
//==========================================================================

FFont::FFont (const char *name, const char *nametemplate, int first, int count, int start)
{
	int i, lump;
	char buffer[12];
	int *charlumps;
	BYTE usedcolors[256], identity[256];
	double *luminosity;
	int maxyoffs;
	bool doomtemplate = gameinfo.gametype == GAME_Doom ? strncmp (nametemplate, "STCFN", 5) == 0 : false;

	Chars = new CharData[count];
	charlumps = new int[count];
	PatchRemap = new BYTE[256];
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
			FTexture *pic = TexMan[buffer];
			if (pic != NULL)
			{
				int height = pic->GetScaledHeight();
				int yoffs = pic->GetScaledTopOffset();

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
		SpaceWidth = (Chars['N' - first].Pic->GetScaledWidth() + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}
	BuildTranslations (luminosity, identity, &TranslationParms[0][0], ActiveColors, NULL);

	delete[] luminosity;
	delete[] charlumps;
}

//==========================================================================
//
// FFont :: ~FFont
//
//==========================================================================

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

//==========================================================================
//
// FFont :: FindFont
//
// Searches for the named font in the list of loaded fonts, returning the
// font if it was found. The disk is not checked if it cannot be found.
//
//==========================================================================

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

//==========================================================================
//
// RecordTextureColors
//
// Given a 256 entry buffer, sets every entry that corresponds to a color
// used by the texture to 1.
//
//==========================================================================

void RecordTextureColors (FTexture *pic, BYTE *usedcolors)
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

//==========================================================================
//
// compare
//
// Used for sorting colors by brightness.
//
//==========================================================================

static int STACK_ARGS compare (const void *arg1, const void *arg2)
{
	if (RPART(GPalette.BaseColors[*((BYTE *)arg1)]) * 299 +
		GPART(GPalette.BaseColors[*((BYTE *)arg1)]) * 587 +
		BPART(GPalette.BaseColors[*((BYTE *)arg1)]) * 114  <
		RPART(GPalette.BaseColors[*((BYTE *)arg2)]) * 299 +
		GPART(GPalette.BaseColors[*((BYTE *)arg2)]) * 587 +
		BPART(GPalette.BaseColors[*((BYTE *)arg2)]) * 114)
		return -1;
	else
		return 1;
}

//==========================================================================
//
// FFont :: SimpleTranslation
//
// Colorsused, translation, and reverse must all be 256 entry buffers.
// Colorsused must already be filled out.
// Translation be set to remap the source colors to a new range of
// consecutive colors based at 1 (0 is transparent).
// Reverse will be just the opposite of translation: It maps the new color
// range to the original colors.
// *Luminosity will be an array just large enough to hold the brightness
// levels of all the used colors, in consecutive order. It is sorted from
// darkest to lightest and scaled such that the darkest color is 0.0 and
// the brightest color is 1.0.
// The return value is the number of used colors and thus the number of
// entries in *luminosity.
//
//==========================================================================

int FFont::SimpleTranslation (BYTE *colorsused, BYTE *translation, BYTE *reverse, double **luminosity)
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

//==========================================================================
//
// FFont :: BuildTranslations
//
// Build color translations for this font. Luminosity is an array of
// brightness levels. The ActiveColors member must be set to indicate how
// large this array is. Identity is an array that remaps the colors to
// their original values; it is only used for CR_UNTRANSLATED. Ranges
// is an array of TranslationParm structs defining the ranges for every
// possible color, in order. Palette is the colors to use for the
// untranslated version of the font.
//
//==========================================================================

void FFont::BuildTranslations (const double *luminosity, const BYTE *identity,
							   const void *ranges, int total_colors, const PalEntry *palette)
{
	int i, j;
	const TranslationParm *parmstart = (const TranslationParm *)ranges;

	FRemapTable remap(total_colors);

	// Create different translations for different color ranges
	Ranges.Clear();
	for (i = 0; i < NumTextColors; i++)
	{
		if (i == CR_UNTRANSLATED)
		{
			if (identity != NULL)
			{
				memcpy (remap.Remap, identity, ActiveColors);
				if (palette != NULL)
				{
					memcpy (remap.Palette, palette, ActiveColors*sizeof(PalEntry));
				}
				else
				{
					remap.Palette[0] = GPalette.BaseColors[identity[0]] & MAKEARGB(0,255,255,255);
					for (j = 1; j < ActiveColors; ++j)
					{
						remap.Palette[j] = GPalette.BaseColors[identity[j]] | MAKEARGB(255,0,0,0);
					}
				}
			}
			else
			{
				remap = Ranges[0];
			}
			Ranges.Push(remap);
			continue;
		}

		assert(parmstart->RangeStart >= 0);

		remap.Remap[0] = 0;
		remap.Palette[0] = 0;

		for (j = 1; j < ActiveColors; j++)
		{
			int v = int(luminosity[j] * 256.0);

			// Find the color range that this luminosity value lies within.
			const TranslationParm *parms = parmstart - 1;
			do
			{
				parms++;
				if (parms->RangeStart <= v && parms->RangeEnd >= v)
					break;
			}
			while (parms[1].RangeStart > parms[0].RangeEnd);

			// Linearly interpolate to find out which color this luminosity level gets.
			int rangev = ((v - parms->RangeStart) << 8) / (parms->RangeEnd - parms->RangeStart);
			int r = ((parms->Start[0] << 8) + rangev * (parms->End[0] - parms->Start[0])) >> 8; // red
			int g = ((parms->Start[1] << 8) + rangev * (parms->End[1] - parms->Start[1])) >> 8; // green
			int b = ((parms->Start[2] << 8) + rangev * (parms->End[2] - parms->Start[2])) >> 8; // blue
			r = clamp(r, 0, 255);
			g = clamp(g, 0, 255);
			b = clamp(b, 0, 255);
			remap.Remap[j] = ColorMatcher.Pick(r, g, b);
			remap.Palette[j] = PalEntry(255,r,g,b);
		}
		Ranges.Push(remap);

		// Advance to the next color range.
		while (parmstart[1].RangeStart > parmstart[0].RangeEnd)
		{
			parmstart++;
		}
		parmstart++;
	}
}

//==========================================================================
//
// FFont :: GetColorTranslation
//
//==========================================================================

FRemapTable *FFont::GetColorTranslation (EColorRange range) const
{
	if (ActiveColors == 0)
		return NULL;
	else if (range >= NumTextColors)
		range = CR_UNTRANSLATED;
	return &Ranges[range];
}

//==========================================================================
//
// FFont :: GetChar
//
//==========================================================================

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
	*width = Chars[code].Pic->GetScaledWidth();
	return Chars[code].Pic;
}

//==========================================================================
//
// FFont :: GetCharWidth
//
//==========================================================================

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

	return Chars[code - FirstChar].Pic->GetScaledWidth();
}

//==========================================================================
//
// FFont :: Preload
//
// Loads most of the 7-bit ASCII characters. In the case of D3DFB, this
// means all the characters of a font have a better chance of being packed
// into the same hardware texture.
//
//==========================================================================

void FFont::Preload() const
{
	// First and last char are the same? Wait until it's actually needed
	// since nothing is gained by preloading now.
	if (FirstChar == LastChar)
	{
		return;
	}
	for (int i = MAX(FirstChar, 0x21); i < MIN(LastChar, 0x7e); ++i)
	{
		int foo;
		FTexture *pic = GetChar(i, &foo);
		if (pic != NULL)
		{
			pic->GetNative(false);
		}
	}
}

//==========================================================================
//
// FFont :: StaticPreloadFonts
//
// Preloads all the defined fonts.
//
//==========================================================================

void FFont::StaticPreloadFonts()
{
	for (FFont *font = FirstFont; font != NULL; font = font->Next)
	{
		font->Preload();
	}
}

//==========================================================================
//
// FFont :: FFont - default constructor
//
//==========================================================================

FFont::FFont ()
{
	Chars = NULL;
	PatchRemap = NULL;
	Name = NULL;
}

//==========================================================================
//
// FSingleLumpFont :: FSingleLumpFont
//
// Loads a FON1 or FON2 font resource.
//
//==========================================================================

FSingleLumpFont::FSingleLumpFont (const char *name, int lump)
{
	assert(lump >= 0);

	Name = copystring (name);

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

	Next = FirstFont;
	FirstFont = this;
}

//==========================================================================
//
// FSingleLumpFont :: CreateFontFromPic
//
//==========================================================================

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

//==========================================================================
//
// FSingleLumpFont :: LoadFON1
//
// FON1 is used for the console font.
//
//==========================================================================

void FSingleLumpFont::LoadFON1 (int lump, const BYTE *data)
{
	double luminosity[256];
	int w, h;

	Chars = new CharData[256];

	w = data[4] + data[5]*256;
	h = data[6] + data[7]*256;

	FontHeight = h;
	SpaceWidth = w;
	FirstChar = 0;
	LastChar = 255;
	GlobalKerning = 0;
	PatchRemap = new BYTE[256];

	CheckFON1Chars (lump, data, luminosity);
	BuildTranslations (luminosity, NULL, &TranslationParms[1][0], ActiveColors, NULL);
}

//==========================================================================
//
// FSingleLumpFont :: LoadFON2
//
// FON2 is used for everything but the console font. The console font should
// probably use FON2 as well, but oh well.
//
//==========================================================================

void FSingleLumpFont::LoadFON2 (int lump, const BYTE *data)
{
	int count, i, totalwidth;
	int *widths2;
	BYTE identity[256];
	double luminosity[256];
	PalEntry local_palette[256];
	WORD *widths;
	const BYTE *palette;
	const BYTE *data_p;

	FontHeight = data[4] + data[5]*256;
	FirstChar = data[6];
	LastChar = data[7];
	ActiveColors = data[10];
	
	count = LastChar - FirstChar + 1;
	Chars = new CharData[count];
	widths2 = new int[count];
	if (data[11] & 1)
	{ // Font specifies a kerning value.
		GlobalKerning = LittleShort(*(SWORD *)&data[12]);
		widths = (WORD *)(data + 14);
	}
	else
	{ // Font does not specify a kerning value.
		GlobalKerning = 0;
		widths = (WORD *)(data + 12);
	}
	totalwidth = 0;

	if (data[8])
	{ // Font is mono-spaced.
		totalwidth = LittleShort(widths[0]);
		for (i = 0; i < count; ++i)
		{
			widths2[i] = totalwidth;
		}
		totalwidth *= count;
		palette = (BYTE *)&widths[1];
	}
	else
	{ // Font has varying character widths.
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

	FixupPalette (identity, luminosity, palette, data[9] == 0, local_palette);

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
			Chars[i].Pic = new FFontChar2 (lump, NULL, data_p - data, widths2[i], FontHeight);
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

	BuildTranslations (luminosity, identity, &TranslationParms[0][0], ActiveColors, local_palette);
	delete[] widths2;
}

//==========================================================================
//
// FSingleLumpFont :: CheckFON1Chars
//
// Scans a FON1 resource for all the color values it uses and sets up
// some tables like SimpleTranslation. Data points to the RLE data for
// the characters. Also sets up the character textures.
//
//==========================================================================

void FSingleLumpFont::CheckFON1Chars (int lump, const BYTE *data, double *luminosity)
{
	BYTE used[256], reverse[256];
	const BYTE *data_p;
	int i, j;

	memset (used, 0, 256);
	data_p = data + 8;

	for (i = 0; i < 256; ++i)
	{
		int destSize = SpaceWidth * FontHeight;

		Chars[i].Pic = new FFontChar2 (lump, PatchRemap, data_p - data, SpaceWidth, FontHeight);

		// Advance to next char's data and count the used colors.
		do
		{
			SBYTE code = *data_p++;
			if (code >= 0)
			{
				destSize -= code+1;
				while (code-- >= 0)
				{
					used[*data_p++] = 1;
				}
			}
			else if (code != -128)
			{
				used[*data_p++] = 1;
				destSize -= 1 - code;
			}
		} while (destSize > 0);
	}

	memset (PatchRemap, 0, 256);
	reverse[0] = 0;
	for (i = 1, j = 1; i < 256; ++i)
	{
		if (used[i])
		{
			reverse[j++] = i;
		}
	}
	for (i = 1; i < j; ++i)
	{
		PatchRemap[reverse[i]] = i;
		luminosity[i] = (reverse[i] - 1) / 254.0;
	}
	ActiveColors = j;
}

//==========================================================================
//
// FSingleLumpFont :: FixupPalette
//
// Finds the best matches for the colors used by a FON2 font and sets up
// some tables like SimpleTranslation.
//
//==========================================================================

void FSingleLumpFont::FixupPalette (BYTE *identity, double *luminosity, const BYTE *palette, bool rescale, PalEntry *out_palette)
{
	int i;
	double maxlum = 0.0;
	double minlum = 100000000.0;
	double diver;

	identity[0] = 0;
	palette += 3;	// Skip the transparent color

	for (i = 1; i <= ActiveColors; ++i, palette += 3)
	{
		int r = palette[0];
		int g = palette[1];
		int b = palette[2];
		double lum = r*0.299 + g*0.587 + b*0.114;
		identity[i] = ColorMatcher.Pick (r, g, b);
		luminosity[i] = lum;
		out_palette[i].r = r;
		out_palette[i].g = g;
		out_palette[i].b = b;
		out_palette[i].a = 255;
		if (lum > maxlum)
			maxlum = lum;
		if (lum < minlum)
			minlum = lum;
	}
	out_palette[0] = 0;

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

//==========================================================================
//
// FSinglePicFont :: FSinglePicFont
//
// Creates a font to wrap a texture so that you can use hudmessage as if it
// were a hudpic command. It does not support translation, but animation
// is supported, unlike all the real fonts.
//
//==========================================================================

FSinglePicFont::FSinglePicFont(const char *picname)
{
	int picnum = TexMan.CheckForTexture (picname, FTexture::TEX_Any);

	if (picnum <= 0)
	{
		I_FatalError ("%s is not a font or texture", picname);
	}

	FTexture *pic = TexMan[picnum];

	Name = copystring(picname);
	FontHeight = pic->GetHeight();
	SpaceWidth = pic->GetWidth();
	GlobalKerning = 0;
	FirstChar = LastChar = 'A';
	ActiveColors = 0;
	PicNum = picnum;

	Next = FirstFont;
	FirstFont = this;
}

//==========================================================================
//
// FSinglePicFont :: GetChar
//
// Returns the texture if code is 'a' or 'A', otherwise NULL.
//
//==========================================================================

FTexture *FSinglePicFont::GetChar (int code, int *const width) const
{
	*width = SpaceWidth;
	if (code == 'a' || code == 'A')
	{
		return TexMan(PicNum);
	}
	else
	{
		return NULL;
	}
}

//==========================================================================
//
// FSinglePicFont :: GetCharWidth
//
// Don't expect the text functions to work properly if I actually allowed
// the character width to vary depending on the animation frame.
//
//==========================================================================

int FSinglePicFont::GetCharWidth (int code) const
{
	return SpaceWidth;
}

//==========================================================================
//
// FFontChar1 :: FFontChar1
//
// Used by fonts made from textures.
//
//==========================================================================

FFontChar1::FFontChar1 (int sourcelump, const BYTE *sourceremap)
: SourceRemap (sourceremap)
{
	UseType = FTexture::TEX_FontChar;
	Wads.GetLumpName(Name, sourcelump);
	Name[8] = 0;
	BaseTexture = TexMan[Name];		// it has already been added!
	Name[0] = 0;					// Make this texture unnamed

	// now copy all the properties from the base texture
	CopySize(BaseTexture);
	Pixels = NULL;
}

//==========================================================================
//
// FFontChar1 :: GetPixels
//
//==========================================================================

const BYTE *FFontChar1::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

//==========================================================================
//
// FFontChar1 :: MakeTexture
//
//==========================================================================

void FFontChar1::MakeTexture ()
{
	// Make the texture as normal, then remap it so that all the colors
	// are at the low end of the palette
	Pixels = new BYTE[Width*Height];
	const BYTE *pix = BaseTexture->GetPixels();

	for (int x = 0; x < Width*Height; ++x)
	{
		Pixels[x] = SourceRemap[pix[x]];
	}
}

//==========================================================================
//
// FFontChar1 :: GetColumn
//
//==========================================================================

const BYTE *FFontChar1::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}

	BaseTexture->GetColumn(column, spans_out);
	return Pixels + column*Height;
}

//==========================================================================
//
// FFontChar1 :: Unload
//
//==========================================================================

void FFontChar1::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

//==========================================================================
//
// FFontChar1 :: ~FFontChar1
//
//==========================================================================

FFontChar1::~FFontChar1 ()
{
	Unload ();
}

//==========================================================================
//
// FFontChar2 :: FFontChar2
//
// Used by FON1 and FON2 fonts.
//
//==========================================================================

FFontChar2::FFontChar2 (int sourcelump, const BYTE *sourceremap, int sourcepos, int width, int height)
: SourceLump (sourcelump), SourcePos (sourcepos), Pixels (0), Spans (0), SourceRemap(sourceremap)
{
	UseType = TEX_FontChar;
	Width = width;
	Height = height;
	TopOffset = 0;
	LeftOffset = 0;
	CalcBitSize ();
}

//==========================================================================
//
// FFontChar2 :: ~FFontChar2
//
//==========================================================================

FFontChar2::~FFontChar2 ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

//==========================================================================
//
// FFontChar2 :: Unload
//
//==========================================================================

void FFontChar2::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

//==========================================================================
//
// FFontChar2 :: GetPixels
//
//==========================================================================

const BYTE *FFontChar2::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

//==========================================================================
//
// FFontChar2 :: GetColumn
//
//==========================================================================

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

//==========================================================================
//
// FFontChar2 :: MakeTexture
//
//==========================================================================

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
				if (SourceRemap != NULL)
				{
					*dest_p = SourceRemap[*dest_p];
				}
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
					if (SourceRemap != NULL)
					{
						setval = SourceRemap[setval];
					}
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

//==========================================================================
//
// FSpecialFont :: FSpecialFont
//
//==========================================================================

FSpecialFont::FSpecialFont (const char *name, int first, int count, int *lumplist, const bool *notranslate)
{
	int i, j, lump;
	char buffer[12];
	int *charlumps;
	BYTE usedcolors[256], identity[256];
	double *luminosity;
	int maxyoffs;
	int TotalColors;

	Name=copystring(name);
	Chars = new CharData[count];
	charlumps = new int[count];
	PatchRemap = new BYTE[256];
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
			buffer[8]=0;
			FTexture *pic = TexMan[buffer];
			if (pic != NULL)
			{
				int height = pic->GetScaledHeight();
				int yoffs = pic->GetScaledTopOffset();

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
		SpaceWidth = (Chars['N' - first].Pic->GetScaledWidth() + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}

	BuildTranslations (luminosity, identity, &TranslationParms[0][0], TotalColors, NULL);

	// add the untranslated colors to the Ranges tables
	if (ActiveColors < TotalColors)
	{
		for (i = 0; i < NumTextColors; i++)
		{
			FRemapTable *remap = &Ranges[i];
			for (j = ActiveColors; j < TotalColors; ++j)
			{
				remap->Remap[j] = identity[j];
				remap->Palette[j] = GPalette.BaseColors[identity[j]];
			}
		}
	}
	ActiveColors = TotalColors;

	delete[] luminosity;
	delete[] charlumps;
}


//==========================================================================
//
// V_InitCustomFonts
//
// Initialize a list of custom multipatch fonts
//
//==========================================================================

void V_InitCustomFonts()
{
	FScanner sc;
	int lumplist[256];
	bool notranslate[256];
	char namebuffer[16], templatebuf[16];
	int i;
	int llump,lastlump=0;
	int format;
	int start;
	int first;
	int count;

	while ((llump = Wads.FindLump ("FONTDEFS", &lastlump)) != -1)
	{
		sc.OpenLumpNum(llump, "FONTDEFS");
		while (sc.GetString())
		{
			memset (lumplist, -1, sizeof(lumplist));
			memset (notranslate, 0, sizeof(notranslate));
			strncpy (namebuffer, sc.String, 15);
			namebuffer[15] = 0;
			format = 0;
			start = 33;
			first = 33;
			count = 223;

			sc.MustGetStringName ("{");
			while (!sc.CheckString ("}"))
			{
				sc.MustGetString();
				if (sc.Compare ("TEMPLATE"))
				{
					if (format == 2) goto wrong;
					sc.MustGetString();
					strncpy (templatebuf, sc.String, 16);
					templatebuf[15] = 0;
					format = 1;
				}
				else if (sc.Compare ("BASE"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					start = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("FIRST"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					first = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("COUNT"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					count = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("NOTRANSLATION"))
				{
					if (format == 1) goto wrong;
					while (sc.CheckNumber() && !sc.Crossed)
					{
						if (sc.Number >= 0 && sc.Number < 256)
							notranslate[sc.Number] = true;
					}
					format=2;
				}
				else
				{
					if (format == 1) goto wrong;
					int *p = &lumplist[*(unsigned char*)sc.String];
					sc.MustGetString();
					*p = Wads.CheckNumForName (sc.String);
					format=2;
				}
			}
			if (format == 1)
			{
				new FFont (namebuffer, templatebuf, first, count, start);
			}
			else if (format == 2)
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
		sc.Close();
	}
	return;

wrong:
	sc.ScriptError ("Invalid combination of properties in font '%s'", namebuffer);
}

//==========================================================================
//
// V_InitFontColors
//
// Reads the list of color translation definitions into memory.
//
//==========================================================================

void V_InitFontColors ()
{
	TArray<FName> names;
	int lump, lastlump = 0;
	TranslationParm tparm = { 0 };	// Silence GCC
	TArray<TranslationParm> parms;
	TArray<TempParmInfo> parminfo;
	TArray<TempColorInfo> colorinfo;
	int c, parmchoice;
	TempParmInfo info;
	TempColorInfo cinfo;
	PalEntry logcolor;
	unsigned int i, j;
	int k, index;

	info.Index = -1;

	while ((lump = Wads.FindLump ("TEXTCOLO", &lastlump)) != -1)
	{
		FScanner sc(lump, "textcolors.txt");
		while (sc.GetString())
		{
			names.Clear();

			logcolor = DEFAULT_LOG_COLOR;

			// Everything until the '{' is considered a valid name for the
			// color range.
			names.Push (sc.String);
			while (sc.MustGetString(), !sc.Compare ("{"))
			{
				if (names[0] == NAME_Untranslated)
				{
					sc.ScriptError ("The \"untranslated\" color may not have any other names");
				}
				names.Push (sc.String);
			}

			parmchoice = 0;
			info.StartParm[0] = parms.Size();
			info.StartParm[1] = 0;
			info.ParmLen[1] = info.ParmLen[0] = 0;
			tparm.RangeEnd = tparm.RangeStart = -1;

			while (sc.MustGetString(), !sc.Compare ("}"))
			{
				if (sc.Compare ("Console:"))
				{
					if (parmchoice == 1)
					{
						sc.ScriptError ("Each color may only have one set of console ranges");
					}
					parmchoice = 1;
					info.StartParm[1] = parms.Size();
					info.ParmLen[0] = info.StartParm[1] - info.StartParm[0];
					tparm.RangeEnd = tparm.RangeStart = -1;
				}
				else if (sc.Compare ("Flat:"))
				{
					sc.MustGetString();
					logcolor = V_GetColor (NULL, sc.String);
				}
				else
				{
					// Get first color
					c = V_GetColor (NULL, sc.String);
					tparm.Start[0] = RPART(c);
					tparm.Start[1] = GPART(c);
					tparm.Start[2] = BPART(c);

					// Get second color
					sc.MustGetString();
					c = V_GetColor (NULL, sc.String);
					tparm.End[0] = RPART(c);
					tparm.End[1] = GPART(c);
					tparm.End[2] = BPART(c);

					// Check for range specifier
					if (sc.CheckNumber())
					{
						if (tparm.RangeStart == -1 && sc.Number != 0)
						{
							sc.ScriptError ("The first color range must start at position 0");
						}
						if (sc.Number < 0 || sc.Number > 256)
						{
							sc.ScriptError ("The color range must be within positions [0,256]");
						}
						if (sc.Number <= tparm.RangeEnd)
						{
							sc.ScriptError ("The color range must not start before the previous one ends");
						}
						tparm.RangeStart = sc.Number;

						sc.MustGetNumber();
						if (sc.Number < 0 || sc.Number > 256)
						{
							sc.ScriptError ("The color range must be within positions [0,256]");
						}
						if (sc.Number <= tparm.RangeStart)
						{
							sc.ScriptError ("The color range end position must be larger than the start position");
						}
						tparm.RangeEnd = sc.Number;
					}
					else
					{
						tparm.RangeStart = tparm.RangeEnd + 1;
						tparm.RangeEnd = 256;
						if (tparm.RangeStart >= tparm.RangeEnd)
						{
							sc.ScriptError ("The color has too many ranges");
						}
					}
					parms.Push (tparm);
				}
			}
			info.ParmLen[parmchoice] = parms.Size() - info.StartParm[parmchoice];
			if (info.ParmLen[0] == 0)
			{
				if (names[0] != NAME_Untranslated)
				{
					sc.ScriptError ("There must be at least one normal range for a color");
				}
			}
			else
			{
				if (names[0] == NAME_Untranslated)
				{
					sc.ScriptError ("The \"untranslated\" color must be left undefined");
				}
			}
			if (info.ParmLen[1] == 0 && names[0] != NAME_Untranslated)
			{ // If a console translation is unspecified, make it white, since the console
			  // font has no color information stored with it.
				tparm.RangeStart = 0;
				tparm.RangeEnd = 256;
				tparm.Start[2] = tparm.Start[1] = tparm.Start[0] = 0;
				tparm.End[2] = tparm.End[1] = tparm.End[0] = 255;
				info.StartParm[1] = parms.Push (tparm);
				info.ParmLen[1] = 1;
			}
			cinfo.ParmInfo = parminfo.Push (info);
			// Record this color information for each name it goes by
			for (i = 0; i < names.Size(); ++i)
			{
				// Redefine duplicates in-place
				for (j = 0; j < colorinfo.Size(); ++j)
				{
					if (colorinfo[j].Name == names[i])
					{
						colorinfo[j].ParmInfo = cinfo.ParmInfo;
						colorinfo[j].LogColor = logcolor;
						break;
					}
				}
				if (j == colorinfo.Size())
				{
					cinfo.Name = names[i];
					cinfo.LogColor = logcolor;
					colorinfo.Push (cinfo);
				}
			}
		}
	}
	// Make permananent copies of all the color information we found.
	for (i = 0, index = 0; i < colorinfo.Size(); ++i)
	{
		TranslationMap tmap;
		TempParmInfo *pinfo;

		tmap.Name = colorinfo[i].Name;
		pinfo = &parminfo[colorinfo[i].ParmInfo];
		if (pinfo->Index < 0)
		{
			// Write out the set of remappings for this color.
			for (k = 0; k < 2; ++k)
			{
				for (j = 0; j < pinfo->ParmLen[k]; ++j)
				{
					TranslationParms[k].Push (parms[pinfo->StartParm[k] + j]);
				}
			}
			TranslationColors.Push (colorinfo[i].LogColor);
			pinfo->Index = index++;
		}
		tmap.Number = pinfo->Index;
		TranslationLookup.Push (tmap);
	}
	// Leave a terminating marker at the ends of the lists.
	tparm.RangeStart = -1;
	TranslationParms[0].Push (tparm);
	TranslationParms[1].Push (tparm);
	// Sort the translation lookups for fast binary searching.
	qsort (&TranslationLookup[0], TranslationLookup.Size(), sizeof(TranslationLookup[0]), TranslationMapCompare);

	NumTextColors = index;
	assert (NumTextColors >= NUM_TEXT_COLORS);
}

//==========================================================================
//
// TranslationMapCompare
//
//==========================================================================

static int STACK_ARGS TranslationMapCompare (const void *a, const void *b)
{
	return int(((const TranslationMap *)a)->Name) - int(((const TranslationMap *)b)->Name);
}

//==========================================================================
//
// V_FindFontColor
//
// Returns the color number for a particular named color range.
//
//==========================================================================

EColorRange V_FindFontColor (FName name)
{
	int min = 0, max = TranslationLookup.Size() - 1;

	while (min <= max)
	{
		unsigned int mid = (min + max) / 2;
		const TranslationMap *probe = &TranslationLookup[mid];
		if (probe->Name == name)
		{
			return EColorRange(probe->Number);
		}
		else if (probe->Name < name)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return CR_UNTRANSLATED;
}

//==========================================================================
//
// V_LogColorFromColorRange
//
// Returns the color to use for text in the startup/error log window.
//
//==========================================================================

PalEntry V_LogColorFromColorRange (EColorRange range)
{
	if ((unsigned int)range >= TranslationColors.Size())
	{ // Return default color
		return DEFAULT_LOG_COLOR;
	}
	return TranslationColors[range];
}

//==========================================================================
//
// V_ParseFontColor
//
// Given a pointer to a color identifier (presumably just after a color
// escape character), return the color it identifies and advances
// color_value to just past it.
//
//==========================================================================

EColorRange V_ParseFontColor (const BYTE *&color_value, int normalcolor, int boldcolor)
{
	const BYTE *ch = color_value;
	int newcolor = *ch++;

	if (newcolor == '-')		// Normal
	{
		newcolor = normalcolor;
	}
	else if (newcolor == '+')		// Bold
	{
		newcolor = boldcolor;
	}
	else if (newcolor == '[')		// Named
	{
		const BYTE *namestart = ch;
		while (*ch != ']' && *ch != '\0')
		{
			ch++;
		}
		FName rangename((const char *)namestart, int(ch - namestart), true);
		if (*ch != '\0')
		{
			ch++;
		}
		newcolor = V_FindFontColor (rangename);
	}
	else if (newcolor >= 'A' && newcolor < NUM_TEXT_COLORS + 'A')	// Standard, uppercase
	{
		newcolor -= 'A';
	}
	else if (newcolor >= 'a' && newcolor < NUM_TEXT_COLORS + 'a')	// Standard, lowercase
	{
		newcolor -= 'a';
	}
	else							// Incomplete!
	{
		color_value = ch - (*ch == '\0');
		return CR_UNDEFINED;
	}
	color_value = ch;
	return EColorRange(newcolor);
}

//==========================================================================
//
// V_InitFonts
//
//==========================================================================

void V_InitFonts()
{
	V_InitFontColors ();
	V_InitCustomFonts ();

	// load the heads-up font
	if (!(SmallFont = FFont::FindFont("SmallFont")))
	{
		if (Wads.CheckNumForName ("FONTA_S") >= 0)
		{
			SmallFont = new FFont ("SmallFont", "FONTA%02u", HU_FONTSTART, HU_FONTSIZE, 1);
		}
		else
		{
			SmallFont = new FFont ("SmallFont", "STCFN%.3d", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART);
		}
	}
	if (!(SmallFont2=FFont::FindFont("SmallFont2")))
	{
		if (Wads.CheckNumForName ("STBFN033", ns_graphics) >= 0)
		{
			SmallFont2 = new FFont ("SmallFont2", "STBFN%.3d", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART);
		}
		else
		{
			SmallFont2 = SmallFont;
		}
	}
	if (!(BigFont=FFont::FindFont("BigFont")))
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			BigFont = new FSingleLumpFont ("BigFont", Wads.GetNumForName ("DBIGFONT"));
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			BigFont = new FSingleLumpFont ("BigFont", Wads.GetNumForName ("SBIGFONT"));
		}
		else
		{
			BigFont = new FFont ("BigFont", "FONTB%02u", HU_FONTSTART, HU_FONTSIZE, 1);
		}
	}
	if (!(ConFont=FFont::FindFont("ConsoleFont")))
	{
		ConFont = new FSingleLumpFont ("ConsoleFont", Wads.GetNumForName ("CONFONT"));
	}
}
