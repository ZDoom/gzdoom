/*
** configfile.cpp
** Implements the basic .ini parsing class
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "configfile.h"
#include "m_random.h"

#define READBUFFERSIZE	256

static FRandom pr_endtag;

//====================================================================
//
// FConfigFile Constructor
//
//====================================================================

FConfigFile::FConfigFile ()
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	PathName = "";
	OkayToWrite = true;
	FileExisted = true;
}

//====================================================================
//
// FConfigFile Constructor
//
//====================================================================

FConfigFile::FConfigFile (const char *pathname)
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	ChangePathName (pathname);
	LoadConfigFile ();
	OkayToWrite = true;
	FileExisted = true;
}

//====================================================================
//
// FConfigFile Copy Constructor
//
//====================================================================

FConfigFile::FConfigFile (const FConfigFile &other)
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	ChangePathName (other.PathName);
	*this = other;
	OkayToWrite = other.OkayToWrite;
	FileExisted = other.FileExisted;
}

//====================================================================
//
// FConfigFile Destructor
//
//====================================================================

FConfigFile::~FConfigFile ()
{
	FConfigSection *section = Sections;

	while (section != NULL)
	{
		FConfigSection *nextsection = section->Next;
		FConfigEntry *entry = section->RootEntry;

		while (entry != NULL)
		{
			FConfigEntry *nextentry = entry->Next;
			delete[] entry->Value;
			delete[] (char *)entry;
			entry = nextentry;
		}
		delete section;
		section = nextsection;
	}
}

//====================================================================
//
// FConfigFile Copy Operator
//
//====================================================================

FConfigFile &FConfigFile::operator = (const FConfigFile &other)
{
	FConfigSection *fromsection, *tosection;
	FConfigEntry *fromentry;

	ClearConfig ();
	fromsection = other.Sections;
	while (fromsection != NULL)
	{
		fromentry = fromsection->RootEntry;
		tosection = NewConfigSection (fromsection->SectionName);
		while (fromentry != NULL)
		{
			NewConfigEntry (tosection, fromentry->Key, fromentry->Value);
			fromentry = fromentry->Next;
		}
		fromsection = fromsection->Next;
	}
	return *this;
}

//====================================================================
//
// FConfigFile :: ClearConfig
//
// Removes all sections and entries from the config file.
//
//====================================================================

void FConfigFile::ClearConfig ()
{
	CurrentSection = Sections;
	while (CurrentSection != NULL)
	{
		FConfigSection *next = CurrentSection->Next;
		ClearCurrentSection ();
		delete CurrentSection;
		CurrentSection = next;
	}
	Sections = NULL;
	LastSectionPtr = &Sections;
}

//====================================================================
//
// FConfigFile :: ChangePathName
//
//====================================================================

void FConfigFile::ChangePathName (const char *pathname)
{
	PathName = pathname;
}

//====================================================================
//
// FConfigFile :: CreateSectionAtStart
//
// Creates the section at the start of the file if it does not exist.
// Otherwise, simply moves the section to the start of the file.
//
//====================================================================

void FConfigFile::CreateSectionAtStart (const char *name)
{
	NewConfigSection (name);
	MoveSectionToStart (name);
}

//====================================================================
//
// FConfigFile :: MoveSectionToStart
//
// Moves the named section to the start of the file if it exists.
// Otherwise, does nothing.
//
//====================================================================

void FConfigFile::MoveSectionToStart (const char *name)
{
	FConfigSection *section = FindSection (name);

	if (section != NULL)
	{
		FConfigSection **prevsec = &Sections;
		while (*prevsec != NULL && *prevsec != section)
		{
			prevsec = &((*prevsec)->Next);
		}
		*prevsec = section->Next;
		section->Next = Sections;
		Sections = section;
		if (LastSectionPtr == &section->Next)
		{
			LastSectionPtr = prevsec;
		}
	}
}


//====================================================================
//
// FConfigFile :: SetSection
//
// Sets the current section to the named one, optionally creating it
// if it does not exist. Returns true if the section exists (even if
// it was newly created), false otherwise.
//
//====================================================================

bool FConfigFile::SetSection (const char *name, bool allowCreate)
{
	FConfigSection *section = FindSection (name);
	if (section == NULL && allowCreate)
	{
		section = NewConfigSection (name);
	}
	if (section != NULL)
	{
		CurrentSection = section;
		CurrentEntry = section->RootEntry;
		return true;
	}
	return false;
}

//====================================================================
//
// FConfigFile :: SetFirstSection
//
// Sets the current section to the first one in the file. Returns
// false if there are no sections.
//
//====================================================================

bool FConfigFile::SetFirstSection ()
{
	CurrentSection = Sections;
	if (CurrentSection != NULL)
	{
		CurrentEntry = CurrentSection->RootEntry;
		return true;
	}
	return false;
}

//====================================================================
//
// FConfigFile :: SetNextSection
//
// Advances the current section to the next one in the file. Returns
// false if there are no more sections.
//
//====================================================================

bool FConfigFile::SetNextSection ()
{
	if (CurrentSection != NULL)
	{
		CurrentSection = CurrentSection->Next;
		if (CurrentSection != NULL)
		{
			CurrentEntry = CurrentSection->RootEntry;
			return true;
		}
	}
	return false;
}

//====================================================================
//
// FConfigFile :: GetCurrentSection
//
// Returns the name of the current section.
//
//====================================================================

const char *FConfigFile::GetCurrentSection () const
{
	if (CurrentSection != NULL)
	{
		return CurrentSection->SectionName.GetChars();
	}
	return NULL;
}

//====================================================================
//
// FConfigFile :: ClearCurrentSection
//
// Removes all entries from the current section.
//
//====================================================================

void FConfigFile::ClearCurrentSection ()
{
	if (CurrentSection != NULL)
	{
		FConfigEntry *entry, *next;

		entry = CurrentSection->RootEntry;
		while (entry != NULL)
		{
			next = entry->Next;
			delete[] entry->Value;
			delete[] (char *)entry;
			entry = next;
		}
		CurrentSection->RootEntry = NULL;
		CurrentSection->LastEntryPtr = &CurrentSection->RootEntry;
	}
}

//====================================================================
//
// FConfigFile :: DeleteCurrentSection
//
// Completely removes the current section. The current section is
// advanced to the next section. Returns true if there is still a
// current section.
//
//====================================================================

bool FConfigFile::DeleteCurrentSection()
{
	if (CurrentSection != NULL)
	{
		FConfigSection *sec;

		ClearCurrentSection();

		// Find the preceding section.
		for (sec = Sections; sec != NULL && sec->Next != CurrentSection; sec = sec->Next)
		{ }

		sec->Next = CurrentSection->Next;
		if (LastSectionPtr == &CurrentSection->Next)
		{
			LastSectionPtr = &sec->Next;
		}

		delete CurrentSection;

		CurrentSection = sec->Next;
		return CurrentSection != NULL;
	}
	return false;
}

//====================================================================
//
// FConfigFile :: ClearKey
//
// Removes a key from the current section, if found. If there are
// duplicates, only the first is removed.
//
//====================================================================

void FConfigFile::ClearKey(const char *key)
{
	if (CurrentSection->RootEntry == NULL)
	{
		return;
	}
	FConfigEntry **prober = &CurrentSection->RootEntry, *probe = *prober;

	while (probe != NULL && stricmp(probe->Key, key) != 0)
	{
		prober = &probe->Next;
		probe = *prober;
	}
	if (probe != NULL)
	{
		*prober = probe->Next;
		if (CurrentSection->LastEntryPtr == &probe->Next)
		{
			CurrentSection->LastEntryPtr = prober;
		}
		delete[] probe->Value;
		delete[] (char *)probe;
	}
}

//====================================================================
//
// FConfigFile :: SectionIsEmpty
//
// Returns true if the current section has no entries. If there is
// no current section, it is also considered empty.
//
//====================================================================

bool FConfigFile::SectionIsEmpty()
{
	return (CurrentSection == NULL) || (CurrentSection->RootEntry == NULL);
}


//====================================================================
//
// FConfigFile :: NextInSection
//
// Provides the next key/value pair in the current section. Returns
// true if there was another, false otherwise.
//
//====================================================================

bool FConfigFile::NextInSection (const char *&key, const char *&value)
{
	FConfigEntry *entry = CurrentEntry;

	if (entry == NULL)
		return false;

	CurrentEntry = entry->Next;
	key = entry->Key;
	value = entry->Value;
	return true;
}

//====================================================================
//
// FConfigFile :: GetValueForKey
//
// Returns the value for the specified key in the current section,
// returning NULL if the key does not exist.
//
//====================================================================

const char *FConfigFile::GetValueForKey (const char *key) const
{
	FConfigEntry *entry = FindEntry (CurrentSection, key);

	if (entry != NULL)
	{
		return entry->Value;
	}
	return NULL;
}

//====================================================================
//
// FConfigFile :: SetValueForKey
//
// Sets they key/value pair as specified in the current section. If
// duplicates are allowed, it always creates a new pair. Otherwise, it
// will overwrite the value of an existing key with the same name.
//
//====================================================================

void FConfigFile::SetValueForKey (const char *key, const char *value, bool duplicates)
{
	if (CurrentSection != NULL)
	{
		FConfigEntry *entry;

		if (duplicates || (entry = FindEntry (CurrentSection, key)) == NULL)
		{
			NewConfigEntry (CurrentSection, key, value);
		}
		else
		{
			entry->SetValue (value);
		}
	}
}

//====================================================================
//
// FConfigFile :: FindSection
//
//====================================================================

FConfigFile::FConfigSection *FConfigFile::FindSection (const char *name) const
{
	FConfigSection *section = Sections;

	while (section != NULL && section->SectionName.CompareNoCase(name) != 0)
	{
		section = section->Next;
	}
	return section;
}

//====================================================================
//
// FConfigFile :: RenameSection
//
//====================================================================

void FConfigFile::RenameSection (const char *oldname, const char *newname) const
{
	FConfigSection *section = FindSection(oldname);

	if (section != NULL)
	{
		section->SectionName = newname;
	}
}

//====================================================================
//
// FConfigFile :: FindEntry
//
//====================================================================

FConfigFile::FConfigEntry *FConfigFile::FindEntry (
	FConfigFile::FConfigSection *section, const char *key) const
{
	FConfigEntry *probe = section->RootEntry;

	while (probe != NULL && stricmp (probe->Key, key) != 0)
	{
		probe = probe->Next;
	}
	return probe;
}

//====================================================================
//
// FConfigFile :: NewConfigSection
//
//====================================================================

FConfigFile::FConfigSection *FConfigFile::NewConfigSection (const char *name)
{
	FConfigSection *section;

	section = FindSection (name);
	if (section == NULL)
	{
		section = new FConfigSection;
		section->RootEntry = NULL;
		section->LastEntryPtr = &section->RootEntry;
		section->Next = NULL;
		section->SectionName = name;
		*LastSectionPtr = section;
		LastSectionPtr = &section->Next;
	}
	return section;
}

//====================================================================
//
// FConfigFile :: NewConfigEntry
//
//====================================================================

FConfigFile::FConfigEntry *FConfigFile::NewConfigEntry (
	FConfigFile::FConfigSection *section, const char *key, const char *value)
{
	FConfigEntry *entry;
	size_t keylen;

	keylen = strlen (key);
	entry = (FConfigEntry *)new char[sizeof(*section)+keylen];
	entry->Value = NULL;
	entry->Next = NULL;
	memcpy (entry->Key, key, keylen);
	entry->Key[keylen] = 0;
	*(section->LastEntryPtr) = entry;
	section->LastEntryPtr = &entry->Next;
	entry->SetValue (value);
	return entry;
}

//====================================================================
//
// FConfigFile :: LoadConfigFile
//
//====================================================================

void FConfigFile::LoadConfigFile ()
{
	FILE *file = fopen (PathName, "r");
	bool succ;

	FileExisted = false;
	if (file == NULL)
	{
		return;
	}

	succ = ReadConfig (file);
	fclose (file);
	FileExisted = succ;
}

//====================================================================
//
// FConfigFile :: ReadConfig
//
//====================================================================

bool FConfigFile::ReadConfig (void *file)
{
	char readbuf[READBUFFERSIZE];
	FConfigSection *section = NULL;
	ClearConfig ();

	while (ReadLine (readbuf, READBUFFERSIZE, file) != NULL)
	{
		char *start = readbuf;
		char *equalpt;
		char *endpt;

		// Remove white space at start of line
		while (*start && *start <= ' ')
		{
			start++;
		}
		// Remove comment lines
		if (*start == '#' || (start[0] == '/' && start[1] == '/'))
		{
			continue;
		}
		// Remove white space at end of line
		endpt = start + strlen (start) - 1;
		while (endpt > start && *endpt <= ' ')
		{
			endpt--;
		}
		endpt[1] = 0;
		if (endpt <= start)
			continue;	// Nothing here

		if (*start == '[')
		{ // Section header
			if (*endpt == ']')
				*endpt = 0;
			section = NewConfigSection (start+1);
		}
		else if (section == NULL)
		{
			return false;
		}
		else
		{ // Should be key=value
			equalpt = strchr (start, '=');
			if (equalpt != NULL && equalpt > start)
			{
				// Remove white space in front of =
				char *whiteprobe = equalpt - 1;
				while (whiteprobe > start && isspace(*whiteprobe))
				{
					whiteprobe--;
				}
				whiteprobe[1] = 0;
				// Remove white space after =
				whiteprobe = equalpt + 1;
				while (*whiteprobe && isspace(*whiteprobe))
				{
					whiteprobe++;
				}
				*(whiteprobe - 1) = 0;
				// Check for multi-line value
				if (whiteprobe[0] == '<' && whiteprobe[1] == '<' && whiteprobe[2] == '<' && whiteprobe[3] != '\0')
				{
					ReadMultiLineValue (file, section, start, whiteprobe + 3);
				}
				else
				{
					NewConfigEntry (section, start, whiteprobe);
				}
			}
		}
	}
	return true;
}

//====================================================================
//
// FConfigFile :: ReadMultiLineValue
//
// Reads a multi-line value, with format as follows:
//
//    key=<<<ENDTAG
//    ... blah blah blah ...
//    >>>ENDTAG
//
// The final ENDTAG must be on a line all by itself.
//
//====================================================================

FConfigFile::FConfigEntry *FConfigFile::ReadMultiLineValue(void *file, FConfigSection *section, const char *key, const char *endtag)
{
	char readbuf[READBUFFERSIZE];
	FString value;
	size_t endlen = strlen(endtag);

	// Keep on reading lines until we reach a line that matches >>>endtag
	while (ReadLine(readbuf, READBUFFERSIZE, file) != NULL)
	{
		// Does the start of this line match the endtag?
		if (readbuf[0] == '>' && readbuf[1] == '>' && readbuf[2] == '>' &&
			strncmp(readbuf + 3, endtag, endlen) == 0)
		{ // Is there nothing but line break characters after the match?
			size_t i;
			for (i = endlen + 3; readbuf[i] != '\0'; ++i)
			{
				if (readbuf[i] != '\n' && readbuf[i] != '\r')
				{ // Not a line break character
					break;
				}
			}
			if (readbuf[i] == '\0')
			{ // We're done; strip the previous line's line breaks, since it's not part of the value.
				value.StripRight("\n\r");
			}
			break;
		}
		// Append this line to the value.
		value << readbuf;
	}
	return NewConfigEntry(section, key, value);
}

//====================================================================
//
// FConfigFile :: ReadLine
//
//====================================================================

char *FConfigFile::ReadLine (char *string, int n, void *file) const
{
	return fgets (string, n, (FILE *)file);
}

//====================================================================
//
// FConfigFile :: WriteConfigFile
//
//====================================================================

bool FConfigFile::WriteConfigFile () const
{
	if (!OkayToWrite && FileExisted)
	{ // Pretend it was written anyway so that the user doesn't get
	  // any "config not written" notifications, but only if the file
	  // already existed. Otherwise, let it write out a default one.
		return true;
	}

	FILE *file = fopen (PathName, "w");
	FConfigSection *section;
	FConfigEntry *entry;

	if (file == NULL)
		return false;

	WriteCommentHeader (file);

	section = Sections;
	while (section != NULL)
	{
		entry = section->RootEntry;
		if (section->Note.IsNotEmpty())
		{
			fputs (section->Note.GetChars(), file);
		}
		fprintf (file, "[%s]\n", section->SectionName.GetChars());
		while (entry != NULL)
		{
			if (strpbrk(entry->Value, "\r\n") == NULL)
			{ // Single-line value
				fprintf (file, "%s=%s\n", entry->Key, entry->Value);
			}
			else
			{ // Multi-line value
				const char *endtag = GenerateEndTag(entry->Value);
				fprintf (file, "%s=<<<%s\n%s\n>>>%s\n", entry->Key,
					endtag, entry->Value, endtag);
			}
			entry = entry->Next;
		}
		section = section->Next;
		fputs ("\n", file);
	}
	fclose (file);
	return true;
}

//====================================================================
//
// FConfigFile :: GenerateEndTag
//
// Generates a terminator sequence for multi-line values that does
// not appear anywhere in the value.
//
//====================================================================

const char *FConfigFile::GenerateEndTag(const char *value)
{
	static const char Base64Table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
	static char EndTag[25] = "EOV-";

	// Try different 20-character sequences until we find one that
	// isn't in the value. We create the sequences by generating two
	// 64-bit random numbers and Base64 encoding the first 15 bytes
	// from them.
	union { QWORD rand_num[2]; BYTE rand_bytes[16]; };
	do
	{
		rand_num[0] = pr_endtag.GenRand64();
		rand_num[1] = pr_endtag.GenRand64();

		for (int i = 0; i < 5; ++i)
		{
			//DWORD three_bytes = (rand_bytes[i*3] << 16) | (rand_bytes[i*3+1] << 8) | (rand_bytes[i*3+2]); // ???
			EndTag[4+i*4  ] = Base64Table[rand_bytes[i*3] >> 2];
			EndTag[4+i*4+1] = Base64Table[((rand_bytes[i*3] & 3) << 4) | (rand_bytes[i*3+1] >> 4)];
			EndTag[4+i*4+2] = Base64Table[((rand_bytes[i*3+1] & 15) << 2) | (rand_bytes[i*3+2] >> 6)];
			EndTag[4+i*4+3] = Base64Table[rand_bytes[i*3+2] & 63];
		}
	}
	while (strstr(value, EndTag) != NULL);
	return EndTag;
}

//====================================================================
//
// FConfigFile :: WriteCommentHeader
//
// Override in a subclass to write a header to the config file.
//
//====================================================================

void FConfigFile::WriteCommentHeader (FILE *file) const
{
}

//====================================================================
//
// FConfigFile :: FConfigEntry :: SetValue
//
//====================================================================

void FConfigFile::FConfigEntry::SetValue (const char *value)
{
	if (Value != NULL)
	{
		delete[] Value;
	}
	Value = new char[strlen (value)+1];
	strcpy (Value, value);
}

//====================================================================
//
// FConfigFile :: GetPosition
//
// Populates a struct with the current position of the parse cursor.
//
//====================================================================

void FConfigFile::GetPosition (FConfigFile::Position &pos) const
{
	pos.Section = CurrentSection;
	pos.Entry = CurrentEntry;
}

//====================================================================
//
// FConfigFile :: SetPosition
//
// Sets the parse cursor to a previously retrieved position.
//
//====================================================================

void FConfigFile::SetPosition (const FConfigFile::Position &pos)
{
	CurrentSection = pos.Section;
	CurrentEntry = pos.Entry;
}

//====================================================================
//
// FConfigFile :: SetSectionNote
//
// Sets a comment note to be inserted into the INI verbatim directly
// ahead of the section. Notes are lost when the INI is read so must
// be explicitly set to be maintained.
//
//====================================================================

void FConfigFile::SetSectionNote(const char *section, const char *note)
{
	SetSectionNote(FindSection(section), note);
}

void FConfigFile::SetSectionNote(const char *note)
{
	SetSectionNote(CurrentSection, note);
}

void FConfigFile::SetSectionNote(FConfigSection *section, const char *note)
{
	if (section != NULL)
	{
		if (note == NULL)
		{
			note = "";
		}
		section->Note = note;
	}
}
