/*
** configfile.cpp
** Implements the basic .ini parsing class
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
** This could have been done with a lot less source code using the STL and
** maps, but how much larger would the object code be?
**
** Regardless of object size, I had not considered the possibility of using
** the STL when I wrote this.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "configfile.h"

#define READBUFFERSIZE	256

FConfigFile::FConfigFile ()
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	PathName = "";
}

FConfigFile::FConfigFile (const char *pathname,
	void (*nosechandler)(const char *pathname, FConfigFile *config, void *userdata),
	void *userdata)
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	ChangePathName (pathname);
	LoadConfigFile (nosechandler, userdata);
}

FConfigFile::FConfigFile (const FConfigFile &other)
{
	Sections = CurrentSection = NULL;
	LastSectionPtr = &Sections;
	CurrentEntry = NULL;
	ChangePathName (other.PathName);
	*this = other;
}

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
		delete[] (char *)section;
		section = nextsection;
	}
}

FConfigFile &FConfigFile::operator = (const FConfigFile &other)
{
	FConfigSection *fromsection, *tosection;
	FConfigEntry *fromentry;

	ClearConfig ();
	fromsection = other.Sections;
	while (fromsection != NULL)
	{
		fromentry = fromsection->RootEntry;
		tosection = NewConfigSection (fromsection->Name);
		while (fromentry != NULL)
		{
			NewConfigEntry (tosection, fromentry->Key, fromentry->Value);
			fromentry = fromentry->Next;
		}
		fromsection = fromsection->Next;
	}
	return *this;
}

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

void FConfigFile::ChangePathName (const char *pathname)
{
	PathName = pathname;
}

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

const char *FConfigFile::GetCurrentSection () const
{
	if (CurrentSection != NULL)
	{
		return CurrentSection->Name;
	}
	return NULL;
}

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

const char *FConfigFile::GetValueForKey (const char *key) const
{
	FConfigEntry *entry = FindEntry (CurrentSection, key);

	if (entry != NULL)
	{
		return entry->Value;
	}
	return NULL;
}

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

FConfigFile::FConfigSection *FConfigFile::FindSection (const char *name) const
{
	FConfigSection *section = Sections;

	while (section != NULL && stricmp (section->Name, name) != 0)
	{
		section = section->Next;
	}
	return section;
}

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

FConfigFile::FConfigSection *FConfigFile::NewConfigSection (const char *name)
{
	FConfigSection *section;

	section = FindSection (name);
	if (section == NULL)
	{
		size_t namelen = strlen (name);
		section = (FConfigSection *)new char[sizeof(*section)+namelen];
		section->RootEntry = NULL;
		section->LastEntryPtr = &section->RootEntry;
		section->Next = NULL;
		memcpy (section->Name, name, namelen);
		section->Name[namelen] = 0;
		*LastSectionPtr = section;
		LastSectionPtr = &section->Next;
	}
	return section;
}

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

void FConfigFile::LoadConfigFile (void (*nosechandler)(const char *pathname, FConfigFile *config, void *userdata), void *userdata)
{
	FILE *file = fopen (PathName, "r");
	bool succ;

	if (file == NULL)
		return;

	succ = ReadConfig (file);
	fclose (file);

	if (!succ)
	{ // First valid line did not define a section
		if (nosechandler != NULL)
		{
			nosechandler (PathName, this, userdata);
		}
	}
}

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
				NewConfigEntry (section, start, whiteprobe);
			}
		}
	}
	return true;
}

char *FConfigFile::ReadLine (char *string, int n, void *file) const
{
	return fgets (string, n, (FILE *)file);
}

bool FConfigFile::WriteConfigFile () const
{
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
		fprintf (file, "[%s]\n", section->Name);
		while (entry != NULL)
		{
			fprintf (file, "%s=%s\n", entry->Key, entry->Value);
			entry = entry->Next;
		}
		section = section->Next;
		fprintf (file, "\n");
	}
	fclose (file);
	return true;
}

void FConfigFile::WriteCommentHeader (FILE *file) const
{
}

void FConfigFile::FConfigEntry::SetValue (const char *value)
{
	if (Value != NULL)
	{
		delete[] Value;
	}
	Value = new char[strlen (value)+1];
	strcpy (Value, value);
}

void FConfigFile::GetPosition (FConfigFile::Position &pos) const
{
	pos.Section = CurrentSection;
	pos.Entry = CurrentEntry;
}

void FConfigFile::SetPosition (const FConfigFile::Position &pos)
{
	CurrentSection = pos.Section;
	CurrentEntry = pos.Entry;
}
