#ifndef __V_FONT_H__
#define __V_FONT_H__

#include "doomtype.h"

struct patch_s;
class DCanvas;

enum EColorRange
{
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
	NUM_TEXT_COLORS
};

class FFont
{
public:
	FFont (const char *nametemplate, int first, int count, int base);
	~FFont ();

	byte *GetChar (int code, int *const width, int *const height, int *const xoffs, int *const yoffs) const;
	int GetCharWidth (int code) const;
	byte *GetColorTranslation (EColorRange range) const;
	int GetSpaceWidth () const { return SpaceWidth; }
	int GetHeight () const { return FontHeight; }

protected:
	FFont ();

	void BuildTranslations (const byte *usedcolors, const byte *translation, const byte *identity, const double *luminosity);

	static int SimpleTranslation (byte *colorsused, byte *translation, byte *identity, double **luminosity);

	int FirstChar, LastChar;
	int SpaceWidth;
	int FontHeight;
	struct CharData
	{
		WORD Width, Height;
		SWORD XOffs, YOffs;
		byte *Data;
	} *Chars;
	byte *Bitmaps;
	int ActiveColors;
	byte *Ranges;
};

class FConsoleFont : public FFont
{
public:
	FConsoleFont (int lump);

protected:
	void BuildTranslations ();
};

void RawDrawPatch (patch_s *patch, byte *output, byte *tlate);
void RecordPatchColors (patch_s *patch, byte *colorsused);

extern FFont *SmallFont, *BigFont, *ConFont;

#endif //__V_FONT_H__