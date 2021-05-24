/*
 ** st_start.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2015 Alexey Lysiuk
 ** Copyright 2021 Cacodemon345
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

#include "st_console.h"
#include "st_start.h"
#include "engineerrors.h"

#include <unistd.h>

extern int ProgressBarCurPos, ProgressBarMaxPos;
extern bool netinited;

FBasicStartupScreen::FBasicStartupScreen(int max_progress, bool show_bar)
: FStartupScreen(max_progress)
{
    FConsoleWindow::GetInstance().SetProgressBar(true);
}

FBasicStartupScreen::~FBasicStartupScreen()
{
	NetDone();
	FConsoleWindow::GetInstance().SetProgressBar(false);
}

#if 0
FStartupScreen *FStartupScreen::CreateInstance(const int maxProgress)
{
	return new FBasicStartupScreen(maxProgress, true);
}
#endif

void FBasicStartupScreen::NetInit(const char* const message, const int playerCount)
{
	FConsoleWindow::GetInstance().NetInit(message, playerCount);
	netinited = true;
	extern void CleanProgressBar();
	CleanProgressBar();
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

	Printf("%s", message.GetChars());
}

void FBasicStartupScreen::Progress()
{
	if (CurPos < MaxPos)
	{
		++CurPos;
	}
	ProgressBarCurPos = CurPos;
	ProgressBarMaxPos = MaxPos;
	
	extern void RedrawProgressBar(int CurPos, int MaxPos);
	RedrawProgressBar(ProgressBarCurPos, ProgressBarMaxPos);
	FConsoleWindow::GetInstance().RunLoop();
}

void FBasicStartupScreen::NetDone()
{
	FConsoleWindow::GetInstance().NetDone();
	extern void CleanProgressBar();
	CleanProgressBar();
}

bool FBasicStartupScreen::NetLoop(bool (*timerCallback)(void*), void* const userData)
{
	while (true)
	{
		if (timerCallback(userData))
		{
			break;
		}

		FConsoleWindow::GetInstance().RunLoop();
		if (FConsoleWindow::GetInstance().NetUserExitRequested())
		{
			Printf(TEXTCOLOR_RED "Network game synchronization aborted.\n");
			return false;
		}

		// Do not poll to often
		usleep(50000);
	}

	return true;
}
