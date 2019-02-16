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
		-- by the font but is used by imagetool when converting the font
		-- back to an image. Color 0 is the transparent color and is also
		-- used only for converting the font back to an image. The other
		-- entries are all presorted in increasing order of brightness.

	ubyte CharacterData[...];
		-- RLE character data, in order
*/

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "templates.h"
#include "doomtype.h"
#include "m_swap.h"
#include "v_font.h"
#include "v_video.h"
#include "w_wad.h"
#include "gi.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "hu_stuff.h"
#include "gstrings.h"
#include "v_text.h"
#include "vm.h"
#include "image.h"
#include "utf8.h"
#include "textures/formats/fontchars.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_LOG_COLOR	PalEntry(223,223,223)

// TYPES -------------------------------------------------------------------
void RecordTextureColors (FImageSource *pic, uint8_t *colorsused);

// This structure is used by BuildTranslations() to hold color information.
struct TranslationParm
{
	short RangeStart;	// First level for this range
	short RangeEnd;		// Last level for this range
	uint8_t Start[3];		// Start color for this range
	uint8_t End[3];		// End color for this range
};

struct TranslationMap
{
	FName Name;
	int Number;
};

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

class FSinglePicFont : public FFont
{
public:
	FSinglePicFont(const char *picname);

	// FFont interface
	FTexture *GetChar(int code, int translation, int *const width, bool *redirected = nullptr) const override;
	int GetCharWidth (int code) const;

protected:
	FTextureID PicNum;
};

// Essentially a normal multilump font but with an explicit list of character patches
class FSpecialFont : public FFont
{
public:
	FSpecialFont (const char *name, int first, int count, FTexture **lumplist, const bool *notranslate, int lump, bool donttranslate);

	void LoadTranslations();

protected:
	bool notranslate[256];
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

static int TranslationMapCompare (const void *a, const void *b);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int PrintColors[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FFont *FFont::FirstFont = nullptr;
int NumTextColors;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<TranslationParm> TranslationParms[2];
static TArray<TranslationMap> TranslationLookup;
static TArray<PalEntry> TranslationColors;

// CODE --------------------------------------------------------------------

uint16_t lowerforupper[65536];
uint16_t upperforlower[65536];
bool islowermap[65536];
bool isuppermap[65536];

// This is a supposedly complete mapping of all lower <-> upper pairs. Most will most likely never be needed by Doom but this way there won't be any future surprises
static const uint16_t loweruppercase[] = {
0x0061,0x0041,
0x0062,0x0042,
0x0063,0x0043,
0x0064,0x0044,
0x0065,0x0045,
0x0066,0x0046,
0x0067,0x0047,
0x0068,0x0048,
0x0069,0x0049,
0x006A,0x004A,
0x006B,0x004B,
0x006C,0x004C,
0x006D,0x004D,
0x006E,0x004E,
0x006F,0x004F,
0x0070,0x0050,
0x0071,0x0051,
0x0072,0x0052,
0x0073,0x0053,
0x0074,0x0054,
0x0075,0x0055,
0x0076,0x0056,
0x0077,0x0057,
0x0078,0x0058,
0x0079,0x0059,
0x007A,0x005A,
0x00E0,0x00C0,
0x00E1,0x00C1,
0x00E2,0x00C2,
0x00E3,0x00C3,
0x00E4,0x00C4,
0x00E5,0x00C5,
0x00E6,0x00C6,
0x00E7,0x00C7,
0x00E8,0x00C8,
0x00E9,0x00C9,
0x00EA,0x00CA,
0x00EB,0x00CB,
0x00EC,0x00CC,
0x00ED,0x00CD,
0x00EE,0x00CE,
0x00EF,0x00CF,
0x00F0,0x00D0,
0x00F1,0x00D1,
0x00F2,0x00D2,
0x00F3,0x00D3,
0x00F4,0x00D4,
0x00F5,0x00D5,
0x00F6,0x00D6,
0x00F8,0x00D8,
0x00F9,0x00D9,
0x00FA,0x00DA,
0x00FB,0x00DB,
0x00FC,0x00DC,
0x00FD,0x00DD,
0x00FE,0x00DE,
0x00FF,0x0178,
0x0101,0x0100,
0x0103,0x0102,
0x0105,0x0104,
0x0107,0x0106,
0x0109,0x0108,
0x010B,0x010A,
0x010D,0x010C,
0x010F,0x010E,
0x0111,0x0110,
0x0113,0x0112,
0x0115,0x0114,
0x0117,0x0116,
0x0119,0x0118,
0x011B,0x011A,
0x011D,0x011C,
0x011F,0x011E,
0x0121,0x0120,
0x0123,0x0122,
0x0125,0x0124,
0x0127,0x0126,
0x0129,0x0128,
0x012B,0x012A,
0x012D,0x012C,
0x012F,0x012E,
0x0131,0x0049,
0x0133,0x0132,
0x0135,0x0134,
0x0137,0x0136,
0x013A,0x0139,
0x013C,0x013B,
0x013E,0x013D,
0x0140,0x013F,
0x0142,0x0141,
0x0144,0x0143,
0x0146,0x0145,
0x0148,0x0147,
0x014B,0x014A,
0x014D,0x014C,
0x014F,0x014E,
0x0151,0x0150,
0x0153,0x0152,
0x0155,0x0154,
0x0157,0x0156,
0x0159,0x0158,
0x015B,0x015A,
0x015D,0x015C,
0x015F,0x015E,
0x0161,0x0160,
0x0163,0x0162,
0x0165,0x0164,
0x0167,0x0166,
0x0169,0x0168,
0x016B,0x016A,
0x016D,0x016C,
0x016F,0x016E,
0x0171,0x0170,
0x0173,0x0172,
0x0175,0x0174,
0x0177,0x0176,
0x017A,0x0179,
0x017C,0x017B,
0x017E,0x017D,
0x0183,0x0182,
0x0185,0x0184,
0x0188,0x0187,
0x018C,0x018B,
0x0192,0x0191,
0x0199,0x0198,
0x01A1,0x01A0,
0x01A3,0x01A2,
0x01A5,0x01A4,
0x01A8,0x01A7,
0x01AD,0x01AC,
0x01B0,0x01AF,
0x01B4,0x01B3,
0x01B6,0x01B5,
0x01B9,0x01B8,
0x01BD,0x01BC,
0x01C6,0x01C4,
0x01C9,0x01C7,
0x01CC,0x01CA,
0x01CE,0x01CD,
0x01D0,0x01CF,
0x01D2,0x01D1,
0x01D4,0x01D3,
0x01D6,0x01D5,
0x01D8,0x01D7,
0x01DA,0x01D9,
0x01DC,0x01DB,
0x01DF,0x01DE,
0x01E1,0x01E0,
0x01E3,0x01E2,
0x01E5,0x01E4,
0x01E7,0x01E6,
0x01E9,0x01E8,
0x01EB,0x01EA,
0x01ED,0x01EC,
0x01EF,0x01EE,
0x01F3,0x01F1,
0x01F5,0x01F4,
0x01FB,0x01FA,
0x01FD,0x01FC,
0x01FF,0x01FE,
0x0201,0x0200,
0x0203,0x0202,
0x0205,0x0204,
0x0207,0x0206,
0x0209,0x0208,
0x020B,0x020A,
0x020D,0x020C,
0x020F,0x020E,
0x0211,0x0210,
0x0213,0x0212,
0x0215,0x0214,
0x0217,0x0216,
0x0253,0x0181,
0x0254,0x0186,
0x0257,0x018A,
0x0258,0x018E,
0x0259,0x018F,
0x025B,0x0190,
0x0260,0x0193,
0x0263,0x0194,
0x0268,0x0197,
0x0269,0x0196,
0x026F,0x019C,
0x0272,0x019D,
0x0275,0x019F,
0x0283,0x01A9,
0x0288,0x01AE,
0x028A,0x01B1,
0x028B,0x01B2,
0x0292,0x01B7,
0x03AC,0x0386,
0x03AD,0x0388,
0x03AE,0x0389,
0x03AF,0x038A,
0x03B1,0x0391,
0x03B2,0x0392,
0x03B3,0x0393,
0x03B4,0x0394,
0x03B5,0x0395,
0x03B6,0x0396,
0x03B7,0x0397,
0x03B8,0x0398,
0x03B9,0x0399,
0x03BA,0x039A,
0x03BB,0x039B,
0x03BC,0x039C,
0x03BD,0x039D,
0x03BE,0x039E,
0x03BF,0x039F,
0x03C0,0x03A0,
0x03C1,0x03A1,
0x03C3,0x03A3,
0x03C4,0x03A4,
0x03C5,0x03A5,
0x03C6,0x03A6,
0x03C7,0x03A7,
0x03C8,0x03A8,
0x03C9,0x03A9,
0x03CA,0x03AA,
0x03CB,0x03AB,
0x03CC,0x038C,
0x03CD,0x038E,
0x03CE,0x038F,
0x03E3,0x03E2,
0x03E5,0x03E4,
0x03E7,0x03E6,
0x03E9,0x03E8,
0x03EB,0x03EA,
0x03ED,0x03EC,
0x03EF,0x03EE,
0x0430,0x0410,
0x0431,0x0411,
0x0432,0x0412,
0x0433,0x0413,
0x0434,0x0414,
0x0435,0x0415,
0x0436,0x0416,
0x0437,0x0417,
0x0438,0x0418,
0x0439,0x0419,
0x043A,0x041A,
0x043B,0x041B,
0x043C,0x041C,
0x043D,0x041D,
0x043E,0x041E,
0x043F,0x041F,
0x0440,0x0420,
0x0441,0x0421,
0x0442,0x0422,
0x0443,0x0423,
0x0444,0x0424,
0x0445,0x0425,
0x0446,0x0426,
0x0447,0x0427,
0x0448,0x0428,
0x0449,0x0429,
0x044A,0x042A,
0x044B,0x042B,
0x044C,0x042C,
0x044D,0x042D,
0x044E,0x042E,
0x044F,0x042F,
0x0451,0x0401,
0x0452,0x0402,
0x0453,0x0403,
0x0454,0x0404,
0x0455,0x0405,
0x0456,0x0406,
0x0457,0x0407,
0x0458,0x0408,
0x0459,0x0409,
0x045A,0x040A,
0x045B,0x040B,
0x045C,0x040C,
0x045E,0x040E,
0x045F,0x040F,
0x0461,0x0460,
0x0463,0x0462,
0x0465,0x0464,
0x0467,0x0466,
0x0469,0x0468,
0x046B,0x046A,
0x046D,0x046C,
0x046F,0x046E,
0x0471,0x0470,
0x0473,0x0472,
0x0475,0x0474,
0x0477,0x0476,
0x0479,0x0478,
0x047B,0x047A,
0x047D,0x047C,
0x047F,0x047E,
0x0481,0x0480,
0x0491,0x0490,
0x0493,0x0492,
0x0495,0x0494,
0x0497,0x0496,
0x0499,0x0498,
0x049B,0x049A,
0x049D,0x049C,
0x049F,0x049E,
0x04A1,0x04A0,
0x04A3,0x04A2,
0x04A5,0x04A4,
0x04A7,0x04A6,
0x04A9,0x04A8,
0x04AB,0x04AA,
0x04AD,0x04AC,
0x04AF,0x04AE,
0x04B1,0x04B0,
0x04B3,0x04B2,
0x04B5,0x04B4,
0x04B7,0x04B6,
0x04B9,0x04B8,
0x04BB,0x04BA,
0x04BD,0x04BC,
0x04BF,0x04BE,
0x04C2,0x04C1,
0x04C4,0x04C3,
0x04C8,0x04C7,
0x04CC,0x04CB,
0x04D1,0x04D0,
0x04D3,0x04D2,
0x04D5,0x04D4,
0x04D7,0x04D6,
0x04D9,0x04D8,
0x04DB,0x04DA,
0x04DD,0x04DC,
0x04DF,0x04DE,
0x04E1,0x04E0,
0x04E3,0x04E2,
0x04E5,0x04E4,
0x04E7,0x04E6,
0x04E9,0x04E8,
0x04EB,0x04EA,
0x04EF,0x04EE,
0x04F1,0x04F0,
0x04F3,0x04F2,
0x04F5,0x04F4,
0x04F9,0x04F8,
0x0561,0x0531,
0x0562,0x0532,
0x0563,0x0533,
0x0564,0x0534,
0x0565,0x0535,
0x0566,0x0536,
0x0567,0x0537,
0x0568,0x0538,
0x0569,0x0539,
0x056A,0x053A,
0x056B,0x053B,
0x056C,0x053C,
0x056D,0x053D,
0x056E,0x053E,
0x056F,0x053F,
0x0570,0x0540,
0x0571,0x0541,
0x0572,0x0542,
0x0573,0x0543,
0x0574,0x0544,
0x0575,0x0545,
0x0576,0x0546,
0x0577,0x0547,
0x0578,0x0548,
0x0579,0x0549,
0x057A,0x054A,
0x057B,0x054B,
0x057C,0x054C,
0x057D,0x054D,
0x057E,0x054E,
0x057F,0x054F,
0x0580,0x0550,
0x0581,0x0551,
0x0582,0x0552,
0x0583,0x0553,
0x0584,0x0554,
0x0585,0x0555,
0x0586,0x0556,
0x10D0,0x10A0,
0x10D1,0x10A1,
0x10D2,0x10A2,
0x10D3,0x10A3,
0x10D4,0x10A4,
0x10D5,0x10A5,
0x10D6,0x10A6,
0x10D7,0x10A7,
0x10D8,0x10A8,
0x10D9,0x10A9,
0x10DA,0x10AA,
0x10DB,0x10AB,
0x10DC,0x10AC,
0x10DD,0x10AD,
0x10DE,0x10AE,
0x10DF,0x10AF,
0x10E0,0x10B0,
0x10E1,0x10B1,
0x10E2,0x10B2,
0x10E3,0x10B3,
0x10E4,0x10B4,
0x10E5,0x10B5,
0x10E6,0x10B6,
0x10E7,0x10B7,
0x10E8,0x10B8,
0x10E9,0x10B9,
0x10EA,0x10BA,
0x10EB,0x10BB,
0x10EC,0x10BC,
0x10ED,0x10BD,
0x10EE,0x10BE,
0x10EF,0x10BF,
0x10F0,0x10C0,
0x10F1,0x10C1,
0x10F2,0x10C2,
0x10F3,0x10C3,
0x10F4,0x10C4,
0x10F5,0x10C5,
0x1E01,0x1E00,
0x1E03,0x1E02,
0x1E05,0x1E04,
0x1E07,0x1E06,
0x1E09,0x1E08,
0x1E0B,0x1E0A,
0x1E0D,0x1E0C,
0x1E0F,0x1E0E,
0x1E11,0x1E10,
0x1E13,0x1E12,
0x1E15,0x1E14,
0x1E17,0x1E16,
0x1E19,0x1E18,
0x1E1B,0x1E1A,
0x1E1D,0x1E1C,
0x1E1F,0x1E1E,
0x1E21,0x1E20,
0x1E23,0x1E22,
0x1E25,0x1E24,
0x1E27,0x1E26,
0x1E29,0x1E28,
0x1E2B,0x1E2A,
0x1E2D,0x1E2C,
0x1E2F,0x1E2E,
0x1E31,0x1E30,
0x1E33,0x1E32,
0x1E35,0x1E34,
0x1E37,0x1E36,
0x1E39,0x1E38,
0x1E3B,0x1E3A,
0x1E3D,0x1E3C,
0x1E3F,0x1E3E,
0x1E41,0x1E40,
0x1E43,0x1E42,
0x1E45,0x1E44,
0x1E47,0x1E46,
0x1E49,0x1E48,
0x1E4B,0x1E4A,
0x1E4D,0x1E4C,
0x1E4F,0x1E4E,
0x1E51,0x1E50,
0x1E53,0x1E52,
0x1E55,0x1E54,
0x1E57,0x1E56,
0x1E59,0x1E58,
0x1E5B,0x1E5A,
0x1E5D,0x1E5C,
0x1E5F,0x1E5E,
0x1E61,0x1E60,
0x1E63,0x1E62,
0x1E65,0x1E64,
0x1E67,0x1E66,
0x1E69,0x1E68,
0x1E6B,0x1E6A,
0x1E6D,0x1E6C,
0x1E6F,0x1E6E,
0x1E71,0x1E70,
0x1E73,0x1E72,
0x1E75,0x1E74,
0x1E77,0x1E76,
0x1E79,0x1E78,
0x1E7B,0x1E7A,
0x1E7D,0x1E7C,
0x1E7F,0x1E7E,
0x1E81,0x1E80,
0x1E83,0x1E82,
0x1E85,0x1E84,
0x1E87,0x1E86,
0x1E89,0x1E88,
0x1E8B,0x1E8A,
0x1E8D,0x1E8C,
0x1E8F,0x1E8E,
0x1E91,0x1E90,
0x1E93,0x1E92,
0x1E95,0x1E94,
0x1EA1,0x1EA0,
0x1EA3,0x1EA2,
0x1EA5,0x1EA4,
0x1EA7,0x1EA6,
0x1EA9,0x1EA8,
0x1EAB,0x1EAA,
0x1EAD,0x1EAC,
0x1EAF,0x1EAE,
0x1EB1,0x1EB0,
0x1EB3,0x1EB2,
0x1EB5,0x1EB4,
0x1EB7,0x1EB6,
0x1EB9,0x1EB8,
0x1EBB,0x1EBA,
0x1EBD,0x1EBC,
0x1EBF,0x1EBE,
0x1EC1,0x1EC0,
0x1EC3,0x1EC2,
0x1EC5,0x1EC4,
0x1EC7,0x1EC6,
0x1EC9,0x1EC8,
0x1ECB,0x1ECA,
0x1ECD,0x1ECC,
0x1ECF,0x1ECE,
0x1ED1,0x1ED0,
0x1ED3,0x1ED2,
0x1ED5,0x1ED4,
0x1ED7,0x1ED6,
0x1ED9,0x1ED8,
0x1EDB,0x1EDA,
0x1EDD,0x1EDC,
0x1EDF,0x1EDE,
0x1EE1,0x1EE0,
0x1EE3,0x1EE2,
0x1EE5,0x1EE4,
0x1EE7,0x1EE6,
0x1EE9,0x1EE8,
0x1EEB,0x1EEA,
0x1EED,0x1EEC,
0x1EEF,0x1EEE,
0x1EF1,0x1EF0,
0x1EF3,0x1EF2,
0x1EF5,0x1EF4,
0x1EF7,0x1EF6,
0x1EF9,0x1EF8,
0x1F00,0x1F08,
0x1F01,0x1F09,
0x1F02,0x1F0A,
0x1F03,0x1F0B,
0x1F04,0x1F0C,
0x1F05,0x1F0D,
0x1F06,0x1F0E,
0x1F07,0x1F0F,
0x1F10,0x1F18,
0x1F11,0x1F19,
0x1F12,0x1F1A,
0x1F13,0x1F1B,
0x1F14,0x1F1C,
0x1F15,0x1F1D,
0x1F20,0x1F28,
0x1F21,0x1F29,
0x1F22,0x1F2A,
0x1F23,0x1F2B,
0x1F24,0x1F2C,
0x1F25,0x1F2D,
0x1F26,0x1F2E,
0x1F27,0x1F2F,
0x1F30,0x1F38,
0x1F31,0x1F39,
0x1F32,0x1F3A,
0x1F33,0x1F3B,
0x1F34,0x1F3C,
0x1F35,0x1F3D,
0x1F36,0x1F3E,
0x1F37,0x1F3F,
0x1F40,0x1F48,
0x1F41,0x1F49,
0x1F42,0x1F4A,
0x1F43,0x1F4B,
0x1F44,0x1F4C,
0x1F45,0x1F4D,
0x1F51,0x1F59,
0x1F53,0x1F5B,
0x1F55,0x1F5D,
0x1F57,0x1F5F,
0x1F60,0x1F68,
0x1F61, 0x1F69,
0x1F62, 0x1F6A,
0x1F63, 0x1F6B,
0x1F64, 0x1F6C,
0x1F65, 0x1F6D,
0x1F66, 0x1F6E,
0x1F67, 0x1F6F,
0x1F80, 0x1F88,
0x1F81, 0x1F89,
0x1F82, 0x1F8A,
0x1F83, 0x1F8B,
0x1F84, 0x1F8C,
0x1F85, 0x1F8D,
0x1F86, 0x1F8E,
0x1F87, 0x1F8F,
0x1F90, 0x1F98,
0x1F91, 0x1F99,
0x1F92, 0x1F9A,
0x1F93, 0x1F9B,
0x1F94, 0x1F9C,
0x1F95, 0x1F9D,
0x1F96, 0x1F9E,
0x1F97, 0x1F9F,
0x1FA0, 0x1FA8,
0x1FA1, 0x1FA9,
0x1FA2, 0x1FAA,
0x1FA3, 0x1FAB,
0x1FA4, 0x1FAC,
0x1FA5, 0x1FAD,
0x1FA6, 0x1FAE,
0x1FA7, 0x1FAF,
0x1FB0, 0x1FB8,
0x1FB1, 0x1FB9,
0x1FD0, 0x1FD8,
0x1FD1, 0x1FD9,
0x1FE0, 0x1FE8,
0x1FE1, 0x1FE9,
0x24D0, 0x24B6,
0x24D1, 0x24B7,
0x24D2, 0x24B8,
0x24D3, 0x24B9,
0x24D4, 0x24BA,
0x24D5, 0x24BB,
0x24D6, 0x24BC,
0x24D7, 0x24BD,
0x24D8, 0x24BE,
0x24D9, 0x24BF,
0x24DA, 0x24C0,
0x24DB, 0x24C1,
0x24DC, 0x24C2,
0x24DD, 0x24C3,
0x24DE, 0x24C4,
0x24DF, 0x24C5,
0x24E0, 0x24C6,
0x24E1, 0x24C7,
0x24E2, 0x24C8,
0x24E3, 0x24C9,
0x24E4, 0x24CA,
0x24E5, 0x24CB,
0x24E6, 0x24CC,
0x24E7, 0x24CD,
0x24E8, 0x24CE,
0x24E9, 0x24CF,
0xFF41, 0xFF21,
0xFF42, 0xFF22,
0xFF43, 0xFF23,
0xFF44, 0xFF24,
0xFF45, 0xFF25,
0xFF46, 0xFF26,
0xFF47, 0xFF27,
0xFF48, 0xFF28,
0xFF49, 0xFF29,
0xFF4A, 0xFF2A,
0xFF4B, 0xFF2B,
0xFF4C, 0xFF2C,
0xFF4D, 0xFF2D,
0xFF4E, 0xFF2E,
0xFF4F, 0xFF2F,
0xFF50, 0xFF30,
0xFF51, 0xFF31,
0xFF52, 0xFF32,
0xFF53, 0xFF33,
0xFF54, 0xFF34,
0xFF55, 0xFF35,
0xFF56, 0xFF36,
0xFF57, 0xFF37,
0xFF58, 0xFF38,
0xFF59, 0xFF39,
0xFF5A, 0xFF3A,
0, 0
};

void InitLowerUpper()
{
	for (int i = 0; i < 65536; i++)
	{
		lowerforupper[i] = i;
		upperforlower[i] = i;
	}
	for (int i = 0; loweruppercase[i]; i += 2)
	{
		auto lower = loweruppercase[i];
		auto upper = loweruppercase[i + 1];
		if (upperforlower[upper] == upper) lowerforupper[upper] = lower;	// This mapping is ambiguous (see 0x0131 -> 0x0049, (small Turkish 'i' without dot.) so only pick the first match.
		if (upperforlower[lower] == lower) upperforlower[lower] = upper;
		isuppermap[upper] = islowermap[lower] = true;
	}
}


static bool myislower(int code)
{
	if (code >= 0 && code < 65536) return islowermap[code];
	return false;
}

// Returns a character without an accent mark.
// FIXME: Only valid for CP-1252; we should go Unicode at some point.

static int stripaccent(int code)
{
	if (code < 0x8a)
		return code;
	if (code == 0x8a)	// Latin capital letter S with caron
		return 'S';
	if (code == 0x8e)	// Latin capital letter Z with caron
		return 'Z';
	if (code == 0x9a)	// Latin small letter S with caron
		return 's';
	if (code == 0x9e)	// Latin small letter Z with caron
		return 'z';
	if (code == 0x9f)	// Latin capital letter Y with diaeresis
		return 'Y';
	if (code == 0xff)	// Latin small letter Y with diaeresis
		return 'y';
	// Every other accented character has the high two bits set.
	if ((code & 0xC0) == 0)
		return code;
	// Make lowercase characters uppercase so there are half as many tests.
	int acode = code & 0xDF;
	if (acode >= 0xC0 && acode <= 0xC5)		// A with accents
		return 'A' + (code & 0x20);
	if (acode == 0xC7)						// Cedilla
		return 'C' + (acode & 0x20);
	if (acode >= 0xC8 && acode <= 0xCB)		// E with accents
		return 'E' + (code & 0x20);
	if (acode >= 0xCC && acode <= 0xCF)		// I with accents
		return 'I' + (code & 0x20);
	if (acode == 0xD0)						// Eth
		return 'D' + (code & 0x20);
	if (acode == 0xD1)						// N with tilde
		return 'N' + (code & 0x20);
	if ((acode >= 0xD2 && acode <= 0xD6) ||	// O with accents
		acode == 0xD8)						// O with stroke
		return 'O' + (code & 0x20);
	if (acode >= 0xD9 && acode <= 0xDC)		// U with accents
		return 'U' + (code & 0x20);
	if (acode == 0xDD)						// Y with accute
		return 'Y' + (code & 0x20);
	if (acode == 0xDE)						// Thorn
		return 'P' + (code & 0x20);			// well, it sort of looks like a 'P'
	// fixme: codes above 0x100 not supported yet!
	return code;
}

FFont *V_GetFont(const char *name, const char *fontlumpname)
{
	FFont *font = FFont::FindFont (name);
	if (font == nullptr)
	{
		int lump = -1;
		int folderfile = -1;
		
		TArray<FolderEntry> folderdata;
		FStringf path("fonts/%s/", name);
		
		// Use a folder-based font only if it comes from a later file than the single lump version.
		if (Wads.GetLumpsInFolder(path, folderdata, true))
		{
			// This assumes that any custom font comes in one piece and not distributed across multiple resource files.
			folderfile = Wads.GetLumpFile(folderdata[0].lumpnum);
		}


		lump = Wads.CheckNumForFullName(fontlumpname? fontlumpname : name, true);
		
		if (lump != -1 && Wads.GetLumpFile(lump) >= folderfile)
		{
			uint32_t head;
			{
				auto lumpy = Wads.OpenLumpReader (lump);
				lumpy.Read (&head, 4);
			}
			if ((head & MAKE_ID(255,255,255,0)) == MAKE_ID('F','O','N',0) ||
				head == MAKE_ID(0xE1,0xE6,0xD5,0x1A))
			{
				return new FSingleLumpFont (name, lump);
			}
		}
			FTextureID picnum = TexMan.CheckForTexture (name, ETextureType::Any);
			if (picnum.isValid())
			{
			FTexture *tex = TexMan.GetTexture(picnum);
			if (tex && tex->GetSourceLump() >= folderfile)
			{
				return new FSinglePicFont (name);
			}
		}
		if (folderdata.Size() > 0)
		{
			return new FFont(name, nullptr, name, HU_FONTSTART, HU_FONTSIZE, 1, -1);
	}
	}
	return font;
}

//==========================================================================
//
// FFont :: FFont
//
// Loads a multi-texture font.
//
//==========================================================================

FFont::FFont (const char *name, const char *nametemplate, const char *filetemplate, int lfirst, int lcount, int start, int fdlump, int spacewidth, bool notranslate)
{
	int i;
	FTextureID lump;
	char buffer[12];
	int maxyoffs;
	bool doomtemplate = (nametemplate && (gameinfo.gametype & GAME_DoomChex)) ? strncmp (nametemplate, "STCFN", 5) == 0 : false;
	DVector2 Scale = { 1, 1 };

	noTranslate = notranslate;
	Lump = fdlump;
	FontHeight = 0;
	GlobalKerning = false;
	FontName = name;
	Next = FirstFont;
	FirstFont = this;
	Cursor = '_';
	ActiveColors = 0;
	SpaceWidth = 0;
	FontHeight = 0;
	uint8_t pp = 0;
	for (auto &p : PatchRemap) p = pp++;
	translateUntranslated = false;

	maxyoffs = 0;

	TMap<int, FTexture*> charMap;
	int minchar = INT_MAX;
	int maxchar = INT_MIN;
	
	// Read the font's configuration.
	// This will not be done for the default fonts, because they are not atomic and the default content does not need it.
	
	TArray<FolderEntry> folderdata;
	if (filetemplate != nullptr)
	{
		FStringf path("fonts/%s/", filetemplate);
		// If a name template is given, collect data from all resource files.
		// For anything else, each folder is being treated as an atomic, self-contained unit and mixing from different glyph sets is blocked.
		Wads.GetLumpsInFolder(path, folderdata, nametemplate == nullptr);
		
		if (nametemplate == nullptr)
		{
			// Only take font.inf from the actual folder we are processing but not from an older folder that may have been superseded.
			FStringf infpath("fonts/%s/font.inf", filetemplate);
			
			unsigned index = folderdata.FindEx([=](const FolderEntry &entry)
			{
				return infpath.CompareNoCase(entry.name) == 0;
			});
			
			if (index < folderdata.Size())
			{
				FScanner sc;
				sc.OpenLumpNum(folderdata[index].lumpnum);
				while (sc.GetToken())
				{
					sc.TokenMustBe(TK_Identifier);
					if (sc.Compare("Kerning"))
					{
						sc.MustGetValue(false);
						GlobalKerning = sc.Number;
					}
					else if (sc.Compare("Scale"))
					{
						sc.MustGetValue(true);
						Scale.Y = Scale.X = sc.Float;
						if (sc.CheckToken(','))
						{
							sc.MustGetValue(true);
							Scale.Y = sc.Float;
						}
					}
					else if (sc.Compare("SpaceWidth"))
					{
						sc.MustGetValue(false);
						SpaceWidth = sc.Number;
					}
					else if (sc.Compare("FontHeight"))
					{
						sc.MustGetValue(false);
						FontHeight = sc.Number;
					}
				}
			}
		}
	}
	
	
	if (nametemplate != nullptr)
	{
		for (i = 0; i < lcount; i++)
		{
			int position = '!' + i;
			mysnprintf(buffer, countof(buffer), nametemplate, i + start);
			
		lump = TexMan.CheckForTexture(buffer, ETextureType::MiscPatch);
		if (doomtemplate && lump.isValid() && i + start == 121)
		{ // HACKHACK: Don't load STCFN121 in doom(2), because
		  // it's not really a lower-case 'y' but a '|'.
		  // Because a lot of wads with their own font seem to foolishly
		  // copy STCFN121 and make it a '|' themselves, wads must
		  // provide STCFN120 (x) and STCFN122 (z) for STCFN121 to load as a 'y'.
			if (!TexMan.CheckForTexture("STCFN120", ETextureType::MiscPatch).isValid() ||
				!TexMan.CheckForTexture("STCFN122", ETextureType::MiscPatch).isValid())
			{
				// insert the incorrectly named '|' graphic in its correct position.
					position = 124;
			}
		}
		if (lump.isValid())
		{
				if (position < minchar) minchar = position;
				if (position > maxchar) maxchar = position;
				charMap.Insert(position, TexMan.GetTexture(lump));
			}
		}
	}
	if (folderdata.Size() > 0)
	{
		// all valid lumps must be named with a hex number that represents its Unicode character index.
		for (auto &entry : folderdata)
		{
			char *endp;
			auto base = ExtractFileBase(entry.name);
			auto position = strtoll(base.GetChars(), &endp, 16);
			if ((*endp == 0 || (*endp == '.' && position >= '!' && position < 0xffff)))
			{
				auto lump = TexMan.CheckForTexture(entry.name, ETextureType::MiscPatch);
				if (lump.isValid())
				{
					if ((int)position < minchar) minchar = (int)position;
					if ((int)position > maxchar) maxchar = (int)position;
					auto tex = TexMan.GetTexture(lump);
					tex->SetScale(Scale);
					charMap.Insert((int)position, tex);
				}
			}
		}
	}

	FirstChar = minchar;
	LastChar = maxchar;
	auto count = maxchar - minchar + 1;
	Chars.Resize(count);
	int fontheight = 0;

	for (i = 0; i < count; i++)
	{
		auto lump = charMap.CheckKey(FirstChar + i);
		if (lump != nullptr)
		{
			FTexture *pic = *lump;
			if (pic != nullptr)
			{
				int height = pic->GetDisplayHeight();
				int yoffs = pic->GetDisplayTopOffset();

				if (yoffs > maxyoffs)
				{
					maxyoffs = yoffs;
				}
				height += abs(yoffs);
				if (height > fontheight)
				{
					fontheight = height;
				}
			}

			pic->SetUseType(ETextureType::FontChar);
			if (!noTranslate)
			{
				Chars[i].OriginalPic = pic;
				Chars[i].TranslatedPic = new FImageTexture(new FFontChar1 (pic->GetImage()), "");
				Chars[i].TranslatedPic->CopySize(pic);
				Chars[i].TranslatedPic->SetUseType(ETextureType::FontChar);
				TexMan.AddTexture(Chars[i].TranslatedPic);
			}
			else
			{
				Chars[i].TranslatedPic = pic;
			}

			Chars[i].XMove = Chars[i].TranslatedPic->GetDisplayWidth();
		}
		else
		{
			Chars[i].TranslatedPic = nullptr;
			Chars[i].XMove = INT_MIN;
		}
	}
	
	if (SpaceWidth == 0) // An explicit override from the .inf file must always take precedence
	{
	if (spacewidth != -1)
	{
		SpaceWidth = spacewidth;
	}
		else if ('N'-FirstChar >= 0 && 'N'-FirstChar < count && Chars['N' - FirstChar].TranslatedPic != nullptr)
	{
			SpaceWidth = (Chars['N' - FirstChar].XMove + 1) / 2;
	}
	else
	{
		SpaceWidth = 4;
	}
	}
	if (FontHeight == 0) FontHeight = fontheight;

	FixXMoves();

	if (!noTranslate) LoadTranslations();
}

//==========================================================================
//
// FFont :: ~FFont
//
//==========================================================================

FFont::~FFont ()
{
	FFont **prev = &FirstFont;
	FFont *font = *prev;

	while (font != nullptr && font != this)
	{
		prev = &font->Next;
		font = *prev;
	}

	if (font != nullptr)
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

FFont *FFont::FindFont (FName name)
{
	if (name == NAME_None)
	{
		return nullptr;
	}
	FFont *font = FirstFont;

	while (font != nullptr)
	{
		if (font->FontName == name) return font;
		font = font->Next;
	}
	return nullptr;
}

//==========================================================================
//
// RecordTextureColors
//
// Given a 256 entry buffer, sets every entry that corresponds to a color
// used by the texture to 1.
//
//==========================================================================

void RecordTextureColors (FImageSource *pic, uint8_t *usedcolors)
{
	int x;
	
	auto pixels = pic->GetPalettedPixels(false);
	auto size = pic->GetWidth() * pic->GetHeight();
	
	for(x = 0;x < size; x++)
	{
		usedcolors[pixels[x]]++;
	}
}

//==========================================================================
//
// compare
//
// Used for sorting colors by brightness.
//
//==========================================================================

static int compare (const void *arg1, const void *arg2)
{
	if (RPART(GPalette.BaseColors[*((uint8_t *)arg1)]) * 299 +
		GPART(GPalette.BaseColors[*((uint8_t *)arg1)]) * 587 +
		BPART(GPalette.BaseColors[*((uint8_t *)arg1)]) * 114  <
		RPART(GPalette.BaseColors[*((uint8_t *)arg2)]) * 299 +
		GPART(GPalette.BaseColors[*((uint8_t *)arg2)]) * 587 +
		BPART(GPalette.BaseColors[*((uint8_t *)arg2)]) * 114)
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

int FFont::SimpleTranslation (uint8_t *colorsused, uint8_t *translation, uint8_t *reverse, TArray<double> &Luminosity)
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

	Luminosity.Resize(j);
	Luminosity[0] = 0.0; // [BL] Prevent uninitalized memory
	max = 0.0;
	min = 100000000.0;
	for (i = 1; i < j; i++)
	{
		translation[reverse[i]] = i;

		Luminosity[i] = RPART(GPalette.BaseColors[reverse[i]]) * 0.299 +
						   GPART(GPalette.BaseColors[reverse[i]]) * 0.587 +
						   BPART(GPalette.BaseColors[reverse[i]]) * 0.114;
		if (Luminosity[i] > max)
			max = Luminosity[i];
		if (Luminosity[i] < min)
			min = Luminosity[i];
	}
	diver = 1.0 / (max - min);
	for (i = 1; i < j; i++)
	{
		Luminosity[i] = (Luminosity[i] - min) * diver;
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

void FFont::BuildTranslations (const double *luminosity, const uint8_t *identity,
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
			if (identity != nullptr)
			{
				memcpy (remap.Remap, identity, ActiveColors);
				if (palette != nullptr)
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

FRemapTable *FFont::GetColorTranslation (EColorRange range, PalEntry *color) const
{
	if (noTranslate)
	{
		PalEntry retcolor = PalEntry(255, 255, 255, 255);
		if (range >= 0 && range < NumTextColors && range != CR_UNTRANSLATED)
		{
			retcolor = TranslationColors[range];
			retcolor.a = 255;
		}
		if (color != nullptr) *color = retcolor;
	}
	if (ActiveColors == 0)
		return nullptr;
	else if (range >= NumTextColors)
		range = CR_UNTRANSLATED;
	//if (range == CR_UNTRANSLATED && !translateUntranslated) return nullptr;
	return &Ranges[range];
}

//==========================================================================
//
// FFont :: GetCharCode
//
// If the character code is in the font, returns it. If it is not, but it
// is lowercase and has an uppercase variant present, return that. Otherwise
// return -1.
//
//==========================================================================

int FFont::GetCharCode(int code, bool needpic) const
{
	if (code < 0 && code >= -128)
	{
		// regular chars turn negative when the 8th bit is set.
		code &= 255;
	}
	if (code >= FirstChar && code <= LastChar && (!needpic || Chars[code - FirstChar].TranslatedPic != nullptr))
	{
		return code;
	}
	// Try converting lowercase characters to uppercase.
	if (myislower(code))
	{
		code = upperforlower[code];
		if (code >= FirstChar && code <= LastChar && (!needpic || Chars[code - FirstChar].TranslatedPic != nullptr))
		{
			return code;
		}
	}
	// Try stripping accents from accented characters.
	int newcode = stripaccent(code);
	if (newcode != code)
	{
		code = newcode;
		if (code >= FirstChar && code <= LastChar && (!needpic || Chars[code - FirstChar].TranslatedPic != nullptr))
		{
			return code;
		}
	}
	return -1;
}

//==========================================================================
//
// FFont :: GetChar
//
//==========================================================================

FTexture *FFont::GetChar (int code, int translation, int *const width, bool *redirected) const
{
	code = GetCharCode(code, false);
	int xmove = SpaceWidth;

	if (code >= 0)
	{
		code -= FirstChar;
		xmove = Chars[code].XMove;
		if (Chars[code].TranslatedPic == nullptr)
		{
			code = GetCharCode(code + FirstChar, true);
			if (code >= 0)
			{
				code -= FirstChar;
				xmove = Chars[code].XMove;
			}
		}
	}
	if (width != nullptr)
	{
		*width = xmove;
	}
	if (code < 0) return nullptr;


	if (translation == CR_UNTRANSLATED)
	{
		bool redirect = Chars[code].OriginalPic && Chars[code].OriginalPic != Chars[code].TranslatedPic;
		if (redirected) *redirected = redirect;
		if (redirect)
		{
			assert(Chars[code].OriginalPic->UseType == ETextureType::FontChar);
			return Chars[code].OriginalPic;
		}
	}
	if (redirected) *redirected = false;
	assert(Chars[code].TranslatedPic->UseType == ETextureType::FontChar);
	return Chars[code].TranslatedPic;
}

//==========================================================================
//
// FFont :: GetCharWidth
//
//==========================================================================

int FFont::GetCharWidth (int code) const
{
	code = GetCharCode(code, false);
	return (code < 0) ? SpaceWidth : Chars[code - FirstChar].XMove;
}

//==========================================================================
//
// 
//
//==========================================================================

double GetBottomAlignOffset(FFont *font, int c)
{
	int w;
	FTexture *tex_zero = font->GetChar('0', CR_UNDEFINED, &w);
	FTexture *texc = font->GetChar(c, CR_UNDEFINED, &w);
	double offset = 0;
	if (texc) offset += texc->GetDisplayTopOffsetDouble();
	if (tex_zero) offset += -tex_zero->GetDisplayTopOffsetDouble() + tex_zero->GetDisplayHeightDouble();
	return offset;
}

//==========================================================================
//
// Find string width using this font
//
//==========================================================================

int FFont::StringWidth(const uint8_t *string) const
{
	int w = 0;
	int maxw = 0;

	while (*string)
	{
		auto chr = GetCharFromString(string);
		if (chr == TEXTCOLOR_ESCAPE)
		{
			// We do not need to check for UTF-8 in here.
			if (*string == '[')
			{
				while (*string != '\0' && *string != ']')
				{
					++string;
				}
			}
			if (*string != '\0')
			{
				++string;
			}
			continue;
		}
		else if (chr == '\n')
		{
			if (w > maxw)
				maxw = w;
			w = 0;
		}
		else
		{
			w += GetCharWidth(chr) + GlobalKerning;
		}
	}

	return MAX(maxw, w);
}

//==========================================================================
//
// FFont :: LoadTranslations
//
//==========================================================================

void FFont::LoadTranslations()
{
	unsigned int count = LastChar - FirstChar + 1;
	uint8_t usedcolors[256], identity[256];
	TArray<double> Luminosity;

	memset (usedcolors, 0, 256);
	for (unsigned int i = 0; i < count; i++)
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

	// Fixme: This needs to build a translation based on the source palette, not some intermediate 'ordered' table.

	ActiveColors = SimpleTranslation (usedcolors, PatchRemap, identity, Luminosity);

	for (unsigned int i = 0; i < count; i++)
	{
		if(Chars[i].TranslatedPic)
			static_cast<FFontChar1 *>(Chars[i].TranslatedPic->GetImage())->SetSourceRemap(PatchRemap);
	}

	BuildTranslations (Luminosity.Data(), identity, &TranslationParms[0][0], ActiveColors, nullptr);
}

//==========================================================================
//
// FFont :: FFont - default constructor
//
//==========================================================================

FFont::FFont (int lump)
{
	Lump = lump;
	FontName = NAME_None;
	Cursor = '_';
	noTranslate = false;
	uint8_t pp = 0;
	for (auto &p : PatchRemap) p = pp++;
}

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

//==========================================================================
//
// FSinglePicFont :: FSinglePicFont
//
// Creates a font to wrap a texture so that you can use hudmessage as if it
// were a hudpic command. It does not support translation, but animation
// is supported, unlike all the real fonts.
//
//==========================================================================

FSinglePicFont::FSinglePicFont(const char *picname) :
	FFont(-1) // Since lump is only needed for priority information we don't need to worry about this here.
{
	FTextureID picnum = TexMan.CheckForTexture (picname, ETextureType::Any);

	if (!picnum.isValid())
	{
		I_FatalError ("%s is not a font or texture", picname);
	}

	FTexture *pic = TexMan.GetTexture(picnum);

	FontName = picname;
	FontHeight = pic->GetDisplayHeight();
	SpaceWidth = pic->GetDisplayWidth();
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
// Returns the texture if code is 'a' or 'A', otherwise nullptr.
//
//==========================================================================

FTexture *FSinglePicFont::GetChar (int code, int translation, int *const width, bool *redirected) const
{
	*width = SpaceWidth;
	if (redirected) *redirected = false;
	if (code == 'a' || code == 'A')
	{
		return TexMan.GetPalettedTexture(PicNum, true);
	}
	else
	{
		return nullptr;
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
	uint8_t usedcolors[256], identity[256];
	TArray<double> Luminosity;
	int TotalColors;
	int i, j;

	memset (usedcolors, 0, 256);
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

//==========================================================================
//
// FFont :: FixXMoves
//
// If a font has gaps in its characters, set the missing characters'
// XMoves to either SpaceWidth or the unaccented or uppercase variant's
// XMove. Missing XMoves must be initialized with INT_MIN beforehand.
//
//==========================================================================

void FFont::FixXMoves()
{
	for (int i = 0; i <= LastChar - FirstChar; ++i)
	{
		if (Chars[i].XMove == INT_MIN)
		{
			// Try an uppercase character.
			if (myislower(i + FirstChar))
			{
				int upper = upperforlower[FirstChar + i];
				if (upper >= FirstChar && upper <= LastChar )
				{
					Chars[i].XMove = Chars[upper - FirstChar].XMove;
					continue;
				}
			}
			// Try an unnaccented character.
			int noaccent = stripaccent(i + FirstChar);
			if (noaccent != i + FirstChar)
			{
				noaccent -= FirstChar;
				if (noaccent >= 0)
				{
					Chars[i].XMove = Chars[noaccent].XMove;
					continue;
				}
			}
			Chars[i].XMove = SpaceWidth;
		}
	}
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
	FTexture *lumplist[256];
	bool notranslate[256];
	bool donttranslate;
	FString namebuffer, templatebuf;
	int i;
	int llump,lastlump=0;
	int format;
	int start;
	int first;
	int count;
	int spacewidth;
	char cursor = '_';

	while ((llump = Wads.FindLump ("FONTDEFS", &lastlump)) != -1)
	{
		sc.OpenLumpNum(llump);
		while (sc.GetString())
		{
			memset (lumplist, 0, sizeof(lumplist));
			memset (notranslate, 0, sizeof(notranslate));
			donttranslate = false;
			namebuffer = sc.String;
			format = 0;
			start = 33;
			first = 33;
			count = 223;
			spacewidth = -1;

			sc.MustGetStringName ("{");
			while (!sc.CheckString ("}"))
			{
				sc.MustGetString();
				if (sc.Compare ("TEMPLATE"))
				{
					if (format == 2) goto wrong;
					sc.MustGetString();
					templatebuf = sc.String;
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
				else if (sc.Compare ("CURSOR"))
				{
					sc.MustGetString();
					cursor = sc.String[0];
				}
				else if (sc.Compare ("SPACEWIDTH"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					spacewidth = sc.Number;
					format = 1;
				}
				else if (sc.Compare("DONTTRANSLATE"))
				{
					donttranslate = true;
				}
				else if (sc.Compare ("NOTRANSLATION"))
				{
					if (format == 1) goto wrong;
					while (sc.CheckNumber() && !sc.Crossed)
					{
						if (sc.Number >= 0 && sc.Number < 256)
							notranslate[sc.Number] = true;
					}
					format = 2;
				}
				else
				{
					if (format == 1) goto wrong;
					FTexture **p = &lumplist[*(unsigned char*)sc.String];
					sc.MustGetString();
					FTextureID texid = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
					if (texid.Exists())
					{
						*p = TexMan.GetTexture(texid);
					}
					else if (Wads.GetLumpFile(sc.LumpNum) >= Wads.GetIwadNum())
					{
						// Print a message only if this isn't in zdoom.pk3
						sc.ScriptMessage("%s: Unable to find texture in font definition for %s", sc.String, namebuffer.GetChars());
					}
					format = 2;
				}
			}
			if (format == 1)
			{
				FFont *fnt = new FFont (namebuffer, templatebuf, nullptr, first, count, start, llump, spacewidth, donttranslate);
				fnt->SetCursor(cursor);
			}
			else if (format == 2)
			{
				for (i = 0; i < 256; i++)
				{
					if (lumplist[i] != nullptr)
					{
						first = i;
						break;
					}
				}
				for (i = 255; i >= 0; i--)
				{
					if (lumplist[i] != nullptr)
					{
						count = i - first + 1;
						break;
					}
				}
				if (count > 0)
				{
					FFont *fnt = new FSpecialFont (namebuffer, first, count, &lumplist[first], notranslate, llump, donttranslate);
					fnt->SetCursor(cursor);
				}
			}
			else goto wrong;
		}
		sc.Close();
	}
	return;

wrong:
	sc.ScriptError ("Invalid combination of properties in font '%s'", namebuffer.GetChars());
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
	TranslationParm tparm = { 0, 0, {0}, {0} };	// Silence GCC (for real with -Wextra )
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

	TranslationParms[0].Clear();
	TranslationParms[1].Clear();
	TranslationLookup.Clear();
	TranslationColors.Clear();

	while ((lump = Wads.FindLump ("TEXTCOLO", &lastlump)) != -1)
	{
		if (gameinfo.flags & GI_NOTEXTCOLOR)
		{
			// Chex3 contains a bad TEXTCOLO lump, probably to force all text to be green.
			// This renders the Gray, Gold, Red and Yellow color range inoperable, some of
			// which are used by the menu. So we have no choice but to skip this lump so that
			// all colors work properly.
			// The text colors should be the end user's choice anyway.
			if (Wads.GetLumpFile(lump) == Wads.GetIwadNum()) continue;
		}
		FScanner sc(lump);
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
					logcolor = V_GetColor (nullptr, sc);
				}
				else
				{
					// Get first color
					c = V_GetColor (nullptr, sc);
					tparm.Start[0] = RPART(c);
					tparm.Start[1] = GPART(c);
					tparm.Start[2] = BPART(c);

					// Get second color
					sc.MustGetString();
					c = V_GetColor (nullptr, sc);
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

static int TranslationMapCompare (const void *a, const void *b)
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

EColorRange V_ParseFontColor (const uint8_t *&color_value, int normalcolor, int boldcolor)
{
	const uint8_t *ch = color_value;
	int newcolor = *ch++;

	if (newcolor == '-')			// Normal
	{
		newcolor = normalcolor;
	}
	else if (newcolor == '+')		// Bold
	{
		newcolor = boldcolor;
	}
	else if (newcolor == '!')		// Team chat
	{
		newcolor = PrintColors[PRINT_TEAMCHAT];
	}
	else if (newcolor == '*')		// Chat
	{
		newcolor = PrintColors[PRINT_CHAT];
	}
	else if (newcolor == '[')		// Named
	{
		const uint8_t *namestart = ch;
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
		color_value = ch - (newcolor == '\0');
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
	InitLowerUpper();
	V_InitCustomFonts ();

	// load the heads-up font
	if (!(SmallFont = V_GetFont("SmallFont", "SMALLFNT")))
	{
		if (Wads.CheckNumForName ("FONTA_S") >= 0)
		{
			SmallFont = new FFont ("SmallFont", "FONTA%02u", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, 1, -1);
			SmallFont->SetCursor('[');
		}
		else if (Wads.CheckNumForName ("STCFN033", ns_graphics) >= 0)
		{
			SmallFont = new FFont ("SmallFont", "STCFN%.3d", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1);
		}
	}
	if (!(SmallFont2 = V_GetFont("SmallFont2")))	// Only used by Strife
	{
		if (Wads.CheckNumForName ("STBFN033", ns_graphics) >= 0)
		{
			SmallFont2 = new FFont ("SmallFont2", "STBFN%.3d", "defsmallfont2", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1);
		}
		}
	if (!(BigFont = V_GetFont("BigFont")))
	{
		if (gameinfo.gametype & GAME_Raven)
		{
			BigFont = new FFont ("BigFont", "FONTB%02u", "defbigfont", HU_FONTSTART, HU_FONTSIZE, 1, -1);
		}
		}
	if (!(ConFont = V_GetFont("ConsoleFont", "CONFONT")))
		{
		ConFont = SmallFont;
		}
	if (!(IntermissionFont = FFont::FindFont("IntermissionFont")))
		{
		if (gameinfo.gametype & GAME_DoomChex)
			{
			IntermissionFont = FFont::FindFont("IntermissionFont_Doom");
			}
		if (IntermissionFont == nullptr)
			{
			IntermissionFont = BigFont;
			}
		}
	// This can only happen if gzdoom.pk3 is corrupted. ConFont should always be present.
	if (ConFont == nullptr)
	{
		I_FatalError("Console font not found.");
	}
	// SmallFont and SmallFont2 have no default provided by the engine. BigFont only has in non-Raven games.
	if (SmallFont == nullptr)
	{
		SmallFont = ConFont;
	}
	if (SmallFont2 == nullptr)
	{
		SmallFont2 = SmallFont;
		}
	if (BigFont == nullptr)
		{
		BigFont = SmallFont;
		}
	
}

void V_ClearFonts()
{
	while (FFont::FirstFont != nullptr)
	{
		delete FFont::FirstFont;
	}
	FFont::FirstFont = nullptr;
	SmallFont = SmallFont2 = BigFont = ConFont = IntermissionFont = nullptr;
}

