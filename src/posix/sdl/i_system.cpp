//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <SDL.h>

#include "d_main.h"
#include "i_system.h"
#include "version.h"
#include "x86.h"


#ifndef NO_GTK
bool I_GtkAvailable ();
int I_PickIWad_Gtk (WadStuff *wads, int numwads, bool showwin, int defaultiwad);
void I_ShowFatalError_Gtk(const char* errortext);
#elif defined(__APPLE__)
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad);
#endif

double PerfToSec, PerfToMillisec;

void I_SetIWADInfo()
{
}

//
// I_Error
//

#ifdef __APPLE__
void Mac_I_FatalError(const char* errortext);
#endif

#ifdef __linux__
void Linux_I_FatalError(const char* errortext)
{
	// Close window or exit fullscreen and release mouse capture
	SDL_Quit();

	const char *str;
	if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
	{
		FString cmd;
		cmd << "kdialog --title \"" GAMENAME " " << GetVersionString()
			<< "\" --msgbox \"" << errortext << "\"";
		popen(cmd, "r");
	}
#ifndef NO_GTK
	else if (I_GtkAvailable())
	{
		I_ShowFatalError_Gtk(errortext);
	}
#endif
	else
	{
		FString title;
		title << GAMENAME " " << GetVersionString();

		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, errortext, NULL) < 0)
		{
			printf("\n%s\n", errortext);
		}
	}
}
#endif


void I_ShowFatalError(const char *message)
{
#ifdef __APPLE__
	Mac_I_FatalError(message);
#elif defined __linux__
	Linux_I_FatalError(message);
#else
	// ???
#endif
}

void CalculateCPUSpeed()
{
}

void I_DebugPrint(const char *cp)
{
}

void I_PrintStr(const char *cp)
{
	// Strip out any color escape sequences before writing to debug output
	TArray<char> copy(strlen(cp) + 1, true);
	const char * srcp = cp;
	char * dstp = copy.Data();

	while (*srcp != 0)
	{
		if (*srcp != 0x1c && *srcp != 0x1d && *srcp != 0x1e && *srcp != 0x1f)
		{
			*dstp++ = *srcp++;
		}
		else
		{
			if (srcp[1] != 0) srcp += 2;
			else break;
		}
	}
	*dstp = 0;

	fputs(copy.Data(), stdout);
	fflush(stdout);
}

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int i;

	if (!showwin)
	{
		return defaultiwad;
	}

#ifndef __APPLE__
	const char *str;
	if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
	{
		FString cmd("kdialog --title \"" GAMENAME " ");
		cmd << GetVersionString() << ": Select an IWAD to use\""
					" --menu \"" GAMENAME " found more than one IWAD\n"
					"Select from the list below to determine which one to use:\"";

		for(i = 0; i < numwads; ++i)
		{
			const char *filepart = strrchr(wads[i].Path, '/');
			if(filepart == NULL)
				filepart = wads[i].Path;
			else
				filepart++;
			// Menu entries are specified in "tag" "item" pairs, where when a
			// particular item is selected (and the Okay button clicked), its
			// corresponding tag is printed to stdout for identification.
			cmd.AppendFormat(" \"%d\" \"%s (%s)\"", i, wads[i].Name.GetChars(), filepart);
		}

		if(defaultiwad >= 0 && defaultiwad < numwads)
		{
			const char *filepart = strrchr(wads[defaultiwad].Path, '/');
			if(filepart == NULL)
				filepart = wads[defaultiwad].Path;
			else
				filepart++;
			cmd.AppendFormat(" --default \"%s (%s)\"", wads[defaultiwad].Name.GetChars(), filepart);
		}

		FILE *f = popen(cmd, "r");
		if(f != NULL)
		{
			char gotstr[16];

			if(fgets(gotstr, sizeof(gotstr), f) == NULL ||
			   sscanf(gotstr, "%d", &i) != 1)
				i = -1;

			// Exit status = 1 means the selection was canceled (either by
			// Cancel/Esc or the X button), not that there was an error running
			// the program. In that case, nothing was printed so fgets will
			// have failed. Other values can indicate an error running the app,
			// so fall back to whatever else can be used.
			int status = pclose(f);
			if(WIFEXITED(status) && (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 1))
				return i;
		}
	}
#endif

#ifndef NO_GTK
	if (I_GtkAvailable())
	{
		return I_PickIWad_Gtk (wads, numwads, showwin, defaultiwad);
	}
#endif

#ifdef __APPLE__
	return I_PickIWad_Cocoa (wads, numwads, showwin, defaultiwad);
#endif
	
	if (!isatty(fileno(stdin)))
	{
		return defaultiwad;
	}

	printf ("Please select a game wad (or 0 to exit):\n");
	for (i = 0; i < numwads; ++i)
	{
		const char *filepart = strrchr (wads[i].Path, '/');
		if (filepart == NULL)
			filepart = wads[i].Path;
		else
			filepart++;
		printf ("%d. %s (%s)\n", i+1, wads[i].Name.GetChars(), filepart);
	}
	printf ("Which one? ");
	if (scanf ("%d", &i) != 1 || i > numwads)
		return -1;
	return i-1;
}

void I_PutInClipboard (const char *str)
{
	SDL_SetClipboardText(str);
}

FString I_GetFromClipboard (bool use_primary_selection)
{
	if(char *ret = SDL_GetClipboardText())
	{
		FString text(ret);
		SDL_free(ret);
		return text;
	}
	return "";
}

// Return a random seed, preferably one with lots of entropy.
unsigned int I_MakeRNGSeed()
{
	unsigned int seed;
	int file;

	// Try reading from /dev/urandom first, then /dev/random, then
	// if all else fails, use a crappy seed from time().
	seed = time(NULL);
	file = open("/dev/urandom", O_RDONLY);
	if (file < 0)
	{
		file = open("/dev/random", O_RDONLY);
	}
	if (file >= 0)
	{
		read(file, &seed, sizeof(seed));
		close(file);
	}
	return seed;
}
