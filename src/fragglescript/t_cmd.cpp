//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2017 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Emulation for selected Legacy console commands
// Unfortunately Legacy allows full access of FS to the console
// so everything that gets used by some map has to be emulated...
//
//---------------------------------------------------------------------------
//


#include <string.h>
#include "p_local.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "sc_man.h"
#include "g_level.h"
#include "r_renderer.h"
#include "d_player.h"
#include "g_levellocals.h"

//==========================================================================
//
//
//
//==========================================================================

static void FS_Gimme(const char * what)
{
	char buffer[80];

	// This is intentionally limited to the few items
	// it can handle in Legacy. 
	if (!strnicmp(what, "health", 6)) what="health";
	else if (!strnicmp(what, "ammo", 4)) what="ammo";
	else if (!strnicmp(what, "armor", 5)) what="greenarmor";
	else if (!strnicmp(what, "keys", 4)) what="keys";
	else if (!strnicmp(what, "weapons", 7)) what="weapons";
	else if (!strnicmp(what, "chainsaw", 8)) what="chainsaw";
	else if (!strnicmp(what, "shotgun", 7)) what="shotgun";
	else if (!strnicmp(what, "supershotgun", 12)) what="supershotgun";
	else if (!strnicmp(what, "rocket", 6)) what="rocketlauncher";
	else if (!strnicmp(what, "plasma", 6)) what="plasmarifle";
	else if (!strnicmp(what, "bfg", 3)) what="BFG9000";
	else if (!strnicmp(what, "chaingun", 8)) what="chaingun";
	else if (!strnicmp(what, "berserk", 7)) what="Berserk";
	else if (!strnicmp(what, "map", 3)) what="Allmap";
	else if (!strnicmp(what, "fullmap", 7)) what="Allmap";
	else return;

	mysnprintf(buffer, countof(buffer), "give %.72s", what);
	AddCommandString(buffer);
}


//==========================================================================
//
//
//
//==========================================================================

void FS_MapCmd(FScanner &sc)
{
	char nextmap[9];
	int NextSkill = -1;
	int flags = CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH;
	if (dmflags & DF_NO_MONSTERS)
		flags |= CHANGELEVEL_NOMONSTERS;
	sc.MustGetString();
	strncpy (nextmap, sc.String, 8);
	nextmap[8]=0;

	while (sc.GetString())
	{
		if (sc.Compare("-skill"))
		{
			sc.MustGetNumber();
			NextSkill = clamp<int>(sc.Number-1, 0, AllSkills.Size()-1);
		}
		else if (sc.Compare("-monsters"))
		{
			sc.MustGetNumber();
			if (sc.Number)
				flags &= ~CHANGELEVEL_NOMONSTERS;
			else
				flags |= CHANGELEVEL_NOMONSTERS;
		}
		else if (sc.Compare("-noresetplayers"))
		{
			flags &= ~(CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH);
		}
	}
	G_ChangeLevel(nextmap, 0, flags, NextSkill);
}

//==========================================================================
//
//
//
//==========================================================================

void FS_EmulateCmd(char * string)
{
	FScanner sc;
	sc.OpenMem("RUNCMD", string, (int)strlen(string));
	while (sc.GetString())
	{
		if (sc.Compare("GIMME"))
		{
			while (sc.GetString())
			{
				if (!sc.Compare(";")) FS_Gimme(sc.String);
				else break;
			}
		}
		else if (sc.Compare("ALLOWJUMP"))
		{
			sc.MustGetNumber();
			if (sc.Number) dmflags = dmflags & ~DF_NO_JUMP;
			else dmflags=dmflags | DF_NO_JUMP;
			while (sc.GetString())
			{
				if (sc.Compare(";")) break;
			}
		}
		else if (sc.Compare("gravity"))
		{
			sc.MustGetFloat();
			level.gravity=(float)(sc.Float*800);
			while (sc.GetString())
			{
				if (sc.Compare(";")) break;
			}
		}
		else if (sc.Compare("viewheight"))
		{
			sc.MustGetFloat();
			double playerviewheight = sc.Float;
			for(int i=0;i<MAXPLAYERS;i++)
			{
				// No, this is not correct. But this is the way Legacy WADs expect it to be handled!
				if (players[i].mo != NULL) players[i].mo->ViewHeight = playerviewheight;
				players[i].viewheight = playerviewheight;
				players[i].Uncrouch();
			}
			while (sc.GetString())
			{
				if (sc.Compare(";")) break;
			}
		}
		else if (sc.Compare("map"))
		{
			FS_MapCmd(sc);
		}
		else if (sc.Compare("gr_fogdensity"))
		{
			sc.MustGetNumber();
			// Using this disables most MAPINFO fog options!
			level.fogdensity = sc.Number * 70 / 400;
			level.outsidefogdensity = 0;
			level.skyfog = 0;
			level.info->outsidefog = 0;
		}
		else if (sc.Compare("gr_fogcolor"))
		{
			sc.MustGetString();
			level.fadeto = (uint32_t)strtoull(sc.String, NULL, 16);
		}

		else
		{
			// Skip unhandled commands
			while (sc.GetString())
			{
				if (sc.Compare(";")) break;
			}
		}
	}
}
