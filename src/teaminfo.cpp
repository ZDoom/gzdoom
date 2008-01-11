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
void TEAMINFO_ParseTeam ();

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
		SC_OpenLumpNum (lump, "TEAMINFO");
		while (SC_GetString ())
		{
			if (SC_Compare("CLEARTEAMS"))
				teams.Clear ();
			else if (SC_Compare("TEAM"))
				TEAMINFO_ParseTeam ();
			else 
				SC_ScriptError ("Unknown command %s in TEAMINFO", sc_String);
		}
		SC_Close();
	}

	if (teams.Size () < 2)
		I_FatalError ("At least two teams must be defined in TEAMINFO");
}

//==========================================================================
//
// TEAMINFO_ParseTeam
//
//==========================================================================

void TEAMINFO_ParseTeam ()
{
	TEAMINFO team;
	int i;
	char *color;

	SC_MustGetString ();
	team.name = sc_String;

	SC_MustGetStringName("{");
	while (!SC_CheckString("}"))
	{
		SC_MustGetString();
		switch(i = SC_MatchString (keywords_teaminfo))
		{
		case 0:
			SC_MustGetString ();
			color = sc_String;
			team.playercolor = V_GetColor (NULL, color);
			break;

		case 1:
			SC_MustGetString();
			team.textcolor = '[';
			team.textcolor << sc_String << ']';
			break;

		case 2:
			SC_MustGetString ();
			team.logo = sc_String;
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
