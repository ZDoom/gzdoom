/*
** i_main.cpp
** System-specific startup code. Eventually calls D_DoomMain.
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include <SDL.h>
#include <unistd.h>
#include <signal.h>
#include <new>
#include <sys/param.h>
#include <locale.h>

#include "doomerrors.h"
#include "m_argv.h"
#include "d_main.h"
#include "c_console.h"
#include "version.h"
#include "w_wad.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "cmdlib.h"
#include "r_utility.h"
#include "doomstat.h"
#include "vm.h"

// MACROS ------------------------------------------------------------------

// The maximum number of functions that can be registered with atterm.
#define MAX_TERMS	64

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern "C" int cc_install_handlers(int, char**, int, int*, const char*, int(*)(char*, char*));

#ifdef __APPLE__
void Mac_I_FatalError(const char* errortext);
#endif

#ifdef __linux__
void Linux_I_FatalError(const char* errortext);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The command line arguments.
FArgs *Args;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static void (*TermFuncs[MAX_TERMS]) ();
static const char *TermNames[MAX_TERMS];
static int NumTerms;

// CODE --------------------------------------------------------------------

void addterm (void (*func) (), const char *name)
{
	// Make sure this function wasn't already registered.
	for (int i = 0; i < NumTerms; ++i)
	{
		if (TermFuncs[i] == func)
		{
			return;
		}
	}
    if (NumTerms == MAX_TERMS)
	{
		func ();
		I_FatalError (
			"Too many exit functions registered.\n"
			"Increase MAX_TERMS in i_main.cpp");
	}
	TermNames[NumTerms] = name;
    TermFuncs[NumTerms++] = func;
}

void popterm ()
{
	if (NumTerms)
		NumTerms--;
}

void call_terms ()
{
    while (NumTerms > 0)
	{
//		printf ("term %d - %s\n", NumTerms, TermNames[NumTerms-1]);
		TermFuncs[--NumTerms] ();
	}
}

static void NewFailure ()
{
    I_FatalError ("Failed to allocate memory from system heap");
}

static int DoomSpecificInfo (char *buffer, char *end)
{
	const char *arg;
	int size = end-buffer-2;
	int i, p;

	p = 0;
	p += snprintf (buffer+p, size-p, GAMENAME" version %s (%s)\n", GetVersionString(), GetGitHash());
#ifdef __VERSION__
	p += snprintf (buffer+p, size-p, "Compiler version: %s\n", __VERSION__);
#endif
	p += snprintf (buffer+p, size-p, "\nCommand line:");
	for (i = 0; i < Args->NumArgs(); ++i)
	{
		p += snprintf (buffer+p, size-p, " %s", Args->GetArg(i));
	}
	p += snprintf (buffer+p, size-p, "\n");
	
	for (i = 0; (arg = Wads.GetWadName (i)) != NULL; ++i)
	{
		p += snprintf (buffer+p, size-p, "\nWad %d: %s", i, arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		p += snprintf (buffer+p, size-p, "\n\nNot in a level.");
	}
	else
	{
		p += snprintf (buffer+p, size-p, "\n\nCurrent map: %s", level.MapName.GetChars());

		if (!viewactive)
		{
			p += snprintf (buffer+p, size-p, "\n\nView not active.");
		}
		else
		{
            auto &vp = r_viewpoint;
			p += snprintf (buffer+p, size-p, "\n\nviewx = %f", vp.Pos.X);
			p += snprintf (buffer+p, size-p, "\nviewy = %f", vp.Pos.Y);
			p += snprintf (buffer+p, size-p, "\nviewz = %f", vp.Pos.Z);
			p += snprintf (buffer+p, size-p, "\nviewangle = %f", vp.Angles.Yaw.Degrees);
		}
	}
	buffer[p++] = '\n';
	buffer[p++] = '\0';

	return p;
}

void I_StartupJoysticks();
void I_ShutdownJoysticks();

int main (int argc, char **argv)
{
#if !defined (__APPLE__)
	{
		int s[4] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS };
		cc_install_handlers(argc, argv, 4, s, GAMENAMELOWERCASE "-crash.log", DoomSpecificInfo);
	}
#endif // !__APPLE__

	printf(GAMENAME" %s - %s - SDL version\nCompiled on %s\n",
		GetVersionString(), GetGitTime(), __DATE__);

	seteuid (getuid ());
    std::set_new_handler (NewFailure);

	// Set LC_NUMERIC environment variable in case some library decides to
	// clear the setlocale call at least this will be correct.
	// Note that the LANG environment variable is overridden by LC_*
	setenv ("LC_NUMERIC", "C", 1);

	setlocale (LC_ALL, "C");

	if (SDL_Init (0) < 0)
	{
		fprintf (stderr, "Could not initialize SDL:\n%s\n", SDL_GetError());
		return -1;
	}
	atterm (SDL_Quit);

	printf("\n");
	
    try
    {
		Args = new FArgs(argc, argv);

		/*
		  killough 1/98:

		  This fixes some problems with exit handling
		  during abnormal situations.

		  The old code called I_Quit() to end program,
		  while now I_Quit() is installed as an exit
		  handler and exit() is called to exit, either
		  normally or abnormally. Seg faults are caught
		  and the error handler is used, to prevent
		  being left in graphics mode or having very
		  loud SFX noise because the sound card is
		  left in an unstable state.
		*/

		atexit (call_terms);
		atterm (I_Quit);

		// Should we even be doing anything with progdir on Unix systems?
		char program[PATH_MAX];
		if (realpath (argv[0], program) == NULL)
			strcpy (program, argv[0]);
		char *slash = strrchr (program, '/');
		if (slash != NULL)
		{
			*(slash + 1) = '\0';
			progdir = program;
		}
		else
		{
			progdir = "./";
		}

		I_StartupJoysticks();
		C_InitConsole (80*8, 25*8, false);
		D_DoomMain ();
    }
    catch (std::exception &error)
    {
		I_ShutdownJoysticks();

		const char *const message = error.what();

		if (strcmp(message, "NoRunExit"))
		{
			if (CVMAbortException::stacktrace.IsNotEmpty())
			{
				Printf("%s", CVMAbortException::stacktrace.GetChars());
			}

			if (batchrun)
			{
				Printf("%s\n", message);
			}
			else
			{
#ifdef __APPLE__
				Mac_I_FatalError(message);
#endif // __APPLE__

#ifdef __linux__
				Linux_I_FatalError(message);
#endif // __linux__
			}
		}

		exit (-1);
    }
    catch (...)
    {
		call_terms ();
		throw;
    }
    return 0;
}
