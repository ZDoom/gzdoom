/*
** teaminfo.cpp
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

// HEADER FILES ------------------------------------------------------------

#include "c_dispatch.h"
#include "gi.h"

#include "teaminfo.h"
#include "v_font.h"
#include "v_video.h"
#include "filesystem.h"
#include "vm.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FTeam			TeamLibrary;
TArray<FTeam>	Teams;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *TeamInfoOptions[] =
{
	"Game",
	"PlayerColor",
	"TextColor",
	"Logo",
	"AllowCustomPlayerColor",
	"RailColor",
	"FlagItem",
	"SkullItem",
	"PlayerStartThingNumber",
	"SmallFlagHUDIcon",
	"SmallSkullHUDIcon",
	"LargeFlagHUDIcon",
	"LargeSkullHUDIcon",
	"WinnerPic",
	"LoserPic",
	"WinnerTheme",
	"LoserTheme",
};

enum ETeamOptions
{
	TEAMINFO_Game,
	TEAMINFO_PlayerColor,
	TEAMINFO_TextColor,
	TEAMINFO_Logo,
	TEAMINFO_AllowCustomPlayerColor,
	TEAMINFO_RailColor,
	TEAMINFO_FlagItem,
	TEAMINFO_SkullItem,
	TEAMINFO_PlayerStartThingNumber,
	TEAMINFO_SmallFlagHUDIcon,
	TEAMINFO_SmallSkullHUDIcon,
	TEAMINFO_LargeFlagHUDIcon,
	TEAMINFO_LargeSkullHUDIcon,
	TEAMINFO_WinnerPic,
	TEAMINFO_LoserPic,
	TEAMINFO_WinnerTheme,
	TEAMINFO_LoserTheme,
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FTeam :: FTeam
//
//==========================================================================

FTeam::FTeam ()
{
	m_iPlayerColor = 0;
	m_iPlayerCount = 0;
	m_iScore = 0;
	m_iPresent = 0;
	m_iTies = 0;
	m_bAllowCustomPlayerColor = false;
}

//==========================================================================
//
// FTeam :: ParseTeamInfo
//
//==========================================================================

void FTeam::ParseTeamInfo ()
{
	int iLump, iLastLump = 0;

	Teams.Clear();
	while ((iLump = fileSystem.FindLump ("TEAMINFO", &iLastLump)) != -1)
	{
		FScanner Scan (iLump);

		while (Scan.GetString ())
		{
			if (Scan.Compare ("ClearTeams"))
				ClearTeams ();
			else if (Scan.Compare ("Team"))
				ParseTeamDefinition (Scan);
			else
				Scan.ScriptError ("ParseTeamInfo: Unknown team command '%s'.\n", Scan.String);
		}
	}

	if (Teams.Size () < 2)
		I_FatalError ("ParseTeamInfo: At least two teams must be defined in TEAMINFO.");
	else if (Teams.Size () > (unsigned)TEAM_MAXIMUM)
		I_FatalError ("ParseTeamInfo: Too many teams defined. (Maximum: %d)", TEAM_MAXIMUM);
}

//==========================================================================
//
// FTeam :: ParseTeamDefinition
//
//==========================================================================

void FTeam::ParseTeamDefinition (FScanner &Scan)
{
	FTeam Team;
	int valid = -1;
	Scan.MustGetString ();
	Team.m_Name = Scan.String;
	Scan.MustGetStringName ("{");

	while (!Scan.CheckString ("}"))
	{
		Scan.MustGetString ();

		switch (Scan.MatchString (TeamInfoOptions))
		{
		case TEAMINFO_Game:
			Scan.MustGetString ();
			if (Scan.Compare("Any")) valid = 1;
			else if (CheckGame(Scan.String, false)) valid = 1;
			else if (valid == -1) valid = 0;
			break;

		case TEAMINFO_PlayerColor:
			Scan.MustGetString ();
			Team.m_iPlayerColor = V_GetColor (Scan);
			break;

		case TEAMINFO_TextColor:
			Scan.MustGetString ();
			Team.m_TextColor.AppendFormat ("[%s]", Scan.String);
			break;

		case TEAMINFO_Logo:
			Scan.MustGetString ();
			Team.m_Logo = Scan.String;
			break;

		case TEAMINFO_AllowCustomPlayerColor:
			Team.m_bAllowCustomPlayerColor = true;
			break;

		case TEAMINFO_PlayerStartThingNumber:
			Scan.MustGetNumber ();
			break;

		case TEAMINFO_RailColor:
		case TEAMINFO_FlagItem:
		case TEAMINFO_SkullItem:
		case TEAMINFO_SmallFlagHUDIcon:
		case TEAMINFO_SmallSkullHUDIcon:
		case TEAMINFO_LargeFlagHUDIcon:
		case TEAMINFO_LargeSkullHUDIcon:
		case TEAMINFO_WinnerPic:
		case TEAMINFO_LoserPic:
		case TEAMINFO_WinnerTheme:
		case TEAMINFO_LoserTheme:
			Scan.MustGetString ();
			break;

		default:
			Scan.ScriptError ("ParseTeamDefinition: Unknown team option '%s'.\n", Scan.String);
			break;
		}
	}

	if (valid) Teams.Push (Team);
}

//==========================================================================
//
// FTeam :: ClearTeams
//
//==========================================================================

void FTeam::ClearTeams ()
{
	Teams.Clear ();
}

//==========================================================================
//
// FTeam :: IsValidTeam
//
//==========================================================================

bool FTeam::IsValidTeam (unsigned int uiTeam)
{
	if (uiTeam >= Teams.Size ())
		return false;

	return true;
}

//==========================================================================
//
// FTeam :: GetName
//
//==========================================================================

const char *FTeam::GetName () const
{
	return m_Name;
}

//==========================================================================
//
// FTeam :: GetPlayerColor
//
//==========================================================================

int FTeam::GetPlayerColor () const
{
	return m_iPlayerColor;
}

//==========================================================================
//
// FTeam :: GetTextColor
//
//==========================================================================

int FTeam::GetTextColor () const
{
	if (m_TextColor.IsEmpty ())
		return CR_UNTRANSLATED;

	const uint8_t *pColor = (const uint8_t *)m_TextColor.GetChars ();
	int iColor = V_ParseFontColor (pColor, 0, 0);

	if (iColor == CR_UNDEFINED)
	{
		Printf ("GetTextColor: Undefined color '%s' in definition of team '%s'.\n", m_TextColor.GetChars (), m_Name.GetChars ());
		return CR_UNTRANSLATED;
	}

	return iColor;
}

//==========================================================================
//
// FTeam :: GetLogo
//
//==========================================================================

FString FTeam::GetLogo () const
{
	return m_Logo;
}

//==========================================================================
//
// FTeam :: GetAllowCustomPlayerColor
//
//==========================================================================

bool FTeam::GetAllowCustomPlayerColor () const
{
	return m_bAllowCustomPlayerColor;
}

//==========================================================================
//
// CCMD teamlist
//
//==========================================================================

CCMD (teamlist)
{
	Printf ("Defined teams are as follows:\n");

	for (unsigned int uiTeam = 0; uiTeam < Teams.Size (); uiTeam++)
		Printf ("%d : %s\n", uiTeam, Teams[uiTeam].GetName ());

	Printf ("End of team list.\n");
}


DEFINE_GLOBAL(Teams)
DEFINE_FIELD_NAMED(FTeam, m_Name, mName)
