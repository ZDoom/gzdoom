/*
** m_argv.cpp
** Manages command line arguments
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <string.h>
#include "m_argv.h"
#include "cmdlib.h"

IMPLEMENT_CLASS (DArgs)

//===========================================================================
//
// DArgs Default Constructor
//
//===========================================================================

DArgs::DArgs()
{
}

//===========================================================================
//
// DArgs Copy Constructor
//
//===========================================================================

DArgs::DArgs(const DArgs &other)
{
	Argv = other.Argv;
}

//===========================================================================
//
// DArgs Argv Constructor
//
//===========================================================================

DArgs::DArgs(int argc, char **argv)
{
	SetArgs(argc, argv);
}

//===========================================================================
//
// DArgs Copy Operator
//
//===========================================================================

DArgs &DArgs::operator=(const DArgs &other)
{
	Argv = other.Argv;
	return *this;
}

//===========================================================================
//
// DArgs :: SetArgs
//
//===========================================================================

void DArgs::SetArgs(int argc, char **argv)
{
	Argv.Resize(argc);
	for (int i = 0; i < argc; ++i)
	{
		Argv[i] = argv[i];
	}
}

//===========================================================================
//
// DArgs :: FlushArgs
//
//===========================================================================

void DArgs::FlushArgs()
{
	Argv.Clear();
}

//===========================================================================
//
// DArgs :: CheckParm
//
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present
//
//===========================================================================

int DArgs::CheckParm(const char *check, int start) const
{
	for (unsigned i = start; i < Argv.Size(); ++i)
	{
		if (0 == stricmp(check, Argv[i]))
		{
			return i;
		}
	}
	return 0;
}

//===========================================================================
//
// DArgs :: CheckValue
//
// Like CheckParm, but it also checks that the parameter has a value after
// it and returns that or NULL if not present.
//
//===========================================================================

const char *DArgs::CheckValue(const char *check) const
{
	int i = CheckParm(check);

	if (i > 0 && i < (int)Argv.Size() - 1)
	{
		i++;
		return Argv[i][0] != '+' && Argv[i][0] != '-' ? Argv[i].GetChars() : NULL;
	}
	else
	{
		return NULL;
	}
}

//===========================================================================
//
// DArgs :: TakeValue
//
// Like CheckValue, except it also removes the parameter and its argument
// (if present) from argv.
//
//===========================================================================

FString DArgs::TakeValue(const char *check)
{
	int i = CheckParm(check);
	FString out;

	if (i > 0 && i < (int)Argv.Size() - 1 &&
		Argv[i+1][0] != '+' && Argv[i+1][0] != '-')
	{
		out = Argv[i+1];
		Argv.Delete(i, 2);	// Delete the parm and its value.
	}
	else
	{
		Argv.Delete(i);		// Just delete the parm, since it has no value.
	}
	return out;
}

//===========================================================================
//
// DArgs :: GetArg
//
// Gets the argument at a particular position.
//
//===========================================================================

const char *DArgs::GetArg(int arg) const
{
	return ((unsigned)arg < Argv.Size()) ? Argv[arg].GetChars() : NULL;
		return Argv[arg];
}

//===========================================================================
//
// DArgs :: GetArgList
//
// Returns a pointer to the FString at a particular position.
//
//===========================================================================

FString *DArgs::GetArgList(int arg) const
{
	return ((unsigned)arg < Argv.Size()) ? &Argv[arg] : NULL;
}

//===========================================================================
//
// DArgs :: NumArgs
//
//===========================================================================

int DArgs::NumArgs() const
{
	return (int)Argv.Size();
}

//===========================================================================
//
// DArgs :: AppendArg
//
// Adds another argument to argv. Invalidates any previous results from
// GetArgList().
//
//===========================================================================

void DArgs::AppendArg(FString arg)
{
	Argv.Push(arg);
}

//===========================================================================
//
// DArgs :: GatherFiles
//
//===========================================================================

DArgs *DArgs::GatherFiles (const char *param, const char *extension, bool acceptNoExt) const
{
	DArgs *out = new DArgs;
	unsigned int i;
	size_t extlen = strlen (extension);

	if (extlen > 0)
	{
		for (i = 1; i < Argv.Size() && Argv[i][0] != '-' && Argv[i][0] != '+'; i++)
		{
			size_t len = Argv[i].Len();
			if (len >= extlen && stricmp(&Argv[i][0] + len - extlen, extension) == 0)
			{
				out->AppendArg(Argv[i]);
			}
			else if (acceptNoExt)
			{
				const char *src = &Argv[i][0] + len - 1;

				while (src != Argv[i] && *src != '/'
			#ifdef _WIN32
					&& *src != '\\'
			#endif
					)
				{
					if (*src == '.')
						goto morefor;					// it has an extension
					src--;
				}
				out->AppendArg(Argv[i]);
morefor:
				;
			}
		}
	}
	if (param != NULL)
	{
		i = 1;
		while (0 != (i = CheckParm (param, i)))
		{
			for (++i; i < Argv.Size() && Argv[i][0] != '-' && Argv[i][0] != '+'; ++i)
				out->Argv.Push(Argv[i]);
		}
	}
	return out;
}
