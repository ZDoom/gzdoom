/*
** stringtable.h
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
**
** FStringTable
**
** This class manages a list of localizable strings stored in a wad file.
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
	struct StringEntry;

	FStringTable ();
	~FStringTable ();

	void LoadStrings (bool enuOnly);

	const char *operator() (const char *name) const;	// Never returns NULL
	const char *operator[] (const char *name) const;	// Can return NULL

	const char *MatchString (const char *string) const;
	void SetString (const char *name, const char *newString);

private:
	enum { HASH_SIZE = 128 };

	StringEntry *Buckets[HASH_SIZE];

	void FreeData ();
	void FreeNonDehackedStrings ();
	void LoadLanguage (int lumpnum, DWORD code, bool exactMatch, int passnum);
	static size_t ProcessEscapes (char *str);
	void FindString (const char *stringName, StringEntry **&pentry, StringEntry *&entry);
};

#endif //__STRINGTABLE_H__
