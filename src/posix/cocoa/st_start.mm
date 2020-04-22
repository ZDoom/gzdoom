/*
 ** st_start.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2015 Alexey Lysiuk
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

#include <unistd.h>

#import <Foundation/NSRunLoop.h>

#include "c_cvars.h"
#include "st_console.h"
#include "st_start.h"
#include "printf.h"
#include "engineerrors.h"


FStartupScreen *StartScreen;


CUSTOM_CVAR(Int, showendoom, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}


// ---------------------------------------------------------------------------


FBasicStartupScreen::FBasicStartupScreen(int maxProgress, bool showBar)
: FStartupScreen(maxProgress)
{
	FConsoleWindow& consoleWindow = FConsoleWindow::GetInstance();
	consoleWindow.SetProgressBar(true);

#if 0
	// Testing code, please do not remove
	consoleWindow.AddText("----------------------------------------------------------------\n");
	consoleWindow.AddText("1234567890 !@#$%^&*() ,<.>/?;:'\" [{]}\\| `~-_=+ "
		"This is very very very long message needed to trigger word wrapping...\n\n");
	consoleWindow.AddText("Multiline...\n\tmessage...\n\t\twith...\n\t\t\ttabs.\n\n");

	consoleWindow.AddText(TEXTCOLOR_BRICK "TEXTCOLOR_BRICK\n" TEXTCOLOR_TAN "TEXTCOLOR_TAN\n");
	consoleWindow.AddText(TEXTCOLOR_GRAY "TEXTCOLOR_GRAY & TEXTCOLOR_GREY\n");
	consoleWindow.AddText(TEXTCOLOR_GREEN "TEXTCOLOR_GREEN\n" TEXTCOLOR_BROWN "TEXTCOLOR_BROWN\n");
	consoleWindow.AddText(TEXTCOLOR_GOLD "TEXTCOLOR_GOLD\n" TEXTCOLOR_RED "TEXTCOLOR_RED\n");
	consoleWindow.AddText(TEXTCOLOR_BLUE "TEXTCOLOR_BLUE\n" TEXTCOLOR_ORANGE "TEXTCOLOR_ORANGE\n");
	consoleWindow.AddText(TEXTCOLOR_WHITE "TEXTCOLOR_WHITE\n" TEXTCOLOR_YELLOW "TEXTCOLOR_YELLOW\n");
	consoleWindow.AddText(TEXTCOLOR_UNTRANSLATED "TEXTCOLOR_UNTRANSLATED\n");
	consoleWindow.AddText(TEXTCOLOR_BLACK "TEXTCOLOR_BLACK\n" TEXTCOLOR_LIGHTBLUE "TEXTCOLOR_LIGHTBLUE\n");
	consoleWindow.AddText(TEXTCOLOR_CREAM "TEXTCOLOR_CREAM\n" TEXTCOLOR_OLIVE "TEXTCOLOR_OLIVE\n");
	consoleWindow.AddText(TEXTCOLOR_DARKGREEN "TEXTCOLOR_DARKGREEN\n" TEXTCOLOR_DARKRED "TEXTCOLOR_DARKRED\n");
	consoleWindow.AddText(TEXTCOLOR_DARKBROWN "TEXTCOLOR_DARKBROWN\n" TEXTCOLOR_PURPLE "TEXTCOLOR_PURPLE\n");
	consoleWindow.AddText(TEXTCOLOR_DARKGRAY "TEXTCOLOR_DARKGRAY\n" TEXTCOLOR_CYAN "TEXTCOLOR_CYAN\n");
	consoleWindow.AddText(TEXTCOLOR_ICE "TEXTCOLOR_ICE\n" TEXTCOLOR_FIRE "TEXTCOLOR_FIRE\n");
	consoleWindow.AddText(TEXTCOLOR_SAPPHIRE "TEXTCOLOR_SAPPHIRE\n" TEXTCOLOR_TEAL "TEXTCOLOR_TEAL\n");
	consoleWindow.AddText(TEXTCOLOR_NORMAL "TEXTCOLOR_NORMAL\n" TEXTCOLOR_BOLD "TEXTCOLOR_BOLD\n");
	consoleWindow.AddText(TEXTCOLOR_CHAT "TEXTCOLOR_CHAT\n" TEXTCOLOR_TEAMCHAT "TEXTCOLOR_TEAMCHAT\n");
	consoleWindow.AddText("----------------------------------------------------------------\n");
#endif // _DEBUG
}

FBasicStartupScreen::~FBasicStartupScreen()
{
	FConsoleWindow::GetInstance().SetProgressBar(false);
}


void FBasicStartupScreen::Progress()
{
	if (CurPos < MaxPos)
	{
		++CurPos;
	}

	FConsoleWindow::GetInstance().Progress(CurPos, MaxPos);
}


void FBasicStartupScreen::NetInit(const char* const message, const int playerCount)
{
	FConsoleWindow::GetInstance().NetInit(message, playerCount);
}

void FBasicStartupScreen::NetProgress(const int count)
{
	FConsoleWindow::GetInstance().NetProgress(count);
}

void FBasicStartupScreen::NetMessage(const char* const format, ...)
{
	va_list args;
	va_start(args, format);

	FString message;
	message.VFormat(format, args);
	va_end(args);

	Printf("%s\n", message.GetChars());
}

void FBasicStartupScreen::NetDone()
{
	FConsoleWindow::GetInstance().NetDone();
}

bool FBasicStartupScreen::NetLoop(bool (*timerCallback)(void*), void* const userData)
{
	while (true)
	{
		if (timerCallback(userData))
		{
			break;
		}

		[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];

		// Do not poll to often
		usleep(50000);
	}

	return true;
}


// ---------------------------------------------------------------------------


FStartupScreen *FStartupScreen::CreateInstance(const int maxProgress)
{
	return new FBasicStartupScreen(maxProgress, true);
}


// ---------------------------------------------------------------------------


void ST_Endoom()
{
	throw CExitEvent(0);
}
