/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
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


#include <stdint.h>
#include <algorithm>
#include "st_start.h"
#include "m_alloc.h"
#include "w_wad.h"
#include "v_palette.h"
#include "s_sound.h"
#include "s_music.h"
#include "d_main.h"

void I_GetEvent();	// i_input.h pulls in too much garbage.

void ST_Util_InvalidateRect(BitmapInfo* bitmap_info, int left, int top, int right, int bottom);
bool ST_Util_CreateStartupWindow();

static const uint16_t IBM437ToUnicode[] = {
	0x0000, //#NULL
	0x0001, //#START OF HEADING
	0x0002, //#START OF TEXT
	0x0003, //#END OF TEXT
	0x0004, //#END OF TRANSMISSION
	0x0005, //#ENQUIRY
	0x0006, //#ACKNOWLEDGE
	0x0007, //#BELL
	0x0008, //#BACKSPACE
	0x0009, //#HORIZONTAL TABULATION
	0x000a, //#LINE FEED
	0x000b, //#VERTICAL TABULATION
	0x000c, //#FORM FEED
	0x000d, //#CARRIAGE RETURN
	0x000e, //#SHIFT OUT
	0x000f, //#SHIFT IN
	0x0010, //#DATA LINK ESCAPE
	0x0011, //#DEVICE CONTROL ONE
	0x0012, //#DEVICE CONTROL TWO
	0x0013, //#DEVICE CONTROL THREE
	0x0014, //#DEVICE CONTROL FOUR
	0x0015, //#NEGATIVE ACKNOWLEDGE
	0x0016, //#SYNCHRONOUS IDLE
	0x0017, //#END OF TRANSMISSION BLOCK
	0x0018, //#CANCEL
	0x0019, //#END OF MEDIUM
	0x001a, //#SUBSTITUTE
	0x001b, //#ESCAPE
	0x001c, //#FILE SEPARATOR
	0x001d, //#GROUP SEPARATOR
	0x001e, //#RECORD SEPARATOR
	0x001f, //#UNIT SEPARATOR
	0x0020, //#SPACE
	0x0021, //#EXCLAMATION MARK
	0x0022, //#QUOTATION MARK
	0x0023, //#NUMBER SIGN
	0x0024, //#DOLLAR SIGN
	0x0025, //#PERCENT SIGN
	0x0026, //#AMPERSAND
	0x0027, //#APOSTROPHE
	0x0028, //#LEFT PARENTHESIS
	0x0029, //#RIGHT PARENTHESIS
	0x002a, //#ASTERISK
	0x002b, //#PLUS SIGN
	0x002c, //#COMMA
	0x002d, //#HYPHEN-MINUS
	0x002e, //#FULL STOP
	0x002f, //#SOLIDUS
	0x0030, //#DIGIT ZERO
	0x0031, //#DIGIT ONE
	0x0032, //#DIGIT TWO
	0x0033, //#DIGIT THREE
	0x0034, //#DIGIT FOUR
	0x0035, //#DIGIT FIVE
	0x0036, //#DIGIT SIX
	0x0037, //#DIGIT SEVEN
	0x0038, //#DIGIT EIGHT
	0x0039, //#DIGIT NINE
	0x003a, //#COLON
	0x003b, //#SEMICOLON
	0x003c, //#LESS-THAN SIGN
	0x003d, //#EQUALS SIGN
	0x003e, //#GREATER-THAN SIGN
	0x003f, //#QUESTION MARK
	0x0040, //#COMMERCIAL AT
	0x0041, //#LATIN CAPITAL LETTER A
	0x0042, //#LATIN CAPITAL LETTER B
	0x0043, //#LATIN CAPITAL LETTER C
	0x0044, //#LATIN CAPITAL LETTER D
	0x0045, //#LATIN CAPITAL LETTER E
	0x0046, //#LATIN CAPITAL LETTER F
	0x0047, //#LATIN CAPITAL LETTER G
	0x0048, //#LATIN CAPITAL LETTER H
	0x0049, //#LATIN CAPITAL LETTER I
	0x004a, //#LATIN CAPITAL LETTER J
	0x004b, //#LATIN CAPITAL LETTER K
	0x004c, //#LATIN CAPITAL LETTER L
	0x004d, //#LATIN CAPITAL LETTER M
	0x004e, //#LATIN CAPITAL LETTER N
	0x004f, //#LATIN CAPITAL LETTER O
	0x0050, //#LATIN CAPITAL LETTER P
	0x0051, //#LATIN CAPITAL LETTER Q
	0x0052, //#LATIN CAPITAL LETTER R
	0x0053, //#LATIN CAPITAL LETTER S
	0x0054, //#LATIN CAPITAL LETTER T
	0x0055, //#LATIN CAPITAL LETTER U
	0x0056, //#LATIN CAPITAL LETTER V
	0x0057, //#LATIN CAPITAL LETTER W
	0x0058, //#LATIN CAPITAL LETTER X
	0x0059, //#LATIN CAPITAL LETTER Y
	0x005a, //#LATIN CAPITAL LETTER Z
	0x005b, //#LEFT SQUARE BRACKET
	0x005c, //#REVERSE SOLIDUS
	0x005d, //#RIGHT SQUARE BRACKET
	0x005e, //#CIRCUMFLEX ACCENT
	0x005f, //#LOW LINE
	0x0060, //#GRAVE ACCENT
	0x0061, //#LATIN SMALL LETTER A
	0x0062, //#LATIN SMALL LETTER B
	0x0063, //#LATIN SMALL LETTER C
	0x0064, //#LATIN SMALL LETTER D
	0x0065, //#LATIN SMALL LETTER E
	0x0066, //#LATIN SMALL LETTER F
	0x0067, //#LATIN SMALL LETTER G
	0x0068, //#LATIN SMALL LETTER H
	0x0069, //#LATIN SMALL LETTER I
	0x006a, //#LATIN SMALL LETTER J
	0x006b, //#LATIN SMALL LETTER K
	0x006c, //#LATIN SMALL LETTER L
	0x006d, //#LATIN SMALL LETTER M
	0x006e, //#LATIN SMALL LETTER N
	0x006f, //#LATIN SMALL LETTER O
	0x0070, //#LATIN SMALL LETTER P
	0x0071, //#LATIN SMALL LETTER Q
	0x0072, //#LATIN SMALL LETTER R
	0x0073, //#LATIN SMALL LETTER S
	0x0074, //#LATIN SMALL LETTER T
	0x0075, //#LATIN SMALL LETTER U
	0x0076, //#LATIN SMALL LETTER V
	0x0077, //#LATIN SMALL LETTER W
	0x0078, //#LATIN SMALL LETTER X
	0x0079, //#LATIN SMALL LETTER Y
	0x007a, //#LATIN SMALL LETTER Z
	0x007b, //#LEFT CURLY BRACKET
	0x007c, //#VERTICAL LINE
	0x007d, //#RIGHT CURLY BRACKET
	0x007e, //#TILDE
	0x007f, //#DELETE
	0x00c7, //#LATIN CAPITAL LETTER C WITH CEDILLA
	0x00fc, //#LATIN SMALL LETTER U WITH DIAERESIS
	0x00e9, //#LATIN SMALL LETTER E WITH ACUTE
	0x00e2, //#LATIN SMALL LETTER A WITH CIRCUMFLEX
	0x00e4, //#LATIN SMALL LETTER A WITH DIAERESIS
	0x00e0, //#LATIN SMALL LETTER A WITH GRAVE
	0x00e5, //#LATIN SMALL LETTER A WITH RING ABOVE
	0x00e7, //#LATIN SMALL LETTER C WITH CEDILLA
	0x00ea, //#LATIN SMALL LETTER E WITH CIRCUMFLEX
	0x00eb, //#LATIN SMALL LETTER E WITH DIAERESIS
	0x00e8, //#LATIN SMALL LETTER E WITH GRAVE
	0x00ef, //#LATIN SMALL LETTER I WITH DIAERESIS
	0x00ee, //#LATIN SMALL LETTER I WITH CIRCUMFLEX
	0x00ec, //#LATIN SMALL LETTER I WITH GRAVE
	0x00c4, //#LATIN CAPITAL LETTER A WITH DIAERESIS
	0x00c5, //#LATIN CAPITAL LETTER A WITH RING ABOVE
	0x00c9, //#LATIN CAPITAL LETTER E WITH ACUTE
	0x00e6, //#LATIN SMALL LIGATURE AE
	0x00c6, //#LATIN CAPITAL LIGATURE AE
	0x00f4, //#LATIN SMALL LETTER O WITH CIRCUMFLEX
	0x00f6, //#LATIN SMALL LETTER O WITH DIAERESIS
	0x00f2, //#LATIN SMALL LETTER O WITH GRAVE
	0x00fb, //#LATIN SMALL LETTER U WITH CIRCUMFLEX
	0x00f9, //#LATIN SMALL LETTER U WITH GRAVE
	0x00ff, //#LATIN SMALL LETTER Y WITH DIAERESIS
	0x00d6, //#LATIN CAPITAL LETTER O WITH DIAERESIS
	0x00dc, //#LATIN CAPITAL LETTER U WITH DIAERESIS
	0x00a2, //#CENT SIGN
	0x00a3, //#POUND SIGN
	0x00a5, //#YEN SIGN
	0x20a7, //#PESETA SIGN
	0x0192, //#LATIN SMALL LETTER F WITH HOOK
	0x00e1, //#LATIN SMALL LETTER A WITH ACUTE
	0x00ed, //#LATIN SMALL LETTER I WITH ACUTE
	0x00f3, //#LATIN SMALL LETTER O WITH ACUTE
	0x00fa, //#LATIN SMALL LETTER U WITH ACUTE
	0x00f1, //#LATIN SMALL LETTER N WITH TILDE
	0x00d1, //#LATIN CAPITAL LETTER N WITH TILDE
	0x00aa, //#FEMININE ORDINAL INDICATOR
	0x00ba, //#MASCULINE ORDINAL INDICATOR
	0x00bf, //#INVERTED QUESTION MARK
	0x2310, //#REVERSED NOT SIGN
	0x00ac, //#NOT SIGN
	0x00bd, //#VULGAR FRACTION ONE HALF
	0x00bc, //#VULGAR FRACTION ONE QUARTER
	0x00a1, //#INVERTED EXCLAMATION MARK
	0x00ab, //#LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	0x00bb, //#RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
	0x2591, //#LIGHT SHADE
	0x2592, //#MEDIUM SHADE
	0x2593, //#DARK SHADE
	0x2502, //#BOX DRAWINGS LIGHT VERTICAL
	0x2524, //#BOX DRAWINGS LIGHT VERTICAL AND LEFT
	0x2561, //#BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
	0x2562, //#BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
	0x2556, //#BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
	0x2555, //#BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
	0x2563, //#BOX DRAWINGS DOUBLE VERTICAL AND LEFT
	0x2551, //#BOX DRAWINGS DOUBLE VERTICAL
	0x2557, //#BOX DRAWINGS DOUBLE DOWN AND LEFT
	0x255d, //#BOX DRAWINGS DOUBLE UP AND LEFT
	0x255c, //#BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
	0x255b, //#BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
	0x2510, //#BOX DRAWINGS LIGHT DOWN AND LEFT
	0x2514, //#BOX DRAWINGS LIGHT UP AND RIGHT
	0x2534, //#BOX DRAWINGS LIGHT UP AND HORIZONTAL
	0x252c, //#BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
	0x251c, //#BOX DRAWINGS LIGHT VERTICAL AND RIGHT
	0x2500, //#BOX DRAWINGS LIGHT HORIZONTAL
	0x253c, //#BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
	0x255e, //#BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
	0x255f, //#BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
	0x255a, //#BOX DRAWINGS DOUBLE UP AND RIGHT
	0x2554, //#BOX DRAWINGS DOUBLE DOWN AND RIGHT
	0x2569, //#BOX DRAWINGS DOUBLE UP AND HORIZONTAL
	0x2566, //#BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
	0x2560, //#BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
	0x2550, //#BOX DRAWINGS DOUBLE HORIZONTAL
	0x256c, //#BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
	0x2567, //#BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
	0x2568, //#BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
	0x2564, //#BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
	0x2565, //#BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
	0x2559, //#BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
	0x2558, //#BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
	0x2552, //#BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
	0x2553, //#BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
	0x256b, //#BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
	0x256a, //#BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
	0x2518, //#BOX DRAWINGS LIGHT UP AND LEFT
	0x250c, //#BOX DRAWINGS LIGHT DOWN AND RIGHT
	0x2588, //#FULL BLOCK
	0x2584, //#LOWER HALF BLOCK
	0x258c, //#LEFT HALF BLOCK
	0x2590, //#RIGHT HALF BLOCK
	0x2580, //#UPPER HALF BLOCK
	0x03b1, //#GREEK SMALL LETTER ALPHA
	0x00df, //#LATIN SMALL LETTER SHARP S
	0x0393, //#GREEK CAPITAL LETTER GAMMA
	0x03c0, //#GREEK SMALL LETTER PI
	0x03a3, //#GREEK CAPITAL LETTER SIGMA
	0x03c3, //#GREEK SMALL LETTER SIGMA
	0x00b5, //#MICRO SIGN
	0x03c4, //#GREEK SMALL LETTER TAU
	0x03a6, //#GREEK CAPITAL LETTER PHI
	0x0398, //#GREEK CAPITAL LETTER THETA
	0x03a9, //#GREEK CAPITAL LETTER OMEGA
	0x03b4, //#GREEK SMALL LETTER DELTA
	0x221e, //#INFINITY
	0x03c6, //#GREEK SMALL LETTER PHI
	0x03b5, //#GREEK SMALL LETTER EPSILON
	0x2229, //#INTERSECTION
	0x2261, //#IDENTICAL TO
	0x00b1, //#PLUS-MINUS SIGN
	0x2265, //#GREATER-THAN OR EQUAL TO
	0x2264, //#LESS-THAN OR EQUAL TO
	0x2320, //#TOP HALF INTEGRAL
	0x2321, //#BOTTOM HALF INTEGRAL
	0x00f7, //#DIVISION SIGN
	0x2248, //#ALMOST EQUAL TO
	0x00b0, //#DEGREE SIGN
	0x2219, //#BULLET OPERATOR
	0x00b7, //#MIDDLE DOT
	0x221a, //#SQUARE ROOT
	0x207f, //#SUPERSCRIPT LATIN SMALL LETTER N
	0x00b2, //#SUPERSCRIPT TWO
	0x25a0, //#BLACK SQUARE
	0x00a0, //#NO-BREAK SPACE
};

BitmapInfo* StartupBitmap;

// Hexen startup screen
#define ST_MAX_NOTCHES			32
#define ST_NOTCH_WIDTH			16
#define ST_NOTCH_HEIGHT			23
#define ST_PROGRESS_X			64			// Start of notches x screen pos.
#define ST_PROGRESS_Y			441			// Start of notches y screen pos.

#define ST_NETPROGRESS_X		288
#define ST_NETPROGRESS_Y		32
#define ST_NETNOTCH_WIDTH		4
#define ST_NETNOTCH_HEIGHT		16
#define ST_MAX_NETNOTCHES		8

// Heretic startup screen
#define HERETIC_MINOR_VERSION	'3'			// Since we're based on Heretic 1.3

#define THERM_X					14
#define THERM_Y					14
#define THERM_LEN				51
#define THERM_COLOR				0xAA		// light green

#define TEXT_FONT_NAME			"vga-rom-font.16"

// Strife startup screen
#define PEASANT_INDEX			0
#define LASER_INDEX				4
#define BOT_INDEX				6

#define ST_LASERSPACE_X			60
#define ST_LASERSPACE_Y			156
#define ST_LASERSPACE_WIDTH		200
#define ST_LASER_WIDTH			16
#define ST_LASER_HEIGHT			16

#define ST_BOT_X				14
#define ST_BOT_Y				138
#define ST_BOT_WIDTH			48
#define ST_BOT_HEIGHT			48

#define ST_PEASANT_X			262
#define ST_PEASANT_Y			136
#define ST_PEASANT_WIDTH		32
#define ST_PEASANT_HEIGHT		64

// Text mode color values
#define LO						 85
#define MD						170
#define HI						255

static const RgbQuad TextModePalette[16] =
{
	{  0,  0,  0, 0 },		// 0 black
	{ MD,  0,  0, 0 },		// 1 blue
	{  0, MD,  0, 0 },		// 2 green
	{ MD, MD,  0, 0 },		// 3 cyan
	{  0,  0, MD, 0 },		// 4 red
	{ MD,  0, MD, 0 },		// 5 magenta
	{  0, LO, MD, 0 },		// 6 brown
	{ MD, MD, MD, 0 },		// 7 light gray

	{ LO, LO, LO, 0 },		// 8 dark gray
	{ HI, LO, LO, 0 },		// 9 light blue
	{ LO, HI, LO, 0 },		// A light green
	{ HI, HI, LO, 0 },		// B light cyan
	{ LO, LO, HI, 0 },		// C light red
	{ HI, LO, HI, 0 },		// D light magenta
	{ LO, HI, HI, 0 },		// E yellow
	{ HI, HI, HI, 0 },		// F white
};

static const char* StrifeStartupPicNames[4 + 2 + 1] =
{
	"STRTPA1", "STRTPB1", "STRTPC1", "STRTPD1",
	"STRTLZ1", "STRTLZ2",
	"STRTBOT"
};
static const int StrifeStartupPicSizes[4 + 2 + 1] =
{
	2048, 2048, 2048, 2048,
	256, 256,
	2304
};


//==========================================================================
//
// FHexenStartupScreen Constructor
//
// Shows the Hexen startup screen. If the screen doesn't appear to be
// valid, it sets hr for a failure.
//
// The startup graphic is a planar, 4-bit 640x480 graphic preceded by a
// 16 entry (48 byte) VGA palette.
//
//==========================================================================

FHexenStartupScreen::FHexenStartupScreen(int max_progress, long& hr)
	: FGraphicalStartupScreen(max_progress)
{
	int startup_lump = Wads.CheckNumForName("STARTUP");
	int netnotch_lump = Wads.CheckNumForName("NETNOTCH");
	int notch_lump = Wads.CheckNumForName("NOTCH");
	hr = -1;

	if (startup_lump < 0 || Wads.LumpLength(startup_lump) != 153648 || !ST_Util_CreateStartupWindow() ||
		netnotch_lump < 0 || Wads.LumpLength(netnotch_lump) != ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT ||
		notch_lump < 0 || Wads.LumpLength(notch_lump) != ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT)
	{
		NetNotchBits = NotchBits = NULL;
		return;
	}

	NetNotchBits = new uint8_t[ST_NETNOTCH_WIDTH / 2 * ST_NETNOTCH_HEIGHT];
	Wads.ReadLump(netnotch_lump, NetNotchBits);
	NotchBits = new uint8_t[ST_NOTCH_WIDTH / 2 * ST_NOTCH_HEIGHT];
	Wads.ReadLump(notch_lump, NotchBits);

	uint8_t startup_screen[153648];
	union
	{
		RgbQuad color;
		uint32_t	quad;
	} c;

	Wads.ReadLump(startup_lump, startup_screen);

	c.color.rgbReserved = 0;

	StartupBitmap = ST_Util_CreateBitmap(640, 480, 4);

	// Initialize the bitmap palette.
	for (int i = 0; i < 16; ++i)
	{
		c.color.rgbRed = startup_screen[i * 3 + 0];
		c.color.rgbGreen = startup_screen[i * 3 + 1];
		c.color.rgbBlue = startup_screen[i * 3 + 2];
		// Convert from 6-bit per component to 8-bit per component.
		c.quad = (c.quad << 2) | ((c.quad >> 4) & 0x03030303);
		StartupBitmap->bmiColors[i] = c.color;
	}

	// Fill in the bitmap data. Convert to chunky, because I can't figure out
	// if Windows actually supports planar images or not, despite the presence
	// of biPlanes in the BITMAPINFOHEADER.
	ST_Util_PlanarToChunky4(ST_Util_BitsForBitmap(StartupBitmap), startup_screen + 48, 640, 480);


	if (!batchrun)
	{
		if (DoomStartupInfo.Song.IsNotEmpty())
		{
			S_ChangeMusic(DoomStartupInfo.Song.GetChars(), true, true);
		}
		else
		{
			S_ChangeMusic("orb", true, true);
		}
	}
	SetWindowSize();
	hr = 0;
}

//==========================================================================
//
// FHexenStartupScreen Deconstructor
//
// Frees the notch pictures.
//
//==========================================================================

FHexenStartupScreen::~FHexenStartupScreen()
{
	if (NotchBits)
		delete[] NotchBits;
	if (NetNotchBits)
		delete[] NetNotchBits;
}

//==========================================================================
//
// FHexenStartupScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FHexenStartupScreen::Progress()
{
	int notch_pos, x, y;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * ST_MAX_NOTCHES) / MaxPos;
		if (notch_pos != NotchPos)
		{ // Time to draw another notch.
			for (; NotchPos < notch_pos; NotchPos++)
			{
				x = ST_PROGRESS_X + ST_NOTCH_WIDTH * NotchPos;
				y = ST_PROGRESS_Y;
				ST_Util_DrawBlock(StartupBitmap, NotchBits, x, y, ST_NOTCH_WIDTH / 2, ST_NOTCH_HEIGHT);
			}
			S_Sound(CHAN_BODY, 0, "StartupTick", 1, ATTN_NONE);
		}
	}
	I_GetEvent();
}

//==========================================================================
//
// FHexenStartupScreen :: NetProgress
//
// Draws the red net noches in addition to the normal progress bar.
//
//==========================================================================

void FHexenStartupScreen::NetProgress(int count)
{
	int oldpos = NetCurPos;
	int x, y;

	FGraphicalStartupScreen::NetProgress(count);
	if (NetMaxPos != 0 && NetCurPos > oldpos)
	{
		for (; oldpos < NetCurPos && oldpos < ST_MAX_NETNOTCHES; ++oldpos)
		{
			x = ST_NETPROGRESS_X + ST_NETNOTCH_WIDTH * oldpos;
			y = ST_NETPROGRESS_Y;
			ST_Util_DrawBlock(StartupBitmap, NetNotchBits, x, y, ST_NETNOTCH_WIDTH / 2, ST_NETNOTCH_HEIGHT);
		}
		S_Sound(CHAN_BODY, 0, "misc/netnotch", 1, ATTN_NONE);
		I_GetEvent();
	}
}

//==========================================================================
//
// FHexenStartupScreen :: NetDone
//
// Aside from the standard processing, also plays a sound.
//
//==========================================================================

void FHexenStartupScreen::NetDone()
{
	S_Sound(CHAN_BODY, 0, "PickupWeapon", 1, ATTN_NORM);
	FGraphicalStartupScreen::NetDone();
}


//==========================================================================
//
// FHereticStartupScreen Constructor
//
// Shows the Heretic startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The loading screen is an 80x25 text screen with character data and
// attributes intermixed, which means it must be exactly 4000 bytes long.
//
//==========================================================================

FHereticStartupScreen::FHereticStartupScreen(int max_progress, long& hr)
	: FGraphicalStartupScreen(max_progress)
{
	int loading_lump = Wads.CheckNumForName("LOADING");
	uint8_t loading_screen[4000];
	uint8_t* font;

	hr = -1;
	if (loading_lump < 0 || Wads.LumpLength(loading_lump) != 4000 || !ST_Util_CreateStartupWindow())
	{
		return;
	}

	font = ST_Util_LoadFont(TEXT_FONT_NAME);
	if (font == NULL)
	{
		return;
	}

	Wads.ReadLump(loading_lump, loading_screen);

	// Slap the Heretic minor version on the loading screen. Heretic
	// did this inside the executable rather than coming with modified
	// LOADING screens, so we need to do the same.
	loading_screen[2 * 160 + 49 * 2] = HERETIC_MINOR_VERSION;

	// Draw the loading screen to a bitmap.
	StartupBitmap = ST_Util_AllocTextBitmap(font);
	ST_Util_DrawTextScreen(StartupBitmap, loading_screen, font);

	ThermX = THERM_X * 8;
	ThermY = THERM_Y * font[0];
	ThermWidth = THERM_LEN * 8 - 4;
	ThermHeight = font[0];
	HMsgY = 7;
	SMsgX = 1;

	ST_Util_FreeFont(font);
	SetWindowSize();
	hr = 0;
}

//==========================================================================
//
// FHereticStartupScreen::Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FHereticStartupScreen::Progress()
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * ThermWidth) / MaxPos;
		if (notch_pos != NotchPos && !(notch_pos & 3))
		{ // Time to draw another notch.
			int left = NotchPos + ThermX;
			int top = ThermY;
			int right = notch_pos + ThermX;
			int bottom = top + ThermHeight;
			ST_Util_ClearBlock(StartupBitmap, THERM_COLOR, left, top, right - left, bottom - top);
			NotchPos = notch_pos;
		}
	}
	I_GetEvent();
}

//==========================================================================
//
// FHereticStartupScreen :: LoadingStatus
//
// Prints text in the center box of the startup screen.
//
//==========================================================================

void FHereticStartupScreen::LoadingStatus(const char* message, int colors)
{
	uint8_t* font = ST_Util_LoadFont(TEXT_FONT_NAME);
	if (font != NULL)
	{
		int x;

		for (x = 0; message[x] != '\0'; ++x)
		{
			ST_Util_DrawChar(StartupBitmap, font, 17 + x, HMsgY, message[x], colors);
		}
		ST_Util_InvalidateRect(StartupBitmap, 17 * 8, HMsgY * font[0], (17 + x) * 8, HMsgY * font[0] + font[0]);
		ST_Util_FreeFont(font);
		HMsgY++;
		I_GetEvent();
	}
}

//==========================================================================
//
// FHereticStartupScreen :: AppendStatusLine
//
// Appends text to Heretic's status line.
//
//==========================================================================

void FHereticStartupScreen::AppendStatusLine(const char* status)
{
	uint8_t* font = ST_Util_LoadFont(TEXT_FONT_NAME);
	if (font != NULL)
	{
		int x;

		for (x = 0; status[x] != '\0'; ++x)
		{
			ST_Util_DrawChar(StartupBitmap, font, SMsgX + x, 24, status[x], 0x1f);
		}
		ST_Util_InvalidateRect(StartupBitmap, SMsgX * 8, 24 * font[0], (SMsgX + x) * 8, 25 * font[0]);
		ST_Util_FreeFont(font);
		SMsgX += x;
		I_GetEvent();
	}
}

//==========================================================================
//
// FStrifeStartupScreen Constructor
//
// Shows the Strife startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The startup background is a raw 320x200 image, however Strife only
// actually uses 95 rows from it, starting at row 57. The rest of the image
// is discarded. (What a shame.)
//
// The peasants are raw 32x64 images. The laser dots are raw 16x16 images.
// The bot is a raw 48x48 image. All use the standard PLAYPAL.
//
//==========================================================================

FStrifeStartupScreen::FStrifeStartupScreen(int max_progress, long& hr)
	: FGraphicalStartupScreen(max_progress)
{
	int startup_lump = Wads.CheckNumForName("STARTUP0");
	int i;

	hr = -1;
	for (i = 0; i < 4 + 2 + 1; ++i)
	{
		StartupPics[i] = NULL;
	}

	if (startup_lump < 0 || Wads.LumpLength(startup_lump) != 64000 || !ST_Util_CreateStartupWindow())
	{
		return;
	}

	StartupBitmap = ST_Util_CreateBitmap(320, 200, 8);
	ST_Util_BitmapColorsFromPlaypal(StartupBitmap);

	// Fill bitmap with the startup image.
	memset(ST_Util_BitsForBitmap(StartupBitmap), 0xF0, 64000);
	auto lumpr = Wads.OpenLumpReader(startup_lump);
	lumpr.Seek(57 * 320, FileReader::SeekSet);
	lumpr.Read(ST_Util_BitsForBitmap(StartupBitmap) + 41 * 320, 95 * 320);

	// Load the animated overlays.
	for (i = 0; i < 4 + 2 + 1; ++i)
	{
		int lumpnum = Wads.CheckNumForName(StrifeStartupPicNames[i]);
		int lumplen;

		if (lumpnum >= 0 && (lumplen = Wads.LumpLength(lumpnum)) == StrifeStartupPicSizes[i])
		{
			auto lumpr = Wads.OpenLumpReader(lumpnum);
			StartupPics[i] = new uint8_t[lumplen];
			lumpr.Read(StartupPics[i], lumplen);
		}
	}

	// Make the startup image appear.
	DrawStuff(0, 0);
	SetWindowSize();
	hr = 0;
}

//==========================================================================
//
// FStrifeStartupScreen Destructor
//
// Frees the strife pictures.
//
//==========================================================================

FStrifeStartupScreen::~FStrifeStartupScreen()
{
	for (int i = 0; i < 4 + 2 + 1; ++i)
	{
		if (StartupPics[i] != NULL)
		{
			delete[] StartupPics[i];
		}
		StartupPics[i] = NULL;
	}
}

//==========================================================================
//
// FStrifeStartupScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

void FStrifeStartupScreen::Progress()
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		CurPos++;
		notch_pos = (CurPos * (ST_LASERSPACE_WIDTH - ST_LASER_WIDTH)) / MaxPos;
		if (notch_pos != NotchPos && !(notch_pos & 1))
		{ // Time to update.
			DrawStuff(NotchPos, notch_pos);
			NotchPos = notch_pos;
		}
	}
	I_GetEvent();
}

//==========================================================================
//
// FStrifeStartupScreen :: DrawStuff
//
// Draws all the moving parts of Strife's startup screen. If you're
// running off a slow drive, it can look kind of good. Otherwise, it
// borders on crazy insane fast.
//
//==========================================================================

void FStrifeStartupScreen::DrawStuff(int old_laser, int new_laser)
{
	int y;
	auto bitmap_info = StartupBitmap;

	// Clear old laser
	ST_Util_ClearBlock(bitmap_info, 0xF0, ST_LASERSPACE_X + old_laser,
		ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);
	// Draw new laser
	ST_Util_DrawBlock(bitmap_info, StartupPics[LASER_INDEX + (new_laser & 1)],
		ST_LASERSPACE_X + new_laser, ST_LASERSPACE_Y, ST_LASER_WIDTH, ST_LASER_HEIGHT);

	// The bot jumps up and down like crazy.
	y = std::max(0, (new_laser >> 1) % 5 - 2);
	if (y > 0)
	{
		ST_Util_ClearBlock(bitmap_info, 0xF0, ST_BOT_X, ST_BOT_Y, ST_BOT_WIDTH, y);
	}
	ST_Util_DrawBlock(bitmap_info, StartupPics[BOT_INDEX], ST_BOT_X, ST_BOT_Y + y, ST_BOT_WIDTH, ST_BOT_HEIGHT);
	if (y < (5 - 1) - 2)
	{
		ST_Util_ClearBlock(bitmap_info, 0xF0, ST_BOT_X, ST_BOT_Y + ST_BOT_HEIGHT + y, ST_BOT_WIDTH, 2 - y);
	}

	// The peasant desperately runs in place, trying to get away from the laser.
	// Yet, despite all his limb flailing, he never manages to get anywhere.
	ST_Util_DrawBlock(bitmap_info, StartupPics[PEASANT_INDEX + ((new_laser >> 1) & 3)],
		ST_PEASANT_X, ST_PEASANT_Y, ST_PEASANT_WIDTH, ST_PEASANT_HEIGHT);
}


//==========================================================================
//
// ST_Util_PlanarToChunky4
//
// Convert a 4-bpp planar image to chunky pixels.
//
//==========================================================================

void ST_Util_PlanarToChunky4(uint8_t* dest, const uint8_t* src, int width, int height)
{
	int y, x;
	const uint8_t* src1, * src2, * src3, * src4;
	size_t plane_size = width / 8 * height;

	src1 = src;
	src2 = src1 + plane_size;
	src3 = src2 + plane_size;
	src4 = src3 + plane_size;

	for (y = height; y > 0; --y)
	{
		for (x = width; x > 0; x -= 8)
		{
			// Pixels 0 and 1
			dest[0] = (*src4 & 0x80) | ((*src3 & 0x80) >> 1) | ((*src2 & 0x80) >> 2) | ((*src1 & 0x80) >> 3) |
				((*src4 & 0x40) >> 3) | ((*src3 & 0x40) >> 4) | ((*src2 & 0x40) >> 5) | ((*src1 & 0x40) >> 6);
			// Pixels 2 and 3
			dest[1] = ((*src4 & 0x20) << 2) | ((*src3 & 0x20) << 1) | ((*src2 & 0x20)) | ((*src1 & 0x20) >> 1) |
				((*src4 & 0x10) >> 1) | ((*src3 & 0x10) >> 2) | ((*src2 & 0x10) >> 3) | ((*src1 & 0x10) >> 4);
			// Pixels 4 and 5
			dest[2] = ((*src4 & 0x08) << 4) | ((*src3 & 0x08) << 3) | ((*src2 & 0x08) << 2) | ((*src1 & 0x08) << 1) |
				((*src4 & 0x04) << 1) | ((*src3 & 0x04)) | ((*src2 & 0x04) >> 1) | ((*src1 & 0x04) >> 2);
			// Pixels 6 and 7
			dest[3] = ((*src4 & 0x02) << 6) | ((*src3 & 0x02) << 5) | ((*src2 & 0x02) << 4) | ((*src1 & 0x02) << 3) |
				((*src4 & 0x01) << 3) | ((*src3 & 0x01) << 2) | ((*src2 & 0x01) << 1) | ((*src1 & 0x01));
			dest += 4;
			src1 += 1;
			src2 += 1;
			src3 += 1;
			src4 += 1;
		}
	}
}

//==========================================================================
//
// ST_Util_DrawBlock
//
//==========================================================================

void ST_Util_DrawBlock(BitmapInfo* bitmap_info, const uint8_t* src, int x, int y, int bytewidth, int height)
{
	if (src == NULL)
	{
		return;
	}

	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	uint8_t* dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

	ST_Util_InvalidateRect(bitmap_info, x, y, x + (bytewidth << pitchshift), y + height);

	if (bytewidth == 8)
	{ // progress notches
		for (; height > 0; --height)
		{
			((uint32_t*)dest)[0] = ((const uint32_t*)src)[0];
			((uint32_t*)dest)[1] = ((const uint32_t*)src)[1];
			dest += destpitch;
			src += 8;
		}
	}
	else if (bytewidth == 2)
	{ // net progress notches
		for (; height > 0; --height)
		{
			*((uint16_t*)dest) = *((const uint16_t*)src);
			dest += destpitch;
			src += 2;
		}
	}
	else
	{
		for (; height > 0; --height)
		{
			memcpy(dest, src, bytewidth);
			dest += destpitch;
			src += bytewidth;
		}
	}
}

//==========================================================================
//
// ST_Util_ClearBlock
//
//==========================================================================

void ST_Util_ClearBlock(BitmapInfo* bitmap_info, uint8_t fill, int x, int y, int bytewidth, int height)
{
	int pitchshift = int(bitmap_info->bmiHeader.biBitCount == 4);
	int destpitch = bitmap_info->bmiHeader.biWidth >> pitchshift;
	uint8_t* dest = ST_Util_BitsForBitmap(bitmap_info) + (x >> pitchshift) + y * destpitch;

	ST_Util_InvalidateRect(bitmap_info, x, y, x + (bytewidth << pitchshift), y + height);

	while (height > 0)
	{
		memset(dest, fill, bytewidth);
		dest += destpitch;
		height--;
	}
}

//==========================================================================
//
// ST_Util_CreateBitmap
//
// Creates a BitmapInfoHeader, RgbQuad, and pixel data arranged
// consecutively in memory (in other words, a normal Windows BMP file).
// The BitmapInfoHeader will be filled in, and the caller must fill
// in the color and pixel data.
//
// You must pass 4 or 8 for color_bits.
//
//==========================================================================

BitmapInfo* ST_Util_CreateBitmap(int width, int height, int color_bits)
{
	uint32_t size_image = (width * height) >> int(color_bits == 4);
	BitmapInfo* bitmap_info = (BitmapInfo*)M_Malloc(sizeof(BitmapInfoHeader) +
		(sizeof(RgbQuad) << color_bits) + size_image);

	// Initialize the header.
	bitmap_info->bmiHeader.biSize = sizeof(BitmapInfoHeader);
	bitmap_info->bmiHeader.biWidth = width;
	bitmap_info->bmiHeader.biHeight = height;
	bitmap_info->bmiHeader.biPlanes = 1;
	bitmap_info->bmiHeader.biBitCount = color_bits;
	bitmap_info->bmiHeader.biCompression = 0;
	bitmap_info->bmiHeader.biSizeImage = size_image;
	bitmap_info->bmiHeader.biXPelsPerMeter = 0;
	bitmap_info->bmiHeader.biYPelsPerMeter = 0;
	bitmap_info->bmiHeader.biClrUsed = 1 << color_bits;
	bitmap_info->bmiHeader.biClrImportant = 0;

	return bitmap_info;
}

//==========================================================================
//
// ST_Util_BitsForBitmap
//
// Given a bitmap created by ST_Util_CreateBitmap, returns the start
// address for the pixel data for the bitmap.
//
//==========================================================================

uint8_t* ST_Util_BitsForBitmap(BitmapInfo* bitmap_info)
{
	return (uint8_t*)bitmap_info + sizeof(BitmapInfoHeader) + (sizeof(RgbQuad) << bitmap_info->bmiHeader.biBitCount);
}

//==========================================================================
//
// ST_Util_FreeBitmap
//
// Frees all the data for a bitmap created by ST_Util_CreateBitmap.
//
//==========================================================================

void ST_Util_FreeBitmap(BitmapInfo* bitmap_info)
{
	M_Free(bitmap_info);
}

//==========================================================================
//
// ST_Util_BitmapColorsFromPlaypal
//
// Fills the bitmap palette from the PLAYPAL lump.
//
//==========================================================================

void ST_Util_BitmapColorsFromPlaypal(BitmapInfo* bitmap_info)
{
	uint8_t playpal[768];

	ReadPalette(Wads.CheckNumForName("PLAYPAL"), playpal);
	for (int i = 0; i < 256; ++i)
	{
		bitmap_info->bmiColors[i].rgbBlue = playpal[i * 3 + 2];
		bitmap_info->bmiColors[i].rgbGreen = playpal[i * 3 + 1];
		bitmap_info->bmiColors[i].rgbRed = playpal[i * 3];
		bitmap_info->bmiColors[i].rgbReserved = 0;
	}
}

//==========================================================================
//
// ST_Util_LoadFont
//
// Loads a monochrome fixed-width font. Every character is one byte
// (eight pixels) wide, so we can deduce the height of each character
// by looking at the size of the font data.
//
//==========================================================================

uint8_t* ST_Util_LoadFont(const char* filename)
{
	int lumpnum, lumplen, height;
	uint8_t* font;

	lumpnum = Wads.CheckNumForFullName(filename);
	if (lumpnum < 0)
	{ // font not found
		return NULL;
	}
	lumplen = Wads.LumpLength(lumpnum);
	height = lumplen / 256;
	if (height * 256 != lumplen)
	{ // font is a bad size
		return NULL;
	}
	if (height < 6 || height > 36)
	{ // let's be reasonable here
		return NULL;
	}
	font = new uint8_t[lumplen + 1];
	font[0] = height;	// Store font height in the first byte.
	Wads.ReadLump(lumpnum, font + 1);
	return font;
}

void ST_Util_FreeFont(uint8_t* font)
{
	delete[] font;
}

//==========================================================================
//
// ST_Util_AllocTextBitmap
//
// Returns a bitmap properly sized to hold an 80x25 display of characters
// using the specified font.
//
//==========================================================================

BitmapInfo* ST_Util_AllocTextBitmap(const uint8_t* font)
{
	BitmapInfo* bitmap = ST_Util_CreateBitmap(80 * 8, 25 * font[0], 4);
	memcpy(bitmap->bmiColors, TextModePalette, sizeof(TextModePalette));
	return bitmap;
}

//==========================================================================
//
// ST_Util_DrawTextScreen
//
// Draws the text screen to the bitmap. The bitmap must be the proper size
// for the font.
//
//==========================================================================

void ST_Util_DrawTextScreen(BitmapInfo* bitmap_info, const uint8_t* text_screen, const uint8_t* font)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			ST_Util_DrawChar(bitmap_info, font, x, y, text_screen[0], text_screen[1]);
			text_screen += 2;
		}
	}
}

//==========================================================================
//
// ST_Util_DrawChar
//
// Draws a character on the bitmap. X and Y specify the character cell,
// and fg and bg are 4-bit colors.
//
//==========================================================================

void ST_Util_DrawChar(BitmapInfo* screen, const uint8_t* font, int x, int y, uint8_t charnum, uint8_t attrib)
{
	const uint8_t bg_left = attrib & 0x70;
	const uint8_t fg = attrib & 0x0F;
	const uint8_t fg_left = fg << 4;
	const uint8_t bg = bg_left >> 4;
	const uint8_t color_array[4] = { (uint8_t)(bg_left | bg), (uint8_t)(attrib & 0x7F), (uint8_t)(fg_left | bg), (uint8_t)(fg_left | fg) };
	const uint8_t* src = font + 1 + charnum * font[0];
	int pitch = screen->bmiHeader.biWidth >> 1;
	uint8_t* dest = ST_Util_BitsForBitmap(screen) + x * 4 + y * font[0] * pitch;

	for (y = font[0]; y > 0; --y)
	{
		uint8_t srcbyte = *src++;

		// Pixels 0 and 1
		dest[0] = color_array[(srcbyte >> 6) & 3];
		// Pixels 2 and 3
		dest[1] = color_array[(srcbyte >> 4) & 3];
		// Pixels 4 and 5
		dest[2] = color_array[(srcbyte >> 2) & 3];
		// Pixels 6 and 7
		dest[3] = color_array[(srcbyte) & 3];
		dest += pitch;
	}
}

//==========================================================================
//
// ST_Util_UpdateTextBlink
//
// Draws the parts of the text screen that blink to the bitmap. The bitmap
// must be the proper size for the font.
//
//==========================================================================

void ST_Util_UpdateTextBlink(BitmapInfo* bitmap_info, const uint8_t* text_screen, const uint8_t* font, bool on)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			if (text_screen[1] & 0x80)
			{
				ST_Util_DrawChar(bitmap_info, font, x, y, on ? text_screen[0] : ' ', text_screen[1]);
				ST_Util_InvalidateRect(bitmap_info, x * 8, y * font[0], x * 8 + 8, y * font[0] + font[0]);
			}
			text_screen += 2;
		}
	}
}
