/*
** configfile.h
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

#ifndef __CONFIGFILE_H__
#define __CONFIGFILE_H__

#include <stdio.h>
#include "files.h"
#include "zstring.h"

class FConfigFile
{
public:
	FConfigFile ();
	FConfigFile (const char *pathname);
	FConfigFile (const FConfigFile &other);
	virtual ~FConfigFile ();

	void ClearConfig ();
	FConfigFile &operator= (const FConfigFile &other);

	bool HaveSections () { return Sections != NULL; }
	void CreateSectionAtStart (const char *name);
	void MoveSectionToStart (const char *name);
	void SetSectionNote (const char *section, const char *note);
	void SetSectionNote (const char *note);
	bool SetSection (const char *section, bool allowCreate=false);
	bool SetFirstSection ();
	bool SetNextSection ();
	const char *GetCurrentSection () const;
	void ClearCurrentSection ();
	bool DeleteCurrentSection ();
	void ClearKey (const char *key);

	bool SectionIsEmpty ();
	bool NextInSection (const char *&key, const char *&value);
	const char *GetValueForKey (const char *key) const;
	void SetValueForKey (const char *key, const char *value, bool duplicates=false);

	const char *GetPathName () const { return PathName.GetChars(); }
	void ChangePathName (const char *path);

	void LoadConfigFile ();
	bool WriteConfigFile () const;

protected:
	virtual void WriteCommentHeader (FileWriter *file) const;

	virtual char *ReadLine (char *string, int n, void *file) const;
	bool ReadConfig (void *file);
	static const char *GenerateEndTag(const char *value);
	void RenameSection(const char *oldname, const char *newname) const;

	bool OkayToWrite;
	bool FileExisted;

private:
	struct FConfigEntry
	{
		char *Value;
		FConfigEntry *Next;
		char Key[1];	// + length of key

		void SetValue (const char *val);
	};
	struct FConfigSection
	{
		FString SectionName;
		FConfigEntry *RootEntry;
		FConfigEntry **LastEntryPtr;
		FConfigSection *Next;
		FString Note;
		//char Name[1];	// + length of name
	};

	FConfigSection *Sections;
	FConfigSection **LastSectionPtr;
	FConfigSection *CurrentSection;
	FConfigEntry *CurrentEntry;
	FString PathName;

	FConfigSection *FindSection (const char *name) const;
	FConfigEntry *FindEntry (FConfigSection *section, const char *key) const;
	FConfigSection *NewConfigSection (const char *name);
	FConfigEntry *NewConfigEntry (FConfigSection *section, const char *key, const char *value);
	FConfigEntry *ReadMultiLineValue (void *file, FConfigSection *section, const char *key, const char *terminator);
	void SetSectionNote (FConfigSection *section, const char *note);

public:
	class Position
	{
		friend class FConfigFile;

		FConfigSection *Section;
		FConfigEntry *Entry;
	};

	void GetPosition (Position &pos) const;
	void SetPosition (const Position &pos);
};

#endif //__CONFIGFILE_H__
