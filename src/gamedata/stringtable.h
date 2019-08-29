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
#include "doomdef.h"
#include "doomtype.h"

struct TableElement
{
	int filenum;
	FString strings[4];
};

// This public interface is for Dehacked
class StringMap : public TMap<FName, TableElement>
{
public:
	const char *MatchString(const char *string) const;
};


struct StringMacro
{
	FString Replacements[4];
};


class FStringTable
{
public:
	enum : uint32_t
	{
		default_table = MAKE_ID('*', '*', 0, 0),
		global_table = MAKE_ID('*', 0, 0, 0),
		dehacked_table = MAKE_ID('*', '*', '*', 0)
	};

	using LangMap = TMap<uint32_t, StringMap>;
	using StringMacroMap = TMap<FName, StringMacro>;

	void LoadStrings ();
	void UpdateLanguage();
	StringMap GetDefaultStrings() { return allStrings[default_table]; }	// Dehacked needs these for comparison
	void SetDehackedStrings(StringMap && map)
	{
		allStrings.Insert(dehacked_table, map);
		UpdateLanguage();
	}
	
	const char *GetLanguageString(const char *name, uint32_t langtable, int gender = -1) const;
	bool MatchDefaultString(const char *name, const char *content) const;
	const char *GetString(const char *name, uint32_t *langtable, int gender = -1) const;
	const char *operator() (const char *name) const;	// Never returns NULL
	const char *operator[] (const char *name) const
	{
		return GetString(name, nullptr);
	}
	bool exists(const char *name);

private:

	StringMacroMap allMacros;
	LangMap allStrings;
	TArray<std::pair<uint32_t, StringMap*>> currentLanguageSet;

	void LoadLanguage (int lumpnum, const TArray<uint8_t> &buffer);
	TArray<TArray<FString>> parseCSV(const TArray<uint8_t> &buffer);
	bool ParseLanguageCSV(int lumpnum, const TArray<uint8_t> &buffer);

	bool LoadLanguageFromSpreadsheet(int lumpnum, const TArray<uint8_t> &buffer);
	bool readMacros(int lumpnum);
	void InsertString(int lumpnum, int langid, FName label, const FString &string);
	void DeleteString(int langid, FName label);
	void DeleteForLabel(int lumpnum, FName label);

	static size_t ProcessEscapes (char *str);
};

#endif //__STRINGTABLE_H__
