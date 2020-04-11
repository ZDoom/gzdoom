/*
** engineerrors.cpp
** Contains error classes that can be thrown around
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
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

bool gameisdead;

#ifdef _WIN32
#include <windows.h>
#include "zstring.h"
void I_DebugPrint(const char *cp)
{
	if (IsDebuggerPresent())
	{
		auto wstr = WideString(cp);
		OutputDebugStringW(wstr.c_str());
	}
}
#else
void I_DebugPrint(const char *cp)
{
}	
#endif

#include "engineerrors.h"

//==========================================================================
//
// I_Error
//
// Throw an error that will send us to the console if we are far enough
// along in the startup process.
//
//==========================================================================

void I_Error(const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start(argptr, error);
	vsnprintf(errortext, MAX_ERRORTEXT, error, argptr);
	va_end(argptr);
	I_DebugPrint(errortext);

	throw CRecoverableError(errortext);
}

//==========================================================================
//
// I_FatalError
//
// Throw an error that will end the game.
//
//==========================================================================
extern FILE *Logfile;

void I_FatalError(const char *error, ...)
{
	static bool alreadyThrown = false;
	gameisdead = true;

	if (!alreadyThrown)		// ignore all but the first message -- killough
	{
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		va_list argptr;
		va_start(argptr, error);
		vsnprintf(errortext, MAX_ERRORTEXT, error, argptr);
		va_end(argptr);
		I_DebugPrint(errortext);

		// Record error to log (if logging)
		if (Logfile)
		{
			fprintf(Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);
			fflush(Logfile);
		}

		throw CFatalError(errortext);
	}
	std::terminate(); // recursive I_FatalErrors must immediately terminate.
}

