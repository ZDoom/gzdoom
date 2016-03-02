/*
** stringtable.cpp
** Implements the FStringTable class
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
#include <stddef.h>

#include "stringtable.h"
#include "cmdlib.h"
#include "m_swap.h"
#include "w_wad.h"
#include "i_system.h"
#include "sc_man.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "gi.h"

// PassNum identifies which language pass this string is from.
// PassNum 0 is for DeHacked.
// PassNum 1 is for * strings.
// PassNum 2+ are for specific locales.

struct FStringTable::StringEntry
{
	StringEntry *Next;
	char *Name;
	BYTE PassNum;
	char String[];
};

FStringTable::FStringTable ()
{
	for (int i = 0; i < HASH_SIZE; ++i)
	{
		Buckets[i] = NULL;
	}
}

FStringTable::~FStringTable ()
{
	FreeData ();
}

void FStringTable::FreeData ()
{
	for (int i = 0; i < HASH_SIZE; ++i)
	{
		StringEntry *entry = Buckets[i], *next;
		Buckets[i] = NULL;
		while (entry != NULL)
		{
			next = entry->Next;
			M_Free (entry);
			entry = next;
		}
	}
}

void FStringTable::FreeNonDehackedStrings ()
{
	for (int i = 0; i < HASH_SIZE; ++i)
	{
		StringEntry *entry, *next, **pentry;

		for (pentry = &Buckets[i], entry = *pentry; entry != NULL; )
		{
			next = entry->Next;
			if (entry->PassNum != 0)
			{
				*pentry = next;
				M_Free (entry);
			}
			else
			{
				pentry = &entry->Next;
			}
			entry = next;
		}
	}
}

#include "doomerrors.h"
void FStringTable::LoadStrings (bool enuOnly)
{
	int lastlump, lump;
	int i, j;

	FreeNonDehackedStrings ();

	lastlump = 0;

	while ((lump = Wads.FindLump ("LANGUAGE", &lastlump)) != -1)
	{
		j = 0;
		if (!enuOnly)
		{
			LoadLanguage (lump, MAKE_ID('*',0,0,0), true, ++j);
			for (i = 0; i < 4; ++i)
			{
				LoadLanguage (lump, LanguageIDs[i], true, ++j);
				LoadLanguage (lump, LanguageIDs[i] & MAKE_ID(0xff,0xff,0,0), true, ++j);
				LoadLanguage (lump, LanguageIDs[i], false, ++j);
			}
		}

		// Fill in any missing strings with the default language
		LoadLanguage (lump, MAKE_ID('*','*',0,0), true, ++j);
	}
}

void FStringTable::LoadLanguage (int lumpnum, DWORD code, bool exactMatch, int passnum)
{
	static bool errordone = false;
	const DWORD orMask = exactMatch ? 0 : MAKE_ID(0,0,0xff,0);
	DWORD inCode = 0;
	StringEntry *entry, **pentry;
	DWORD bucket;
	int cmpval;
	bool skip = true;

	code |= orMask;

	FScanner sc(lumpnum);
	sc.SetCMode (true);
	while (sc.GetString ())
	{
		if (sc.Compare ("["))
		{ // Process language identifiers
			bool donot = false;
			bool forceskip = false;
			skip = true;
			sc.MustGetString ();
			do
			{
				size_t len = sc.StringLen;
				if (len != 2 && len != 3)
				{
					if (len == 1 && sc.String[0] == '~')
					{
						donot = true;
						sc.MustGetString ();
						continue;
					}
					if (len == 1 && sc.String[0] == '*')
					{
						inCode = MAKE_ID('*',0,0,0);
					}
					else if (len == 7 && stricmp (sc.String, "default") == 0)
					{
						inCode = MAKE_ID('*','*',0,0);
					}
					else
					{
						sc.ScriptError ("The language code must be 2 or 3 characters long.\n'%s' is %lu characters long.",
							sc.String, len);
					}
				}
				else
				{
					inCode = MAKE_ID(tolower(sc.String[0]), tolower(sc.String[1]), tolower(sc.String[2]), 0);
				}
				if ((inCode | orMask) == code)
				{
					if (donot)
					{
						forceskip = true;
						donot = false;
					}
					else
					{
						skip = false;
					}
				}
				sc.MustGetString ();
			} while (!sc.Compare ("]"));
			if (donot)
			{
				sc.ScriptError ("You must specify a language after ~");
			}
			skip |= forceskip;
		}
		else
		{ // Process string definitions.
			if (inCode == 0)
			{
				// LANGUAGE lump is bad. We need to check if this is an old binary
				// lump and if so just skip it to allow old WADs to run which contain
				// such a lump.
				if (!sc.isText())
				{
					if (!errordone) Printf("Skipping binary 'LANGUAGE' lump.\n"); 
					errordone = true;
					return;
				}
				sc.ScriptError ("Found a string without a language specified.");
			}

			bool savedskip = skip;
			if (sc.Compare("$"))
			{
				sc.MustGetStringName("ifgame");
				sc.MustGetStringName("(");
				sc.MustGetString();
				skip |= !sc.Compare(GameTypeName());
				sc.MustGetStringName(")");
				sc.MustGetString();

			}

			if (skip)
			{ // We're not interested in this language, so skip the string.
				sc.MustGetStringName ("=");
				sc.MustGetString ();
				do
				{
					sc.MustGetString ();
				} 
				while (!sc.Compare (";"));
				skip = savedskip;
				continue;
			}

			FString strName (sc.String);
			sc.MustGetStringName ("=");
			sc.MustGetString ();
			FString strText (sc.String, ProcessEscapes (sc.String));
			sc.MustGetString ();
			while (!sc.Compare (";"))
			{
				ProcessEscapes (sc.String);
				strText += sc.String;
				sc.MustGetString ();
			}

			// Does this string exist? If so, should we overwrite it?
			bucket = MakeKey (strName.GetChars()) & (HASH_SIZE-1);
			pentry = &Buckets[bucket];
			entry = *pentry;
			cmpval = 1;
			while (entry != NULL)
			{
				cmpval = stricmp (entry->Name, strName.GetChars());
				if (cmpval >= 0)
					break;
				pentry = &entry->Next;
				entry = *pentry;
			}
			if (cmpval == 0 && entry->PassNum >= passnum)
			{
				*pentry = entry->Next;
				M_Free (entry);
				entry = NULL;
			}
			if (entry == NULL || cmpval > 0)
			{
				entry = (StringEntry *)M_Malloc (sizeof(*entry) + strText.Len() + strName.Len() + 2);
				entry->Next = *pentry;
				*pentry = entry;
				strcpy (entry->String, strText.GetChars());
				strcpy (entry->Name = entry->String + strText.Len() + 1, strName.GetChars());
				entry->PassNum = passnum;
			}
		}
	}
}

// Replace \ escape sequences in a string with the escaped characters.
size_t FStringTable::ProcessEscapes (char *iptr)
{
	char *sptr = iptr, *optr = iptr, c;

	while ((c = *iptr++) != '\0')
	{
		if (c == '\\')
		{
			c = *iptr++;
			if (c == 'n')
				c = '\n';
			else if (c == 'c')
				c = TEXTCOLOR_ESCAPE;
			else if (c == 'r')
				c = '\r';
			else if (c == 't')
				c = '\t';
			else if (c == '\n')
				continue;
		}
		*optr++ = c;
	}
	*optr = '\0';
	return optr - sptr;
}

// Finds a string by name and returns its value
const char *FStringTable::operator[] (const char *name) const
{
	if (name == NULL)
	{
		return NULL;
	}
	DWORD bucket = MakeKey (name) & (HASH_SIZE - 1);
	StringEntry *entry = Buckets[bucket];

	while (entry != NULL)
	{
		int cmpval = stricmp (entry->Name, name);
		if (cmpval == 0)
		{
			return entry->String;
		}
		if (cmpval == 1)
		{
			return NULL;
		}
		entry = entry->Next;
	}
	return NULL;
}

// Finds a string by name and returns its value. If the string does
// not exist, returns the passed name instead.
const char *FStringTable::operator() (const char *name) const
{
	const char *str = operator[] (name);
	return str ? str : name;
}

// Find a string by name. pentry1 is a pointer to a pointer to it, and entry1 is a
// pointer to it. Return NULL for entry1 if it wasn't found.
void FStringTable::FindString (const char *name, StringEntry **&pentry1, StringEntry *&entry1)
{
	DWORD bucket = MakeKey (name) & (HASH_SIZE - 1);
	StringEntry **pentry = &Buckets[bucket], *entry = *pentry;

	while (entry != NULL)
	{
		int cmpval = stricmp (entry->Name, name);
		if (cmpval == 0)
		{
			pentry1 = pentry;
			entry1 = entry;
			return;
		}
		if (cmpval == 1)
		{
			pentry1 = pentry;
			entry1 = NULL;
			return;
		}
		pentry = &entry->Next;
		entry = *pentry;
	}
	pentry1 = pentry;
	entry1 = entry;
}

// Find a string with the same exact text. Returns its name.
const char *FStringTable::MatchString (const char *string) const
{
	for (int i = 0; i < HASH_SIZE; ++i)
	{
		for (StringEntry *entry = Buckets[i]; entry != NULL; entry = entry->Next)
		{
			if (strcmp (entry->String, string) == 0)
			{
				return entry->Name;
			}
		}
	}
	return NULL;
}

void FStringTable::SetString (const char *name, const char *newString)
{
	StringEntry **pentry, *oentry;
	FindString (name, pentry, oentry);

	size_t newlen = strlen (newString);
	size_t namelen = strlen (name);

	// Create a new string entry
	StringEntry *entry = (StringEntry *)M_Malloc (sizeof(*entry) + newlen + namelen + 2);
	strcpy (entry->String, newString);
	strcpy (entry->Name = entry->String + newlen + 1, name);
	entry->PassNum = 0;

	// If this is a new string, insert it. Otherwise, replace the old one.
	if (oentry == NULL)
	{
		entry->Next = *pentry;
		*pentry = entry;
	}
	else
	{
		*pentry = entry;
		entry->Next = oentry->Next;
		M_Free (oentry);
	}
}
