/*
** i_system.cpp
** Main startup code
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2019-2020 Christoph Oelckers
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

#include <dirent.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <unistd.h>

#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#endif

#if defined(__sun) || defined(__sun__)
#include <termios.h>
#endif

#include <SDL.h>

#include "version.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "i_sound.h"
#include "i_interface.h"
#include "v_font.h"
#include "c_cvars.h"
#include "palutil.h"
#include "st_start.h"
#include "printf.h"
#include "launcherwindow.h"

#ifndef NO_GTK
bool I_GtkAvailable ();
void I_ShowFatalError_Gtk(const char* errortext);
#elif defined(__APPLE__)
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad);
#endif

double PerfToSec, PerfToMillisec;
CVAR(Bool, con_printansi, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
CVAR(Bool, con_4bitansi, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
EXTERN_CVAR(Bool, longsavemessages)

extern FStartupScreen *StartWindow;

void I_SetIWADInfo()
{
}

extern "C" int I_FileAvailable(const char* filename)
{
	FString cmd = "which {0} >/dev/null 2>&1";
	cmd.Substitute("{0}", filename);

	if (FILE* f = popen(cmd.GetChars(), "r"))
	{
		int status = pclose(f);
		return WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}

	return 0;
}

//
// I_Error
//

#ifdef __APPLE__
void Mac_I_FatalError(const char* errortext);
#endif

#ifdef __unix__
void Unix_I_FatalError(const char* errortext)
{
	// Close window or exit fullscreen and release mouse capture
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if(I_FileAvailable("kdialog"))
	{
		FString cmd;
		cmd << "kdialog --title \"" GAMENAME " " << GetVersionString()
			<< "\" --msgbox \"" << errortext << "\"";
		popen(cmd.GetChars(), "r");
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

		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.GetChars(), errortext, NULL) < 0)
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
#elif defined __unix__
	Unix_I_FatalError(message);
#else
	// ???
#endif
}

bool PerfAvailable;

void CalculateCPUSpeed()
{
	PerfAvailable = false;
	PerfToMillisec = PerfToSec = 0.;
#ifdef __aarch64__
	// [MK] on aarch64 rather than having to calculate cpu speed, there is
	// already an independent frequency for the perf timer
	uint64_t frq;
	asm volatile("mrs %0, cntfrq_el0":"=r"(frq));
	PerfAvailable = true;
	PerfToSec = 1./frq;
	PerfToMillisec = PerfToSec*1000.;
#elif defined(__linux__)
	// [MK] read from perf values if we can
	struct perf_event_attr pe;
	memset(&pe,0,sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_HARDWARE;
	pe.size = sizeof(struct perf_event_attr);
	pe.config = PERF_COUNT_HW_INSTRUCTIONS;
	pe.disabled = 1;
	pe.exclude_kernel = 1;
	pe.exclude_hv = 1;
	int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
	if (fd == -1)
	{
		return;
	}
	void *addr = mmap(nullptr, 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == nullptr)
	{
		close(fd);
		return;
	}
	struct perf_event_mmap_page *pc = (struct perf_event_mmap_page *)addr;
	if (pc->cap_user_time != 1)
	{
		close(fd);
		return;
	}
	double mhz = (1000LU << pc->time_shift) / (double)pc->time_mult;
	PerfAvailable = true;
	PerfToSec = .000001/mhz;
	PerfToMillisec = PerfToSec*1000.;
	if (!batchrun) Printf("CPU speed: %.0f MHz\n", mhz);
	close(fd);
#endif
}

void CleanProgressBar()
{
	if (!isatty(STDOUT_FILENO)) return;
	struct winsize sizeOfWindow;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &sizeOfWindow);
	fprintf(stdout,"\0337\033[%d;%dH\033[0J\0338",sizeOfWindow.ws_row, 0);
	fflush(stdout);
}

static int ProgressBarCurPos, ProgressBarMaxPos;

void RedrawProgressBar(int CurPos, int MaxPos)
{
	if (!isatty(STDOUT_FILENO)) return;
	CleanProgressBar();
	struct winsize sizeOfWindow;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &sizeOfWindow);
	int windowColClamped = std::min((int)sizeOfWindow.ws_col, 512);
	double progVal = std::clamp((double)CurPos / (double)MaxPos,0.0,1.0);
	int curProgVal = std::clamp(int(windowColClamped * progVal),0,windowColClamped);

	char progressBuffer[512];
	memset(progressBuffer,'.',512);
	progressBuffer[windowColClamped - 1] = 0;
	int lengthOfStr = 0;

	while (curProgVal-- > 0)
	{
		progressBuffer[lengthOfStr++] = '=';
		if (lengthOfStr >= windowColClamped - 1) break;
	}
	fprintf(stdout, "\0337\033[%d;%dH\033[2K[%s\033[%d;%dH]\0338", sizeOfWindow.ws_row, 0, progressBuffer, sizeOfWindow.ws_row, windowColClamped);
	fflush(stdout);
	ProgressBarCurPos = CurPos;
	ProgressBarMaxPos = MaxPos;
}

void I_PrintStr(const char *cp)
{
	const char * srcp = cp;
	FString printData = "";
	bool terminal = isatty(STDOUT_FILENO);

	while (*srcp != 0)
	{
		if (*srcp == 0x1c && con_printansi && terminal)
		{
			srcp += 1;
			const uint8_t* scratch = (const uint8_t*)srcp; // GCC does not like direct casting of the parameter.
			EColorRange range = V_ParseFontColor(scratch, CR_UNTRANSLATED, CR_YELLOW);
			srcp = (char*)scratch;
			if (range != CR_UNDEFINED)
			{
				PalEntry color = V_LogColorFromColorRange(range);
				if (con_4bitansi)
				{
					float h, s, v, r, g, b;
					int attrib = 0;

					RGBtoHSV(color.r / 255.f, color.g / 255.f, color.b / 255.f, &h, &s, &v);
					if (s != 0)
					{ // color
						HSVtoRGB(&r, &g, &b, h, 1, 1);
						if (r == 1)  attrib  = 0x1;
						if (g == 1)  attrib |= 0x2;
						if (b == 1)  attrib |= 0x4;
						if (v > 0.6) attrib |= 0x8;
					}
					else
					{ // gray
						     if (v < 0.33) attrib = 0x8;
						else if (v < 0.90) attrib = 0x7;
						else			   attrib = 0xF;
					}

					printData.AppendFormat("\033[%um",((attrib & 0x8) ? 90 : 30) + (attrib & 0x7));
				}
				else printData.AppendFormat("\033[38;2;%u;%u;%um",color.r,color.g,color.b);
			}
		}
		else if (*srcp != 0x1c && *srcp != 0x1d && *srcp != 0x1e && *srcp != 0x1f)
		{
			printData += *srcp++;
		}
		else
		{
			if (srcp[1] != 0) srcp += 2;
			else break;
		}
	}

	if (StartWindow) CleanProgressBar();
	fputs(printData.GetChars(),stdout);
	if (terminal) fputs("\033[0m",stdout);
	if (StartWindow) RedrawProgressBar(ProgressBarCurPos,ProgressBarMaxPos);
}

bool I_PickIWad (bool showwin, FStartupSelectionInfo& info)
{
	if (!showwin)
	{
		return true;
	}

#ifdef __APPLE__
	const int ret = I_PickIWad_Cocoa(&(*info.Wads)[0], (int)info.Wads->Size(), showwin, info.DefaultIWAD);
	if (ret >= 0)
	{
		info.DefaultIWAD = ret;
		return true;
	}
	return false;
#else
	return LauncherWindow::ExecModal(info);
#endif
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

FString I_GetCWD()
{
	char* curdir = getcwd(NULL,0);
	if (!curdir) 
	{
		return "";
	}
	FString ret(curdir);
	free(curdir);
	return ret;
}

bool I_ChDir(const char* path)
{
	return chdir(path) == 0;
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

void I_OpenShellFolder(const char* infolder)
{
	char* curdir = getcwd(NULL,0);

	if (!chdir(infolder))
	{
		if (longsavemessages)
			Printf("Opening folder: %s\n", infolder);
		std::system("xdg-open .");
		chdir(curdir);
	}
	else
	{
		if (longsavemessages)
			Printf("Unable to open directory '%s\n", infolder);
		else
			Printf("Unable to open requested directory\n");
	}
	free(curdir);
}

