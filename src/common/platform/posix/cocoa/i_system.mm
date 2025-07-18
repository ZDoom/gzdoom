/*
 ** i_system.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2018 Alexey Lysiuk
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

#include "i_common.h"
#include "c_cvars.h"
#include "i_interface.h"

#include <fnmatch.h>
#include <sys/sysctl.h>

#include "i_system.h"
#include "st_console.h"
#include "v_text.h"

EXTERN_CVAR(Bool, longsavemessages)
double PerfToSec, PerfToMillisec;

void CalculateCPUSpeed()
{
	long long frequency;
	size_t size = sizeof frequency;

	if (0 == sysctlbyname("machdep.tsc.frequency", &frequency, &size, nullptr, 0) && 0 != frequency)
	{
		PerfToSec = 1.0 / frequency;
		PerfToMillisec = 1000.0 / frequency;

		if (!batchrun)
		{
			Printf("CPU speed: %.0f MHz\n", 0.001 / PerfToMillisec);
		}
	}
}


void I_SetIWADInfo()
{
	FConsoleWindow::GetInstance().SetTitleText();
}


void I_PrintStr(const char* const message)
{
	FConsoleWindow::GetInstance().AddText(message);

	// Strip out any color escape sequences before writing to output
	char* const copy = new char[strlen(message) + 1];
	const char* srcp = message;
	char* dstp = copy;

	while ('\0' != *srcp)
	{
		if (TEXTCOLOR_ESCAPE == *srcp)
		{
			if ('\0' != srcp[1])
			{
				srcp += 2;
			}
			else
			{
				break;
			}
		}
		else if (0x1d == *srcp || 0x1f == *srcp) // Opening and closing bar character
		{
			*dstp++ = '-';
			++srcp;
		}
		else if (0x1e == *srcp) // Middle bar character
		{
			*dstp++ = '=';
			++srcp;
		}
		else
		{
			*dstp++ = *srcp++;
		}
	}

	*dstp = '\0';

	fputs(copy, stdout);
	delete[] copy;
	fflush(stdout);
}


void Mac_I_FatalError(const char* const message);

void I_ShowFatalError(const char *message)
{
	Mac_I_FatalError(message);
}


bool I_PickIWad(bool showwin, FStartupSelectionInfo& info)
{
	if (!showwin)
	{
		return true;
	}

	I_SetMainWindowVisible(false);

	extern int I_PickIWad_Cocoa(WadStuff*, int, bool, int);
	const int result = I_PickIWad_Cocoa(&(*info.Wads)[0], (int)info.Wads->Size(), showwin, info.DefaultIWAD);

	I_SetMainWindowVisible(true);

	if (result >= 0)
	{
		info.DefaultIWAD = result;
		return true;
	}

	return false;
}


void I_PutInClipboard(const char* const string)
{
	NSPasteboard* const pasteBoard = [NSPasteboard generalPasteboard];
	NSString* const stringType = NSStringPboardType;
	NSArray* const types = [NSArray arrayWithObjects:stringType, nil];
	NSString* const content = [NSString stringWithUTF8String:string];

	[pasteBoard declareTypes:types
					   owner:nil];
	[pasteBoard setString:content
				  forType:stringType];
}

FString I_GetFromClipboard(bool returnNothing)
{
	if (returnNothing)
	{
		return FString();
	}

	NSPasteboard* const pasteBoard = [NSPasteboard generalPasteboard];
	NSString* const value = [pasteBoard stringForType:NSStringPboardType];

	return FString([value UTF8String]);
}


unsigned int I_MakeRNGSeed()
{
	return static_cast<unsigned int>(arc4random());
}

FString I_GetCWD()
{
	NSString *currentpath = [[NSFileManager defaultManager] currentDirectoryPath];
	return currentpath.UTF8String;
}

bool I_ChDir(const char* path)
{
	return [[NSFileManager defaultManager] changeCurrentDirectoryPath:[NSString stringWithUTF8String:path]];
}

void I_OpenShellFolder(const char* folder)
{
	NSFileManager *filemgr = [NSFileManager defaultManager];
	NSString *currentpath = [filemgr currentDirectoryPath];

	[filemgr changeCurrentDirectoryPath:[NSString stringWithUTF8String:folder]];
	if (longsavemessages)
		Printf("Opening folder: %s\n", folder);
	std::system("open .");
	[filemgr changeCurrentDirectoryPath:currentpath];
}

