/*
** r_translate.cpp
** Translation table handling
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

#include "palutil.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "printf.h"
#include "colormatcher.h"
#include "templates.h"
#include "palettecontainer.h"

PaletteContainer GPalette;
FColorMatcher ColorMatcher;

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void PaletteContainer::Init(int numslots)	// This cannot be a constructor!!!
{
	Clear();
	// Make sure that index 0 is always the identity translation.
	FRemapTable remap;
	remap.MakeIdentity();
	remap.Inactive = true;
	AddRemap(&remap);
	TranslationTables.Resize(numslots);
}

void PaletteContainer::SetPalette(const uint8_t* colors)
{
	for (int i = 0; i < 256; i++, colors += 3)
	{
		BaseColors[i] = PalEntry(colors[0], colors[1], colors[2]);
		Remap[i] = i;
	}

	// Find white and black from the original palette so that they can be
	// used to make an educated guess of the translucency % for a BOOM
	// translucency map.
	WhiteIndex = BestColor((uint32_t*)BaseColors, 255, 255, 255, 0, 255);
	BlackIndex = BestColor((uint32_t*)BaseColors, 0, 0, 0, 0, 255);
}


//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void PaletteContainer::Clear()
{
	remapArena.FreeAllBlocks();
	uniqueRemaps.Reset();
	TranslationTables.Reset();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FRemapTable* PaletteContainer::AddRemap(FRemapTable* remap)
{
	if (!remap) return uniqueRemaps[0];

	remap->crc32 = CalcCRC32((uint8_t*)remap->Palette, sizeof(remap->Palette));

	for (auto uremap : uniqueRemaps)
	{
		if (uremap->crc32 == remap->crc32 && uremap->NumEntries == remap->NumEntries && *uremap == *remap)
			return uremap;
	}
	auto newremap = (FRemapTable*)remapArena.Alloc(sizeof(FRemapTable));
	*newremap = *remap;
	auto index = uniqueRemaps.Push(newremap);
	newremap->Index = index;
	return newremap;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void PaletteContainer::UpdateTranslation(int trans, FRemapTable* remap)
{
	auto newremap = AddRemap(remap);
	TranslationTables[GetTranslationType(trans)].SetVal(GetTranslationIndex(trans), newremap);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

int PaletteContainer::AddTranslation(int slot, FRemapTable* remap, int count)
{
	uint32_t id;
	for (int i = 0; i < count; i++)
	{
		auto newremap = AddRemap(&remap[i]);
		id = TranslationTables[slot].Push(newremap);
	}
	return TRANSLATION(slot, id);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void PaletteContainer::CopyTranslation(int dest, int src)
{
	TranslationTables[GetTranslationType(dest)][GetTranslationType(src)] = TranslationToTable(src);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

FRemapTable *PaletteContainer::TranslationToTable(int translation)
{
	unsigned int type = GetTranslationType(translation);
	unsigned int index = GetTranslationIndex(translation);

	if (type <= 0 || type >= TranslationTables.Size() || index >= NumTranslations(type))
	{
		return NULL;
	}
	return GetTranslation(type, index);
}

//----------------------------------------------------------------------------
//
// Stores a copy of this translation but avoids duplicates
//
//----------------------------------------------------------------------------

int PaletteContainer::StoreTranslation(int slot, FRemapTable *remap)
{
	unsigned int i;

	auto size = NumTranslations(slot);
	for (i = 0; i < size; i++)
	{
		if (*remap == *TranslationToTable(TRANSLATION(slot, i)))
		{
			// A duplicate of this translation already exists
			return TRANSLATION(slot, i);
		}
	}
	if (size >= 65535)	// Translation IDs only have 16 bits for the index.
	{
		I_Error("Too many DECORATE translations");
	}
	return AddTranslation(slot, remap);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static bool IndexOutOfRange(const int color)
{
	const bool outOfRange = color < 0 || color > 255;

	if (outOfRange)
	{
		Printf(TEXTCOLOR_ORANGE "Palette index %i is out of range [0..255]\n", color);
	}

	return outOfRange;
}

static bool IndexOutOfRange(const int start, const int end)
{
	const bool outOfRange = IndexOutOfRange(start);
	return IndexOutOfRange(end) || outOfRange;
}

static bool IndexOutOfRange(const int start1, const int end1, const int start2, const int end2)
{
	const bool outOfRange = IndexOutOfRange(start1, end1);
	return IndexOutOfRange(start2, end2) || outOfRange;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::operator==(const FRemapTable& o)
{
	// Two translations are identical when they have the same amount of colors
	// and the palette values for both are identical.
	if (&o == this) return true;
	if (o.NumEntries != NumEntries) return false;
	return !memcmp(o.Palette, Palette, NumEntries * sizeof(*Palette));
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FRemapTable::MakeIdentity()
{
	int i;

	for (i = 0; i < NumEntries; ++i)
	{
		Remap[i] = i;
	}
	for (i = 0; i < NumEntries; ++i)
	{
		Palette[i] = GPalette.BaseColors[i];
	}
	for (i = 1; i < NumEntries; ++i)
	{
		Palette[i].a = 255;
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::IsIdentity() const
{
	for (int j = 0; j < 256; ++j)
	{
		if (Remap[j] != j)
		{
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddIndexRange(int start, int end, int pal1, int pal2)
{
	if (IndexOutOfRange(start, end, pal1, pal2))
	{
		return false;
	}

	double palcol, palstep;

	if (start > end)
	{
		std::swap (start, end);
		std::swap (pal1, pal2);
	}
	else if (start == end)
	{
		start = GPalette.Remap[start];
		pal1 = GPalette.Remap[pal1];
		Remap[start] = pal1;
		Palette[start] = GPalette.BaseColors[pal1];
		Palette[start].a = start == 0 ? 0 : 255;
		return true;
	}
	palcol = pal1;
	palstep = (pal2 - palcol) / (end - start);
	for (int i = start; i <= end; palcol += palstep, ++i)
	{
		int j = GPalette.Remap[i], k = GPalette.Remap[int(round(palcol))];
		Remap[j] = k;
		Palette[j] = GPalette.BaseColors[k];
		Palette[j].a = j == 0 ? 0 : 255;
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddColorRange(int start, int end, int _r1,int _g1, int _b1, int _r2, int _g2, int _b2)
{
	if (IndexOutOfRange(start, end))
	{
		return false;
	}

	double r1 = _r1;
	double g1 = _g1;
	double b1 = _b1;
	double r2 = _r2;
	double g2 = _g2;
	double b2 = _b2;
	double r, g, b;
	double rs, gs, bs;

	if (start > end)
	{
		std::swap (start, end);
		r = r2;
		g = g2;
		b = b2;
		rs = r1 - r2;
		gs = g1 - g2;
		bs = b1 - b2;
	}
	else
	{
		r = r1;
		g = g1;
		b = b1;
		rs = r2 - r1;
		gs = g2 - g1;
		bs = b2 - b1;
	}
	if (start == end)
	{
		start = GPalette.Remap[start];
		Palette[start] = PalEntry(start == 0 ? 0 : 255, int(r), int(g), int(b));
		Remap[start] = ColorMatcher.Pick(Palette[start]);
	}
	else
	{
		rs /= (end - start);
		gs /= (end - start);
		bs /= (end - start);
		for (int i = start; i <= end; ++i)
		{
			int j = GPalette.Remap[i];
			Palette[j] = PalEntry(j == 0 ? 0 : 255, int(r), int(g), int(b));
			Remap[j] = ColorMatcher.Pick(Palette[j]);
			r += rs;
			g += gs;
			b += bs;
		}
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddDesaturation(int start, int end, double r1, double g1, double b1, double r2, double g2, double b2)
{
	if (IndexOutOfRange(start, end))
	{
		return false;
	}

	r1 = clamp(r1, 0.0, 2.0);
	g1 = clamp(g1, 0.0, 2.0);
	b1 = clamp(b1, 0.0, 2.0);
	r2 = clamp(r2, 0.0, 2.0);
	g2 = clamp(g2, 0.0, 2.0);
	b2 = clamp(b2, 0.0, 2.0);

	if (start > end)
	{
		std::swap(start, end);
		std::swap(r1, r2);
		std::swap(g1, g2);
		std::swap(b1, b2);
	}

	r2 -= r1;
	g2 -= g1;
	b2 -= b1;
	r1 *= 255;
	g1 *= 255;
	b1 *= 255;

	for(int c = start; c <= end; c++)
	{
		double intensity = (GPalette.BaseColors[c].r * 77 +
							GPalette.BaseColors[c].g * 143 +
							GPalette.BaseColors[c].b * 37) / 256.0;

		PalEntry pe = PalEntry(	MIN(255, int(r1 + intensity*r2)), 
								MIN(255, int(g1 + intensity*g2)), 
								MIN(255, int(b1 + intensity*b2)));

		int cc = GPalette.Remap[c];

		Remap[cc] = ColorMatcher.Pick(pe);
		Palette[cc] = pe;
		Palette[cc].a = cc == 0 ? 0:255;
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddColourisation(int start, int end, int r, int g, int b)
{
	if (IndexOutOfRange(start, end))
	{
		return false;
	}

	for (int i = start; i < end; ++i)
	{
		double br = GPalette.BaseColors[i].r;
		double bg = GPalette.BaseColors[i].g;
		double bb = GPalette.BaseColors[i].b;
		double grey = (br * 0.299 + bg * 0.587 + bb * 0.114) / 255.0f;
		if (grey > 1.0) grey = 1.0;
		br = r * grey;
		bg = g * grey;
		bb = b * grey;

		int j = GPalette.Remap[i];
		Palette[j] = PalEntry(j == 0 ? 0 : 255, int(br), int(bg), int(bb));
		Remap[j] = ColorMatcher.Pick(Palette[j]);
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddTint(int start, int end, int r, int g, int b, int amount)
{
	if (IndexOutOfRange(start, end))
	{
		return false;
	}

	for (int i = start; i < end; ++i)
	{
		float br = GPalette.BaseColors[i].r;
		float bg = GPalette.BaseColors[i].g;
		float bb = GPalette.BaseColors[i].b;
		float a = amount * 0.01f;
		float ia = 1.0f - a;
		br = br * ia + r * a;
		bg = bg * ia + g * a;
		bb = bb * ia + b * a;

		int j = GPalette.Remap[i];
		Palette[j] = PalEntry(j == 0 ? 0 : 255, int(br), int(bg), int(bb));
		Remap[j] = ColorMatcher.Pick(Palette[j]);
	}
	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool FRemapTable::AddToTranslation(const char *range)
{
	int start,end;
	bool desaturated = false;
	FScanner sc;

	sc.OpenMem("translation", range, int(strlen(range)));
	sc.SetCMode(true);

	sc.MustGetToken(TK_IntConst);
	start = sc.Number;
	sc.MustGetToken(':');
	sc.MustGetToken(TK_IntConst);
	end = sc.Number;
	sc.MustGetToken('=');
	if (start < 0 || start > 255 || end < 0 || end > 255)
	{
		sc.ScriptError("Palette index out of range");
		return false;
	}

	sc.MustGetAnyToken();

	if (sc.TokenType == '[')
	{ 
		// translation using RGB values
		int r1,g1,b1,r2,g2,b2;

		sc.MustGetToken(TK_IntConst);
		r1 = sc.Number;
		sc.MustGetToken(',');

		sc.MustGetToken(TK_IntConst);
		g1 = sc.Number;
		sc.MustGetToken(',');

		sc.MustGetToken(TK_IntConst);
		b1 = sc.Number;
		sc.MustGetToken(']');
		sc.MustGetToken(':');
		sc.MustGetToken('[');

		sc.MustGetToken(TK_IntConst);
		r2 = sc.Number;
		sc.MustGetToken(',');

		sc.MustGetToken(TK_IntConst);
		g2 = sc.Number;
		sc.MustGetToken(',');

		sc.MustGetToken(TK_IntConst);
		b2 = sc.Number;
		sc.MustGetToken(']');

		return AddColorRange(start, end, r1, g1, b1, r2, g2, b2);
	}
	else if (sc.TokenType == '%')
	{
		// translation using RGB values
		double r1,g1,b1,r2,g2,b2;

		sc.MustGetToken('[');
		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		r1 = sc.Float;
		sc.MustGetToken(',');

		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		g1 = sc.Float;
		sc.MustGetToken(',');

		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		b1 = sc.Float;
		sc.MustGetToken(']');
		sc.MustGetToken(':');
		sc.MustGetToken('[');

		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		r2 = sc.Float;
		sc.MustGetToken(',');

		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		g2 = sc.Float;
		sc.MustGetToken(',');

		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst) sc.TokenMustBe(TK_FloatConst);
		b2 = sc.Float;
		sc.MustGetToken(']');

		return AddDesaturation(start, end, r1, g1, b1, r2, g2, b2);
	}
	else if (sc.TokenType == '#')
	{
		// Colourise translation
		int r, g, b;
		sc.MustGetToken('[');
		sc.MustGetToken(TK_IntConst);
		r = sc.Number;
		sc.MustGetToken(',');
		sc.MustGetToken(TK_IntConst);
		g = sc.Number;
		sc.MustGetToken(',');
		sc.MustGetToken(TK_IntConst);
		b = sc.Number;
		sc.MustGetToken(']');

		return AddColourisation(start, end, r, g, b);
	}
	else if (sc.TokenType == '@')
	{
		// Tint translation
		int a, r, g, b;

		sc.MustGetToken(TK_IntConst);
		a = sc.Number;
		sc.MustGetToken('[');
		sc.MustGetToken(TK_IntConst);
		r = sc.Number;
		sc.MustGetToken(',');
		sc.MustGetToken(TK_IntConst);
		g = sc.Number;
		sc.MustGetToken(',');
		sc.MustGetToken(TK_IntConst);
		b = sc.Number;
		sc.MustGetToken(']');

		return AddTint(start, end, r, g, b, a);
	}
	else
	{
		int pal1, pal2;

		sc.TokenMustBe(TK_IntConst);
		pal1 = sc.Number;
		sc.MustGetToken(':');
		sc.MustGetToken(TK_IntConst);
		pal2 = sc.Number;
		return AddIndexRange(start, end, pal1, pal2);
	}
}

//----------------------------------------------------------------------------
//
// Adds raw colors to a given translation
//
//----------------------------------------------------------------------------

bool FRemapTable::AddColors(int start, int count, const uint8_t*colors)
{
	int end = start + count;
	if (IndexOutOfRange(start, end))
	{
		return false;
	}

	for (int i = start; i < end; ++i)
	{
		auto br = colors[0];
		auto bg = colors[1];
		auto bb = colors[2];
		colors += 3;

		int j = GPalette.Remap[i];
		Palette[j] = PalEntry(j == 0 ? 0 : 255, br, bg, bb);
		Remap[j] = ColorMatcher.Pick(Palette[j]);
	}
	return true;

}

