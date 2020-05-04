/*
** bdffont.cpp
** Management for the VGA consolefont
**
**---------------------------------------------------------------------------
** Copyright 2019 Christoph Oelckers
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
#include "sc_man.h"
#include "texturemanager.h"

#include "fontinternals.h"


struct HexDataSource
{
	int FirstChar = INT_MAX, LastChar = INT_MIN;
	TArray<uint8_t> glyphdata;
	unsigned glyphmap[65536] = {};

	//==========================================================================
	//
	// parse a HEX font
	//
	//==========================================================================

	void ParseDefinition(int lumpnum)
	{
		FScanner sc;

		sc.OpenLumpNum(lumpnum);
		sc.SetCMode(true);
		glyphdata.Push(0);	// ensure that index 0 can be used as 'not present'.
		while (sc.GetString())
		{
			int codepoint = (int)strtoull(sc.String, nullptr, 16);
			sc.MustGetStringName(":");
			sc.MustGetString();
			if (codepoint >= 0 && codepoint < 65536 && !sc.Compare("00000000000000000000000000000000"))	// don't set up empty glyphs.
			{
				unsigned size = (unsigned)strlen(sc.String);
				unsigned offset = glyphdata.Reserve(size / 2 + 1);
				glyphmap[codepoint] = offset;
				glyphdata[offset++] = size / 2;
				for (unsigned i = 0; i < size; i += 2)
				{
					char hex[] = { sc.String[i], sc.String[i + 1], 0 };
					glyphdata[offset++] = (uint8_t)strtoull(hex, nullptr, 16);
				}
				if (codepoint < FirstChar) FirstChar = codepoint;
				if (codepoint > LastChar) LastChar = codepoint;
			}
		}
	}
};

static HexDataSource hexdata;

// This is a font character that reads RLE compressed data.
class FHexFontChar : public FImageSource
{
public:
	FHexFontChar(uint8_t *sourcedata, int swidth, int width, int height);

	TArray<uint8_t> CreatePalettedPixels(int conversion) override;

protected:
	int SourceWidth;
	const uint8_t *SourceData;
};


//==========================================================================
//
// FHexFontChar :: FHexFontChar
//
// Used by HEX fonts.
//
//==========================================================================

FHexFontChar::FHexFontChar (uint8_t *sourcedata, int swidth, int width, int height)
: SourceData (sourcedata)
{
	SourceWidth = swidth;
	Width = width;
	Height = height;
	LeftOffset = 0;
	TopOffset = 0;
}

//==========================================================================
//
// FHexFontChar :: Get8BitPixels
//
// The render style has no relevance here.
//
//==========================================================================

TArray<uint8_t> FHexFontChar::CreatePalettedPixels(int)
{
	int destSize = Width * Height;
	TArray<uint8_t> Pixels(destSize, true);
	uint8_t *dest_p = Pixels.Data();
	const uint8_t *src_p = SourceData;

	memset(dest_p, 0, destSize);
	for (int y = 0; y < Height; y++)
	{
		for (int x = 0; x < SourceWidth; x++)
		{
			int byte = *src_p++;
			uint8_t *pixelstart = dest_p + 8 * x * Height + y;
			for (int bit = 0; bit < 8; bit++)
			{
				if (byte & (128 >> bit))
				{
					pixelstart[bit*Height] = y+2;
					// Add a shadow at the bottom right, similar to the old console font.
					if (y != Height - 1)
					{
						pixelstart[bit*Height + Height + 1] = 1;
					}
				}
			}
		}
	}
	return Pixels;
}

class FHexFontChar2 : public FHexFontChar
{
public:
	FHexFontChar2(uint8_t *sourcedata, int swidth, int width, int height);

	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
};


//==========================================================================
//
// FHexFontChar :: FHexFontChar
//
// Used by HEX fonts.
//
//==========================================================================

FHexFontChar2::FHexFontChar2(uint8_t *sourcedata, int swidth, int width, int height)
	: FHexFontChar(sourcedata, swidth, width, height)
{
}

//==========================================================================
//
// FHexFontChar :: Get8BitPixels
//
// The render style has no relevance here.
//
//==========================================================================

TArray<uint8_t> FHexFontChar2::CreatePalettedPixels(int)
{
	int destSize = Width * Height;
	TArray<uint8_t> Pixels(destSize, true);
	uint8_t *dest_p = Pixels.Data();

	assert(SourceData);
	if (SourceData)
	{
		auto drawLayer = [&](int ix, int iy, int color)
		{
			const uint8_t *src_p = SourceData;
			for (int y = 0; y < Height - 2; y++)
			{
				for (int x = 0; x < SourceWidth; x++)
				{
					int byte = *src_p++;
					uint8_t *pixelstart = dest_p + (ix + 8 * x) * Height + (iy + y);
					for (int bit = 0; bit < 8; bit++)
					{
						if (byte & (128 >> bit))
						{
							pixelstart[bit*Height] = color;
						}
					}
				}
			}
		};
		memset(dest_p, 0, destSize);

		const int darkcolor = 1;
		const int brightcolor = 14;
		for (int xx = 0; xx < 3; xx++) for (int yy = 0; yy < 3; yy++) if (xx != 1 || yy != 1)
			drawLayer(xx, yy, darkcolor);
		drawLayer(1, 1, brightcolor);
	}
	return Pixels;
}



class FHexFont : public FFont
{
	
public:
	//==========================================================================
	//
	// FHexFont :: FHexFont
	//
	// Loads a HEX font
	//
	//==========================================================================

	FHexFont (const char *fontname, int lump)
		: FFont(lump)
	{
		const int spacing = 9;
		assert(lump >= 0);

		FontName = fontname;
		
		FirstChar = hexdata.FirstChar;
		LastChar = hexdata.LastChar;

		Next = FirstFont;
		FirstFont = this;
		FontHeight = 16;
		SpaceWidth = 9;
		GlobalKerning = 0;
		translateUntranslated = true;

		Chars.Resize(LastChar - FirstChar + 1);
		for (int i = FirstChar; i <= LastChar; i++)
		{
			if (hexdata.glyphmap[i] > 0)
			{
				auto offset = hexdata.glyphmap[i];
				int size = hexdata.glyphdata[offset] / 16;
				Chars[i - FirstChar].TranslatedPic = MakeGameTexture(new FImageTexture(new FHexFontChar(&hexdata.glyphdata[offset + 1], size, size * 9, 16)), nullptr, ETextureType::FontChar);
				Chars[i - FirstChar].OriginalPic = Chars[i - FirstChar].TranslatedPic;
				Chars[i - FirstChar].XMove = size * spacing;
				TexMan.AddGameTexture(Chars[i - FirstChar].TranslatedPic);
			}
			else Chars[i - FirstChar].XMove = spacing;

		}
	}

	//==========================================================================
	//
	// FHexFont :: LoadTranslations
	//
	//==========================================================================

	void LoadTranslations()
	{
		double luminosity[256];

		memset (PatchRemap, 0, 256);
		for (int i = 0; i < 18; i++) 
		{	
			// Create a gradient similar to the old console font.
			PatchRemap[i] = i;
			luminosity[i] = i == 1? 0.01 : 0.5 + (i-2) * (0.5 / 17.);
		}
		ActiveColors = 18;
		
		BuildTranslations (luminosity, nullptr, &TranslationParms[1][0], ActiveColors, nullptr);
	}
	
};


class FHexFont2 : public FFont
{

public:
	//==========================================================================
	//
	// FHexFont :: FHexFont
	//
	// Loads a HEX font
	//
	//==========================================================================

	FHexFont2(const char *fontname, int lump)
		: FFont(lump)
	{
		const int spacing = 9;
		assert(lump >= 0);

		FontName = fontname;

		FirstChar = hexdata.FirstChar;
		LastChar = hexdata.LastChar;

		Next = FirstFont;
		FirstFont = this;
		FontHeight = 18;
		SpaceWidth = 9;
		GlobalKerning = -1;
		translateUntranslated = true;
		Chars.Resize(LastChar - FirstChar + 1);
		for (int i = FirstChar; i <= LastChar; i++)
		{
			if (hexdata.glyphmap[i] > 0)
			{
				auto offset = hexdata.glyphmap[i];
				int size = hexdata.glyphdata[offset] / 16;
				Chars[i - FirstChar].TranslatedPic = MakeGameTexture(new FImageTexture(new FHexFontChar2(&hexdata.glyphdata[offset + 1], size, 2 + size * 8, 18)), nullptr, ETextureType::FontChar);
				Chars[i - FirstChar].OriginalPic = Chars[i - FirstChar].TranslatedPic;
				Chars[i - FirstChar].XMove = size * spacing;
				TexMan.AddGameTexture(Chars[i - FirstChar].TranslatedPic);
			}
			else Chars[i - FirstChar].XMove = spacing;

		}
	}

	//==========================================================================
	//
	// FHexFont :: LoadTranslations
	//
	//==========================================================================

	void LoadTranslations() override
	{
		double luminosity[256];

		memset(PatchRemap, 0, 256);
		for (int i = 0; i < 18; i++)
		{
			// Create a gradient similar to the old console font.
			PatchRemap[i] = i;
			luminosity[i] = i / 17.;
		}
		ActiveColors = 18;

		BuildTranslations(luminosity, nullptr, &TranslationParms[0][0], ActiveColors, nullptr);
	}

	void SetDefaultTranslation(uint32_t *colors) override
	{
		double myluminosity[18];

		myluminosity[0] = 0;
		for (int i = 1; i < 18; i++)
		{
			myluminosity[i] = (i - 1) / 16.;
		}

		uint8_t othertranslation[256], otherreverse[256];
		TArray<double> otherluminosity;

		SimpleTranslation(colors, othertranslation, otherreverse, otherluminosity);

		FRemapTable remap(ActiveColors);
		remap.Remap[0] = 0;
		remap.Palette[0] = 0;
		remap.ForFont = true;

		for (unsigned l = 1; l < 18; l++)
		{
			for (unsigned o = 1; o < otherluminosity.Size() - 1; o++)	// luminosity[0] is for the transparent color
			{
				if (myluminosity[l] >= otherluminosity[o] && myluminosity[l] <= otherluminosity[o + 1])
				{
					PalEntry color1 = GPalette.BaseColors[otherreverse[o]];
					PalEntry color2 = GPalette.BaseColors[otherreverse[o + 1]];
					double weight = 0;
					if (otherluminosity[o] != otherluminosity[o + 1])
					{
						weight = (myluminosity[l] - otherluminosity[o]) / (otherluminosity[o + 1] - otherluminosity[o]);
					}
					int r = int(color1.r + weight * (color2.r - color1.r));
					int g = int(color1.g + weight * (color2.g - color1.g));
					int b = int(color1.b + weight * (color2.b - color1.b));

					r = clamp(r, 0, 255);
					g = clamp(g, 0, 255);
					b = clamp(b, 0, 255);
					remap.Remap[l] = ColorMatcher.Pick(r, g, b);
					remap.Palette[l] = PalEntry(255, r, g, b);
					break;
				}
			}
		}
		Translations[CR_UNTRANSLATED] = GPalette.StoreTranslation(TRANSLATION_Internal, &remap);
		forceremap = true;

	}

};


//==========================================================================
//
//
//
//==========================================================================

FFont *CreateHexLumpFont (const char *fontname, int lump)
{
	if (hexdata.FirstChar == INT_MAX) hexdata.ParseDefinition(lump);
	return new FHexFont(fontname, lump);
}

//==========================================================================
//
//
//
//==========================================================================

FFont *CreateHexLumpFont2(const char *fontname, int lump)
{
	if (hexdata.FirstChar == INT_MAX) hexdata.ParseDefinition(lump);
	return new FHexFont2(fontname, lump);
}
