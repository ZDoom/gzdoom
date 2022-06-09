/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
** Copyright 2006-2022 Christoph Oelckers
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

#include "startscreen.h"
#include "filesystem.h"
#include "palutil.h"
#include "v_font.h"
#include "i_interface.h"
#include "startupinfo.h"
#include "m_argv.h"
#include "engineerrors.h"
#include "utf8.h"
#include "gstrings.h"
#include "printf.h"
#include "i_time.h"
#include "v_video.h"
#include "v_draw.h"
#include "g_input.h"
#include "texturemanager.h"
#include "gi.h"

// Text mode color values
enum{
	LO = 85,
	MD = 170,
	HI = 255,
};

const RgbQuad TextModePalette[16] =
{
	{  0,  0,  0, 255 },		// 0 black
	{ MD,  0,  0, 255 },		// 1 blue
	{  0, MD,  0, 255 },		// 2 green
	{ MD, MD,  0, 255 },		// 3 cyan
	{  0,  0, MD, 255 },		// 4 red
	{ MD,  0, MD, 255 },		// 5 magenta
	{  0, LO, MD, 255 },		// 6 brown
	{ MD, MD, MD, 255 },		// 7 light gray

	{ LO, LO, LO, 255 },		// 8 dark gray
	{ HI, LO, LO, 255 },		// 9 light blue
	{ LO, HI, LO, 255 },		// A light green
	{ HI, HI, LO, 255 },		// B light cyan
	{ LO, LO, HI, 255 },		// C light red
	{ HI, LO, HI, 255 },		// D light magenta
	{ LO, HI, HI, 255 },		// E yellow
	{ HI, HI, HI, 255 },		// F white
};

static const uint16_t IBM437ToUnicode[] = {
	0x0000, //#NULL
	0x263a, //#START OF HEADING
	0x263B, //#START OF TEXT
	0x2665, //#END OF TEXT
	0x2666, //#END OF TRANSMISSION
	0x2663, //#ENQUIRY
	0x2660, //#ACKNOWLEDGE
	0x2022, //#BELL
	0x25d8, //#BACKSPACE
	0x25cb, //#HORIZONTAL TABULATION
	0x25d9, //#LINE FEED
	0x2642, //#VERTICAL TABULATION
	0x2640, //#FORM FEED
	0x266a, //#CARRIAGE RETURN
	0x266b, //#SHIFT OUT
	0x263c, //#SHIFT IN
	0x25ba, //#DATA LINK ESCAPE
	0x25c4, //#DEVICE CONTROL ONE
	0x2195, //#DEVICE CONTROL TWO
	0x203c, //#DEVICE CONTROL THREE
	0x00b6, //#DEVICE CONTROL FOUR
	0x00a7, //#NEGATIVE ACKNOWLEDGE
	0x25ac, //#SYNCHRONOUS IDLE
	0x21ab, //#END OF TRANSMISSION BLOCK
	0x2191, //#CANCEL
	0x2193, //#END OF MEDIUM
	0x2192, //#SUBSTITUTE
	0x2190, //#ESCAPE
	0x221f, //#FILE SEPARATOR
	0x2194, //#GROUP SEPARATOR
	0x25b2, //#RECORD SEPARATOR
	0x25bc, //#UNIT SEPARATOR
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
	0x2302, //#DELETE
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

FStartScreen* CreateHexenStartScreen(int max_progress);
FStartScreen* CreateHereticStartScreen(int max_progress);
FStartScreen* CreateStrifeStartScreen(int max_progress);
FStartScreen* CreateGenericStartScreen(int max_progress);


FStartScreen* GetGameStartScreen(int max_progress)
{
	if (!Args->CheckParm("-nostartup"))
	{
		try
		{
			if (GameStartupInfo.Type == FStartupInfo::HexenStartup)
			{
				return CreateHexenStartScreen(max_progress);
			}
			else if (GameStartupInfo.Type == FStartupInfo::HereticStartup)
			{
				return CreateHereticStartScreen(max_progress);
			}
			else if (GameStartupInfo.Type == FStartupInfo::StrifeStartup)
			{
				return CreateStrifeStartScreen(max_progress);
			}
		}
		catch(const CRecoverableError& err)
		{
			Printf("Error creating start screen: %s\n", err.what());
			// fall through to the generic startup screen
		}
		//return CreateGenericStartScreen(max_progress);
	}
	return nullptr;
}

//==========================================================================
//
// ST_Util_ClearBlock
//
//==========================================================================

void FStartScreen::ClearBlock(FBitmap& bitmap_info, RgbQuad fill, int x, int y, int bytewidth, int height)
{
	int destpitch = bitmap_info.GetWidth();
	auto dest = (RgbQuad*)(bitmap_info.GetPixels()) + x + y * destpitch;

	while (height > 0)
	{
		for(int i = 0; i < bytewidth; i++)
		{
			dest[i] = fill;
		}
		dest += destpitch;
		height--;
	}
}

//==========================================================================
//
// ST_Util_DrawTextScreen
//
// Draws the text screen to the bitmap. The bitmap must be the proper size
// for the font.
//
//==========================================================================

void FStartScreen::DrawTextScreen(FBitmap& bitmap_info, const uint8_t* text_screen)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			DrawChar(bitmap_info, x, y, IBM437ToUnicode[text_screen[0]], text_screen[1]);
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
uint8_t* GetHexChar(int codepoint);

int FStartScreen::DrawChar(FBitmap& screen, double x, double y, unsigned charnum, RgbQuad fg, RgbQuad bg)
{
	if (x < 0 || y < 0 || y >= screen.GetHeight() - 16) return 1;
	static const uint8_t space[17] = { 16 };
	const RgbQuad color_array[4] = { bg, fg };
	const uint8_t* src = GetHexChar(charnum);
	if (!src) src = space;
	int size = (*src++) == 32? 2 : 1;
	if (x > (screen.GetWidth() >> 3) - size) return size;
	int pitch = screen.GetWidth();
	RgbQuad* dest = (RgbQuad*)screen.GetPixels() + int(x * 8) + int(y * 16) * pitch;

	for (y = 0; y <16; ++y)
	{
		uint8_t srcbyte = *src++;

		dest[0] = color_array[(srcbyte >> 7) & 1];
		dest[1] = color_array[(srcbyte >> 6) & 1];
		dest[2] = color_array[(srcbyte >> 5) & 1];
		dest[3] = color_array[(srcbyte >> 4) & 1];
		dest[4] = color_array[(srcbyte >> 3) & 1];
		dest[5] = color_array[(srcbyte >> 2) & 1];
		dest[6] = color_array[(srcbyte >> 1) & 1];
		dest[7] = color_array[(srcbyte) & 1];
		if (size == 16)
		{
			srcbyte = *src++;

			dest[8] = color_array[(srcbyte >> 7) & 1];
			dest[9] = color_array[(srcbyte >> 6) & 1];
			dest[10] = color_array[(srcbyte >> 5) & 1];
			dest[11] = color_array[(srcbyte >> 4) & 1];
			dest[12] = color_array[(srcbyte >> 3) & 1];
			dest[13] = color_array[(srcbyte >> 2) & 1];
			dest[14] = color_array[(srcbyte >> 1) & 1];
			dest[15] = color_array[(srcbyte) & 1];
		}
		dest += pitch;
	}
	return size;
}

int FStartScreen::DrawChar(FBitmap& screen, double x, double y, unsigned charnum, uint8_t attrib)
{
	const uint8_t bg = (attrib & 0x70) >> 4;
	const uint8_t fg = attrib & 0x0F;
	auto bgc = TextModePalette[bg], fgc = TextModePalette[fg];
	return DrawChar(screen, x, y, charnum, fgc, bgc);
}

int FStartScreen::DrawString(FBitmap& screen, double x, double y, const char* text, RgbQuad fg, RgbQuad bg)
{
	double oldx = x;
	auto str = (const uint8_t*)text;
	while (auto chr = GetCharFromString(str))
	{
		x += DrawChar(screen, x, y, chr, fg, bg);
	}
	return int(x - oldx);
}

//==========================================================================
//
// SizeOfText
//
// returns width in pixels
//
//==========================================================================

int FStartScreen::SizeOfText(const char* text)
{
	int len = 0;
	const uint8_t* utext = (uint8_t*)text;
	while(auto code = GetCharFromString(utext))
	{
		const uint8_t* src = GetHexChar(code);
		if (src && *src == 32) len += 2;
		else len ++;
	}
	return len;
}

//==========================================================================
//
// ST_Util_UpdateTextBlink
//
// Draws the parts of the text screen that blink to the bitmap. The bitmap
// must be the proper size for the font.
//
//==========================================================================

void FStartScreen::UpdateTextBlink(FBitmap& bitmap_info, const uint8_t* text_screen, bool on)
{
	int x, y;

	for (y = 0; y < 25; ++y)
	{
		for (x = 0; x < 80; ++x)
		{
			if (text_screen[1] & 0x80)
			{
				DrawChar(bitmap_info, x, y, on ? IBM437ToUnicode[text_screen[0]] : ' ', text_screen[1]);
			}
			text_screen += 2;
		}
	}
}

//==========================================================================
//
// ST_Sound
//
// plays a sound on the start screen
//
//==========================================================================

void FStartScreen::ST_Sound(const char* sndname)
{
	if (sysCallbacks.PlayStartupSound)
		sysCallbacks.PlayStartupSound(sndname);
}

//==========================================================================
//
// CreateHeader
//
// creates a bitmap for the title header
//
//==========================================================================

void FStartScreen::CreateHeader()
{
	HeaderBitmap.Create(StartupBitmap.GetWidth() * Scale, 2 * 16);
	RgbQuad bcolor, fcolor;
	bcolor.rgbRed = RPART(GameStartupInfo.BkColor);
	bcolor.rgbGreen = GPART(GameStartupInfo.BkColor);
	bcolor.rgbBlue = BPART(GameStartupInfo.BkColor);
	bcolor.rgbReserved = 255;
	fcolor.rgbRed = RPART(GameStartupInfo.FgColor);
	fcolor.rgbGreen = GPART(GameStartupInfo.FgColor);
	fcolor.rgbBlue = BPART(GameStartupInfo.FgColor);
	fcolor.rgbReserved = 255;
	ClearBlock(HeaderBitmap, bcolor, 0, 0, HeaderBitmap.GetWidth(), HeaderBitmap.GetHeight());
	int textlen = SizeOfText(GameStartupInfo.Name);
	DrawString(HeaderBitmap, (HeaderBitmap.GetWidth() >> 4) - (textlen >> 1), 0.5, GameStartupInfo.Name, fcolor, bcolor);
	NetBitmap.Create(StartupBitmap.GetWidth() * Scale, 16);
}

//==========================================================================
//
// DrawNetStatus
//
// Draws network status into the last line of the startup screen.
//
//==========================================================================

void FStartScreen::DrawNetStatus(int found, int total)
{
	RgbQuad black = { 0, 0, 0, 255 };
	RgbQuad gray = { 100, 100, 100, 255 };
	ClearBlock(NetBitmap, black, 0, NetBitmap.GetHeight() - 16, NetBitmap.GetWidth(), 16);
	DrawString(NetBitmap, 0, 0, NetMessageString, gray, black);
	char of[10];
	mysnprintf(of, 10, "%d/%d", found, total);
	int siz = SizeOfText(of);
	DrawString(NetBitmap, (NetBitmap.GetWidth() >> 3) - siz, 0, of, gray, black);
}

//==========================================================================
//
// NetInit
//
// sets network status message
//
//==========================================================================

bool FStartScreen::NetInit(const char* message, int numplayers)
{
	NetMaxPos = numplayers;
	NetCurPos = 0;
	NetMessageString.Format("%s %s", message, GStrings("TXT_NET_PRESSESC"));
	NetProgress(1);	// You always know about yourself
	return true;
}

//==========================================================================
//
// Progress
//
// advances the progress bar
//
//==========================================================================

bool FStartScreen::DoProgress(int advance)
{
	CurPos = min(CurPos + advance, MaxPos);
	return true;
}

void FStartScreen::DoNetProgress(int count)
{
	if (count == 0)
	{
		NetCurPos++;
	}
	else if (count > 0)
	{
		NetCurPos = count;
	}
	NetTexture->CleanHardwareData();
}

bool FStartScreen::Progress(int advance)
{
	bool done = DoProgress(advance);
	Render();
	return done;
}

void FStartScreen::NetProgress(int count)
{
	DoNetProgress(count);
	Render();
}

void FStartScreen::Render(bool force)
{
	auto nowtime = I_msTime();
	// Do not refresh too often. This function gets called a lot more frequently than the screen can update.
	if (nowtime - screen->FrameTime > 30 || force)
	{
		screen->FrameTime = nowtime;
		screen->BeginFrame();
		twod->ClearClipRect();
		I_GetEvent();
		ValidateTexture();
		float displaywidth;
		float displayheight;
		twod->Begin(screen->GetWidth(), screen->GetHeight());

		// At this point the shader for untextured rendering has not been loaded yet, so we got to clear the screen by rendering a texture with black color.
		DrawTexture(twod, StartupTexture, 0, 0, DTA_VirtualWidthF, StartupTexture->GetDisplayWidth(), DTA_VirtualHeightF, StartupTexture->GetDisplayHeight(), DTA_KeepRatio, true, DTA_Color, PalEntry(255,0,0,0), TAG_END);

		if (HeaderTexture)
		{
			displaywidth = HeaderTexture->GetDisplayWidth();
			displayheight = HeaderTexture->GetDisplayHeight() + StartupTexture->GetDisplayHeight();
			DrawTexture(twod, HeaderTexture, 0, 0, DTA_VirtualWidthF, displaywidth, DTA_VirtualHeightF, displayheight, TAG_END);
			DrawTexture(twod, StartupTexture, 0, 32, DTA_VirtualWidthF, displaywidth, DTA_VirtualHeightF, displayheight, TAG_END);
			if (NetMaxPos >= 0) DrawTexture(twod, NetTexture, 0, displayheight - 16, DTA_VirtualWidthF, displaywidth, DTA_VirtualHeightF, displayheight, TAG_END);
		}
		else
		{
			displaywidth = StartupTexture->GetDisplayWidth();
			displayheight = StartupTexture->GetDisplayHeight();
			DrawTexture(twod, StartupTexture, 0, 0, DTA_VirtualWidthF, displaywidth, DTA_VirtualHeightF, displayheight, TAG_END);
		}

		twod->End();
		screen->Update();
		twod->OnFrameDone();
	}
}

FImageSource* CreateStartScreenTexture(FBitmap& srcdata);

void FStartScreen::ValidateTexture()
{
	if (StartupTexture == nullptr)
	{
		auto imgsource = CreateStartScreenTexture(StartupBitmap);
		StartupTexture = MakeGameTexture(new FImageTexture(imgsource), nullptr, ETextureType::Override);
		StartupTexture->SetScale(1.f / Scale, 1.f / Scale);
	}
	if (HeaderTexture == nullptr && HeaderBitmap.GetWidth() > 0)
	{
		auto imgsource = CreateStartScreenTexture(HeaderBitmap);
		HeaderTexture = MakeGameTexture(new FImageTexture(imgsource), nullptr, ETextureType::Override);
	}
	if (NetTexture == nullptr && NetBitmap.GetWidth() > 0)
	{
		auto imgsource = CreateStartScreenTexture(NetBitmap);
		NetTexture = MakeGameTexture(new FImageTexture(imgsource), nullptr, ETextureType::Override);
	}
}

