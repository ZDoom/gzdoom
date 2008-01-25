/*
** teaminfo.cpp
** Implementation of the TEAMINFO lump.
**
**---------------------------------------------------------------------------
** Copyright 2007-2008 Christopher Westley
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

#include "i_system.h"
#include "sc_man.h"
#include "teaminfo.h"
#include "v_palette.h"
#include "w_wad.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void TEAMINFO_Init ();
void TEAMINFO_ParseTeam (FScanner &sc);

bool TEAMINFO_IsValidTeam (int team);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TArray <TEAMINFO> teams;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *keywords_teaminfo [] = {
	"PLAYERCOLOR",
	"TEXTCOLOR",
	"LOGO",
	NULL
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TEAMINFO_Init
//
//==========================================================================

void TEAMINFO_Init ()
{
	int lastlump = 0, lump;

	while ((lump = Wads.FindLump ("TEAMINFO", &lastlump)) != -1)
	{
		FScanner sc(lump, "TEAMINFO");
		while (sc.GetString ())
		{
			if (sc.Compare("CLEARTEAMS"))
				teams.Clear ();
			else if (sc.Compare("TEAM"))
				TEAMINFO_ParseTeam (sc);
			else 
				sc.ScriptError ("Unknown command %s in TEAMINFO", sc.String);
		}
	}

	if (teams.Size () < 2)
		I_FatalError ("At least two teams must be defined in TEAMINFO");
}

//==========================================================================
//
// TEAMINFO_ParseTeam
//
//==========================================================================

void TEAMINFO_ParseTeam (FScanner &sc)
{
	TEAMINFO team;
	int i;

	sc.MustGetString ();
	team.name = sc.String;

	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		switch(i = sc.MatchString (keywords_teaminfo))
		{
		case 0:
			sc.MustGetString ();
			team.playercolor = V_GetColor (NULL, sc.String);
			break;

		case 1:
			sc.MustGetString();
			team.textcolor = '[';
			team.textcolor << sc.String << ']';
			break;

		case 2:
			sc.MustGetString ();
			team.logo = sc.String;
			break;

		default:
			break;
		}
	}

	teams.Push (team);
}

//==========================================================================
//
// TEAMINFO_IsValidTeam
//
//==========================================================================

bool TEAMINFO_IsValidTeam (int team)
{
	if (team < 0 || team >= (signed)teams.Size ())
	{
		return false;
	}

	return true;
}

//==========================================================================
//
// TEAMINFO :: GetTextColor
//
//==========================================================================

int TEAMINFO::GetTextColor () const
{
	if (textcolor.IsEmpty())
	{
		return CR_UNTRANSLATED;
	}
	const BYTE *cp = (const BYTE *)textcolor.GetChars();
	int color = V_ParseFontColor(cp, 0, 0);
	if (color == CR_UNDEFINED)
	{
		Printf("Undefined color '%s' in definition of team %s\n", textcolor.GetChars (), name.GetChars ());
		color = CR_UNTRANSLATED;
	}
	return color;
}
