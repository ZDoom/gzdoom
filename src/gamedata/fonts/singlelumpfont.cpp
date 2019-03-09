/*
** singlelumpfont.cpp
** Management for compiled font lumps
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2019 Christoph Oelckers
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

#include "doomerrors.h"
#include "textures.h"
#include "image.h"
#include "v_font.h"
#include "w_wad.h"
#include "utf8.h"
#include "textures/formats/fontchars.h"

#include "fontinternals.h"

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
		-- by the font but is used by imagetool when converting the font
		-- back to an image. Color 0 is the transparent color and is also
		-- used only for converting the font back to an image. The other
		-- entries are all presorted in increasing order of brightness.

	ubyte CharacterData[...];
		-- RLE character data, in order
*/

class FSingleLumpFont : public FFont
{
public:
	FSingleLumpFont (const char *fontname, int lump);

protected:
	void CheckFON1Chars (double *luminosity);
	void BuildTranslations2 ();
	void FixupPalette (uint8_t *identity, double *luminosity, const uint8_t *palette,
		bool rescale, PalEntry *out_palette);
	void LoadTranslations ();
	void LoadFON1 (int lump, const uint8_t *data);
	void LoadFON2 (int lump, const uint8_t *data);
	void LoadBMF (int lump, const uint8_t *data);
	void CreateFontFromPic (FTextureID picnum);

	static int BMFCompare(const void *a, const void *b);

	enum
	{
		FONT1,
		FONT2,
		BMFFONT
	} FontType;
	uint8_t PaletteData[768];
	bool RescalePalette;
};


//==========================================================================
//
// FSingleLumpFont :: FSingleLumpFont
//
// Loads a FON1 or FON2 font resource.
//
//==========================================================================

FSingleLumpFont::FSingleLumpFont (const char *name, int lump) : FFont(lump)
{
	assert(lump >= 0);

	FontName = name;

	FMemLump data1 = Wads.ReadLump (lump);
	const uint8_t *data = (const uint8_t *)data1.GetMem();

	if (data[0] == 0xE1 && data[1] == 0xE6 && data[2] == 0xD5 && data[3] == 0x1A)
	{
		LoadBMF(lump, data);
		Type = BMF;
	}
	else if (data[0] != 'F' || data[1] != 'O' || data[2] != 'N' ||
		(data[3] != '1' && data[3] != '2'))
	{
		I_Error ("%s is not a recognizable font", name);
	}
	else
	{
		switch (data[3])
		{
		case '1':
			LoadFON1 (lump, data);
			Type = Fon1;
			break;

		case '2':
			LoadFON2 (lump, data);
			Type = Fon2;
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

void FSingleLumpFont::CreateFontFromPic (FTextureID picnum)
{
	FTexture *pic = TexMan.GetTexture(picnum);

	FontHeight = pic->GetDisplayHeight ();
	SpaceWidth = pic->GetDisplayWidth ();
	GlobalKerning = 0;

	FirstChar = LastChar = 'A';
	Chars.Resize(1);
	Chars[0].TranslatedPic = pic;

	// Only one color range. Don't bother with the others.
	ActiveColors = 0;
}

//==========================================================================
//
// FSingleLumpFont :: LoadTranslations
//
//==========================================================================

void FSingleLumpFont::LoadTranslations()
{
	double luminosity[256];
	uint8_t identity[256];
	PalEntry local_palette[256];
	bool useidentity = true;
	bool usepalette = false;
	const void* ranges;
	unsigned int count = LastChar - FirstChar + 1;

	switch(FontType)
	{
		case FONT1:
			useidentity = false;
			ranges = &TranslationParms[1][0];
			CheckFON1Chars (luminosity);
			break;

		case BMFFONT:
		case FONT2:
			usepalette = true;
			FixupPalette (identity, luminosity, PaletteData, RescalePalette, local_palette);

			ranges = &TranslationParms[0][0];
			break;

		default:
			// Should be unreachable.
			I_Error("Unknown font type in FSingleLumpFont::LoadTranslation.");
			return;
	}

	for(unsigned int i = 0;i < count;++i)
	{
		if(Chars[i].TranslatedPic)
			static_cast<FFontChar2*>(Chars[i].TranslatedPic->GetImage())->SetSourceRemap(PatchRemap);
	}

	BuildTranslations (luminosity, useidentity ? identity : nullptr, ranges, ActiveColors, usepalette ? local_palette : nullptr);
}

//==========================================================================
//
// FSingleLumpFont :: LoadFON1
//
// FON1 is used for the console font.
//
//==========================================================================

void FSingleLumpFont::LoadFON1 (int lump, const uint8_t *data)
{
	int w, h;

	// The default console font is for Windows-1252 and fills the 0x80-0x9f range with valid glyphs.
	// Since now all internal text is processed as Unicode, these have to be remapped to their proper places.
	// The highest valid character in this range is 0x2122, so we need 0x2123 entries in our character table.
	Chars.Resize(0x2123);

	w = data[4] + data[5]*256;
	h = data[6] + data[7]*256;

	FontType = FONT1;
	FontHeight = h;
	SpaceWidth = w;
	FirstChar = 0;
	LastChar = 255;	// This is to allow LoadTranslations to function. The way this is all set up really needs to be changed.
	GlobalKerning = 0;
	translateUntranslated = true;
	LoadTranslations();
	LastChar = 0x2122;

	// Move the Windows-1252 characters to their proper place.
	for (int i = 0x80; i < 0xa0; i++)
	{
		if (win1252map[i-0x80] != i && Chars[i].TranslatedPic != nullptr && Chars[win1252map[i - 0x80]].TranslatedPic == nullptr)
		{ 
			std::swap(Chars[i], Chars[win1252map[i - 0x80]]);
		}
	}
}

//==========================================================================
//
// FSingleLumpFont :: LoadFON2
//
// FON2 is used for everything but the console font. The console font should
// probably use FON2 as well, but oh well.
//
//==========================================================================

void FSingleLumpFont::LoadFON2 (int lump, const uint8_t *data)
{
	int count, i, totalwidth;
	uint16_t *widths;
	const uint8_t *palette;
	const uint8_t *data_p;

	FontType = FONT2;
	FontHeight = data[4] + data[5]*256;
	FirstChar = data[6];
	LastChar = data[7];
	ActiveColors = data[10]+1;
	RescalePalette = data[9] == 0;
	
	count = LastChar - FirstChar + 1;
	Chars.Resize(count);
	TArray<int> widths2(count, true);
	if (data[11] & 1)
	{ // Font specifies a kerning value.
		GlobalKerning = LittleShort(*(int16_t *)&data[12]);
		widths = (uint16_t *)(data + 14);
	}
	else
	{ // Font does not specify a kerning value.
		GlobalKerning = 0;
		widths = (uint16_t *)(data + 12);
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
		palette = (uint8_t *)&widths[1];
	}
	else
	{ // Font has varying character widths.
		for (i = 0; i < count; ++i)
		{
			widths2[i] = LittleShort(widths[i]);
			totalwidth += widths2[i];
		}
		palette = (uint8_t *)(widths + i);
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

	memcpy(PaletteData, palette, ActiveColors*3);

	data_p = palette + ActiveColors*3;

	for (i = 0; i < count; ++i)
	{
		int destSize = widths2[i] * FontHeight;
		Chars[i].XMove = widths2[i];
		if (destSize <= 0)
		{
			Chars[i].TranslatedPic = nullptr;
		}
		else
		{
			Chars[i].TranslatedPic = new FImageTexture(new FFontChar2 (lump, int(data_p - data), widths2[i], FontHeight));
			Chars[i].TranslatedPic->SetUseType(ETextureType::FontChar);
			TexMan.AddTexture(Chars[i].TranslatedPic);
			do
			{
				int8_t code = *data_p++;
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
			I_FatalError ("Overflow decompressing char %d (%c) of %s", i, i, FontName.GetChars());
		}
	}

	LoadTranslations();
}

//==========================================================================
//
// FSingleLumpFont :: LoadBMF
//
// Loads a BMF font. The file format is described at
// <http://bmf.wz.cz/bmf-format.htm>
//
//==========================================================================

void FSingleLumpFont::LoadBMF(int lump, const uint8_t *data)
{
	const uint8_t *chardata;
	int numchars, count, totalwidth, nwidth;
	int infolen;
	int i, chari;
	uint8_t raw_palette[256*3];
	PalEntry sort_palette[256];

	FontType = BMFFONT;
	FontHeight = data[5];
	GlobalKerning = (int8_t)data[8];
	ActiveColors = data[16];
	SpaceWidth = -1;
	nwidth = -1;
	RescalePalette = true;

	infolen = data[17 + ActiveColors*3];
	chardata = data + 18 + ActiveColors*3 + infolen;
	numchars = chardata[0] + 256*chardata[1];
	chardata += 2;

	// Scan for lowest and highest characters defined and total font width.
	FirstChar = 256;
	LastChar = 0;
	totalwidth = 0;
	for (i = chari = 0; i < numchars; ++i, chari += 6 + chardata[chari+1] * chardata[chari+2])
	{
		if ((chardata[chari+1] == 0 || chardata[chari+2] == 0) && chardata[chari+5] == 0)
		{ // Don't count empty characters.
			continue;
		}
		if (chardata[chari] < FirstChar)
		{
			FirstChar = chardata[chari];
		}
		if (chardata[chari] > LastChar)
		{
			LastChar = chardata[chari];
		}
		totalwidth += chardata[chari+1];
	}
	if (LastChar < FirstChar)
	{
		I_FatalError("BMF font defines no characters");
	}
	count = LastChar - FirstChar + 1;
	Chars.Resize(count);
	// BMF palettes are only six bits per component. Fix that.
	for (i = 0; i < ActiveColors*3; ++i)
	{
		raw_palette[i+3] = (data[17 + i] << 2) | (data[17 + i] >> 4);
	}
	ActiveColors++;

	// Sort the palette by increasing brightness
	for (i = 0; i < ActiveColors; ++i)
	{
		PalEntry *pal = &sort_palette[i];
		pal->a = i;		// Use alpha part to point back to original entry
		pal->r = raw_palette[i*3 + 0];
		pal->g = raw_palette[i*3 + 1];
		pal->b = raw_palette[i*3 + 2];
	}
	qsort(sort_palette + 1, ActiveColors - 1, sizeof(PalEntry), BMFCompare);

	// Create the PatchRemap table from the sorted "alpha" values.
	PatchRemap[0] = 0;
	for (i = 1; i < ActiveColors; ++i)
	{
		PatchRemap[sort_palette[i].a] = i;
	}

	memcpy(PaletteData, raw_palette, 768);

	// Now scan through the characters again, creating glyphs for each one.
	for (i = chari = 0; i < numchars; ++i, chari += 6 + chardata[chari+1] * chardata[chari+2])
	{
		assert(chardata[chari] - FirstChar >= 0);
		assert(chardata[chari] - FirstChar < count);
		if (chardata[chari] == ' ')
		{
			SpaceWidth = chardata[chari+5];
		}
		else if (chardata[chari] == 'N')
		{
			nwidth = chardata[chari+5];
		}
		Chars[chardata[chari] - FirstChar].XMove = chardata[chari+5];
		if (chardata[chari+1] == 0 || chardata[chari+2] == 0)
		{ // Empty character: skip it.
			continue;
		}
		auto tex = new FImageTexture(new FFontChar2(lump, int(chardata + chari + 6 - data),
			chardata[chari+1],	// width
			chardata[chari+2],	// height
			-(int8_t)chardata[chari+3],	// x offset
			-(int8_t)chardata[chari+4]	// y offset
		));
		tex->SetUseType(ETextureType::FontChar);
		Chars[chardata[chari] - FirstChar].TranslatedPic = tex;
		TexMan.AddTexture(tex);
	}

	// If the font did not define a space character, determine a suitable space width now.
	if (SpaceWidth < 0)
	{
		if (nwidth >= 0)
		{
			SpaceWidth = nwidth;
		}
		else
		{
			SpaceWidth = totalwidth * 2 / (3 * count);
		}
	}

	FixXMoves();
	LoadTranslations();
}

//==========================================================================
//
// FSingleLumpFont :: BMFCompare									STATIC
//
// Helper to sort BMF palettes.
//
//==========================================================================

int FSingleLumpFont::BMFCompare(const void *a, const void *b)
{
	const PalEntry *pa = (const PalEntry *)a;
	const PalEntry *pb = (const PalEntry *)b;

	return (pa->r * 299 + pa->g * 587 + pa->b * 114) -
		   (pb->r * 299 + pb->g * 587 + pb->b * 114);
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

void FSingleLumpFont::CheckFON1Chars (double *luminosity)
{
	FMemLump memLump = Wads.ReadLump(Lump);
	const uint8_t* data = (const uint8_t*) memLump.GetMem();

	uint8_t used[256], reverse[256];
	const uint8_t *data_p;
	int i, j;

	memset (used, 0, 256);
	data_p = data + 8;

	for (i = 0; i < 256; ++i)
	{
		int destSize = SpaceWidth * FontHeight;

		if(!Chars[i].TranslatedPic)
		{
			Chars[i].TranslatedPic = new FImageTexture(new FFontChar2 (Lump, int(data_p - data), SpaceWidth, FontHeight));
			Chars[i].TranslatedPic->SetUseType(ETextureType::FontChar);
			Chars[i].XMove = SpaceWidth;
			TexMan.AddTexture(Chars[i].TranslatedPic);
		}

		// Advance to next char's data and count the used colors.
		do
		{
			int8_t code = *data_p++;
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

void FSingleLumpFont::FixupPalette (uint8_t *identity, double *luminosity, const uint8_t *palette, bool rescale, PalEntry *out_palette)
{
	int i;
	double maxlum = 0.0;
	double minlum = 100000000.0;
	double diver;

	identity[0] = 0;
	palette += 3;	// Skip the transparent color

	for (i = 1; i < ActiveColors; ++i, palette += 3)
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
	for (i = 1; i < ActiveColors; ++i)
	{
		luminosity[i] = (luminosity[i] - minlum) * diver;
	}
}

FFont *CreateSingleLumpFont (const char *fontname, int lump)
{
	return new FSingleLumpFont(fontname, lump);
}
