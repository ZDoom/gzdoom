
/*
** endoom.cpp
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

// HEADER FILES ------------------------------------------------------------

#include "startscreen.h"
#include "cmdlib.h"

#include "i_system.h"
#include "gstrings.h"
#include "filesystem.h"
#include "m_argv.h"
#include "engineerrors.h"
#include "s_music.h"
#include "printf.h"
#include "startupinfo.h"
#include "i_interface.h"
#include "texturemanager.h"
#include "c_cvars.h"
#include "i_time.h"
#include "g_input.h"
#include "d_eventbase.h"

// MACROS ------------------------------------------------------------------

// How many ms elapse between blinking text flips. On a standard VGA
// adapter, the characters are on for 16 frames and then off for another 16.
// The number here therefore corresponds roughly to the blink rate on a
// 60 Hz display.
#define BLINK_PERIOD			267


// TYPES -------------------------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, showendoom, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 0;
	else if (self > 2) self=2;
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

class FEndoomScreen : public FStartScreen
{
	uint64_t lastUpdateTime;
	bool blinkstate = false;
	bool blinking = true;
	uint8_t endoom_screen[4000];

public:
	FEndoomScreen(int);
	void Update();
};


//==========================================================================
//
// FHereticStartScreen Constructor
//
// Shows the Heretic startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The loading screen is an 80x25 text screen with character data and
// attributes intermixed, which means it must be exactly 4000 bytes long.
//
//==========================================================================

FEndoomScreen::FEndoomScreen(int loading_lump)
	: FStartScreen(0)
{
	fileSystem.ReadFile(loading_lump, endoom_screen);

	// Draw the loading screen to a bitmap.
	StartupBitmap.Create(80 * 8, 26 * 16); // line 26 is for our own 'press any key to quit' message.
	DrawTextScreen(StartupBitmap, endoom_screen);
	ClearBlock(StartupBitmap, {0, 0, 0, 255}, 0, 25*16, 640, 16);
	DrawString(StartupBitmap, 0, 25, GStrings("TXT_QUITENDOOM"), { 128, 128, 128 ,255}, { 0, 0, 0, 255});
	lastUpdateTime = I_msTime();
	
	// Does this screen need blinking?
	for (int i = 0; i < 80*25; ++i)
	{
		if (endoom_screen[1+i*2] & 0x80)
		{
			blinking = true;
			break;
		}
	}
}

void FEndoomScreen::Update()
{
	if (blinking && I_msTime() > lastUpdateTime + BLINK_PERIOD)
	{
		lastUpdateTime = I_msTime();
		UpdateTextBlink (StartupBitmap, endoom_screen, blinkstate);
		blinkstate = !blinkstate;
		StartupTexture->CleanHardwareData();
		Render(true);
	}
}


//==========================================================================
//
// ST_Endoom
//
// Shows an ENDOOM text screen
//
//==========================================================================

int RunEndoom()
{
	if (showendoom == 0 || endoomName.Len() == 0)
	{
		return 0;
	}

	int endoom_lump = fileSystem.CheckNumForFullName (endoomName.GetChars(), true);
	
	if (endoom_lump < 0 || fileSystem.FileLength (endoom_lump) != 4000)
	{
		return 0;
	}

	if (fileSystem.GetFileContainer(endoom_lump) == fileSystem.GetMaxIwadNum() && showendoom == 2)
	{
		// showendoom==2 means to show only lumps from PWADs.
		return 0;
	}
	
	S_StopMusic(true);
	auto endoom = new FEndoomScreen(endoom_lump);
	endoom->Render(true);

	while(true)
	{
		I_GetEvent();
		endoom->Update();
		while (eventtail != eventhead)
		{
			event_t *ev = &events[eventtail];
			eventtail = (eventtail + 1) & (MAXEVENTS - 1);

			if (ev->type == EV_KeyDown || ev->type == EV_KeyUp)
			{
				return 0;
			}
			if (ev->type == EV_GUI_Event && (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_LButtonDown || ev->subtype == EV_GUI_RButtonDown || ev->subtype == EV_GUI_MButtonDown))
			{
				return 0;
			}
		}
	}
	return 0;
}

[[noreturn]]
void ST_Endoom()
{
	int code = RunEndoom();
	throw CExitEvent(code);
}
