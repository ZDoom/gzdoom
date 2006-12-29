/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006 Randy Heit
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
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>

#include "st_start.h"
#include "doomdef.h"
#include "i_system.h"

static termios OldTermIOS;
static bool DidNetInit;
static int NetProgressMax, NetProgressTicker;
static const char *NetMessage;
static char SpinnyProgressChars[8] = { '|', '/', '-', '\\', '|', '/', '-', '\\' };

//===========================================================================
//
// ST_Init
//
// Sets the size of the progress bar and displays the startup screen.
//
//===========================================================================

void ST_Init(int maxProgress)
{
}

//===========================================================================
//
// ST_Done
//
// Called just before entering graphics mode to deconstruct the startup
// screen.
//
//===========================================================================

void ST_Done()
{
}

//===========================================================================
//
// ST_Progress
//
// Bumps the progress meter one notch.
//
//===========================================================================

void ST_Progress()
{
}

//===========================================================================
//
// ST_NetInit
//
// Sets stdin for unbuffered I/O, displays the given message, and shows
// a progress meter.
//
//===========================================================================

void ST_NetInit(const char *message, int numplayers)
{
	if (!DidNetInit)
	{
		termios rawtermios;

		fprintf (stderr, "Press 'Q' to abort network game synchronization.");
		// Set stdin to raw mode so we can get keypresses in ST_CheckNetAbort()
		// immediately without waiting for an EOL.
		tcgetattr (STDIN_FILENO, &OldTermIOS);
		rawtermios = OldTermIOS;
		rawtermios.c_lflag &= ~(ICANON | ECHO);
		tcsetattr (STDIN_FILENO, TCSANOW, &rawtermios);
		DidNetInit = true;
		atterm (ST_NetDone);
	}
	if (numplayers == 1)
	{
		// Status message without any real progress info.
		fprintf (stderr, "\n%s.", message);
	}
	else
	{
		fprintf (stderr, "\n%s: ", message);
	}
	fflush (stderr);
	NetMessage = message;
	NetProgressMax = numplayers;
	NetProgressTicker = 0;
	ST_NetProgress(1);	// You always know about yourself
}

//===========================================================================
//
// ST_NetDone
//
// Restores the old stdin tty settings.
//
//===========================================================================

void ST_NetDone()
{
	// Restore stdin settings
	if (DidNetInit)
	{
		tcsetattr (STDIN_FILENO, TCSANOW, &OldTermIOS);
		printf ("\n");
		DidNetInit = false;
	}
}

//===========================================================================
//
// ST_NetMessage
//
// Call this between ST_NetInit() and ST_NetDone() instead of Printf() to
// display messages, because the progress meter is mixed in the same output
// stream as normal messages.
//
//===========================================================================

void ST_NetMessage(const char *format, ...)
{
	FString str;
	va_list argptr;
	
	va_start (argptr, format);
	str.VFormat (format, argptr);
	va_end (argptr);
	fprintf (stderr, "\r%-40s\n", str.GetChars());
}

//===========================================================================
//
// ST_NetProgress
//
// Sets the network progress meter. If count is 0, it gets bumped by 1.
// Otherwise, it is set to count.
//
//===========================================================================

void ST_NetProgress(int count)
{
	int i;

	if (count == 0)
	{
		NetProgressTicker++;
	}
	else if (count > 0)
	{
		NetProgressTicker = count;
	}
	if (NetProgressMax == 0)
	{
		// Spinny-type progress meter, because we're a guest waiting for the host.
		fprintf (stderr, "\r%s: %c", NetMessage, SpinnyProgressChars[NetProgressTicker & 7]);
		fflush (stderr);
	}
	else if (NetProgressMax > 1)
	{
		// Dotty-type progress meter.
		fprintf (stderr, "\r%s: ", NetMessage);
		for (i = 0; i < NetProgressTicker; ++i)
		{
			fputc ('.', stderr);
		}
		fprintf (stderr, "%*c[%2d/%2d]", NetProgressMax + 1 - NetProgressTicker, ' ', NetProgressTicker, NetProgressMax);
		fflush (stderr);
	}
}

//===========================================================================
//
// ST_NetLoop
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

bool ST_NetLoop(bool (*timer_callback)(void *), void *userdata)
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	char k;

	for (;;)
	{
		// Don't flood the network with packets on startup.
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		FD_ZERO (&rfds);
		FD_SET (STDIN_FILENO, &rfds);

		retval = select (1, &rfds, NULL, NULL, &tv);

		if (retval == -1)
		{
			// Error
		}
		else if (retval == 0)
		{
			if (timer_callback (userdata))
			{
				fputc ('\n', stderr);
				return true;
			}
		}
		else if (read (STDIN_FILENO, &k, 1) == 1)
		{
			// Check input on stdin
			if (k == 'q' || k == 'Q')
			{
				fprintf (stderr, "\nNetwork game synchronization aborted.");
				return false;
			}
		}
	}
}
