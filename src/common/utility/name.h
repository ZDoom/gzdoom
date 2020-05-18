/*
** name.h
**
**---------------------------------------------------------------------------
** Copyright 2005-2007 Randy Heit
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

#ifndef NAME_H
#define NAME_H

#include "tarray.h"
#include "zstring.h"

enum ENamedName
{
#define xx(n) NAME_##n,
#include "namedef.h"
#undef xx
};

class FString;

class FName
{
public:
	FName() = default;
	FName (const char *text) { Index = NameData.FindName (text, false); }
	FName (const char *text, bool noCreate) { Index = NameData.FindName (text, noCreate); }
	FName (const char *text, size_t textlen, bool noCreate) { Index = NameData.FindName (text, textlen, noCreate); }
	FName(const FString& text) { Index = NameData.FindName(text.GetChars(), text.Len(), false); }
	FName(const FString& text, bool noCreate) { Index = NameData.FindName(text.GetChars(), text.Len(), noCreate); }
	FName (const FName &other) = default;
	FName (ENamedName index) { Index = index; }
 //   ~FName () {}	// Names can be added but never removed.

	int GetIndex() const { return Index; }
	const char *GetChars() const { return NameData.NameArray[Index].Text; }

	FName &operator = (const char *text) { Index = NameData.FindName (text, false); return *this; }
	FName& operator = (const FString& text) { Index = NameData.FindName(text.GetChars(), text.Len(), false); return *this; }
	FName &operator = (const FName &other) = default;
	FName &operator = (ENamedName index) { Index = index; return *this; }

	int SetName (const char *text, bool noCreate=false) { return Index = NameData.FindName (text, noCreate); }

	bool IsValidName() const { return (unsigned)Index < (unsigned)NameData.NumNames; }

	// Note that the comparison operators compare the names' indices, not
	// their text, so they cannot be used to do a lexicographical sort.
	bool operator == (const FName &other) const { return Index == other.Index; }
	bool operator != (const FName &other) const { return Index != other.Index; }
	bool operator <  (const FName &other) const { return Index <  other.Index; }
	bool operator <= (const FName &other) const { return Index <= other.Index; }
	bool operator >  (const FName &other) const { return Index >  other.Index; }
	bool operator >= (const FName &other) const { return Index >= other.Index; }

	bool operator == (ENamedName index) const { return Index == index; }
	bool operator != (ENamedName index) const { return Index != index; }
	bool operator <  (ENamedName index) const { return Index <  index; }
	bool operator <= (ENamedName index) const { return Index <= index; }
	bool operator >  (ENamedName index) const { return Index >  index; }
	bool operator >= (ENamedName index) const { return Index >= index; }

protected:
	int Index;

	struct NameEntry
	{
		char *Text;
		unsigned int Hash;
		int NextHash;
	};

	struct NameManager
	{
		// No constructor because we can't ensure that it actually gets
		// called before any FNames are constructed during startup. This
		// means this struct must only exist in the program's BSS section.
		~NameManager();

		enum { HASH_SIZE = 1024 };
		struct NameBlock;

		NameBlock *Blocks;
		NameEntry *NameArray;
		int NumNames, MaxNames;
		int Buckets[HASH_SIZE];

		int FindName (const char *text, bool noCreate);
		int FindName (const char *text, size_t textlen, bool noCreate);
		int AddName (const char *text, unsigned int hash, unsigned int bucket);
		NameBlock *AddBlock (size_t len);
		void InitBuckets ();
		static bool Inited;
	};

	static NameManager NameData;
};


template<> struct THashTraits<FName>
{
	hash_t Hash(FName key)
	{
		return key.GetIndex();
	}
	int Compare(FName left, FName right) { return left != right; }
}; 
#endif
