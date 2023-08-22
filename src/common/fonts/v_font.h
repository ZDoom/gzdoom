#pragma once
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


#include "filesystem.h"
#include "vectors.h"
#include "palentry.h"
#include "name.h"

class FGameTexture;
struct FRemapTable;
class FFont;

FFont* V_GetFont(const char* fontname, const char* fontlumpname = nullptr);

enum EColorRange : int
{
	CR_UNDEFINED = -1,
	CR_NATIVEPAL = -1,
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

using GlyphSet = TMap<int, FGameTexture*>;

class FFont
{
	friend void V_LoadTranslations();
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

	FFont (const char *fontname, const char *nametemplate, const char *filetemplate, int first, int count, int base, int fdlump, int spacewidth=-1, bool notranslate = false, bool iwadonly = false, bool doomtemplate = false, GlyphSet *baseGlpyphs = nullptr);
	FFont(int lump, FName nm = NAME_None);
	virtual ~FFont ();

	virtual FGameTexture *GetChar (int code, int translation, int *const width) const;
	virtual int GetCharWidth (int code) const;
	int GetColorTranslation (EColorRange range, PalEntry *color = nullptr) const;
	int GetLump() const { return Lump; }
	int GetSpaceWidth () const { return SpaceWidth; }
	int GetHeight () const { return FontHeight; }
	int GetDefaultKerning () const { return GlobalKerning; }
	int GetMaxAscender(const uint8_t* text) const;
	int GetMaxAscender(const char* text) const { return GetMaxAscender((uint8_t*)text); }
	int GetMaxAscender(const FString &text) const { return GetMaxAscender((uint8_t*)text.GetChars()); }
	virtual void LoadTranslations();
	FName GetName() const { return FontName; }

	static FFont *FindFont(FName fontname);

	// Return width of string in pixels (unscaled)
	int StringWidth (const uint8_t *str, int spacing = 0) const;
	inline int StringWidth (const char *str, int spacing = 0) const { return StringWidth ((const uint8_t *)str, spacing); }
	inline int StringWidth (const FString &str, int spacing = 0) const { return StringWidth ((const uint8_t *)str.GetChars(), spacing); }

	// Checks if the font contains all characters to print this text.
	bool CanPrint(const uint8_t *str) const;
	inline bool CanPrint(const char *str) const { return CanPrint((const uint8_t *)str); }
	inline bool CanPrint(const FString &str) const { return CanPrint((const uint8_t *)str.GetChars()); }

	inline FFont* AltFont()
	{
		if (AltFontName != NAME_None) return V_GetFont(AltFontName.GetChars());
		return nullptr;
	}

	int GetCharCode(int code, bool needpic) const;
	char GetCursor() const { return Cursor; }
	void SetCursor(char c) { Cursor = c; }
	void SetKerning(int c) { GlobalKerning = c; }
	void SetHeight(int c) { FontHeight = c; }
	void ClearOffsets();
	bool NoTranslate() const { return noTranslate; }
	virtual void RecordAllTextureColors(uint32_t *usedcolors);
	void CheckCase();
	void SetName(FName nm) { FontName = nm; }

	int GetDisplacement() const { return Displacement; }

	static int GetLuminosity(uint32_t* colorsused, TArray<double>& Luminosity, int* minlum = nullptr, int* maxlum = nullptr);
	EFontType GetType() const { return Type; }

	friend void V_InitCustomFonts();

	void CopyFrom(const FFont& other)
	{
		Type = other.Type;
		FirstChar = other.FirstChar;
		LastChar = other.LastChar;
		SpaceWidth = other.SpaceWidth;
		FontHeight = other.FontHeight;
		GlobalKerning = other.GlobalKerning;
		TranslationType = other.TranslationType;
		Displacement = other.Displacement;
		Cursor = other.Cursor;
		noTranslate = other.noTranslate;
		MixedCase = other.MixedCase;
		forceremap = other.forceremap;
		Chars = other.Chars;
		Translations = other.Translations;
		lowercaselatinonly = other.lowercaselatinonly;
		Lump = other.Lump;
	}

protected:

	void FixXMoves();

	void ReadSheetFont(std::vector<FileSys::FolderEntry> &folderdata, int width, int height, const DVector2 &Scale);

	EFontType Type = EFontType::Unknown;
	FName AltFontName = NAME_None;
	int FirstChar, LastChar;
	int SpaceWidth;
	int FontHeight;
	int GlobalKerning;
	int TranslationType = 0;
	int Displacement = 0;
	int16_t MinLum = -1, MaxLum = -1;
	char Cursor;
	bool noTranslate = false;
	bool MixedCase = false;
	bool forceremap = false;
	bool lowercaselatinonly = false;
	struct CharData
	{
		FGameTexture *OriginalPic = nullptr;
		int XMove = INT_MIN;
	};
	TArray<CharData> Chars;
	TArray<int> Translations;

	int Lump;
	FName FontName = NAME_None;
	FFont *Next;

	static FFont *FirstFont;
	friend struct FontsDeleter;

	friend void V_ClearFonts();
	friend void V_InitFonts();
};

extern FFont *SmallFont, *SmallFont2, *BigFont, *BigUpper, *ConFont, *IntermissionFont, *NewConsoleFont, *NewSmallFont, *CurrentConsoleFont, *OriginalSmallFont, *AlternativeSmallFont, *OriginalBigFont, *AlternativeBigFont;

void V_InitFonts();
void V_ClearFonts();
EColorRange V_FindFontColor (FName name);
PalEntry V_LogColorFromColorRange (EColorRange range);
EColorRange V_ParseFontColor (const uint8_t *&color_value, int normalcolor, int boldcolor);
void V_InitFontColors();
char* CleanseString(char* str);
void V_ApplyLuminosityTranslation(int translation, uint8_t* pixel, int size);
void V_LoadTranslations();
class FBitmap;


