/*
** stringtable.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
**
** FStringTable
**
** This class manages a list of localizable strings stored in a wad file.
** It does not support adding new strings, although existing ones can
** be changed.
*/

#ifndef __STRINGTABLE_H__
#define __STRINGTABLE_H__

#ifdef _MSC_VER
#pragma once
#endif


#include <stdlib.h>
#include "doomtype.h"

class FStringTable
{
public:
	FStringTable () :
		StringStatus(NULL),
		NumStrings (0),
		Names(NULL),
		Strings(NULL),
		CompactBase(NULL),
		CompactSize(0),
		LumpNum (-1) {}
	~FStringTable () { FreeData (); }

	void LoadStrings (int lump, int expectedSize, bool enuOnly);
	void ReloadStrings ();

	void LoadNames () const;
	void FlushNames () const;
	int FindString (const char *stringName) const;
	int MatchString (const char *string) const;

	void SetString (int index, const char *newString);
	void Compact ();
	const char *operator() (int index) { return Strings[index]; }

private:
	struct Header;

	BYTE *StringStatus;
	int NumStrings;
	mutable BYTE *Names;
	char **Strings;
	char *CompactBase;
	size_t CompactSize;
	int LumpNum;

	void FreeData ();
	void FreeStrings ();
	void FreeStandardStrings ();
	int SumStringSizes () const;
	int LoadLanguage (DWORD code, bool exactMatch, BYTE *startPos, BYTE *endPos);
	void DoneLoading (BYTE *startPos, BYTE *endPos);
};

#endif //__STRINGTABLE_H__
