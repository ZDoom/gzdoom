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
#include "basics.h"
#include "zstring.h"
#include "tarray.h"
#include "name.h"

class StringMap;

struct TableElement
{
	int filenum;
private:
	bool is_random;
	union
	{
		FString strings[4]; // is_random = false
		TArray<FName> rand_alternatives; // is_random = true , can only point to non-random entries
	};
public:
	FString& Get(int gender,StringMap&);
	const FString& Get(int gender, const StringMap&) const;

	bool isRandom() const
		{ return is_random; }

	const TArray<FName> * GetAll() const
		{ return is_random ? &rand_alternatives : nullptr; }

	TableElement()
		: filenum(0), is_random(false) , strings()
	{}

	TableElement(int _filenum, FString g1, FString g2, FString g3, FString g4)  // non-random
		: filenum(_filenum), is_random(false) , strings {g1, g2, g3, g4}
	{}
	
	TableElement(int _filenum, const TArray<FName> &_rand_alternatives) // random
		: filenum(_filenum), is_random(true) , rand_alternatives(_rand_alternatives)
	{}

	TableElement(int _filenum, TArray<FName> && _rand_alternatives) // random
		: filenum(_filenum), is_random(true) , rand_alternatives(std::move(_rand_alternatives))
	{}

	TableElement(const TableElement &other)
		: filenum(other.filenum) , is_random(other.is_random)
	{
		if(other.is_random)
		{
			new(&rand_alternatives) TArray<FName>(other.rand_alternatives);
		}
		else
		{
			new(&strings) FString[4] {other.strings[0] , other.strings[1] , other.strings[2] , other.strings[3]};
		}
	}

	TableElement(TableElement && other)
		: filenum(other.filenum) , is_random(other.is_random)
	{
		if(other.is_random)
		{
			new(&rand_alternatives) TArray<FName>(std::move(other.rand_alternatives));
		}
		else
		{
			new(&strings) FString[4] {std::move(other.strings[0]) , std::move(other.strings[1]) , std::move(other.strings[2]) , std::move(other.strings[3])};
		}
	}

	TableElement& operator=(const TableElement &other)
	{
		if(other.is_random)
		{
			if(!is_random)
			{
				strings[0].~FString();
				strings[1].~FString();
				strings[2].~FString();
				strings[3].~FString();
				new(&rand_alternatives) TArray<FName>(other.rand_alternatives);
			}
			else
			{
				rand_alternatives = other.rand_alternatives;
			}
		}
		else
		{
			if(is_random)
			{
				rand_alternatives.~TArray();
				new(&strings) FString[4] {other.strings[0] , other.strings[1] , other.strings[2] , other.strings[3]};
			}
			else
			{
				strings[0] = other.strings[0];
				strings[1] = other.strings[1];
				strings[2] = other.strings[2];
				strings[3] = other.strings[3];
			}
		}
		filenum = other.filenum;
		is_random = other.is_random;
		return *this;
	}

	TableElement& operator=(TableElement && other)
	{
		if(other.is_random)
		{
			if(!is_random)
			{
				strings[0].~FString();
				strings[1].~FString();
				strings[2].~FString();
				strings[3].~FString();
				new(&rand_alternatives) TArray<FName>(std::move(other.rand_alternatives));
			}
			else
			{
				rand_alternatives = std::move(other.rand_alternatives);
			}
		}
		else
		{
			if(is_random)
			{
				rand_alternatives.~TArray();
				new(&strings) FString[4] {std::move(other.strings[0]) , std::move(other.strings[1]) , std::move(other.strings[2]) , std::move(other.strings[3])};
			}
			else
			{
				strings[0] = std::move(other.strings[0]);
				strings[1] = std::move(other.strings[1]);
				strings[2] = std::move(other.strings[2]);
				strings[3] = std::move(other.strings[3]);
			}
		}
		filenum = other.filenum;
		is_random = other.is_random;
		return *this;
	}

	~TableElement()
	{
		if(is_random)
		{
			rand_alternatives.~TArray();
		}
		else
		{
			strings[0].~FString();
			strings[1].~FString();
			strings[2].~FString();
			strings[3].~FString();
		}
	}

	friend class FStringTable;
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
		override_table = MAKE_ID('*', '*', '*', 0)
	};

	using LangMap = TMap<uint32_t, StringMap>;
	using StringMacroMap = TMap<FName, StringMacro>;

	void LoadStrings(const char *language);
	void UpdateLanguage(const char* language);
	StringMap GetDefaultStrings() { return allStrings[default_table]; }	// Dehacked needs these for comparison
	void SetOverrideStrings(StringMap && map)
	{
		allStrings.Insert(override_table, map);
		UpdateLanguage(nullptr);
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

	void InsertString(int lumpnum, int langid, FName label, const FString& string);

private:

	FString activeLanguage;
	StringMacroMap allMacros;
	LangMap allStrings;
	TArray<std::pair<uint32_t, StringMap*>> currentLanguageSet;

	void LoadLanguage (int lumpnum, const TArray<uint8_t> &buffer);
	TArray<TArray<FString>> parseCSV(const TArray<uint8_t> &buffer);
	bool ParseLanguageCSV(int lumpnum, const TArray<uint8_t> &buffer);

	bool LoadLanguageFromSpreadsheet(int lumpnum, const TArray<uint8_t> &buffer);
	bool readMacros(int lumpnum);
	void DeleteString(int langid, FName label);
	void DeleteForLabel(int lumpnum, FName label);

	static size_t ProcessEscapes (char *str);

	struct LangRandomEntry
	{
		bool operator == (const LangRandomEntry& other) const
			{ return name == other.name; }

		FName name;
		bool persistent;
		TArray<FName> alternatives;
	};
	void ProcessRandomEntries(int lumpnum, TArray<LangRandomEntry> &entries, StringMap &map);

public:
	static FString MakeMacro(const char *str)
	{
		if (*str == '$') return str;
		return FString("$") + str;
	}

	static FString MakeMacro(const char *str, size_t len)
	{
		if (*str == '$') return FString(str, len);
		return "$" + FString(str, len);
	}

	const char* localize(const char* str)
	{
		return *str == '$' ? operator()(str + 1) : str;
	}
};

#endif //__STRINGTABLE_H__
