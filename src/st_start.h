#pragma once
/*
** st_start.h
** Interface for the startup screen.
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
** The startup screen interface is based on a mix of Heretic and Hexen.
** Actual implementation is system-specific.
*/
#include <stdint.h>

class FTexture;

// The entire set of functions here uses native Windows types. These are recreations of those types so that the code doesn't need to be changed more than necessary

struct BitmapInfoHeader
{
	uint32_t      biSize;
	int32_t       biWidth;
	int32_t       biHeight;
	uint16_t      biPlanes;
	uint16_t      biBitCount;
	uint32_t      biCompression;
	uint32_t      biSizeImage;
	int32_t       biXPelsPerMeter;
	int32_t       biYPelsPerMeter;
	uint32_t      biClrUsed;
	uint32_t      biClrImportant;
};

struct RgbQuad
{
	uint8_t    rgbBlue;
	uint8_t    rgbGreen;
	uint8_t    rgbRed;
	uint8_t    rgbReserved;
};


struct BitmapInfo
{
	BitmapInfoHeader    bmiHeader;
	RgbQuad             bmiColors[1];
};



class FStartupScreen
{
public:
	static FStartupScreen *CreateInstance(int max_progress);

	FStartupScreen(int max_progress);
	virtual ~FStartupScreen();

	void Progress();
	virtual void DoProgress();
	virtual void LoadingStatus(const char *message, int colors); // Used by Heretic only
	virtual void AppendStatusLine(const char *status);			 // Used by Heretic only

	virtual void NetInit(const char *message, int num_players);
	virtual void NetProgress(int count);
	virtual void NetMessage(const char *format, ...);	// cover for printf
	virtual void NetDone();
	virtual bool NetLoop(bool (*timer_callback)(void *), void *userdata);
	int RunEndoom();
	static uint8_t* BitsForBitmap(BitmapInfo* bitmap_info);
protected:
	int MaxPos, CurPos, NotchPos;
	int Scale = 1;
	BitmapInfo* StartupBitmap = nullptr;
	FTexture* StartupTexture = nullptr;	

	void InvalidateTexture();
	void SizeWindowForBitmap(int scale);
	void PlanarToChunky4(uint8_t* dest, const uint8_t* src, int width, int height);
	void DrawBlock(BitmapInfo* bitmap_info, const uint8_t* src, int x, int y, int bytewidth, int height);
	void ClearBlock(BitmapInfo* bitmap_info, uint8_t fill, int x, int y, int bytewidth, int height);
	BitmapInfo* CreateBitmap(int width, int height, int color_bits);
	void FreeBitmap(BitmapInfo* bitmap_info);
	void BitmapColorsFromPlaypal(BitmapInfo* bitmap_info);
	BitmapInfo* AllocTextBitmap();
	void DrawTextScreen(BitmapInfo* bitmap_info, const uint8_t* text_screen);
	void DrawChar(BitmapInfo* screen, int x, int y, uint8_t charnum, uint8_t attrib);
	void DrawUniChar(BitmapInfo* screen, int x, int y, uint32_t charnum, uint8_t attrib);
	void UpdateTextBlink(BitmapInfo* bitmap_info, const uint8_t* text_screen, bool on);
	int CellSize(const char* unitext);

};

class FBasicStartupScreen : public FStartupScreen
{
public:
	FBasicStartupScreen(int max_progress, bool show_bar);
	~FBasicStartupScreen();

	//void DoProgress();
	void NetInit(const char* message, int num_players);
	void NetProgress(int count);
	void NetMessage(const char* format, ...);	// cover for printf
	void NetDone();
	bool NetLoop(bool (*timer_callback)(void*), void* userdata);
protected:
	long long NetMarqueeMode;
	int NetMaxPos, NetCurPos;
};

class FGraphicalStartupScreen : public FBasicStartupScreen
{
public:
	FGraphicalStartupScreen(int max_progress);
	~FGraphicalStartupScreen();
};

class FHereticStartupScreen : public FGraphicalStartupScreen
{
public:
	FHereticStartupScreen(int max_progress, long &hr);

	void DoProgress();
	void LoadingStatus(const char *message, int colors);
	void AppendStatusLine(const char *status);
protected:
	void SetWindowSize();
	
	int ThermX, ThermY, ThermWidth, ThermHeight;
	int HMsgY, SMsgX;
};

class FHexenStartupScreen : public FGraphicalStartupScreen
{
public:
	FHexenStartupScreen(int max_progress, long &hr);
	~FHexenStartupScreen();

	void DoProgress();
	void NetProgress(int count);
	void NetDone();
	void SetWindowSize();

	// Hexen's notch graphics, converted to chunky pixels.
	uint8_t * NotchBits;
	uint8_t * NetNotchBits;
};

class FStrifeStartupScreen : public FGraphicalStartupScreen
{
public:
	FStrifeStartupScreen(int max_progress, long &hr);
	~FStrifeStartupScreen();

	void DoProgress();
protected:
	void DrawStuff(int old_laser, int new_laser);
	void SetWindowSize();

	uint8_t *StartupPics[4+2+1];
};



extern FStartupScreen *StartScreen;

void DeleteStartupScreen();
extern void ST_Endoom();

