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

// HEADER FILES ------------------------------------------------------------

#include <unistd.h>
#include <sys/time.h>
#include <termios.h>

#include "st_start.h"
#include "i_system.h"
#include "c_cvars.h"
#include "engineerrors.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class FTTYStartupScreen : public FStartupScreen
{
	public:
		FTTYStartupScreen(int max_progress);
		~FTTYStartupScreen();

		void Progress(int amount = 1) override;

		void NetInit(const char* message, bool host) override;
		void NetMessage(const char* message) override;
		void NetConnect(int client, const char* name, unsigned flags, int status) override;
		void NetUpdate(int client, int status) override;
		void NetDisconnect(int client) override;
		void NetProgress(int cur, int limit) override;
		void NetDone() override;
		void NetClose() override;
		bool ShouldStartNet() override;
		int GetNetKickClient() override;
		int GetNetBanClient() override;
		bool NetLoop(bool (*loopCallback)(void*), void* data) override;

	protected:
		bool DidNetInit;
		int NetMaxPos, NetCurPos;
		const char *TheNetMessage;
		termios OldTermIOS;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void RedrawProgressBar(int CurPos, int MaxPos);
extern void CleanProgressBar();

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char SpinnyProgressChars[4] = { '|', '/', '-', '\\' };

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FStartupScreen :: CreateInstance
//
// Initializes the startup screen for the detected game.
// Sets the size of the progress bar and displays the startup screen.
//
//==========================================================================

FStartupScreen *FStartupScreen::CreateInstance(int max_progress)
{
	return new FTTYStartupScreen(max_progress);
}

//===========================================================================
//
// FTTYStartupScreen Constructor
//
// Sets the size of the progress bar and displays the startup screen.
//
//===========================================================================

FTTYStartupScreen::FTTYStartupScreen(int max_progress)
	: FStartupScreen(max_progress)
{
	DidNetInit = false;
	NetMaxPos = 0;
	NetCurPos = 0;
	TheNetMessage = NULL;
}

//===========================================================================
//
// FTTYStartupScreen Destructor
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//===========================================================================

FTTYStartupScreen::~FTTYStartupScreen()
{
	NetDone();	// Just in case it wasn't called yet and needs to be.
}

//===========================================================================
//
// FTTYStartupScreen :: Progress
//
//===========================================================================

void FTTYStartupScreen::Progress(int advance)
{
	CurPos = min(CurPos + advance, MaxPos);
	RedrawProgressBar(CurPos, MaxPos);
}

//===========================================================================
//
// FTTYStartupScreen :: NetInit
//
// Sets stdin for unbuffered I/O, displays the given message, and shows
// a progress meter.
//
//===========================================================================

void FTTYStartupScreen::NetInit(const char* message, bool host)
{
	if (!DidNetInit)
	{
		termios rawtermios;

		CleanProgressBar();
		fprintf (stderr, "Press 'Q' to abort network game synchronization.");
		// Set stdin to raw mode so we can get keypresses in ST_CheckNetAbort()
		// immediately without waiting for an EOL.
		tcgetattr (STDIN_FILENO, &OldTermIOS);
		rawtermios = OldTermIOS;
		rawtermios.c_lflag &= ~(ICANON | ECHO);
		tcsetattr (STDIN_FILENO, TCSANOW, &rawtermios);
		DidNetInit = true;
	}
	fprintf(stderr, "\n%s.", message);
	fflush (stderr);
	TheNetMessage = message;
	NetCurPos = 0;
}

void FTTYStartupScreen::NetMessage(const char* message)
{
	TheNetMessage = message;
}

void FTTYStartupScreen::NetConnect(int client, const char* name, unsigned flags, int status)
{

}

void FTTYStartupScreen::NetUpdate(int client, int status)
{

}

void FTTYStartupScreen::NetDisconnect(int client)
{

}

//===========================================================================
//
// FTTYStartupScreen :: NetProgress
//
// Sets the network progress meter.
//
//===========================================================================

void FTTYStartupScreen::NetProgress(int cur, int limit)
{
	int i;

	NetMaxPos = limit;
	NetCurPos = cur;
	if (NetMaxPos == 0)
	{
		// Spinny-type progress meter, because we're a guest waiting for the host.
		fprintf(stderr, "\r%s: %c", TheNetMessage, SpinnyProgressChars[NetCurPos & 3]);
		fflush(stderr);
	}
	else if (NetMaxPos > 1)
	{
		// Dotty-type progress meter.
		fprintf(stderr, "\r%s: ", TheNetMessage);
		for (i = 0; i < NetCurPos; ++i)
		{
			fputc('.', stderr);
		}
		fprintf(stderr, "%*c[%2d/%2d]", NetMaxPos + 1 - NetCurPos, ' ', NetCurPos, NetMaxPos);
		fflush(stderr);
	}
}

//===========================================================================
//
// FTTYStartupScreen :: NetDone
//
// Restores the old stdin tty settings.
//
//===========================================================================

void FTTYStartupScreen::NetDone()
{	
	CurPos = MaxPos;
	CleanProgressBar();
	// Restore stdin settings
	if (DidNetInit)
	{
		tcsetattr (STDIN_FILENO, TCSANOW, &OldTermIOS);
		printf ("\n");
		DidNetInit = false;
	}	
}

void FTTYStartupScreen::NetClose()
{
	// TODO: Implement this
}

bool FTTYStartupScreen::ShouldStartNet()
{
	return false;
}

int FTTYStartupScreen::GetNetKickClient()
{
	return -1;
}

int FTTYStartupScreen::GetNetBanClient()
{
	return -1;
}

//===========================================================================
//
// FTTYStartupScreen :: NetLoop
//
// The timer_callback function is called at least two times per second
// and passed the userdata value. It should return true to stop the loop and
// return control to the caller or false to continue the loop.
//
// ST_NetLoop will return true if the loop was halted by the callback and
// false if the loop was halted because the user wants to abort the
// network synchronization.
//
//===========================================================================

bool FTTYStartupScreen::NetLoop(bool (*loopCallback)(void *), void *data)
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	char k;
	bool stdin_eof = false;

	for (;;)
	{
		// Don't flood the network with packets on startup.
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		FD_ZERO (&rfds);
		if (!stdin_eof)
		{
			FD_SET (STDIN_FILENO, &rfds);
		}

		retval = select (1, &rfds, NULL, NULL, &tv);

		if (retval == -1)
		{
			// Error
		}
		else if (retval == 0)
		{
			if (loopCallback (data))
			{
				fputc ('\n', stderr);
				return true;
			}
		}
		else
		{
			ssize_t amt = read (STDIN_FILENO, &k, 1);	// Check input on stdin
			if (amt == 0)
			{
				// EOF. Stop reading
				stdin_eof = true;
			}
			else if (amt == 1)
			{
				if (k == 'q' || k == 'Q')
				{
					fprintf (stderr, "\nNetwork game synchronization aborted.");
					return false;
				}
			}
		}
	}
}

