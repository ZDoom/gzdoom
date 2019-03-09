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

#include "stringtable.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "i_system.h"
#include "sc_man.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "gi.h"
#include "d_player.h"
#include "xlsxread/xlsxio_read.h"


//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadStrings ()
{
	int lastlump, lump;

	lastlump = 0;

	while ((lump = Wads.FindLump ("LANGUAGE", &lastlump)) != -1)
	{
		if (!LoadLanguageFromSpreadsheet(lump))
			LoadLanguage (lump);
	}
	SetLanguageIDs();
	UpdateLanguage();
	allMacros.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::LoadLanguageFromSpreadsheet(int lumpnum)
{
	xlsxioreader xlsxio;
	//open .xlsx file for reading
	if ((xlsxio = xlsxioread_open(lumpnum)) == nullptr)
	{
		return false;
	}
	readMacros(xlsxio, "macros");
	readSheetIntoTable(xlsxio, "strings");

	xlsxioread_close(xlsxio);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::readMacros(xlsxioreader reader, const char *sheetname)
{
	xlsxioreadersheet sheet = xlsxioread_sheet_open(reader, sheetname, XLSXIOREAD_SKIP_NONE);
	if (sheet == nullptr) return false;

	while (xlsxioread_sheet_next_row(sheet))
	{
		auto macroname = xlsxioread_sheet_next_cell(sheet);
		auto language = xlsxioread_sheet_next_cell(sheet);
		if (!macroname || !language) continue;
		FStringf combined_name("%s/%s", language, macroname);
		free(language);
		free(macroname);
		FName name = combined_name;

		StringMacro macro;

		char *value;
		for (int i = 0; i < 4; i++)
		{
			value = xlsxioread_sheet_next_cell(sheet);
			macro.Replacements[i] = value;
			free(value);
		}
		// This is needed because the reader code would choke on incompletely read rows.
		while ((value = xlsxioread_sheet_next_cell(sheet)) != nullptr)
		{
			free(value);
		}
		allMacros.Insert(name, macro);
	}
	xlsxioread_sheet_close(sheet);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::readSheetIntoTable(xlsxioreader reader, const char *sheetname)
{

	xlsxioreadersheet sheet = xlsxioread_sheet_open(reader, sheetname, XLSXIOREAD_SKIP_NONE);
	if (sheet == nullptr) return false;

	int row = 0;
	TArray<TArray<FString>> table;

	while (xlsxioread_sheet_next_row(sheet))
	{
		int column = 0;
		char *value;
		table.Reserve(1);
		while ((value = xlsxioread_sheet_next_cell(sheet)) != nullptr)
		{
			auto vcopy = value;
			if (table.Size() <= (unsigned)row) table.Reserve(1);
			while (*vcopy && iswspace((unsigned char)*vcopy)) vcopy++;	// skip over leaading whitespace;
			auto vend = vcopy + strlen(vcopy);
			while (vend > vcopy && iswspace((unsigned char)vend[-1])) *--vend = 0;	// skip over trailing whitespace
			ProcessEscapes(vcopy);
			table[row].Push(vcopy);
			column++;
			free(value);
		}
		row++;
	}

	int labelcol = -1;
	int filtercol = -1;
	TArray<std::pair<int, unsigned>> langrows;

	if (table.Size() > 0)
	{
		for (unsigned column = 0; column < table[0].Size(); column++)
		{
			auto &entry = table[0][column];
			if (entry.CompareNoCase("filter") == 0)
			{
				filtercol = column;
			}
			else if (entry.CompareNoCase("identifier") == 0)
			{
				labelcol = column;;
			}
			else
			{
				auto languages = entry.Split(" ", FString::TOK_SKIPEMPTY);
				for (auto &lang : languages)
				{
					if (lang.CompareNoCase("default") == 0)
					{
						langrows.Push(std::make_pair(column, default_table));
					}
					else if (lang.Len() < 4)
					{
						lang.ToLower();
						langrows.Push(std::make_pair(column, MAKE_ID(lang[0], lang[1], lang[2], 0)));
					}
				}
			}
		}

		for (unsigned i = 1; i < table.Size(); i++)
		{
			auto &row = table[i];
			if (filtercol > -1)
			{
				auto filterstr = row[filtercol];
				auto filter = filterstr.Split(" ", FString::TOK_SKIPEMPTY);
				if (filter.Size() > 0 && filter.FindEx([](const auto &str) { return str.CompareNoCase(GameNames[gameinfo.gametype]) == 0; }) == filter.Size())
					continue;
			}

			FName strName = row[labelcol];
			for (auto &langentry : langrows)
			{
				auto str = row[langentry.first];
				if (str.Len() > 0)
				{
					InsertString(langentry.second, strName, str);
				}
			}
		}
	}
	xlsxioread_sheet_close(sheet);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadLanguage (int lumpnum)
{
	bool errordone = false;
	TArray<uint32_t> activeMaps;
	FScanner sc(lumpnum);
	sc.SetCMode (true);
	while (sc.GetString ())
	{
		if (sc.Compare ("["))
		{ // Process language identifiers
			activeMaps.Clear();
			sc.MustGetString ();
			do
			{
				size_t len = sc.StringLen;
				if (len != 2 && len != 3)
				{
					if (len == 1 && sc.String[0] == '~')
					{
						// deprecated and ignored
						sc.ScriptMessage("Deprecated option '~' found in language list");
						sc.MustGetString ();
						continue;
					}
					if (len == 1 && sc.String[0] == '*')
					{
						activeMaps.Push(global_table);
					}
					else if (len == 7 && stricmp (sc.String, "default") == 0)
					{
						activeMaps.Push(default_table);
					}
					else
					{
						sc.ScriptError ("The language code must be 2 or 3 characters long.\n'%s' is %lu characters long.",
							sc.String, len);
					}
				}
				else
				{
					activeMaps.Push(MAKE_ID(tolower(sc.String[0]), tolower(sc.String[1]), tolower(sc.String[2]), 0));
				}
				sc.MustGetString ();
			} while (!sc.Compare ("]"));
		}
		else
		{ // Process string definitions.
			if (activeMaps.Size() == 0)
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

			bool skip = false;
			if (sc.Compare("$"))
			{
				sc.MustGetStringName("ifgame");
				sc.MustGetStringName("(");
				sc.MustGetString();
				if (sc.Compare("strifeteaser"))
				{
					skip |= (gameinfo.gametype != GAME_Strife) || !(gameinfo.flags & GI_SHAREWARE);
				}
				else
				{
					skip |= !sc.Compare(GameTypeName());
				}
				sc.MustGetStringName(")");
				sc.MustGetString();

			}

			FName strName (sc.String);
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
			if (!skip)
			{
				// Insert the string into all relevant tables.
				for (auto map : activeMaps)
				{
					InsertString(map, strName, strText);
				}
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::InsertString(int langid, FName label, const FString &string)
{
	const char *strlangid = (const char *)&langid;
	TableElement te = { string, string, string, string };
	long index;
	while ((index = te.strings[0].IndexOf("@[")) >= 0)
	{
		auto endindex = te.strings[0].IndexOf(']', index);
		if (endindex == -1)
		{
			Printf("Bad macro in %s : %s\n", strlangid, label.GetChars());
			break;
		}
		FString macroname(te.strings[0].GetChars() + index + 2, endindex - index - 2);
 		FStringf lookupstr("%s/%s", strlangid, macroname.GetChars());
		FStringf replacee("@[%s]", macroname.GetChars());
		FName lookupname(lookupstr, true);
		auto replace = allMacros.CheckKey(lookupname);
		for (int i = 0; i < 4; i++)
		{
			const char *replacement = replace && replace->Replacements[i] ? replace->Replacements[i] : "";
			te.strings[i].Substitute(replacee, replacement);
		}
	}
	allStrings[langid].Insert(label, te);
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::UpdateLanguage()
{
	currentLanguageSet.Clear();

	auto checkone = [&](uint32_t lang_id)
	{
		auto list = allStrings.CheckKey(lang_id);
		if (list && currentLanguageSet.FindEx([&](const auto &element) { return element.first == lang_id; }) == currentLanguageSet.Size())
			currentLanguageSet.Push(std::make_pair(lang_id, list));
	};

	checkone(dehacked_table);
	checkone(global_table);
	for (int i = 0; i < 4; ++i)
	{
		checkone(LanguageIDs[i]);
		checkone(LanguageIDs[i] & MAKE_ID(0xff, 0xff, 0, 0));
	}
	checkone(default_table);
}

//==========================================================================
//
// Replace \ escape sequences in a string with the escaped characters.
//
//==========================================================================

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

//==========================================================================
//
// Checks if the given key exists in any one of the default string tables that are valid for all languages.
// To replace IWAD content this condition must be true.
//
//==========================================================================

bool FStringTable::exists(const char *name)
{
	if (name == nullptr || *name == 0)
	{
		return false;
	}
	FName nm(name, true);
	if (nm != NAME_None)
	{
		uint32_t defaultStrings[] = { default_table, global_table, dehacked_table };

		for (auto mapid : defaultStrings)
		{
			auto map = allStrings.CheckKey(mapid);
			if (map)
			{
				auto item = map->CheckKey(nm);
				if (item) return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// Finds a string by name and returns its value
//
//==========================================================================

const char *FStringTable::GetString(const char *name, uint32_t *langtable, int gender) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	if (gender == -1) gender = players[consoleplayer].userinfo.GetGender();
	if (gender < 0 || gender > 3) gender = 0;
	FName nm(name, true);
	if (nm != NAME_None)
	{
		for (auto map : currentLanguageSet)
		{
			auto item = map.second->CheckKey(nm);
			if (item)
			{
				if (langtable) *langtable = map.first;
				return item->strings[gender].GetChars();
			}
		}
	}
	return nullptr;
}

//==========================================================================
//
// Finds a string by name in a given language
//
//==========================================================================

const char *FStringTable::GetLanguageString(const char *name, uint32_t langtable, int gender) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	if (gender == -1) gender = players[consoleplayer].userinfo.GetGender();
	if (gender < 0 || gender > 3) gender = 0;
	FName nm(name, true);
	if (nm != NAME_None)
	{
		auto map = allStrings.CheckKey(langtable);
		if (map == nullptr) return nullptr;
		auto item = map->CheckKey(nm);
		if (item)
		{
			return item->strings[gender].GetChars();
		}
	}
	return nullptr;
}

//==========================================================================
//
// Finds a string by name and returns its value. If the string does
// not exist, returns the passed name instead.
//
//==========================================================================

const char *FStringTable::operator() (const char *name) const
{
	const char *str = operator[] (name);
	return str ? str : name;
}


//==========================================================================
//
// Find a string with the same exact text. Returns its name.
// This does not need to check genders, it is only used by
// Dehacked on the English table for finding stock strings.
//
//==========================================================================

const char *StringMap::MatchString (const char *string) const
{
	StringMap::ConstIterator it(*this);
	StringMap::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.strings[0].CompareNoCase(string) == 0)
		{
			return pair->Key.GetChars();
		}
	}
	return nullptr;
}
