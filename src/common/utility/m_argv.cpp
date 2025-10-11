/*
** m_argv.cpp
** Manages command line arguments
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2017-2025 GZDoom Maintainers and Contributors
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

#include <cassert>
#include "m_argv.h"
#include "name.h"
#include "tarray.h"
#include "zstring.h"

#ifdef __linux
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

FArg::FArg(
	const char * _name,
	const char * _section,
	const char * _summary,
	const char * _usage,
	const char * _details,
	bool _advanced
):
	name(_name),
	section(_section),
	summary(_summary),
	usage(_usage),
	advanced(_advanced)
{
	assert(FArg::Available().CheckKey(name) == nullptr);

	if (section == "")
	{
		section = "Unknown";
		advanced = true;
	}

	details = (strlen(_details) > 0)
		? _details
		: _summary;

	FArg::Available().Insert(name, this);
}

//===========================================================================
//
// FArgs Default Constructor
//
//===========================================================================

FArgs::FArgs()
{
}

//===========================================================================
//
// FArgs Copy Constructor
//
//===========================================================================

FArgs::FArgs(const FArgs &other)
: Argv(other.Argv)
{
}

//===========================================================================
//
// FArgs Argv Constructor
//
//===========================================================================

FArgs::FArgs(int argc, char **argv)
{
	SetRawArgs(argc, argv);
}

//===========================================================================
//
// FArgs Argv Constructor
//
//===========================================================================

FArgs::FArgs(int argc, const char** argv)
{
	SetRawArgs(argc, const_cast<char **>(argv));	// Thanks, C++, for the inflexible const casting rules...
}

//===========================================================================
//
// FArgs String Argv Constructor
//
//===========================================================================

FArgs::FArgs(int argc, FString *argv)
{
	AppendRawArgs(argc, argv);
}

//===========================================================================
//
// FArgs Copy Operator
//
//===========================================================================

FArgs &FArgs::operator=(const FArgs &other)
{
	Argv = other.Argv;
	return *this;
}

//===========================================================================
//
// FArgs :: SetRawArgs
//
//===========================================================================

void FArgs::SetRawArgs(int argc, char **argv)
{
	Argv.Resize(argc);
	for (int i = 0; i < argc; ++i)
	{
		Argv[i] = argv[i];
	}
}

//===========================================================================
//
// FArgs :: FlushArgs
//
//===========================================================================

void FArgs::FlushArgs()
{
	Argv.Clear();
}

//===========================================================================
//
// FArgs :: CheckParm
//
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present
//
//===========================================================================


int FArgs::CheckParm(const FArg check, int start) const
{
	const FArg * array[] = { &check, nullptr };
	return CheckParm(array, start);
}

int FArgs::TestArgList(const FArg** check, const char* str)
{
	for (int i = 0; check[i]; i++)
	{
		const char * name = check[i]->name.GetChars();
		if (!stricmp(name, str)) return 0;
	}
	return 1;	// we do not care about order here.
}

int FArgs::CheckParm(const FArg ** check, int start) const
{
	for (unsigned i = start; i < Argv.Size(); ++i)
	{
		if (0 == TestArgList(check, Argv[i].GetChars()))
		{
			return i;
		}
	}
	return 0;
}

//===========================================================================
//
// FArgs :: CheckParmList
//
// Returns the number of arguments after the parameter (if found) and also
// returns a pointer to the first argument.
//
//===========================================================================

int FArgs::CheckParmList(const FArg check, FString **strings, int start) const
{
	unsigned int i, parmat = CheckParm(check, start);

	if (parmat == 0)
	{
		if (strings != nullptr)
		{
			*strings = nullptr;
		}
		return 0;
	}
	for (i = ++parmat; i < Argv.Size(); ++i)
	{
		if (Argv[i][0] == '-' || Argv[i][0] == '+')
		{
			break;
		}
	}
	if (strings != nullptr)
	{
		*strings = &Argv[parmat];
	}
	return i - parmat;
}

//===========================================================================
//
// FArgs :: CheckValue
//
// Like CheckParm, but it also checks that the parameter has a value after
// it and returns that or nullptr if not present.
//
//===========================================================================

const char * FArgs::CheckValue(const FArg check) const
{
	int i = CheckParm(check);

	if (i > 0 && i < (int)Argv.Size() - 1)
	{
		i++;
		return Argv[i][0] != '+' && Argv[i][0] != '-' ? Argv[i].GetChars() : nullptr;
	}
	else
	{
		return nullptr;
	}
}

//===========================================================================
//
// FArgs :: TakeValue
//
// Like CheckValue, except it also removes the parameter and its argument
// (if present) from argv.
//
//===========================================================================

FString FArgs::TakeValue(const FArg check)
{
	int i = CheckParm(check);
	FString out;

	if (i > 0 && i < (int)Argv.Size())
	{
		if (i < (int)Argv.Size() - 1 && Argv[i+1][0] != '+' && Argv[i+1][0] != '-')
		{
			out = Argv[i+1];
			Argv.Delete(i, 2);	// Delete the parm and its value.
		}
		else
		{
			Argv.Delete(i);		// Just delete the parm, since it has no value.
		}
	}
	return out;
}

//===========================================================================
//
// FArgs :: RemoveArg
//
//===========================================================================

void FArgs::RemoveArgs(const FArg check)
{
	int i = CheckParm(check);

	if (i > 0 && i < (int)Argv.Size() - 1)
	{
		do
		{
			RemoveArg(i);
		}
		while (Argv[i][0] != '+' && Argv[i][0] != '-' && i < (int)Argv.Size() - 1);
	}
}

//===========================================================================
//
// FArgs :: GetArg
//
// Gets the argument at a particular position.
//
//===========================================================================

const char *FArgs::GetArg(int arg) const
{
	return ((unsigned)arg < Argv.Size()) ? Argv[arg].GetChars() : nullptr;
}

//===========================================================================
//
// FArgs :: GetArgList
//
// Returns a pointer to the FString at a particular position.
//
//===========================================================================

FString *FArgs::GetArgList(int arg) const
{
	return ((unsigned)arg < Argv.Size()) ? &Argv[arg] : nullptr;
}

//===========================================================================
//
// FArgs :: NumArgs
//
//===========================================================================

int FArgs::NumArgs() const
{
	return (int)Argv.Size();
}

//===========================================================================
//
// FArgs :: AppendRawArg
//
// Adds another argument to argv. Invalidates any previous results from
// GetArgList(). This is to only be used for parsing text from external
// sources, not from internal sources
//
//===========================================================================

void FArgs::AppendRawArg(FString arg)
{
	Argv.Push(arg);
}

//===========================================================================
//
// FArgs :: AppendArg
//
// Adds another argument to argv. Invalidates any previous results from
// GetArgList().
//
//===========================================================================

void FArgs::AppendArg(const FArg arg)
{
	Argv.Push(arg.name.GetChars());
}

//===========================================================================
//
// FArgs :: AppendRawArgs
//
// Adds an array of FStrings to argv. Invalidates any previous results from
// GetArgList(). This is to only be used for parsing text from external
// sources, not from internal sources
//
//===========================================================================

void FArgs::AppendRawArgs(int argc, const FString *argv)
{
	if (argv != NULL && argc > 0)
	{
		Argv.Grow(argc);
		for (int i = 0; i < argc; ++i)
		{
			Argv.Push(argv[i]);
		}
	}
}

//===========================================================================
//
// FArgs :: AppendRawArgsString
//
// Adds extra args as a space-separated string, supporting simple quoting,
// and inserting -file args into the right place. Invalidates any previous
// results from GetArgList(). This is to only be used for parsing text from
// externalw sources, not from internal sources
//
//===========================================================================

void FArgs::AppendRawArgsString(FString argv)
{
	auto file_index = Argv.Find("-file");
	auto files_end = file_index + 1;

	for (; files_end < Argv.Size() && Argv[files_end][0] != '-' && Argv[files_end][0] != '+'; ++files_end);

	if(file_index == Argv.Size())
	{
		Argv.Push("-file");
	}

	bool inserting_file = true;

	argv.StripLeftRight();

	size_t i = 0;
	size_t lastSection = 0;
	size_t lastStart = 0;
	char lastQuoteType = 0;

	FString tmp;
	bool has_tmp = false;

	for(i = 0; i < argv.Len(); i++)
	{
		if(argv[i] == ' ')
		{
			FString arg = tmp + argv.Mid(lastSection, i - lastSection);

			if(arg[0] == '-' || arg[0] == '+') inserting_file = false;

			if(inserting_file)
			{
				Argv.Insert(files_end++, arg);
			}
			else if(arg.Compare("-file") == 0)
			{
				inserting_file = true;
			}
			else
			{
				files_end++;
				Argv.Insert(file_index++, arg);
			}

			lastSection = i + 1;
			tmp = "";
			has_tmp = false;
			for(;(i + 1) < argv.Len() && argv[i + 1] == ' '; i++, lastSection++);
			lastStart = i + 1;
		}
		else if(argv[i] == '\'' || argv[i] == '"')
		{
			lastQuoteType = argv[i];
			tmp += argv.Mid(lastSection, i - lastSection);
			has_tmp = true;
			bool wasSlash = false;

			for(i++; (argv[i] != lastQuoteType || wasSlash) && i < argv.Len(); i++)
			{
				if(i == '\\' && !wasSlash)
				{
					wasSlash = true;
				}
				else
				{
					tmp += argv[i];
					wasSlash = false;
				}
			}
			lastSection = i + 1;
		}
	}

	if(lastSection != i)
	{ // ended on an unquoted section
		FString arg = tmp + argv.Mid(lastSection);
		if(inserting_file)
		{
			Argv.Insert(files_end, arg);
		}
		else if(arg.Compare("-file") != 0)
		{
			Argv.Insert(file_index, arg);
		}
	}
	else if(has_tmp)
	{ // ended on a quote
		if(inserting_file)
		{
			Argv.Insert(files_end, tmp);
		}
		else if(tmp.Compare("-file") != 0)
		{
			Argv.Insert(file_index, tmp);
		}
	}
}
//===========================================================================
//
// FArgs :: RemoveArg
//
// Removes a single argument from argv.
//
//===========================================================================

void FArgs::RemoveArg(int argindex)
{
	Argv.Delete(argindex);
}

//===========================================================================
//
// FArgs :: CollectFiles
//
// Takes all arguments after any instance of -param and any arguments before
// all switches that end in .extension and combines them into a single
// -switch block at the end of the arguments. If extension is nullptr, then
// every parameter before the first switch is added after this -param.
//
//===========================================================================

void FArgs::CollectFiles(const FArg arg, const char* extension)
{
	const char * param = arg.name.GetChars();
	const char* array[] = { param, nullptr };
	CollectFiles(param, array, extension);
}

int stricmp(const char** check, const char* str)
{
	for (int i = 0; check[i]; i++)
	{
		if (!stricmp(check[i], str)) return 0;
	}
	return 1;	// we do not care about order here.
}

int _CheckParm(TArray<FString> Argv, const char ** check, int start)
{
	for (unsigned i = start; i < Argv.Size(); ++i)
	{
		if (0 == stricmp(check, Argv[i].GetChars()))
		{
			return i;
		}
	}
	return 0;
}

void FArgs::CollectFiles(const char *finalname, const char **param, const char *extension)
{
	TArray<FString> work;
	unsigned int i;
	size_t extlen = extension == nullptr ? 0 : strlen(extension);

	// Step 1: Find suitable arguments before the first switch.
	i = 1;
	while (i < Argv.Size() && Argv[i][0] != '-' && Argv[i][0] != '+')
	{
		bool useit;

		if (extlen > 0)
		{ // Argument's extension must match.
			size_t len = Argv[i].Len();
			useit = (len >= extlen && stricmp(&Argv[i][len - extlen], extension) == 0);
		}
		else
		{ // Anything will do so long as it's before the first switch.
			useit = true;
		}
		if (useit)
		{
			work.Push(Argv[i]);
			Argv.Delete(i);
		}
		else
		{
			i++;
		}
	}

	// Step 2: Find each occurence of -param and add its arguments to work.
	while ((i = _CheckParm(Argv, param, i)) > 0)
	{
		Argv.Delete(i);
		while (i < Argv.Size() && Argv[i][0] != '-' && Argv[i][0] != '+')
		{
			work.Push(Argv[i]);
			Argv.Delete(i);
		}
	}

	// Optional: Replace short path names with long path names
#if 0 //def _WIN32
	for (i = 0; i < work.Size(); ++i)
	{
		work[i] = I_GetLongPathName(work[i]);
	}
#endif

	// Step 3: Add work back to Argv, as long as it's non-empty.
	if (work.Size() > 0)
	{
		Argv.Push(finalname);
		AppendRawArgs(work.Size(), &work[0]);
	}
}

//===========================================================================
//
// FArgs :: GatherFiles
//
// Returns all the arguments after the first instance of -param. If you want
// to combine more than one or get switchless stuff included, you need to
// call CollectFiles first.
//
//===========================================================================

FArgs *FArgs::GatherFiles(const FArg param) const
{
	FString *files;
	int filecount;

	filecount = CheckParmList(param, &files);
	return new FArgs(filecount, files);
}
