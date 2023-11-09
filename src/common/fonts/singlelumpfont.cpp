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

#include "engineerrors.h"
#include "textures.h"
#include "image.h"
#include "v_font.h"
#include "filesystem.h"
#include "utf8.h"
#include "fontchars.h"
#include "texturemanager.h"
#include "m_swap.h"

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
	void RecordAllTextureColors(uint32_t* usedcolors) override;

protected:
	void CheckFON1Chars ();
	void FixupPalette (uint8_t *identity, const PalEntry *palette, int* minlum ,int* maxlum);
	void LoadTranslations ();
	void LoadFON1 (int lump, const uint8_t *data);
	void LoadFON2 (int lump, const uint8_t *data);
	void LoadBMF (int lump, const uint8_t *data);

	enum
	{
		FONT1,
		FONT2,
		BMFFONT
	} FontType;
	PalEntry Palette[256];
	bool RescalePalette;
	int ActiveColors = -1;
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

	auto data1 = fileSystem.ReadFile (lump);
	auto data = data1.GetBytes();

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
}

//==========================================================================
//
// FSingleLumpFont :: LoadTranslations
//
//==========================================================================

void FSingleLumpFont::LoadTranslations()
{
	uint8_t identity[256];
	unsigned int count = LastChar - FirstChar + 1;
	int minlum, maxlum;

	switch(FontType)
	{
		case FONT1:
			CheckFON1Chars();
			minlum = 1;
			maxlum = 255;
			break;

		case BMFFONT:
		case FONT2:
			FixupPalette (identity, Palette, &minlum, &maxlum);
			break;

		default:
			// Should be unreachable.
			I_Error("Unknown font type in FSingleLumpFont::LoadTranslation.");
			return;
	}

	for(unsigned int i = 0;i < count;++i)
	{
		if(Chars[i].OriginalPic)
			static_cast<FFontChar2*>(Chars[i].OriginalPic->GetTexture()->GetImage())->SetSourceRemap(Palette);
	}

	Translations.Resize(NumTextColors);
	for (int i = 0; i < NumTextColors; i++)
	{
		if (i == CR_UNTRANSLATED) Translations[i] = NO_TRANSLATION;
		else Translations[i] = MakeLuminosityTranslation(i * 2 + (FontType == FONT1 ? 1 : 0), minlum, maxlum);
	}
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
	LastChar = 0x2122;

	// Move the Windows-1252 characters to their proper place.
	for (int i = 0x80; i < 0xa0; i++)
	{
		if (win1252map[i-0x80] != i && Chars[i].OriginalPic != nullptr && Chars[win1252map[i - 0x80]].OriginalPic == nullptr)
		{ 
			std::swap(Chars[i], Chars[win1252map[i - 0x80]]);
		}
	}
	Palette[0] = 0;
	for (int i = 1; i < 256; i++) Palette[i] = PalEntry(255, i, i, i);
}

//==========================================================================
//
// FSingleLumpFont :: LoadFON2
//
// FON2 is used for everything but the console font.
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

	Palette[0] = 0;
	for (int pp = 1; pp < ActiveColors; pp++)
	{
		Palette[pp] = PalEntry(255, palette[pp * 3], palette[pp * 3 + 1], palette[pp * 3 + 2]);
	}

	data_p = palette + ActiveColors*3;

	for (i = 0; i < count; ++i)
	{
		int destSize = widths2[i] * FontHeight;
		Chars[i].XMove = widths2[i];
		if (destSize <= 0)
		{
			Chars[i].OriginalPic = nullptr;
		}
		else
		{
			Chars[i].OriginalPic = MakeGameTexture(new FImageTexture(new FFontChar2 (lump, int(data_p - data), widths2[i], FontHeight)), nullptr, ETextureType::FontChar);
			TexMan.AddGameTexture(Chars[i].OriginalPic);
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
			I_Error ("Overflow decompressing char %d (%c) of %s", i, i, FontName.GetChars());
		}
	}
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
		I_Error("BMF font defines no characters");
	}
	count = LastChar - FirstChar + 1;
	Chars.Resize(count);
	// BMF palettes are only six bits per component. Fix that.
	Palette[0] = 0;
	for (i = 0; i < ActiveColors; ++i)
	{
		int r = (data[17 + i * 3] << 2) | (data[17 + i * 3] >> 4);
		int g = (data[18 + i * 3] << 2) | (data[18 + i * 3] >> 4);
		int b = (data[19 + i * 3] << 2) | (data[19 + i * 3] >> 4);
		Palette[i + 1] = PalEntry(255, r, g, b); // entry 0 (transparent) is not stored in the font file.
	}
	ActiveColors++;

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
		auto tex = MakeGameTexture(new FImageTexture(new FFontChar2(lump, int(chardata + chari + 6 - data),
			chardata[chari+1],	// width
			chardata[chari+2],	// height
			-(int8_t)chardata[chari+3],	// x offset
			-(int8_t)chardata[chari+4]	// y offset
		)), nullptr, ETextureType::FontChar);
		Chars[chardata[chari] - FirstChar].OriginalPic = tex;
		TexMan.AddGameTexture(tex);
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
}

//==========================================================================
//
// FSingleLumpFont :: CheckFON1Chars
//
// Scans a FON1 resource for all the color values it uses and sets up
// some tables. Data points to the RLE data for
// the characters. Also sets up the character textures.
//
//==========================================================================

void FSingleLumpFont::CheckFON1Chars()
{
	auto memLump = fileSystem.ReadFile(Lump);
	auto data = memLump.GetBytes();
	const uint8_t* data_p;

	data_p = data + 8;

	for (int i = 0; i < 256; ++i)
	{
		int destSize = SpaceWidth * FontHeight;

		if (!Chars[i].OriginalPic)
		{
			Chars[i].OriginalPic = MakeGameTexture(new FImageTexture(new FFontChar2(Lump, int(data_p - data), SpaceWidth, FontHeight)), nullptr, ETextureType::FontChar);
			Chars[i].XMove = SpaceWidth;
			TexMan.AddGameTexture(Chars[i].OriginalPic);
		}

		// Advance to next char's data and count the used colors.
		do
		{
			int8_t code = *data_p++;
			if (code >= 0)
			{
				destSize -= code + 1;
				while (code-- >= 0)
				{
					data_p++;
				}
			}
			else if (code != -128)
			{
				data_p++;
				destSize -= 1 - code;
			}
		} while (destSize > 0);
	}
	ActiveColors = 256;
}

//==========================================================================
//
// FSingleLumpFont :: FixupPalette
//
// Finds the best matches for the colors used by a FON2 font and finds thr
// used luminosity range
//
//==========================================================================

void FSingleLumpFont::FixupPalette (uint8_t *identity, const PalEntry *palette, int *pminlum, int *pmaxlum)
{
	double maxlum = 0.0;
	double minlum = 100000000.0;

	identity[0] = 0;
	palette++;	// Skip the transparent color

	for (int i = 1; i < ActiveColors; ++i, palette ++)
	{
		int r = palette->r;
		int g = palette->g;
		int b = palette->b;
		double lum = r*0.299 + g*0.587 + b*0.114;
		identity[i] = ColorMatcher.Pick(r, g, b);
		if (lum > maxlum) maxlum = lum;
		if (lum < minlum) minlum = lum;
	}

	if (pminlum) *pminlum = int(minlum);
	if (pmaxlum) *pmaxlum = int(maxlum);
}

//==========================================================================
//
// RecordAllTextureColors
//
// Given a 256 entry buffer, sets every entry that corresponds to a color
// used by the font.
//
//==========================================================================

void FSingleLumpFont::RecordAllTextureColors(uint32_t* usedcolors)
{
	uint8_t identity[256];

	if (FontType == BMFFONT || FontType == FONT2)
	{
		FixupPalette(identity, Palette, nullptr, nullptr);
		for (int i = 0; i < 256; i++)
		{
			if (identity[i] != 0) usedcolors[identity[i]]++;
		}
	}
}


FFont *CreateSingleLumpFont (const char *fontname, int lump)
{
	return new FSingleLumpFont(fontname, lump);
}
