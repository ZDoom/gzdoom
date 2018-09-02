/*
** gameconfigfile.h
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

#ifndef __GAMECONFIGFILE_H__
#define __GAMECONFIGFILE_H__

#include "doomtype.h"
#include "configfile.h"

class FArgs;
class FIWadManager;

class FGameConfigFile : public FConfigFile
{
public:
	FGameConfigFile ();
	~FGameConfigFile ();

	void DoAutoloadSetup (FIWadManager *iwad_man);
	void DoGlobalSetup ();
	void DoGameSetup (const char *gamename);
	void DoKeySetup (const char *gamename);
	void DoModSetup (const char *gamename);
	void ArchiveGlobalData ();
	void ArchiveGameData (const char *gamename);
	void AddAutoexec (FArgs *list, const char *gamename);
	FString GetConfigPath (bool tryProg);
	void ReadNetVars ();

protected:
	void WriteCommentHeader (FileWriter *file) const;
	void CreateStandardAutoExec (const char *section, bool start);

private:
	void SetRavenDefaults (bool isHexen);
	void ReadCVars (uint32_t flags);

	bool bModSetup;

	char section[64];
	char *subsection;
	size_t sublen;
};

extern FGameConfigFile *GameConfig;

#endif //__GAMECONFIGFILE_H__
