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
#include <stdlib.h>
#include <signal.h>
#include <new>
#include <sys/param.h>
#ifndef NO_GTK
#include <gtk/gtk.h>
#endif
#include <locale.h>
#if defined(__MACH__) && !defined(NOASM)
#include <sys/types.h>
#include <sys/mman.h>
#endif

#include "doomerrors.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "c_console.h"
#include "errors.h"
#include "version.h"
#include "w_wad.h"
#include "g_level.h"
#include "r_state.h"
#include "cmdlib.h"
#include "r_utility.h"
#include "doomstat.h"

// MACROS ------------------------------------------------------------------

// The maximum number of functions that can be registered with atterm.
#define MAX_TERMS	64

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern "C" int cc_install_handlers(int, char**, int, int*, const char*, int(*)(char*, char*));

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef USE_XCURSOR
extern bool UseXCursor;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifndef NO_GTK
bool GtkAvailable;
#endif

// The command line arguments.
DArgs *Args;

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

void STACK_ARGS call_terms ()
{
    while (NumTerms > 0)
	{
//		printf ("term %d - %s\n", NumTerms, TermNames[NumTerms-1]);
		TermFuncs[--NumTerms] ();
	}
}

static void STACK_ARGS NewFailure ()
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
		char name[9];

		strncpy (name, level.mapname, 8);
		name[8] = 0;
		p += snprintf (buffer+p, size-p, "\n\nCurrent map: %s", name);

		if (!viewactive)
		{
			p += snprintf (buffer+p, size-p, "\n\nView not active.");
		}
		else
		{
			p += snprintf (buffer+p, size-p, "\n\nviewx = %d", (int)viewx);
			p += snprintf (buffer+p, size-p, "\nviewy = %d", (int)viewy);
			p += snprintf (buffer+p, size-p, "\nviewz = %d", (int)viewz);
			p += snprintf (buffer+p, size-p, "\nviewangle = %x", (unsigned int)viewangle);
		}
	}
	buffer[p++] = '\n';
	buffer[p++] = '\0';

	return p;
}

#if defined(__MACH__) && !defined(NOASM)
// NASM won't let us create custom sections for Mach-O. Whether that's a limitation of NASM
// or of Mach-O, I don't know, but since we're using NASM for the assembly, it doesn't much
// matter.
extern "C"
{
	extern void *rtext_a_start, *rtext_a_end;
	extern void *rtext_tmap_start, *rtext_tmap_end;
	extern void *rtext_tmap2_start, *rtext_tmap2_end;
	extern void *rtext_tmap3_start, *rtext_tmap3_end;
};

static void unprotect_pages(long pagesize, void *start, void *end)
{
	char *page = (char *)((intptr_t)start & ~(pagesize - 1));
	size_t len = (char *)end - (char *)start;
	if (mprotect(page, len, PROT_READ|PROT_WRITE|PROT_EXEC) != 0)
	{
		fprintf(stderr, "mprotect failed\n");
		exit(1);
	}
}

static void unprotect_rtext()
{
	static void *const pages[] =
	{
		rtext_a_start, rtext_a_end,
		rtext_tmap_start, rtext_tmap_end,
		rtext_tmap2_start, rtext_tmap2_end,
		rtext_tmap3_start, rtext_tmap3_end
	};
	long pagesize = sysconf(_SC_PAGESIZE);
	for (void *const *p = pages; p < &pages[countof(pages)]; p += 2)
	{
		unprotect_pages(pagesize, p[0], p[1]);
	}
}
#endif

void I_StartupJoysticks();
void I_ShutdownJoysticks();

int main (int argc, char **argv)
{
#if !defined (__APPLE__)
	{
		int s[4] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS };
		cc_install_handlers(argc, argv, 4, s, "zdoom-crash.log", DoomSpecificInfo);
	}
#endif // !__APPLE__

	printf(GAMENAME" %s - %s - SDL version\nCompiled on %s\n",
		GetVersionString(), GetGitTime(), __DATE__);

	seteuid (getuid ());
    std::set_new_handler (NewFailure);

#if defined(__MACH__) && !defined(NOASM)
	unprotect_rtext();
#endif

	// Set LC_NUMERIC environment variable in case some library decides to
	// clear the setlocale call at least this will be correct.
	// Note that the LANG environment variable is overridden by LC_*
	setenv ("LC_NUMERIC", "C", 1);

#ifndef NO_GTK
	GtkAvailable = gtk_init_check (&argc, &argv);
#endif
	
	setlocale (LC_ALL, "C");

	if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE|SDL_INIT_JOYSTICK) == -1)
	{
		fprintf (stderr, "Could not initialize SDL:\n%s\n", SDL_GetError());
		return -1;
	}
	atterm (SDL_Quit);

	{
		char viddriver[80];

		if (SDL_VideoDriverName(viddriver, sizeof(viddriver)) != NULL)
		{
			printf("Using video driver %s\n", viddriver);
#ifdef USE_XCURSOR
			UseXCursor = (strcmp(viddriver, "x11") == 0);
#endif
		}
		printf("\n");
	}

	char caption[100];
	mysnprintf(caption, countof(caption), GAMESIG " %s (%s)", GetVersionString(), GetGitTime());
	SDL_WM_SetCaption(caption, caption);

#ifdef __APPLE__
	
	const SDL_VideoInfo* videoInfo = SDL_GetVideoInfo();
	if ( NULL != videoInfo )
	{
		EXTERN_CVAR(  Int, vid_defwidth  )
		EXTERN_CVAR(  Int, vid_defheight )
		EXTERN_CVAR(  Int, vid_defbits   )
		EXTERN_CVAR( Bool, vid_vsync     )
		EXTERN_CVAR( Bool, fullscreen    )
		
		vid_defwidth  = videoInfo->current_w;
		vid_defheight = videoInfo->current_h;
		vid_defbits   = videoInfo->vfmt->BitsPerPixel;
		vid_vsync     = True;
		fullscreen    = True;
	}
	
#endif // __APPLE__
	
    try
    {
		Args = new DArgs(argc, argv);

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
    catch (class CDoomError &error)
    {
		I_ShutdownJoysticks();
		if (error.GetMessage ())
			fprintf (stderr, "%s\n", error.GetMessage ());
		exit (-1);
    }
    catch (...)
    {
		call_terms ();
		throw;
    }
    return 0;
}
