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
#include "filesystem.h"
#include "sc_man.h"
#include "printf.h"
#include "i_interface.h"

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadStrings (const char *language)
{
	int lastlump, lump;

	lastlump = 0;
	while ((lump = fileSystem.FindLump("LMACROS", &lastlump)) != -1)
	{
		readMacros(lump);
	}

	lastlump = 0;
	while ((lump = fileSystem.FindLump ("LANGUAGE", &lastlump)) != -1)
	{
		auto lumpdata = fileSystem.GetFileData(lump);

		if (!ParseLanguageCSV(lump, lumpdata))
 			LoadLanguage (lump, lumpdata);
	}
	UpdateLanguage(language);
	allMacros.Clear();
}


//==========================================================================
//
// This was tailored to parse CSV as exported by Google Docs.
//
//==========================================================================


TArray<TArray<FString>> FStringTable::parseCSV(const TArray<uint8_t> &buffer)
{
	const size_t bufLength = buffer.Size();
	TArray<TArray<FString>> data;
	TArray<FString> row;
	TArray<char> cell;
	bool quoted = false;

	/*
			auto myisspace = [](int ch) { return ch == '\t' || ch == '\r' || ch == '\n' || ch == ' '; };
			while (*vcopy && myisspace((unsigned char)*vcopy)) vcopy++;	// skip over leaading whitespace;
			auto vend = vcopy + strlen(vcopy);
			while (vend > vcopy && myisspace((unsigned char)vend[-1])) *--vend = 0;	// skip over trailing whitespace
	*/

	for (size_t i = 0; i < bufLength; ++i)
	{
		if (buffer[i] == '"')
		{
			// Double quotes inside a quoted string count as an escaped quotation mark.
			if (quoted && i < bufLength - 1 && buffer[i + 1] == '"')
			{
				cell.Push('"');
				i++;
			}
			else if (cell.Size() == 0 || quoted)
			{
				quoted = !quoted;
			}
		}
		else if (buffer[i] == ',')
		{
			if (!quoted)
			{
				cell.Push(0);
				ProcessEscapes(cell.Data());
				row.Push(cell.Data());
				cell.Clear();
			}
			else
			{
				cell.Push(buffer[i]);
			}
		}
		else if (buffer[i] == '\r')
		{
			// Ignore all CR's.
		}
		else if (buffer[i] == '\n' && !quoted)
		{
			cell.Push(0);
			ProcessEscapes(cell.Data());
			row.Push(cell.Data());
			data.Push(std::move(row));
			cell.Clear();
		}
		else
		{
			cell.Push(buffer[i]);
		}
	}

	// Handle last line without linebreak
	if (cell.Size() > 0 || row.Size() > 0)
	{
		cell.Push(0);
		ProcessEscapes(cell.Data());
		row.Push(cell.Data());
		data.Push(std::move(row));
	}
	return data;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::readMacros(int lumpnum)
{
	auto lumpdata = fileSystem.GetFileData(lumpnum);
	auto data = parseCSV(lumpdata);

	for (unsigned i = 1; i < data.Size(); i++)
	{
		auto macroname = data[i][0];
		auto language = data[i][1];
		if (macroname.IsEmpty() || language.IsEmpty()) continue;
		FStringf combined_name("%s/%s", language.GetChars(), macroname.GetChars());
		FName name = combined_name.GetChars();

		StringMacro macro;

		for (int k = 0; k < 4; k++)
		{
			macro.Replacements[k] = data[i][k+2];
		}
		allMacros.Insert(name, macro);
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::ParseLanguageCSV(int lumpnum, const TArray<uint8_t> &buffer)
{
	if (buffer.Size() < 11) return false;
	if (strnicmp((const char*)buffer.Data(), "default,", 8) && strnicmp((const char*)buffer.Data(), "identifier,", 11 )) return false;
	auto data = parseCSV(buffer);

	int labelcol = -1;
	int filtercol = -1;
	TArray<std::pair<int, unsigned>> langrows;
	bool hasDefaultEntry = false;

	if (data.Size() > 0)
	{
		for (unsigned column = 0; column < data[0].Size(); column++)
		{
			auto &entry = data[0][column];
			if (entry.CompareNoCase("filter") == 0)
			{
				filtercol = column;
			}
			else if (entry.CompareNoCase("identifier") == 0)
			{
				labelcol = column;
			}
			else
			{
				auto languages = entry.Split(" ", FString::TOK_SKIPEMPTY);
				for (auto &lang : languages)
				{
					if (lang.CompareNoCase("default") == 0)
					{
						langrows.Push(std::make_pair(column, default_table));
						hasDefaultEntry = true;
					}
					else if (lang.Len() < 4)
					{
						lang.ToLower();
						langrows.Push(std::make_pair(column, MAKE_ID(lang[0], lang[1], lang[2], 0)));
					}
				}
			}
		}

		for (unsigned i = 1; i < data.Size(); i++)
		{
			auto &row = data[i];
			if (filtercol > -1)
			{
				auto filterstr = row[filtercol];
				if (filterstr.IsNotEmpty())
				{
					bool ok = false;
					if (sysCallbacks.CheckGame)
					{
						auto filter = filterstr.Split(" ", FString::TOK_SKIPEMPTY);
						for (auto& entry : filter)
						{
							if (sysCallbacks.CheckGame(entry))
							{
								ok = true;
								break;
							}
						}
					}
					if (!ok) continue;
				}
			}

			FName strName = row[labelcol].GetChars();
			if (hasDefaultEntry)
			{
				DeleteForLabel(lumpnum, strName);
			}
			for (auto &langentry : langrows)
			{
				auto str = row[langentry.first];
				if (str.Len() > 0)
				{
					InsertString(lumpnum, langentry.second, strName, str);
				}
				else
				{
					DeleteString(langentry.second, strName);
				}
			}
		}
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadLanguage (int lumpnum, const TArray<uint8_t> &buffer)
{
	bool errordone = false;
	TArray<uint32_t> activeMaps;
	FScanner sc;
	bool hasDefaultEntry = false;

	sc.OpenMem("LANGUAGE", buffer);
	sc.SetCMode (true);
	while (sc.GetString ())
	{
		if (sc.Compare ("["))
		{ // Process language identifiers
			activeMaps.Clear();
			hasDefaultEntry = false;
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
						activeMaps.Clear();
						activeMaps.Push(global_table);
					}
					else if (len == 7 && stricmp (sc.String, "default") == 0)
					{
						activeMaps.Clear();
						activeMaps.Push(default_table);
						hasDefaultEntry = true;
					}
					else
					{
						sc.ScriptError ("The language code must be 2 or 3 characters long.\n'%s' is %lu characters long.",
							sc.String, len);
					}
				}
				else
				{
					if (activeMaps.Size() != 1 || (activeMaps[0] != default_table && activeMaps[0] != global_table))
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
				skip |= (!sysCallbacks.CheckGame || !sysCallbacks.CheckGame(sc.String));
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
				if (hasDefaultEntry)
				{
					DeleteForLabel(lumpnum, strName);
				}
				// Insert the string into all relevant tables.
				for (auto map : activeMaps)
				{
					InsertString(lumpnum, map, strName, strText);
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

void FStringTable::DeleteString(int langid, FName label)
{
	allStrings[langid].Remove(label);
}

//==========================================================================
//
// This deletes all older entries for a given label. This gets called
// when a string in the default table gets updated. 
//
//==========================================================================

void FStringTable::DeleteForLabel(int lumpnum, FName label)
{
	decltype(allStrings)::Iterator it(allStrings);
	decltype(allStrings)::Pair *pair;
	auto filenum = fileSystem.GetFileContainer(lumpnum);

	while (it.NextPair(pair))
	{
		auto entry = pair->Value.CheckKey(label);
		if (entry && entry->filenum < filenum)
		{
			pair->Value.Remove(label);
		}
	}

}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::InsertString(int lumpnum, int langid, FName label, const FString &string)
{
	const char *strlangid = (const char *)&langid;
	TableElement te = { fileSystem.GetFileContainer(lumpnum), { string, string, string, string } };
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
		FName lookupname(lookupstr.GetChars(), true);
		auto replace = allMacros.CheckKey(lookupname);
		for (int i = 0; i < 4; i++)
		{
			const char *replacement = replace? replace->Replacements[i].GetChars() : "";
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

void FStringTable::UpdateLanguage(const char *language)
{
	if (language) activeLanguage = language;
	else language = activeLanguage.GetChars();
	size_t langlen = strlen(language);

	int LanguageID = (langlen < 2 || langlen > 3) ?
		MAKE_ID('e', 'n', 'u', '\0') :
		MAKE_ID(language[0], language[1], language[2], '\0');

	currentLanguageSet.Clear();

	auto checkone = [&](uint32_t lang_id)
	{
		auto list = allStrings.CheckKey(lang_id);
		if (list && currentLanguageSet.FindEx([&](const auto &element) { return element.first == lang_id; }) == currentLanguageSet.Size())
			currentLanguageSet.Push(std::make_pair(lang_id, list));
	};

	checkone(override_table);
	checkone(global_table);
	checkone(LanguageID);
	checkone(LanguageID & MAKE_ID(0xff, 0xff, 0, 0));
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
			else if (c == 'x')
			{
				c = 0;
				for (int i = 0; i < 2; i++)
				{
					char cc = *iptr++;
					if (cc >= '0' && cc <= '9')
						c = (c << 4) + cc - '0';
					else if (cc >= 'a' && cc <= 'f')
						c = (c << 4) + 10 + cc - 'a';
					else if (cc >= 'A' && cc <= 'F')
						c = (c << 4) + 10 + cc - 'A';
					else
					{
						iptr--;
						break;
					}
				}
				if (c == 0) continue;
			}
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
		uint32_t defaultStrings[] = { default_table, global_table, override_table };

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
	if (gender == -1 && sysCallbacks.GetGender) gender = sysCallbacks.GetGender();
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
				auto c = item->strings[gender].GetChars();
				if (c && *c == '$' && c[1] == '$')
					return GetString(c + 2, langtable, gender);
				return c;
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
	if (gender == -1 && sysCallbacks.GetGender) gender = sysCallbacks.GetGender();
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

bool FStringTable::MatchDefaultString(const char *name, const char *content) const
{
	// This only compares the first line to avoid problems with bad linefeeds. For the few cases where this feature is needed it is sufficient.
	auto c = GetLanguageString(name, FStringTable::default_table);
	if (!c) return false;
	
	// Check a secondary key, in case the text comparison cannot be done due to needed orthographic fixes (see Harmony's exit text)
	FStringf checkkey("%s_CHECK", name);
	auto cc = GetLanguageString(checkkey, FStringTable::default_table);
	if (cc) c = cc;

	return (c && !strnicmp(c, content, strcspn(content, "\n\r\t")));
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

FStringTable GStrings;


