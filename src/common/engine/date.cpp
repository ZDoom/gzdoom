/*
** date.cpp
**
** VM exports for engine backend classes
**
**---------------------------------------------------------------------------
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
**
*/ 

#include <time.h>
#include "c_dispatch.h"
#include "vm.h"
#include "zstring.h"
#include "printf.h"

time_t 			epochoffset = 0;		// epoch start in seconds (0 = January 1st, 1970)


//==========================================================================
//
// CCMD setdate
//
// Set the time to a specific value
//
//==========================================================================

UNSAFE_CCMD(setdate)
{
	if (argv.argc() != 3)
	{
		Printf("setdate HH:MM:SS DD-MM-YYYY: Set the current date\n");
		return;
	}

	time_t today;
	time(&today);
	struct tm* timeinfo = localtime(&today);
	if (timeinfo != nullptr)
	{
		auto clock = FString(argv[1]).Split(":");
		auto date  = FString(argv[2]).Split("-");
		if(clock.Size() != 3 || date.Size() != 3)
		{
			Printf("setdate HH:MM:SS DD-MM-YYYY: Set the current date\n");
			return;
		}

		if(!clock[0].IsInt())
		{
			Printf("Invalid hour\n");
			return;
		}
		if (!clock[1].IsInt())
		{
			Printf("Invalid minutes\n");
			return;
		}
		if (!clock[2].IsInt())
		{
			Printf("Invalid seconds\n");
			return;
		}
		if (!date[0].IsInt())
		{
			Printf("Invalid day\n");
			return;
		}
		if (!date[1].IsInt())
		{
			Printf("Invalid month\n");
			return;
		}
		if (!date[2].IsInt())
		{
			Printf("Invalid year\n");
			return;
		}

		//Set Date
		timeinfo->tm_hour = int( clock[0].ToLong() );
		timeinfo->tm_min  = int( clock[1].ToLong() );
		timeinfo->tm_sec  = int( clock[2].ToLong() );
		timeinfo->tm_mday = int(  date[0].ToLong() );
		timeinfo->tm_mon  = int(  date[1].ToLong() - 1);     // Month interally is 0 - 11
		timeinfo->tm_year = int(  date[2].ToLong() - 1900 ); // Year interally is 00 - 138

		time_t newTime = mktime(timeinfo);
		tm* t_old = localtime(&today);
		time_t oldTime = mktime(t_old);

		if (newTime == -1 || oldTime == -1)
		{
			Printf("Unable to set the date\n");
			return;
		}

		epochoffset = newTime - oldTime;

		// This deals with some inconsistent display behaviour for DST
		// In this case, we want to emulate GCC's behaviour
		today += epochoffset;
		struct tm* t_new = localtime(&today);
		if (t_new != nullptr)
		{
			char timeString[1024];
			if (strftime(timeString, sizeof(timeString), "%H", t_new))
			{
				auto hour = FString(timeString).ToLong();
				if (hour - clock[0].ToLong() == -1 || hour - clock[0].ToLong() == 23)
					epochoffset += 3600;
				else if (hour - clock[0].ToLong() == 1 || hour - clock[0].ToLong() == -23)
					epochoffset -= 3600;
			}
		}

		return;
	}
	else
	{
		Printf("Unable to set the date\n");
		return;
	}
}

CCMD(getdate)
{
	time_t now;
	time(&now);
	now += epochoffset;
	struct tm* timeinfo = localtime(&now);
	if (timeinfo != nullptr)
	{
		char timeString[1024];
		if (strftime(timeString, sizeof(timeString), "%H:%M:%S %d-%m-%Y%n", timeinfo))
			Printf("%s\n", timeString);
		else
			Printf("Error Retrieving Current Date\n");
	}
	else
	{
		Printf("Error Retrieving Current Date\n");
	}
}

//=====================================================================================
//
//
//
//=====================================================================================

extern time_t epochoffset;

static int GetEpochTime()
{
	time_t now;
	time(&now);
	return now != (time_t)(-1) ? int(now + epochoffset) : -1;
}

//Returns an empty string if the Strf tokens are valid, otherwise returns the problematic token
static FString CheckStrfString(FString timeForm)
{
	// Valid Characters after %
	const char validSingles[] = { 'a','A','b','B','c','C','d','D','e','F','g','G','h','H','I','j','m','M','n','p','r','R','S','t','T','u','U','V','w','W','x','X','y','Y','z','Z' };

	timeForm.Substitute("%%", "%a"); //Prevent %% from causing tokenizing problems
	timeForm = "a" + timeForm; //Prevent %* at the beginning from causing a false error from tokenizing

	auto tokens = timeForm.Split("%");
	for (auto t : tokens)
	{
		bool found = false;
		// % at end
		if (t.Len() == 0) return FString("%");

		// Single Character
		for (size_t i = 0; i < sizeof(validSingles) / sizeof(validSingles[0]); i++)
		{
			if (t[0] == validSingles[i])
			{
				found = true;
				break;
			}
		}
		if (found) continue;
		return FString("%") + t[0];
	}
	return "";
}

static void FormatTime(const FString& timeForm, int timeVal, FString* result)
{
	FString error = CheckStrfString(timeForm);
	if (!error.IsEmpty())
		ThrowAbortException(X_FORMAT_ERROR, "'%s' is not a valid format specifier of SystemTime.Format()", error.GetChars());

	time_t val = timeVal;
	struct tm* timeinfo = localtime(&val);
	if (timeinfo != nullptr)
	{
		char timeString[1024];
		if (strftime(timeString, sizeof(timeString), timeForm, timeinfo))
			*result = timeString;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_SystemTime, Now, GetEpochTime)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(GetEpochTime());
}

DEFINE_ACTION_FUNCTION_NATIVE(_SystemTime, Format, FormatTime)
{
	PARAM_PROLOGUE;
	PARAM_STRING(timeForm);
	PARAM_INT(timeVal);
	FString result;
	FormatTime(timeForm, timeVal, &result);
	ACTION_RETURN_STRING(result);
}


