/*
** stringtable.cpp
** Implements the FStringTable class
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
*/

#include <string.h>
#include <stddef.h>

#include "stringtable.h"
#include "cmdlib.h"
#include "m_swap.h"
#include "w_wad.h"
#include "i_system.h"

struct FStringTable::Header
{
	DWORD FileSize;
	WORD NameCount;
	WORD NameLen;
};

void FStringTable::FreeData ()
{
	if (Strings != NULL)
	{
		FreeStrings ();
	}

	if (StringStatus)	delete[] StringStatus;
	if (Strings)		delete[] Strings;
	if (Names)			delete[] Names;

	StringStatus = NULL;
	Strings = NULL;
	Names = NULL;
	NumStrings = 0;
}

void FStringTable::FreeStrings ()
{
	for (int i = 0; i < NumStrings; ++i)
	{
		if (Strings[i] < CompactBase ||
			Strings[i] >= CompactBase + CompactSize)
		{
			delete[] Strings[i];
		}
	}
	if (CompactBase)
	{
		delete[] CompactBase;
		CompactBase = NULL;
		CompactSize = 0;
	}
}

void FStringTable::FreeStandardStrings ()
{
	if (Strings != NULL)
	{
		for (int i = 0; i < NumStrings; ++i)
		{
			if ((StringStatus[i/8] & (1<<(i&7))) == 0)
			{
				if (Strings[i] < CompactBase ||
					Strings[i] >= CompactBase + CompactSize)
				{
					delete[] Strings[i];
				}
				Strings[i] = NULL;
			}
		}
	}
}

#include "doomerrors.h"
void FStringTable::LoadStrings (int lump, int expectedSize, bool enuOnly)
{
	BYTE *strData = (BYTE *)W_MapLumpNum (lump, true);
	int lumpLen = LONG(((Header *)strData)->FileSize);
	int nameCount = SHORT(((Header *)strData)->NameCount);
	int nameLen = SHORT(((Header *)strData)->NameLen);

	int languageStart = sizeof(Header) + nameCount*4 + nameLen;
	languageStart += (4 - languageStart) & 3;

	if (expectedSize >= 0 && nameCount != expectedSize)
	{
		char name[9];

		W_GetLumpName (name, lump);
		name[8] = 0;
		I_FatalError ("%s had %d strings.\nThis version of ZDoom expects it to have %d.",
			name, nameCount, expectedSize);
	}

	FreeStandardStrings ();

	NumStrings = nameCount;
	LumpNum = lump;
	if (Strings == NULL)
	{
		Strings = new char *[nameCount];
		StringStatus = new BYTE[(nameCount+7)/8];
		memset (StringStatus, 0, (nameCount+7)/8);	// 0 means: from wad (standard)
		memset (Strings, 0, sizeof(char *)*nameCount);
	}

	BYTE *const start = strData + languageStart;
	BYTE *const end = strData + lumpLen;
	int loadedCount, i;

	for (loadedCount = i = 0; i < NumStrings; ++i)
	{
		if (Strings[i] != NULL)
		{
			++loadedCount;
		}
	}

	if (!enuOnly)
	{
		for (i = 0; i < 4 && loadedCount != nameCount; ++i)
		{
			loadedCount += LoadLanguage (LanguageIDs[i], true, start, end);
			loadedCount += LoadLanguage (LanguageIDs[i] & MAKE_ID(0xff,0xff,0,0), true, start, end);
			loadedCount += LoadLanguage (LanguageIDs[i], false, start, end);
		}
	}

	// Fill in any missing strings with the default language (enu)
	if (loadedCount != nameCount)
	{
		loadedCount += LoadLanguage (MAKE_ID('e','n','u',0), true, start, end);
	}

	DoneLoading (start, end);

	if (loadedCount != nameCount)
	{
		I_FatalError ("Loaded %d strings (expected %d)", loadedCount, nameCount);
	}

	W_UnMapLump (strData);
}

void FStringTable::ReloadStrings ()
{
	LoadStrings (LumpNum, -1, false);
}

int FStringTable::LoadLanguage (DWORD code, bool exactMatch, BYTE *start, BYTE *end)
{
	const DWORD orMask = exactMatch ? 0 : MAKE_ID(0,0,0xff,0);
	int count = 0;

	code |= orMask;

	while (start < end)
	{
		const DWORD langLen = LONG(*(DWORD *)&start[4]);

		if (((*(DWORD *)start) | orMask) == code)
		{
			start[3] = 1;

			const BYTE *probe = start + 8;

			while (probe < start + langLen)
			{
				int index = probe[0] | (probe[1]<<8);

				if (Strings[index] == NULL)
				{
					Strings[index] = copystring ((const char *)(probe + 2));
					++count;
				}
				probe += 3 + strlen ((const char *)(probe + 2));
			}
		}

		start += langLen + 8;
		start += (4 - (ptrdiff_t)start) & 3;
	}
	return count;
}

void FStringTable::DoneLoading (BYTE *start, BYTE *end)
{
	while (start < end)
	{
		start[3] = 0;
		start += LONG(*(DWORD *)&start[4]) + 8;
		start += (4 - (ptrdiff_t)start) & 3;
	}
}

void FStringTable::SetString (int index, const char *newString)
{
	if ((unsigned)index >= (unsigned)NumStrings)
		return;

	if (Strings[index] < CompactBase ||
		Strings[index] >= CompactBase + CompactSize)
	{
		delete[] Strings[index];
	}
	Strings[index] = copystring (newString);
	StringStatus[index/8] |= 1<<(index&7);
}

// Compact all strings into a single block of memory
void FStringTable::Compact ()
{
	if (NumStrings == 0)
		return;

	int len = SumStringSizes ();
	char *newspace = new char[len];
	char *pos = newspace;
	int i;

	for (i = 0; i < NumStrings; ++i)
	{
		strcpy (pos, Strings[i]);
		pos += strlen (pos) + 1;
	}

	FreeStrings ();

	pos = newspace;
	for (i = 0; i < NumStrings; ++i)
	{
		Strings[i] = pos;
		pos += strlen (pos) + 1;
	}

	CompactBase = newspace;
	CompactSize = len;
}

int FStringTable::SumStringSizes () const
{
	size_t len;
	int i;

	for (i = 0, len = 0; i < NumStrings; ++i)
	{
		len += strlen (Strings[i]) + 1;
	}
	return (int)len;
}


void FStringTable::LoadNames () const
{
	const BYTE *lump = (BYTE *)W_MapLumpNum (LumpNum);
	int nameLen = SHORT(((Header *)lump)->NameLen);

	FlushNames ();
	Names = new BYTE[nameLen + 4*NumStrings];
	memcpy (Names, lump + sizeof(Header), nameLen + 4*NumStrings);
	W_UnMapLump (lump);
}

void FStringTable::FlushNames () const
{
	if (Names != NULL)
	{
		delete[] Names;
		Names = NULL;
	}
}

// Find a string by name
int FStringTable::FindString (const char *name) const
{
	if (Names == NULL)
	{
		LoadNames ();
	}

	const WORD *nameOfs = (WORD *)Names;
	const char *nameBase = (char *)Names + NumStrings*4;

	int min = 0;
	int max = NumStrings-1;

	while (min <= max)
	{
		const int mid = (min + max) / 2;
		const char *const tablename = SHORT(nameOfs[mid*2]) + nameBase;
		const int lex = stricmp (name, tablename);
		if (lex == 0)
			return nameOfs[mid*2+1];
		else if (lex < 0)
			max = mid - 1;
		else
			min = mid + 1;
	}
	return -1;
}

// Find a string with the same text
int FStringTable::MatchString (const char *string) const
{
	for (int i = 0; i < NumStrings; i++)
	{
		if (strcmp (Strings[i], string) == 0)
			return i;
	}
	return -1;
}
