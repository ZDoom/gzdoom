/*
** c_dispatch.cpp
** Functions for executing console commands and aliases
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "c_commandline.h"
#include "c_cvars.h"
#include "v_text.h"

// ParseCommandLine
//
// Parse a command line (passed in args). If argc is non-NULL, it will
// be set to the number of arguments. If argv is non-NULL, it will be
// filled with pointers to each argument; argv[0] should be initialized
// to point to a buffer large enough to hold all the arguments. The
// return value is the necessary size of this buffer.
//
// Special processing:
//   Inside quoted strings, \" becomes just "
//                          \\ becomes just a single backslash          
//							\c becomes just TEXTCOLOR_ESCAPE
//   $<cvar> is replaced by the contents of <cvar>

static size_t ParseCommandLine(const char* args, int* argc, char** argv, bool no_escapes)
{
	int count;
	char* buffstart;
	char* buffplace;

	count = 0;
	buffstart = NULL;
	if (argv != NULL)
	{
		buffstart = argv[0];
	}
	buffplace = buffstart;

	for (;;)
	{
		while (*args <= ' ' && *args)
		{ // skip white space
			args++;
		}
		if (*args == 0)
		{
			break;
		}
		else if (*args == '\"')
		{ // read quoted string
			char stuff;
			if (argv != NULL)
			{
				argv[count] = buffplace;
			}
			count++;
			args++;
			do
			{
				stuff = *args++;
				if (!no_escapes && stuff == '\\' && *args == '\"')
				{
					stuff = '\"', args++;
				}
				else if (!no_escapes && stuff == '\\' && *args == '\\')
				{
					args++;
				}
				else if (!no_escapes && stuff == '\\' && *args == 'c')
				{
					stuff = TEXTCOLOR_ESCAPE, args++;
				}
				else if (stuff == '\"')
				{
					stuff = 0;
				}
				else if (stuff == 0)
				{
					args--;
				}
				if (argv != NULL)
				{
					*buffplace = stuff;
				}
				buffplace++;
			} while (stuff);
		}
		else
		{ // read unquoted string
			const char* start = args++, * end;
			FBaseCVar* var;
			UCVarValue val;

			while (*args && *args > ' ' && *args != '\"')
				args++;
			if (*start == '$' && (var = FindCVarSub(start + 1, int(args - start - 1))))
			{
				val = var->GetGenericRep(CVAR_String);
				start = val.String;
				end = start + strlen(start);
			}
			else
			{
				end = args;
			}
			if (argv != NULL)
			{
				argv[count] = buffplace;
				while (start < end)
					*buffplace++ = *start++;
				*buffplace++ = 0;
			}
			else
			{
				buffplace += end - start + 1;
			}
			count++;
		}
	}
	if (argc != NULL)
	{
		*argc = count;
	}
	return (buffplace - buffstart);
}

FCommandLine::FCommandLine (const char *commandline, bool no_escapes)
{
	cmd = commandline;
	_argc = -1;
	_argv = NULL;
	noescapes = no_escapes;
}

FCommandLine::~FCommandLine ()
{
	if (_argv != NULL)
	{
		delete[] _argv;
	}
}

void FCommandLine::Shift()
{
	// Only valid after _argv has been filled.
	for (int i = 1; i < _argc; ++i)
	{
		_argv[i - 1] = _argv[i];
	}
}

int FCommandLine::argc ()
{
	if (_argc == -1)
	{
		argsize = ParseCommandLine (cmd, &_argc, NULL, noescapes);
	}
	return _argc;
}

const char *FCommandLine::operator[] (int i)
{
	if (_argv == NULL)
	{
		int count = argc();
		_argv = new char *[count + (argsize+sizeof(char*)-1)/sizeof(char*)];
		_argv[0] = (char *)_argv + count*sizeof(char *);
		ParseCommandLine (cmd, NULL, _argv, noescapes);
	}
	return _argv[i];
}
