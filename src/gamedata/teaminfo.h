/*
** teaminfo.h
** Parses TEAMINFO and manages teams.
**
**---------------------------------------------------------------------------
** Copyright 2007-2009 Christopher Westley
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

#ifndef __TEAMINFO_H__
#define __TEAMINFO_H__

#include "doomtype.h"
#include "sc_man.h"

const int TEAM_NONE = 255;
const int TEAM_MAXIMUM = 16;

class FTeam
{
public:
	FTeam ();
	void ParseTeamInfo ();
	bool IsValidTeam (unsigned int uiTeam) const;

	const char *GetName () const;
	int GetPlayerColor () const;
	int GetTextColor () const;
	const FString& GetLogo () const;
	bool GetAllowCustomPlayerColor () const;

	int			m_iPlayerCount;
	int			m_iScore;
	int			m_iPresent;
	int			m_iTies;

private:
	void ParseTeamDefinition (FScanner &Scan);
	void ClearTeams ();

public:	// needed for script access.
	FString		m_Name;
private:
	int			m_iPlayerColor;
	FString		m_TextColor;
	FString		m_Logo;
	bool		m_bAllowCustomPlayerColor;
};

extern FTeam			TeamLibrary;
extern TArray<FTeam>	Teams;

#endif
