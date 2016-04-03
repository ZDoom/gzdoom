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
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "version.h"
#include "c_console.h"
#include "c_dispatch.h"

#include "i_system.h"

#include "doomerrors.h"
#include "doomstat.h"
#include "gstrings.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_level.h"
#include "w_wad.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "d_player.h"
#include "templates.h"
#include "p_local.h"
#include "r_sky.h"
#include "p_setup.h"
#include "cmdlib.h"
#include "d_net.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "v_video.h"
#include "r_utility.h"
#include "r_data/r_interpolate.h"

extern FILE *Logfile;
extern bool insave;

CVAR (Bool, sv_cheats, false, CVAR_SERVERINFO | CVAR_LATCH)
CVAR (Bool, sv_unlimited_pickup, false, CVAR_SERVERINFO)

CCMD (toggleconsole)
{
	C_ToggleConsole();
}

bool CheckCheatmode (bool printmsg)
{
	if ((G_SkillProperty(SKILLP_DisableCheats) || netgame || deathmatch) && (!sv_cheats))
	{
		if (printmsg) Printf ("sv_cheats must be true to enable this command.\n");
		return true;
	}
	else
	{
		return false;
	}
}

CCMD (quit)
{
	if (!insave) exit (0);
}

CCMD (exit)
{
	if (!insave) exit (0);
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
	if (who == NULL || !usergame)
	{
		Printf ("Use the map command when not in a game.\n");
		return;
	}

	if (!players[who->player - players].settings_controller && netgame)
	{
		Printf ("Only setting controllers can change the map.\n");
		return;
	}

	if (argv.argc() > 1)
	{
		try
		{
			if (!P_CheckMapData(argv[1]))
			{
				Printf ("No map %s\n", argv[1]);
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
				Net_WriteString (argv[1]);
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
		Net_WriteWord (clamp (atoi (argv[2]), 1, 32767));
	else
		Net_WriteWord (0);
}

CCMD (take)
{
	if (CheckCheatmode () || argv.argc() < 2)
		return;

	Net_WriteByte (DEM_TAKECHEAT);
	Net_WriteString (argv[1]);
	if (argv.argc() > 2)
		Net_WriteWord (clamp (atoi (argv[2]), 1, 32767));
	else
		Net_WriteWord (0);
}

CCMD (gameversion)
{
	Printf ("%s @ %s\nCommit %s\n", GetVersionString(), GetGitTime(), GetGitHash());
}

CCMD (print)
{
	if (argv.argc() != 2)
	{
		Printf ("print <name>: Print a string from the string table\n");
		return;
	}
	const char *str = GStrings[argv[1]];
	if (str == NULL)
	{
		Printf ("%s unknown\n", argv[1]);
	}
	else
	{
		Printf ("%s\n", str);
	}
}

CCMD (exec)
{
	if (argv.argc() < 2)
		return;

	for (int i = 1; i < argv.argc(); ++i)
	{
		if (!C_ExecFile(argv[i]))
		{
			Printf ("Could not exec \"%s\"\n", argv[i]);
			break;
		}
	}
}

void execLogfile(const char *fn, bool append)
{
	if ((Logfile = fopen(fn, append? "a" : "w")))
	{
		const char *timestr = myasctime();
		Printf("Log started: %s\n", timestr);
	}
	else
	{
		Printf("Could not start log\n");
	}
}

CCMD (logfile)
{

	if (Logfile)
	{
		const char *timestr = myasctime();
		Printf("Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argv.argc() >= 2)
	{
		execLogfile(argv[1], argv.argc() >=3? !!argv[2]:false);
	}
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
		int argn = MIN<int>(argc - 2, countof(arg)), i;

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
			argn = MIN<int>(argc - argstart, countof(arg));
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

CCMD (error)
{
	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_Error ("%s", textcopy);
	}
	else
	{
		Printf ("Usage: error <error text>\n");
	}
}

CCMD (error_fatal)
{
	if (argv.argc() > 1)
	{
		char *textcopy = copystring (argv[1]);
		I_FatalError ("%s", textcopy);
	}
	else
	{
		Printf ("Usage: error_fatal <error text>\n");
	}
}

//==========================================================================
//
// CCMD crashout
//
// Debugging routine for testing the crash logger.
// Useless in a win32 debug build, because that doesn't enable the crash logger.
//
//==========================================================================

#if !defined(_WIN32) || !defined(_DEBUG)
CCMD (crashout)
{
	*(volatile int *)0 = 0;
}
#endif


CCMD (dir)
{
	FString dir, path;
	char curdir[256];
	const char *match;
	findstate_t c_file;
	void *file;

	if (!getcwd (curdir, countof(curdir)))
	{
		Printf ("Current path too long\n");
		return;
	}

	if (argv.argc() > 1)
	{
		path = NicePath(argv[1]);
		if (chdir(path))
		{
			match = path;
			dir = ExtractFilePath(path);
			if (dir[0] != '\0')
			{
				match += dir.Len();
			}
			else
			{
				dir = "./";
			}
			if (match[0] == '\0')
			{
				match = "*";
			}
			if (chdir (dir))
			{
				Printf ("%s not found\n", dir.GetChars());
				return;
			}
		}
		else
		{
			match = "*";
			dir = path;
		}
	}
	else
	{
		match = "*";
		dir = curdir;
	}
	if (dir[dir.Len()-1] != '/')
	{
		dir += '/';
	}

	if ( (file = I_FindFirst (match, &c_file)) == ((void *)(-1)))
		Printf ("Nothing matching %s%s\n", dir.GetChars(), match);
	else
	{
		Printf ("Listing of %s%s:\n", dir.GetChars(), match);
		do
		{
			if (I_FindAttr (&c_file) & FA_DIREC)
				Printf (PRINT_BOLD, "%s <dir>\n", I_FindName (&c_file));
			else
				Printf ("%s\n", I_FindName (&c_file));
		} while (I_FindNext (file, &c_file) == 0);
		I_FindClose (file);
	}

	chdir (curdir);
}

CCMD (fov)
{
	player_t *player = who ? who->player : &players[consoleplayer];

	if (argv.argc() != 2)
	{
		Printf ("fov is %g\n", player->DesiredFOV);
		return;
	}
	else if (dmflags & DF_NO_FOV)
	{
		if (consoleplayer == Net_Arbitrator)
		{
			Net_WriteByte (DEM_FOV);
		}
		else
		{
			Printf ("A setting controller has disabled FOV changes.\n");
			return;
		}
	}
	else
	{
		Net_WriteByte (DEM_MYFOV);
	}
	Net_WriteByte (clamp (atoi (argv[1]), 5, 179));
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

CCMD (load)
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
	DefaultExtension (fname, ".zds");
    G_LoadGame (fname);
}

//==========================================================================
//
// CCMD save
//
// Save the current game.
//
//==========================================================================

CCMD (save)
{
    if (argv.argc() < 2 || argv.argc() > 3)
	{
        Printf ("usage: save <filename> [description]\n");
        return;
    }
    FString fname = argv[1];
	DefaultExtension (fname, ".zds");
	G_SaveGame (fname, argv.argc() > 2 ? argv[2] : argv[1]);
}

//==========================================================================
//
// CCMD wdir
//
// Lists the contents of a loaded wad file.
//
//==========================================================================

CCMD (wdir)
{
	if (argv.argc() != 2)
	{
		Printf ("usage: wdir <wadfile>\n");
		return;
	}
	int wadnum = Wads.CheckIfWadLoaded (argv[1]);
	if (wadnum < 0)
	{
		Printf ("%s must be loaded to view its directory.\n", argv[1]);
		return;
	}
	for (int i = 0; i < Wads.GetNumLumps(); ++i)
	{
		if (Wads.GetLumpFile(i) == wadnum)
		{
			Printf ("%s\n", Wads.GetLumpFullName(i));
		}
	}
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
	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->Angles.Yaw, MISSILERANGE, &t, 0.);
	if (t.linetarget)
	{
		Printf("Target=%s, Health=%d, Spawnhealth=%d\n",
			t.linetarget->GetClass()->TypeName.GetChars(),
			t.linetarget->health,
			t.linetarget->SpawnHealth());
	}
	else Printf("No target found\n");
}

// As linetarget, but also give info about non-shootable actors
CCMD(info)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL) return;
	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->Angles.Yaw, MISSILERANGE,
		&t, 0.,	ALF_CHECKNONSHOOTABLE|ALF_FORCENOSMART);
	if (t.linetarget)
	{
		Printf("Target=%s, Health=%d, Spawnhealth=%d\n",
			t.linetarget->GetClass()->TypeName.GetChars(),
			t.linetarget->health,
			t.linetarget->SpawnHealth());
		PrintMiscActorInfo(t.linetarget);
	}
	else Printf("No target found. Info cannot find actors that have "
				"the NOBLOCKMAP flag or have height/radius of 0.\n");
}

typedef bool (*ActorTypeChecker) (AActor *);

static bool IsActorAMonster(AActor *mo)
{
	return mo->flags3&MF3_ISMONSTER && !(mo->flags&MF_CORPSE) && !(mo->flags&MF_FRIENDLY);
}

static bool IsActorAnItem(AActor *mo)
{
	return mo->IsKindOf(RUNTIME_CLASS(AInventory)) && mo->flags&MF_SPECIAL;
}

static bool IsActorACountItem(AActor *mo)
{
	return mo->IsKindOf(RUNTIME_CLASS(AInventory)) && mo->flags&MF_SPECIAL && mo->flags&MF_COUNTITEM;
}

static void PrintFilteredActorList(const ActorTypeChecker IsActorType, const char *FilterName)
{
	AActor *mo;
	const PClass *FilterClass = NULL;

	if (FilterName != NULL)
	{
		FilterClass = PClass::FindActor(FilterName);
		if (FilterClass == NULL)
		{
			Printf("%s is not an actor class.\n", FilterName);
			return;
		}
	}
	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		if ((FilterClass == NULL || mo->IsA(FilterClass)) && IsActorType(mo))
		{
			Printf ("%s at (%f,%f,%f)\n",
				mo->GetClass()->TypeName.GetChars(), mo->X(), mo->Y(), mo->Z());
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(monster)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAMonster, argv.argc() > 1 ? argv[1] : NULL);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(items)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorAnItem, argv.argc() > 1 ? argv[1] : NULL);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(countitems)
{
	if (CheckCheatmode ()) return;

	PrintFilteredActorList(IsActorACountItem, argv.argc() > 1 ? argv[1] : NULL);
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

	sky1name = argv[1];
	if (sky1name[0] != 0)
	{
		FTextureID newsky = TexMan.GetTexture(sky1name, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
		if (newsky.Exists())
		{
			sky1texture = level.skytexture1 = newsky;
		}
		else
		{
			Printf("changesky: Texture '%s' not found\n", sky1name);
		}
	}
	R_InitSkyMap ();
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
	
	if (level.NextMap.Len() > 0 && level.NextMap.Compare("enDSeQ", 6))
	{
		G_DeferedInitNew(level.NextMap);
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

	if (level.NextSecretMap.Len() > 0 && level.NextSecretMap.Compare("enDSeQ", 6))
	{
		G_DeferedInitNew(level.NextSecretMap);
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
		Printf("Current player position: (%1.3f,%1.3f,%1.3f), angle: %1.3f, floorheight: %1.3f, sector:%d, lightlevel: %d\n",
			mo->X(), mo->Y(), mo->Z(), mo->Angles.Yaw.Normalized360().Degrees, mo->floorz, mo->Sector->sectornum, mo->Sector->lightlevel);
	}
	else
	{
		Printf("You are not in game!");
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(vmengine)
{
	if (argv.argc() == 2)
	{
		if (stricmp(argv[1], "default") == 0)
		{
			VMSelectEngine(VMEngine_Default);
			return;
		}
		else if (stricmp(argv[1], "checked") == 0)
		{
			VMSelectEngine(VMEngine_Checked);
			return;
		}
		else if (stricmp(argv[1], "unchecked") == 0)
		{
			VMSelectEngine(VMEngine_Unchecked);
			return;
		}
	}
	Printf("Usage: vmengine <default|checked|unchecked>\n");
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
				long secnum = strtol(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				if (thislevel && secnum >= 0 && secnum < numsectors)
				{
					if (sectors[secnum].isSecret()) colstr = TEXTCOLOR_RED;
					else if (sectors[secnum].wasSecret()) colstr = TEXTCOLOR_GREEN;
					else colstr = TEXTCOLOR_ORANGE;
				}
			}
			else if (string[1] == 'T' || string[1] == 't')
			{
				long tid = strtol(string+2, (char**)&string, 10);
				if (*string == ';') string++;
				FActorIterator it(tid);
				AActor *actor;
				bool foundone = false;
				if (thislevel)
				{
					while ((actor = it.Next()))
					{
						if (!actor->IsKindOf(PClass::FindClass("SecretTrigger"))) continue;
						foundone = true;
						break;
					}
				}
				if (foundone) colstr = TEXTCOLOR_RED;
				else colstr = TEXTCOLOR_GREEN;
			}
		}
		FBrokenLines *brok = V_BreakLines(ConFont, screen->GetWidth()*95/100, string);

		for (int k = 0; brok[k].Width >= 0; k++)
		{
			Printf("%s%s\n", colstr, brok[k].Text.GetChars());
		}
		V_FreeBrokenLines(brok);
	}
}

//============================================================================
//
// Print secret hints
//
//============================================================================

CCMD(secret)
{
	const char *mapname = argv.argc() < 2? level.MapName.GetChars() : argv[1];
	bool thislevel = !stricmp(mapname, level.MapName);
	bool foundsome = false;

	int lumpno=Wads.CheckNumForName("SECRETS");
	if (lumpno < 0) return;

	FWadLump lump = Wads.OpenLumpNum(lumpno);
	FString maphdr;
	maphdr.Format("[%s]", mapname);

	FString linebuild;
	char readbuffer[1024];
	bool inlevel = false;

	while (lump.Gets(readbuffer, 1024))
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
					levelname.Format("%s - %s\n", mapname, ln);
					size_t llen = levelname.Len() - 1;
					for(size_t ii=0; ii<llen; ii++) levelname += '-';
					Printf(TEXTCOLOR_YELLOW"%s\n", levelname.GetChars());
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
		unsigned ang1 = DAngle(ang).BAMs();
		unsigned ang2 = (unsigned)(ang * (0x40000000 / 90.));
		unsigned ang3 = (unsigned)(int)(ang * (0x40000000 / 90.));
		Printf("Angle = %.5f: xs_RoundToInt = %08x, unsigned cast = %08x, signed cast = %08x\n",
			ang, ang1, ang2, ang3);
	}
}
