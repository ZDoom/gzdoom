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
#include "fontchars.h"
#include "texturemanager.h"
#include "i_interface.h"

#include "fontinternals.h"

// Essentially a normal multilump font but with an explicit list of character patches
class FSpecialFont : public FFont
{
public:
	FSpecialFont (const char *name, int first, int count, FGameTexture **lumplist, const bool *notranslate, int lump, bool donttranslate);

	void LoadTranslations();

protected:
	bool notranslate[256];
};


//==========================================================================
//
// FSpecialFont :: FSpecialFont
//
//==========================================================================

FSpecialFont::FSpecialFont (const char *name, int first, int count, FGameTexture **lumplist, const bool *notranslate, int lump, bool donttranslate) 
	: FFont(lump)
{
	int i;
	TArray<FGameTexture *> charlumps(count, true);
	int maxyoffs;
	FGameTexture *pic;

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
			int height = (int)pic->GetDisplayHeight();
			int yoffs = (int)pic->GetDisplayTopOffset();

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
			auto pic = charlumps[i];
			Chars[i].OriginalPic = MakeGameTexture(pic->GetTexture(), nullptr, ETextureType::FontChar);
			Chars[i].OriginalPic->CopySize(pic, true);
			TexMan.AddGameTexture(Chars[i].OriginalPic);
			Chars[i].XMove = (int)Chars[i].OriginalPic->GetDisplayWidth();
			if (sysCallbacks.FontCharCreated) sysCallbacks.FontCharCreated(pic, Chars[i].OriginalPic, Chars[i].OriginalPic);
		}
		else
		{
			Chars[i].OriginalPic = nullptr;
			Chars[i].XMove = INT_MIN;
		}
	}

	// Special fonts normally don't have all characters so be careful here!
	if ('N'-first >= 0 && 'N'-first < count && Chars['N' - first].OriginalPic != nullptr)
	{
		SpaceWidth = (Chars['N' - first].XMove + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}

	FixXMoves();
}

//==========================================================================
//
// FSpecialFont :: LoadTranslations
//
//==========================================================================

void FSpecialFont::LoadTranslations()
{
	FFont::LoadTranslations();

	bool empty = true;
	for (auto& c : Chars)
	{
		if (c.OriginalPic != nullptr)
		{
			empty = false;
			break;
		}
	}
	if (empty) return; // Font has no characters.

	bool needsnotrans = false;
	// exclude the non-translated colors from the translation calculation
	for (int i = 0; i < 256; i++)
		if (notranslate[i])
		{
			needsnotrans = true;
			break;
		}

	// If we have no non-translateable colors, we can use the base data as-is.
	if (!needsnotrans)
	{
		return;
	}

	// we only need to add special handling if there's colors that should not be translated.
	// Obviously 'notranslate' should only be used on data that uses the base palette, otherwise results are undefined!
	for (auto &trans : Translations)
	{
		if (!IsLuminosityTranslation(trans)) continue; // this should only happen for CR_UNTRANSLATED.

		FRemapTable remap(256);
		remap.ForFont = true;

		uint8_t workpal[1024];
		for (int i = 0; i < 256; i++)
		{
			workpal[i * 4 + 0] = GPalette.BaseColors[i].b;
			workpal[i * 4 + 1] = GPalette.BaseColors[i].g;
			workpal[i * 4 + 2] = GPalette.BaseColors[i].r;
			workpal[i * 4 + 3] = GPalette.BaseColors[i].a;
		}
		V_ApplyLuminosityTranslation(trans, workpal, 256);
		for (int i = 0; i < 256; i++)
		{
			if (!notranslate[i])
			{
				remap.Palette[i] = PalEntry(workpal[i * 4 + 3], workpal[i * 4 + 2], workpal[i * 4 + 1], workpal[i * 4 + 0]);
				remap.Remap[i] = ColorMatcher.Pick(remap.Palette[i]);
			}
			else
			{
				remap.Palette[i] = GPalette.BaseColors[i];
				remap.Remap[i] = i;
			}
		}
		trans = GPalette.StoreTranslation(TRANSLATION_Internal, &remap);
	}
}

FFont *CreateSpecialFont (const char *name, int first, int count, FGameTexture **lumplist, const bool *notranslate, int lump, bool donttranslate) 
{
	return new FSpecialFont(name, first, count, lumplist, notranslate, lump, donttranslate);
}
