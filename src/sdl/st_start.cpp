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

termios OldTermIOS;
bool DidNetInit;
int NetProgressMax, NetProgressTicker;
char SpinnyProgressChars[8] = { '|', '/', '-', '\\', '|', '/', '-', '\\' };

void ST_Init(int maxProgress)
{
}

void ST_Done()
{
}

void ST_Progress()
{
}

void ST_NetInit(const char *message, int numplayers)
{
	if (DidNetInit)
	{
		if (numplayers == 1)
		{
			// Status message without any real progress info.
			printf ("\n%s.", message);
		}
		else
		{
			printf ("\n%s: ", message);
		}
	}
	else
	{
		termios rawtermios;

		printf ("Press 'Q' to abort network game synchronization.\n%s: ", message);
		// Set stdin to raw mode so we can get keypresses in ST_CheckNetAbort()
		// immediately without waiting for an EOL.
		tcgetattr (STDIN_FILENO, &OldTermIOS);
		rawtermios = OldTermIOS;
		tcsetattr (STDIN_FILENO, &rawtermios);
	}
	NetProgressMax = numplayers;
	NetProgressTicker = 0;
	ST_NetProgress();	// You always know about yourself
}

void ST_NetDone()
{
	// Restore stdin settings
	tcsetattr (STDIN_FILENO, &OldTermIOS);
	printf ("\n");
}

void ST_NetProgress(int count)
{
	int i;

	if (count == 0)
	{
		NetProgressTicker++;
	}
	else
	{
		NetProgressTicker = count;
	}
	if (NetProgressMax == 0)
	{
		// Spinny-type progress meter, because we're a guest.
		printf ("%c\b", SpinnyProgressChars[NetProgressTicker & 7]);
	}
	else if (NetProgressMax > 1)
	{
		// Dotty-type progress meter, because we're a host.
		printf (".%*c[%2d/%2d]", MAXPLAYERS + 1 - NetProgressTicker, NetProgressMax);
		printf ("\b\b\b\b\b\b\b");
		for (i = NetProgressTicker; i < MAXPLAYERS + 1; ++i)
		{
			printf ("\b");
		}
	}
}

bool ST_NetLoop(bool (*timer_callback)(void *), void *userdata)
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	char k;

	FD_ZERO (&rfds);
	FD_SET (STDIN_FILENO, &rfds);

	for (;;)
	{
		// Don't flood the network with packets on startup.
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		retval = select (1, &rfds, NULL, NULL, &tv);

		if (retval == -1)
		{
			// Error
		}
		else if (retval == 0)
		{
			if (timer_callback (userdata))
			{
				return true;
			}
		}
		else if (read (STDIN_FILENO, &k, 1) == 1)
		{
			// Check input on stdin
			if (k == 'q' || k == 'Q')
			{
				fprintf (stderr, "Network game synchronization aborted.");
				return false;
			}
		}
	}
}
