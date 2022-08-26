/*
** c_cmds.cpp
** Miscellaneous console commands.
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "version.h"
#include "c_console.h"
#include "c_dispatch.h"

#include "i_system.h"
#include "engineerrors.h"
#include "doomstat.h"
#include "gstrings.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_level.h"
#include "filesystem.h"
#include "gi.h"
#include "r_defs.h"
#include "d_player.h"
#include "p_local.h"
#include "r_sky.h"
#include "p_setup.h"
#include "cmdlib.h"
#include "d_net.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "r_utility.h"
#include "c_functions.h"
#include "g_levellocals.h"
#include "v_video.h"
#include "md5.h"
#include "findfile.h"
#include "i_music.h"
#include "s_music.h"
#include "texturemanager.h"
#include "v_draw.h"

extern FILE *Logfile;
extern bool insave;

CVAR (Bool, sv_cheats, false, CVAR_SERVERINFO)
CVAR (Bool, sv_unlimited_pickup, false, CVAR_SERVERINFO)
CVAR (Int, cl_blockcheats, 0, 0)

CVARD(Bool, show_messages, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "enable/disable showing messages")
CVAR(Bool, show_obituaries, true, CVAR_ARCHIVE)


bool CheckCheatmode (bool printmsg, bool sponly)
{
	if (sponly && netgame)
	{
		if (printmsg) Printf("Not in a singleplayer game.\n");
		return true;
	}
	if ((G_SkillProperty(SKILLP_DisableCheats) || netgame || deathmatch) && (!sv_cheats))
	{
		if (printmsg) Printf ("sv_cheats must be true to enable this command.\n");
		return true;
	}
	else if (cl_blockcheats != 0)
	{
		if (printmsg && cl_blockcheats == 1) Printf ("cl_blockcheats is turned on and disabled this command.\n");
		return true;
	}
	else
	{
		return false;
	}
}

/*
==================
Cmd_God

Sets client to godmode

argv(0) god
==================
*/
CCMD (god)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_GOD);
}

CCMD(god2)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte(DEM_GENERICCHEAT);
	Net_WriteByte(CHT_GOD2);
}

CCMD (iddqd)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_IDDQD);
}

CCMD (buddha)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte(DEM_GENERICCHEAT);
	Net_WriteByte(CHT_BUDDHA);
}

CCMD(buddha2)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte(DEM_GENERICCHEAT);
	Net_WriteByte(CHT_BUDDHA2);
}

CCMD (notarget)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOTARGET);
}

CCMD (fly)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_FLY);
}

/*
==================
Cmd_Noclip

argv(0) noclip
==================
*/
CCMD (noclip)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP);
}

CCMD (noclip2)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP2);
}

CCMD (powerup)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_POWER);
}

CCMD (morphme)
{
	if (CheckCheatmode ())
		return;

	if (argv.argc() == 1)
	{
		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_MORPH);
	}
	else
	{
		Net_WriteByte (DEM_MORPHEX);
		Net_WriteString (argv[1]);
	}
}

CCMD (anubis)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_ANUBIS);
}

// [GRB]
CCMD (resurrect)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_RESSURECT);
}

EXTERN_CVAR (Bool, chasedemo)

CCMD (chase)
{
	if (demoplayback)
	{
		int i;

		if (chasedemo)
		{
			chasedemo = false;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats &= ~CF_CHASECAM;
		}
		else
		{
			chasedemo = true;
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats |= CF_CHASECAM;
		}
		R_ResetViewInterpolation ();
	}
	else
	{
		// Check if we're allowed to use chasecam.
		if (gamestate != GS_LEVEL || (!(dmflags2 & DF2_CHASECAM) && deathmatch && CheckCheatmode ()))
			return;

		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_CHASECAM);
	}
}

CCMD (idclev)
{
	if (netgame)
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		int epsd, map;
		char buf[2];
		FString mapname;

		buf[0] = argv[1][0] - '0';
		buf[1] = argv[1][1] - '0';

		if (gameinfo.flags & GI_MAPxx)
		{
			epsd = 1;
			map = buf[0]*10 + buf[1];
		}
		else
		{
			epsd = buf[0];
			map = buf[1];
		}

		// Catch invalid maps.
		mapname = CalcMapName (epsd, map);

		if (!P_CheckMapData(mapname))
			return;

		// So be it.
		Printf ("%s\n", GStrings("STSTR_CLEV"));
      	G_DeferedInitNew (mapname);
		//players[0].health = 0;		// Force reset
	}
}

CCMD (hxvisit)
{
	if (netgame)
		return;

	if ((argv.argc() > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		FString mapname("&wt@");

		mapname << argv[1][0] << argv[1][1];

		if (CheckWarpTransMap (mapname, false))
		{
			// Just because it's in MAPINFO doesn't mean it's in the wad.

			if (P_CheckMapData(mapname))
			{
				// So be it.
				Printf ("%s\n", GStrings("STSTR_CLEV"));
      			G_DeferedInitNew (mapname);
				return;
			}
		}
		Printf ("No such map found\n");
	}
}

CCMD (changemap)
{
	if (!players[consoleplayer].mo || !usergame)
	{
		Printf ("Use the map command when not in a game.\n");
		return;
	}

	if (!players[consoleplayer].settings_controller && netgame)
	{
		Printf ("Only setting controllers can change the map.\n");
		return;
	}

	if (argv.argc() > 1)
	{
		const char *mapname = argv[1];
		if (!strcmp(mapname, "*"))
		{
			mapname = primaryLevel->MapName.GetChars();
		}
		else if (!strcmp(mapname, "+") && primaryLevel->NextMap.Len() > 0 && primaryLevel->NextMap.Compare("enDSeQ", 6))
		{
			mapname = primaryLevel->NextMap.GetChars();
		}
		else if (!strcmp(mapname, "+$") && primaryLevel->NextSecretMap.Len() > 0 && primaryLevel->NextSecretMap.Compare("enDSeQ", 6))
		{
			mapname = primaryLevel->NextSecretMap.GetChars();
		}

		try
		{
			if (!P_CheckMapData(mapname))
			{
				Printf ("No map %s\n", mapname);
			}
			else
			{
				if (argv.argc() > 2)
				{
					Net_WriteByte (DEM_CHANGEMAP2);
					Net_WriteByte (atoi(argv[2]));
				}
				else
				{
					Net_WriteByte (DEM_CHANGEMAP);
				}
				Net_WriteString (mapname);
			}
		}
		catch(CRecoverableError &error)
		{
			if (error.GetMessage())
				Printf("%s", error.GetMessage());
		}
	}
	else
	{
		Printf ("Usage: changemap <map name> [position]\n");
	}
}

CCMD (give)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	Net_WriteByte (DEM_GIVECHEAT);
	Net_WriteString (argv[1]);
	if (argv.argc() > 2)
		Net_WriteLong(atoi(argv[2]));
	else
		Net_WriteLong(0);
}

CCMD (take)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	Net_WriteByte (DEM_TAKECHEAT);
	Net_WriteString (argv[1]);
	if (argv.argc() > 2)
		Net_WriteLong(atoi (argv[2]));
	else
		Net_WriteLong (0);
}

CCMD(setinv)
{
	if (CheckCheatmode() || argv.argc() < 2)
		return;

	Net_WriteByte(DEM_SETINV);
	Net_WriteString(argv[1]);
	if (argv.argc() > 2)
		Net_WriteLong(atoi(argv[2]));
	else
		Net_WriteLong(0);

	if (argv.argc() > 3)
		Net_WriteByte(!!atoi(argv[3]));
	else
		Net_WriteByte(0);

}


CCMD (puke)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 6)
	{
		Printf ("Usage: puke <script> [arg1] [arg2] [arg3] [arg4]\n");
	}
	else
	{
		int script = atoi (argv[1]);

		if (script == 0)
		{ // Script 0 is reserved for Strife support. It is not pukable.
			return;
		}
		int arg[4] = { 0, 0, 0, 0 };
		int argn = min<int>(argc - 2, countof(arg)), i;

		for (i = 0; i < argn; ++i)
		{
			arg[i] = atoi (argv[2+i]);
		}

		if (script > 0)
		{
			Net_WriteByte (DEM_RUNSCRIPT);
			Net_WriteWord (script);
		}
		else
		{
			Net_WriteByte (DEM_RUNSCRIPT2);
			Net_WriteWord (-script);
		}
		Net_WriteByte (argn);
		for (i = 0; i < argn; ++i)
		{
			Net_WriteLong (arg[i]);
		}
	}
}

CCMD (pukename)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 7)
	{
		Printf ("Usage: pukename \"<script>\" [\"always\"] [arg1] [arg2] [arg3] [arg4]\n");
	}
	else
	{
		bool always = false;
		int argstart = 2;
		int arg[4] = { 0, 0, 0, 0 };
		int argn = 0, i;
		
		if (argc > 2)
		{
			if (stricmp(argv[2], "always") == 0)
			{
				always = true;
				argstart = 3;
			}
			argn = min<int>(argc - argstart, countof(arg));
			for (i = 0; i < argn; ++i)
			{
				arg[i] = atoi(argv[argstart + i]);
			}
		}
		Net_WriteByte(DEM_RUNNAMEDSCRIPT);
		Net_WriteString(argv[1]);
		Net_WriteByte(argn | (always << 7));
		for (i = 0; i < argn; ++i)
		{
			Net_WriteLong(arg[i]);
		}
	}
}

CCMD (special)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 7)
	{
		Printf("Usage: special <special-name> [arg1] [arg2] [arg3] [arg4] [arg5]\n");
	}
	else
	{
		int specnum;

		if (argv[1][0] >= '0' && argv[1][0] <= '9')
		{
			specnum = atoi(argv[1]);
			if (specnum < 0 || specnum > 255)
			{
				Printf("Bad special number\n");
				return;
			}
		}
		else
		{
			int min_args;
			specnum = P_FindLineSpecial(argv[1], &min_args);
			if (specnum == 0 || min_args < 0)
			{
				Printf("Unknown special\n");
				return;
			}
			if (argc < 2 + min_args)
			{
				Printf("%s needs at least %d argument%s\n", argv[1], min_args, min_args == 1 ? "" : "s");
				return;
			}
		}
		Net_WriteByte(DEM_RUNSPECIAL);
		Net_WriteWord(specnum);
		Net_WriteByte(argc - 2);
		for (int i = 2; i < argc; ++i)
		{
			Net_WriteLong(atoi(argv[i]));
		}
	}
}


//==========================================================================
//
// CCMD warp
//
// Warps to a specific location on a map
//
//==========================================================================

CCMD (warp)
{
	if (CheckCheatmode ())
	{
		return;
	}
	if (gamestate != GS_LEVEL)
	{
		Printf ("You can only warp inside a level.\n");
		return;
	}
	if (argv.argc() < 3 || argv.argc() > 4)
	{
		Printf ("Usage: warp <x> <y> [z]\n");
	}
	else
	{
		Net_WriteByte (DEM_WARPCHEAT);
		Net_WriteWord (atoi (argv[1]));
		Net_WriteWord (atoi (argv[2]));
		Net_WriteWord (argv.argc() == 3 ? ONFLOORZ/65536 : atoi (argv[3]));
	}
}

//==========================================================================
//
// CCMD load
//
// Load a saved game.
//
//==========================================================================

UNSAFE_CCMD (load)
{
    if (argv.argc() != 2)
	{
        Printf ("usage: load <filename>\n");
        return;
    }
	if (netgame)
	{
		Printf ("cannot load during a network game\n");
		return;
	}
	FString fname = argv[1];
	DefaultExtension (fname, "." SAVEGAME_EXT);
    G_LoadGame (fname);
}

//==========================================================================
//
// CCMD save
//
// Save the current game.
//
//==========================================================================

UNSAFE_CCMD (save)
{
    if (argv.argc() < 2 || argv.argc() > 3)
	{
        Printf ("usage: save <filename> [description]\n");
        return;
    }
    FString fname = argv[1];
	DefaultExtension (fname, "." SAVEGAME_EXT);
	G_SaveGame (fname, argv.argc() > 2 ? argv[2] : argv[1]);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

CCMD(linetarget)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	C_AimLine(&t, false);
	if (t.linetarget)
		C_PrintInfo(t.linetarget, argv.argc() > 1 && atoi(argv[1]) != 0);
	else
		Printf("No target found\n");
}

// As linetarget, but also give info about non-shootable actors
CCMD(info)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	C_AimLine(&t, true);
	if (t.linetarget)
		C_PrintInfo(t.linetarget, !(argv.argc() > 1 && atoi(argv[1]) == 0));
	else
		Printf("No target found. Info cannot find actors that have "
				"the NOBLOCKMAP flag or have height/radius of 0.\n");
}

CCMD(myinfo)
{
	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	C_PrintInfo(players[consoleplayer].mo, true);
}

typedef bool (*ActorTypeChecker) (AActor *);

static bool IsActorAMonster(AActor *mo)
{
	return mo->flags3&MF3_ISMONSTER && !(mo->flags&MF_CORPSE) && !(mo->flags&MF_FRIENDLY);
}

static bool IsActorAnItem(AActor *mo)
{
	return mo->IsKindOf(NAME_Inventory) && mo->flags&MF_SPECIAL;
}

static bool IsActorACountItem(AActor *mo)
{
	return mo->IsKindOf(NAME_Inventory) && mo->flags&MF_SPECIAL && mo->flags&MF_COUNTITEM;
}

// [SP] for all actors
static bool IsActor(AActor *mo)
{
	return mo->IsMapActor();
}

// [SP] modified - now allows showing count only, new arg must be passed. Also now still counts regardless, if lists are printed.
static void PrintFilteredActorList(const ActorTypeChecker IsActorType, const char *FilterName, bool countOnly)
{
	AActor *mo;
	const PClass *FilterClass = NULL;
	int counter = 0;
	int tid = 0;

	if (FilterName != NULL)
	{
		FilterClass = PClass::FindActor(FilterName);
		if (FilterClass == NULL)
		{
			char *endp;
			tid = (int)strtol(FilterName, &endp, 10);
			if (*endp != 0)
			{
				Printf("%s is not an actor class.\n", FilterName);
				return;
			}
		}
	}
	// This only works on the primary level.
	auto it = primaryLevel->GetThinkerIterator<AActor>();

	while ( (mo = it.Next()) )
	{
		if ((FilterClass == NULL || mo->IsA(FilterClass)) && IsActorType(mo))
		{
			if (tid == 0 || tid == mo->tid)
			{
				counter++;
				if (!countOnly)
				{
					Printf("%s at (%f,%f,%f)",
						mo->GetClass()->TypeName.GetChars(), mo->X(), mo->Y(), mo->Z());
					if (mo->tid)
						Printf(" (TID:%d)", mo->tid);
					Printf("\n");
				}
			}
		}
	}
	Printf("%i match(s) found.\n", counter);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(actorlist) // [SP] print all actors (this can get quite big?)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActor, argv.argc() > 1 ? argv[1] : NULL, false);
}

CCMD(actornum) // [SP] count all actors
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActor, argv.argc() > 1 ? argv[1] : NULL, true);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(monster)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAMonster, argv.argc() > 1 ? argv[1] : NULL, false);
}

CCMD(monsternum) // [SP] count monsters
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAMonster, argv.argc() > 1 ? argv[1] : NULL, true);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(items)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAnItem, argv.argc() > 1 ? argv[1] : NULL, false);
}

CCMD(itemsnum) // [SP] # of any items
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAnItem, argv.argc() > 1 ? argv[1] : NULL, true);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(countitems)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorACountItem, argv.argc() > 1 ? argv[1] : NULL, false);
}

CCMD(countitemsnum) // [SP] # of counted items
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorACountItem, argv.argc() > 1 ? argv[1] : NULL, true);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(changesky)
{
	const char *sky1name;

	if (netgame || argv.argc()<2) return;

	// This only alters the primary level's sky setting. For testing out a sky that is sufficient.
	sky1name = argv[1];
	if (sky1name[0] != 0)
	{
		FTextureID newsky = TexMan.GetTextureID(sky1name, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
		if (newsky.Exists())
		{
			primaryLevel->skytexture1 = newsky;
		}
		else
		{
			Printf("changesky: Texture '%s' not found\n", sky1name);
		}
	}
	InitSkyMap (primaryLevel);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(thaw)
{
	if (CheckCheatmode())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_CLEARFROZENPROPS);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(nextmap)
{
	if (netgame)
	{
		Printf ("Use " TEXTCOLOR_BOLD "changemap" TEXTCOLOR_NORMAL " instead. " TEXTCOLOR_BOLD "Nextmap"
				TEXTCOLOR_NORMAL " is for single-player only.\n");
		return;
	}
	
	if (primaryLevel->NextMap.Len() > 0 && primaryLevel->NextMap.Compare("enDSeQ", 6))
	{
		G_DeferedInitNew(primaryLevel->NextMap);
	}
	else
	{
		Printf("no next map!\n");
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(nextsecret)
{
	if (netgame)
	{
		Printf ("Use " TEXTCOLOR_BOLD "changemap" TEXTCOLOR_NORMAL " instead. " TEXTCOLOR_BOLD "Nextsecret"
				TEXTCOLOR_NORMAL " is for single-player only.\n");
		return;
	}

	if (primaryLevel->NextSecretMap.Len() > 0 && primaryLevel->NextSecretMap.Compare("enDSeQ", 6))
	{
		G_DeferedInitNew(primaryLevel->NextSecretMap);
	}
	else
	{
		Printf("no next secret map!\n");
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

CCMD(currentpos)
{
	AActor *mo = players[consoleplayer].mo;
	if(mo)
	{
		Printf("Current player position: (%1.3f,%1.3f,%1.3f), angle: %1.3f, floorheight: %1.3f, sector:%d, sector lightlevel: %d, actor lightlevel: %d\n",
			mo->X(), mo->Y(), mo->Z(), mo->Angles.Yaw.Normalized360().Degrees(), mo->floorz, mo->Sector->sectornum, mo->Sector->lightlevel, mo->LightLevel);
	}
	else
	{
		Printf("You are not in game!\n");
	}
}

//-----------------------------------------------------------------------------
//
// Print secret info (submitted by Karl Murks)
//
//-----------------------------------------------------------------------------

static void PrintSecretString(const char *string, bool thislevel)
{
	const char *colstr = thislevel? TEXTCOLOR_YELLOW : TEXTCOLOR_CYAN;
	if (string != NULL)
	{
		if (*string == '$')
		{
			if (string[1] == 'S' || string[1] == 's')
			{
				auto secnum = (unsigned)strtoull(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				if (thislevel && secnum < primaryLevel->sectors.Size())
				{
					if (primaryLevel->sectors[secnum].isSecret()) colstr = TEXTCOLOR_RED;
					else if (primaryLevel->sectors[secnum].wasSecret()) colstr = TEXTCOLOR_GREEN;
					else colstr = TEXTCOLOR_ORANGE;
				}
			}
			else if (string[1] == 'T' || string[1] == 't')
			{
				long tid = (long)strtoll(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				auto it = primaryLevel->GetActorIterator(tid);
				AActor *actor;
				bool foundone = false;
				if (thislevel)
				{
					while ((actor = it.Next()))
					{
						if (!actor->IsKindOf("SecretTrigger")) continue;
						foundone = true;
						break;
					}
				}
				if (foundone) colstr = TEXTCOLOR_RED;
				else colstr = TEXTCOLOR_GREEN;
			}
		}
		auto brok = V_BreakLines(CurrentConsoleFont, twod->GetWidth()*95/100, *string == '$' ? GStrings(++string) : string);

		for (auto &line : brok)
		{
			Printf("%s%s\n", colstr, line.Text.GetChars());
		}
	}
}

//============================================================================
//
// Print secret hints
//
//============================================================================

CCMD(secret)
{
	const char *mapname = argv.argc() < 2? primaryLevel->MapName.GetChars() : argv[1];
	bool thislevel = !stricmp(mapname, primaryLevel->MapName);
	bool foundsome = false;

	int lumpno=fileSystem.CheckNumForName("SECRETS");
	if (lumpno < 0) return;

	auto lump = fileSystem.OpenFileReader(lumpno);
	FString maphdr;
	maphdr.Format("[%s]", mapname);

	FString linebuild;
	char readbuffer[10240];
	bool inlevel = false;

	while (lump.Gets(readbuffer, 10240))
	{
		if (!inlevel)
		{
			if (readbuffer[0] == '[')
			{
				inlevel = !strnicmp(readbuffer, maphdr, maphdr.Len());
				if (!foundsome)
				{
					FString levelname;
					level_info_t *info = FindLevelInfo(mapname);
					const char *ln = !(info->flags & LEVEL_LOOKUPLEVELNAME)? info->LevelName.GetChars() : GStrings[info->LevelName.GetChars()];
					levelname.Format("%s - %s", mapname, ln);
					Printf(TEXTCOLOR_YELLOW "%s\n", levelname.GetChars());
					size_t llen = levelname.Len();
					levelname = "";
					for(size_t ii=0; ii<llen; ii++) levelname += '-';
					Printf(TEXTCOLOR_YELLOW "%s\n", levelname.GetChars());
					foundsome = true;
				}
			}
			continue;
		}
		else
		{
			if (readbuffer[0] != '[')
			{
				linebuild += readbuffer;
				if (linebuild.Len() < 1023 || linebuild[1022] == '\n')
				{
					// line complete so print it.
					linebuild.Substitute("\r", "");
					linebuild.StripRight(" \t\n");
					PrintSecretString(linebuild, thislevel);
					linebuild = "";
				}
			}
			else inlevel = false;
		}
	}
}

CCMD(angleconvtest)
{
	Printf("Testing degrees to angle conversion:\n");
	for (double ang = -5 * 180.; ang < 5 * 180.; ang += 45.)
	{
		unsigned ang1 = DAngle::fromDeg(ang).BAMs();
		unsigned ang2 = (unsigned)(ang * (0x40000000 / 90.));
		unsigned ang3 = (unsigned)(int)(ang * (0x40000000 / 90.));
		Printf("Angle = %.5f: xs_RoundToInt = %08x, unsigned cast = %08x, signed cast = %08x\n",
			ang, ang1, ang2, ang3);
	}
}

extern uint32_t r_renderercaps;
#define PRINT_CAP(X, Y) Printf("  %-18s: %s (%s)\n", #Y, !!(r_renderercaps & Y) ? "Yes" : "No ", X);
CCMD(r_showcaps)
{
	Printf("Renderer capabilities:\n");
	PRINT_CAP("Flat Sprites", RFF_FLATSPRITES)
	PRINT_CAP("3D Models", RFF_MODELS)
	PRINT_CAP("Sloped 3D floors", RFF_SLOPE3DFLOORS)
	PRINT_CAP("Full Freelook", RFF_TILTPITCH)	
	PRINT_CAP("Roll Sprites", RFF_ROLLSPRITES)
	PRINT_CAP("Unclipped Sprites", RFF_UNCLIPPEDTEX)
	PRINT_CAP("Material Shaders", RFF_MATSHADER)
	PRINT_CAP("Post-processing Shaders", RFF_POSTSHADER)
	PRINT_CAP("Brightmaps", RFF_BRIGHTMAP)
	PRINT_CAP("Custom COLORMAP lumps", RFF_COLORMAP)
	PRINT_CAP("Uses Polygon rendering", RFF_POLYGONAL)
	PRINT_CAP("Truecolor Enabled", RFF_TRUECOLOR)
	PRINT_CAP("Voxels", RFF_VOXELS)
}


//==========================================================================
//
// CCMD idmus
//
//==========================================================================

CCMD(idmus)
{
	level_info_t* info;
	FString map;
	int l;

	if (MusicEnabled())
	{
		if (argv.argc() > 1)
		{
			if (gameinfo.flags & GI_MAPxx)
			{
				l = atoi(argv[1]);
				if (l <= 99)
				{
					map = CalcMapName(0, l);
				}
				else
				{
					Printf("%s\n", GStrings("STSTR_NOMUS"));
					return;
				}
			}
			else
			{
				map = CalcMapName(argv[1][0] - '0', argv[1][1] - '0');
			}

			if ((info = FindLevelInfo(map)))
			{
				if (info->Music.IsNotEmpty())
				{
					S_ChangeMusic(info->Music, info->musicorder);
					Printf("%s\n", GStrings("STSTR_MUS"));
				}
			}
			else
			{
				Printf("%s\n", GStrings("STSTR_NOMUS"));
			}
		}
	}
	else
	{
		Printf("Music is disabled\n");
	}
}


//==========================================================================
//
//
//
//==========================================================================

CCMD(dumpactors)
{
	const char* const filters[32] =
	{
		"0:All", "1:Doom", "2:Heretic", "3:DoomHeretic", "4:Hexen", "5:DoomHexen", "6:Raven", "7:IdRaven",
		"8:Strife", "9:DoomStrife", "10:HereticStrife", "11:DoomHereticStrife", "12:HexenStrife",
		"13:DoomHexenStrife", "14:RavenStrife", "15:NotChex", "16:Chex", "17:DoomChex", "18:HereticChex",
		"19:DoomHereticChex", "20:HexenChex", "21:DoomHexenChex", "22:RavenChex", "23:NotStrife", "24:StrifeChex",
		"25:DoomStrifeChex", "26:HereticStrifeChex", "27:NotHexen",	"28:HexenStrifeChex", "29:NotHeretic",
		"30:NotDoom", "31:All",
	};
	Printf("%u object class types total\nActor\tEd Num\tSpawnID\tFilter\tSource\n", PClass::AllClasses.Size());
	for (unsigned int i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass* cls = PClass::AllClasses[i];
		PClassActor* acls = ValidateActor(cls);
		if (acls != NULL)
		{
			auto ainfo = acls->ActorInfo();
			Printf("%s\t%i\t%i\t%s\t%s\n",
				acls->TypeName.GetChars(), ainfo->DoomEdNum,
				ainfo->SpawnID, filters[ainfo->GameFilter & 31],
				acls->SourceLumpName.GetChars());
		}
		else if (cls != NULL)
		{
			Printf("%s\tn/a\tn/a\tn/a\tEngine (not an actor type)\tSource: %s\n", cls->TypeName.GetChars(), cls->SourceLumpName.GetChars());
		}
		else
		{
			Printf("Type %i is not an object class\n", i);
		}
	}
}
