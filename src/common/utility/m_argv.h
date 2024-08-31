/*
** m_argv.h
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

#ifndef __M_ARGV_H__
#define __M_ARGV_H__

#include "tarray.h"
#include "zstring.h"

//
// MISC
//
class FArgs
{
public:

	typedef TIterator<FString>                  iterator;
	typedef TIterator<const FString>            const_iterator;
	typedef FString								value_type;

	iterator begin()
	{
		return Argv.begin();
	}
	const_iterator begin() const
	{
		return Argv.begin();
	}
	const_iterator cbegin() const
	{
		return Argv.begin();
	}

	iterator end()
	{
		return Argv.end();
	}
	const_iterator end() const
	{
		return Argv.end();
	}
	const_iterator cend() const
	{
		return Argv.end();
	}

	FArgs();
	FArgs(const FArgs &args);
	FArgs(int argc, char **argv);
	FArgs(int argc, const char** argv);
	FArgs(int argc, FString *argv);

	FArgs &operator=(const FArgs &other);
	const FString& operator[](size_t index) { return Argv[index]; }

	void AppendArg(FString arg);
	void AppendArgs(int argc, const FString *argv);
	void AppendArgsString(FString argv);
	void RemoveArg(int argindex);
	void RemoveArgs(const char *check);
	void SetArgs(int argc, char **argv);
	void CollectFiles(const char *finalname, const char** param, const char* extension);
	void CollectFiles(const char *param, const char *extension);
	FArgs *GatherFiles(const char *param) const;
	void SetArg(int argnum, const char *arg);

	int CheckParm(const char *check, int start=1) const;	// Returns the position of the given parameter in the arg list (0 if not found).
	int CheckParm(const char** check, int start = 1) const;	// Returns the position of the given parameter in the arg list (0 if not found). Allows checking for multiple switches
	int CheckParmList(const char *check, FString **strings, int start=1) const;
	const char *CheckValue(const char *check) const;
	const char *GetArg(int arg) const;
	FString *GetArgList(int arg) const;
	FString TakeValue(const char *check);
	int NumArgs() const;
	void FlushArgs();
	TArray<FString>& Array() { return Argv; }

private:
	TArray<FString> Argv;
};

extern FArgs *Args;

#endif //__M_ARGV_H__
