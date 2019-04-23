/*
** v_font.h
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

#ifndef __V_FONT_H__
#define __V_FONT_H__

#include "doomtype.h"
#include "w_wad.h"
#include "vectors.h"

class DCanvas;
struct FRemapTable;
class FTexture;

enum EColorRange : int
{
	CR_UNDEFINED = -1,
	CR_BRICK,
	CR_TAN,
	CR_GRAY,
	CR_GREY = CR_GRAY,
	CR_GREEN,
	CR_BROWN,
	CR_GOLD,
	CR_RED,
	CR_BLUE,
	CR_ORANGE,
	CR_WHITE,
	CR_YELLOW,
	CR_UNTRANSLATED,
	CR_BLACK,
	CR_LIGHTBLUE,
	CR_CREAM,
	CR_OLIVE,
	CR_DARKGREEN,
	CR_DARKRED,
	CR_DARKBROWN,
	CR_PURPLE,
	CR_DARKGRAY,
	CR_CYAN,
	CR_ICE,
	CR_FIRE,
	CR_SAPPHIRE,
	CR_TEAL,
	NUM_TEXT_COLORS,
};

extern int NumTextColors;


class FFont
{
public:

	enum EFontType
	{
		Unknown,
		Folder,
		Multilump,
		Fon1,
		Fon2,
		BMF,
		Custom
	};

	FFont (const char *fontname, const char *nametemplate, const char *filetemplate, int first, int count, int base, int fdlump, int spacewidth=-1, bool notranslate = false, bool iwadonly = false);
	virtual ~FFont ();

	virtual FTexture *GetChar (int code, int translation, int *const width, bool *redirected = nullptr) const;
	virtual int GetCharWidth (int code) const;
	FRemapTable *GetColorTranslation (EColorRange range, PalEntry *color = nullptr) const;
	int GetLump() const { return Lump; }
	int GetSpaceWidth () const { return SpaceWidth; }
	int GetHeight () const { return FontHeight; }
	int GetDefaultKerning () const { return GlobalKerning; }
	virtual void LoadTranslations();
	FName GetName() const { return FontName; }

	static FFont *FindFont(FName fontname);

	// Return width of string in pixels (unscaled)
	int StringWidth (const uint8_t *str) const;
	inline int StringWidth (const char *str) const { return StringWidth ((const uint8_t *)str); }
	inline int StringWidth (const FString &str) const { return StringWidth ((const uint8_t *)str.GetChars()); }

	// Checks if the font contains all characters to print this text.
	bool CanPrint(const uint8_t *str) const;
	inline bool CanPrint(const char *str) const { return CanPrint((const uint8_t *)str); }
	inline bool CanPrint(const FString &str) const { return CanPrint((const uint8_t *)str.GetChars()); }

	int GetCharCode(int code, bool needpic) const;
	char GetCursor() const { return Cursor; }
	void SetCursor(char c) { Cursor = c; }
	void SetKerning(int c) { GlobalKerning = c; }
	bool NoTranslate() const { return noTranslate; }
	void RecordAllTextureColors(uint32_t *usedcolors);
	virtual void SetDefaultTranslation(uint32_t *colors);
	void CheckCase();



protected:
	FFont (int lump);

	void BuildTranslations (const double *luminosity, const uint8_t *identity,
		const void *ranges, int total_colors, const PalEntry *palette);
	void FixXMoves();

	static int SimpleTranslation (uint32_t *colorsused, uint8_t *translation,
		uint8_t *identity, TArray<double> &Luminosity);

	void ReadSheetFont(TArray<FolderEntry> &folderdata, int width, int height, const DVector2 &Scale);

	EFontType Type = EFontType::Unknown;
	int FirstChar, LastChar;
	int SpaceWidth;
	int FontHeight;
	int GlobalKerning;
	int TranslationType = 0;
	char Cursor;
	bool noTranslate;
	bool translateUntranslated;
	bool MixedCase = false;
	bool forceremap = false;
	struct CharData
	{
		FTexture *TranslatedPic = nullptr;	// Texture for use with font translations.
		FTexture *OriginalPic = nullptr;	// Texture for use with CR_UNTRANSLATED or font colorization. 
		int XMove = INT_MIN;
	};
	TArray<CharData> Chars;
	int ActiveColors;
	TArray<FRemapTable> Ranges;
	uint8_t PatchRemap[256];

	int Lump;
	FName FontName = NAME_None;
	FFont *Next;

	static FFont *FirstFont;
	friend struct FontsDeleter;

	friend void V_ClearFonts();
	friend void V_InitFonts();
};


extern FFont *SmallFont, *SmallFont2, *BigFont, *BigUpper, *ConFont, *IntermissionFont, *NewConsoleFont, *NewSmallFont, *CurrentConsoleFont, *OriginalSmallFont, *AlternativeSmallFont, *OriginalBigFont;

void V_InitFonts();
void V_ClearFonts();
EColorRange V_FindFontColor (FName name);
PalEntry V_LogColorFromColorRange (EColorRange range);
EColorRange V_ParseFontColor (const uint8_t *&color_value, int normalcolor, int boldcolor);
FFont *V_GetFont(const char *fontname, const char *fontlumpname = nullptr);
void V_InitFontColors();

FFont * C_GetDefaultHUDFont();


#endif //__V_FONT_H__
