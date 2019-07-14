/*
** v_font.cpp
** Font management
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

#include "v_font.h"
#include "textures.h"
#include "image.h"
#include "textures/formats/fontchars.h"

#include "fontinternals.h"

// Essentially a normal multilump font but with an explicit list of character patches
class FSpecialFont : public FFont
{
public:
	FSpecialFont (const char *name, int first, int count, FTexture **lumplist, const bool *notranslate, int lump, bool donttranslate);

	void LoadTranslations();

protected:
	bool notranslate[256];
};


//==========================================================================
//
// FSpecialFont :: FSpecialFont
//
//==========================================================================

FSpecialFont::FSpecialFont (const char *name, int first, int count, FTexture **lumplist, const bool *notranslate, int lump, bool donttranslate) 
	: FFont(lump)
{
	int i;
	TArray<FTexture *> charlumps(count, true);
	int maxyoffs;
	FTexture *pic;

	memcpy(this->notranslate, notranslate, 256*sizeof(bool));

	noTranslate = donttranslate;
	FontName = name;
	Chars.Resize(count);
	FirstChar = first;
	LastChar = first + count - 1;
	FontHeight = 0;
	GlobalKerning = false;
	Next = FirstFont;
	FirstFont = this;

	maxyoffs = 0;

	for (i = 0; i < count; i++)
	{
		pic = charlumps[i] = lumplist[i];
		if (pic != nullptr)
		{
			int height = pic->GetDisplayHeight();
			int yoffs = pic->GetDisplayTopOffset();

			if (yoffs > maxyoffs)
			{
				maxyoffs = yoffs;
			}
			height += abs (yoffs);
			if (height > FontHeight)
			{
				FontHeight = height;
			}
		}

		if (charlumps[i] != nullptr)
		{
			charlumps[i]->SetUseType(ETextureType::FontChar);

			Chars[i].OriginalPic = charlumps[i];
			if (!noTranslate)
			{
				Chars[i].TranslatedPic = new FImageTexture(new FFontChar1 (charlumps[i]->GetImage()), "");
				Chars[i].TranslatedPic->CopySize(charlumps[i]);
				Chars[i].TranslatedPic->SetUseType(ETextureType::FontChar);
				TexMan.AddTexture(Chars[i].TranslatedPic);
			}
			else Chars[i].TranslatedPic = charlumps[i];
			Chars[i].XMove = Chars[i].TranslatedPic->GetDisplayWidth();
		}
		else
		{
			Chars[i].TranslatedPic = nullptr;
			Chars[i].XMove = INT_MIN;
		}
	}

	// Special fonts normally don't have all characters so be careful here!
	if ('N'-first >= 0 && 'N'-first < count && Chars['N' - first].TranslatedPic != nullptr)
	{
		SpaceWidth = (Chars['N' - first].XMove + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}

	FixXMoves();

	if (noTranslate)
	{
		ActiveColors = 0;
	}
	else
	{
		LoadTranslations();
	}
}

//==========================================================================
//
// FSpecialFont :: LoadTranslations
//
//==========================================================================

void FSpecialFont::LoadTranslations()
{
	int count = LastChar - FirstChar + 1;
	uint32_t usedcolors[256] = {};
	uint8_t identity[256];
	TArray<double> Luminosity;
	int TotalColors;
	int i, j;

	for (i = 0; i < count; i++)
	{
		if (Chars[i].TranslatedPic)
		{
			FFontChar1 *pic = static_cast<FFontChar1 *>(Chars[i].TranslatedPic->GetImage());
			if (pic)
			{
				pic->SetSourceRemap(nullptr); // Force the FFontChar1 to return the same pixels as the base texture
				RecordTextureColors(pic, usedcolors);
			}
		}
	}

	// exclude the non-translated colors from the translation calculation
	for (i = 0; i < 256; i++)
		if (notranslate[i])
			usedcolors[i] = false;

	TotalColors = ActiveColors = SimpleTranslation (usedcolors, PatchRemap, identity, Luminosity);

	// Map all untranslated colors into the table of used colors
	for (i = 0; i < 256; i++) 
	{
		if (notranslate[i])
		{
			PatchRemap[i] = TotalColors;
			identity[TotalColors] = i;
			TotalColors++;
		}
	}

	for (i = 0; i < count; i++)
	{
		if(Chars[i].TranslatedPic)
			static_cast<FFontChar1 *>(Chars[i].TranslatedPic->GetImage())->SetSourceRemap(PatchRemap);
	}

	BuildTranslations (Luminosity.Data(), identity, &TranslationParms[0][0], TotalColors, nullptr);

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
				remap->Palette[j].a = 0xff;
			}
		}
	}
	ActiveColors = TotalColors;
}

FFont *CreateSpecialFont (const char *name, int first, int count, FTexture **lumplist, const bool *notranslate, int lump, bool donttranslate) 
{
	return new FSpecialFont(name, first, count, lumplist, notranslate, lump, donttranslate);
}
