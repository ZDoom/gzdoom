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

#include "doomstat.h"
#include "dstrings.h"
#include "s_sound.h"
#include "g_game.h"
#include "d_items.h"
#include "p_inter.h"
#include "z_zone.h"
#include "w_wad.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "d_player.h"
#include "r_main.h"

extern FILE *Logfile;

CVAR (sv_cheats, "0", CVAR_SERVERINFO | CVAR_LATCH)

BEGIN_COMMAND (toggleconsole)
{
	C_ToggleConsole();
}
END_COMMAND (toggleconsole)

BOOL CheckCheatmode (void)
{
	if (((gameskill.value == sk_nightmare) || netgame || deathmatch.value) && (sv_cheats.value == 0.0))
	{
		Printf (PRINT_HIGH, "You must run the server with '+set sv_cheats 1' to enable this command.\n");
		return true;
	}
	else
	{
		return false;
	}
}

BEGIN_COMMAND (quit)
{
	exit (0);
}
END_COMMAND (quit)

BEGIN_COMMAND (changemus)
{
	if (argc > 1)
	{
		S_ChangeMusic (argv[1], 1);
	}
}
END_COMMAND (changemus)

/*
==================
Cmd_God

Sets client to godmode

argv(0) god
==================
*/
BEGIN_COMMAND (god)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_GOD);
}
END_COMMAND (god)

BEGIN_COMMAND (iddqd)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_IDDQD);
}
END_COMMAND (iddqd)

BEGIN_COMMAND (notarget)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOTARGET);
}
END_COMMAND (notarget)

BEGIN_COMMAND (fly)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_FLY);
}
END_COMMAND (fly)

/*
==================
Cmd_Noclip

argv(0) noclip
==================
*/
BEGIN_COMMAND (noclip)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP);
}
END_COMMAND (noclip)

EXTERN_CVAR (chasedemo)

BEGIN_COMMAND (chase)
{
	if (demoplayback)
	{
		int i;

		if (chasedemo.value)
		{
			chasedemo.Set (0.0f);
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats &= ~CF_CHASECAM;
		}
		else
		{
			chasedemo.Set (1.0f);
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats |= CF_CHASECAM;
		}
	}
	else
	{
		if (deathmatch.value && CheckCheatmode ())
			return;

		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_CHASECAM);
	}
}
END_COMMAND (chase)

BEGIN_COMMAND (idclev)
{
	if (CheckCheatmode ())
		return;

	if ((argc > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1])
	{
		int epsd, map;
		char buf[2];
		char *mapname;

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
		if (W_CheckNumForName (mapname) == -1)
			return;

		// So be it.
		Printf (PRINT_HIGH, "%s\n", STSTR_CLEV);
      	G_DeferedInitNew (mapname);
	}
}
END_COMMAND (idclev)

BEGIN_COMMAND (changemap)
{
	if (m_Instigator->player - players != Net_Arbitrator && multiplayer)
	{
		Printf (PRINT_HIGH, "Only player %d can change the map.\n", Net_Arbitrator+1);
		return;
	}

	if (argc > 1)
	{
		if (W_CheckNumForName (argv[1]) == -1)
		{
			Printf (PRINT_HIGH, "No map %s\n", argv[1]);
		}
		else
		{
			Net_WriteByte (DEM_CHANGEMAP);
			Net_WriteString (argv[1]);
		}
	}
}
END_COMMAND (changemap)

BEGIN_COMMAND (idmus)
{
	level_info_t *info;
	char *map;
	int l;

	if (argc > 1)
	{
		if (gameinfo.flags & GI_MAPxx)
		{
			l = atoi (argv[1]);
			if (l <= 99)
				map = CalcMapName (0, l);
			else
			{
				Printf (PRINT_HIGH, "%s\n", STSTR_NOMUS);
				return;
			}
		}
		else
		{
			map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
		}

		if ( (info = FindLevelInfo (map)) )
		{
			if (info->music[0])
			{
				S_ChangeMusic (info->music, 1);
				Printf (PRINT_HIGH, "%s\n", STSTR_MUS);
			}
		} else
			Printf (PRINT_HIGH, "%s\n", STSTR_NOMUS);
	}
}
END_COMMAND (idmus)

BEGIN_COMMAND (give)
{
	char *name;

	if (CheckCheatmode ())
		return;

	if (argc < 2)
		return;

	if ( (name = BuildString (argc - 1, argv + 1)) )
	{
		Net_WriteByte (DEM_GIVECHEAT);
		Net_WriteString (name);
		delete[] name;
	}
}
END_COMMAND (give)

BEGIN_COMMAND (gameversion)
{
	Printf (PRINT_HIGH, "%d.%d : " __DATE__ "\n", VERSION / 100, VERSION % 100);
}
END_COMMAND (gameversion)

BEGIN_COMMAND (exec)
{
	FILE *f;
	char cmd[4096];

	if (argc < 2)
		return;

	if ( (f = fopen (argv[1], "rt")) )
	{
		while (fgets (cmd, 4095, f))
		{
			// Comments begin with //
			char *comment = strstr (cmd, "//");
			if (comment)
				*comment = 0;

			char *end = cmd + strlen (cmd) - 1;
			if (*end == '\n')
				*end = 0;
			AddCommandString (cmd);
		}
		if (!feof (f))
			Printf (PRINT_HIGH, "Error parsing \"%s\"\n", argv[1]);

		fclose (f);
	} else
		Printf (PRINT_HIGH, "Could not open \"%s\"\n", argv[1]);
}
END_COMMAND (exec)

BEGIN_COMMAND (dumpheap)
{
	int lo = PU_STATIC, hi = PU_CACHE;

	if (argc >= 2) {
		lo = atoi (argv[1]);
		if (argc >= 3) {
			hi = atoi (argv[2]);
		}
	}

	Z_DumpHeap (lo, hi);
}
END_COMMAND (dumpheap)

BEGIN_COMMAND (logfile)
{
	time_t clock;
	char *timestr;

	time (&clock);
	timestr = asctime (localtime (&clock));

	if (Logfile)
	{
		Printf (PRINT_HIGH, "Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argc >= 2)
	{
		if ( (Logfile = fopen (argv[1], "w")) )
		{
			Printf (PRINT_HIGH, "Log started: %s\n", timestr);
		}
		else
		{
			Printf (PRINT_HIGH, "Could not start log\n");
		}
	}
}
END_COMMAND (logfile)

BOOL P_StartScript (AActor *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always);

BEGIN_COMMAND (puke)
{
	if (argc < 2 || argc > 5) {
		Printf (PRINT_HIGH, " puke <script> [arg1] [arg2] [arg3]\n");
	} else {
		int script = atoi (argv[1]);
		int arg0=0, arg1=0, arg2=0;

		if (argc > 2) {
			arg0 = atoi (argv[2]);
			if (argc > 3) {
				arg1 = atoi (argv[3]);
				if (argc > 4) {
					arg2 = atoi (argv[4]);
				}
			}
		}
		P_StartScript (m_Instigator, NULL, script, level.mapname, 0, arg0, arg1, arg2, false);
	}
}
END_COMMAND (puke)

BEGIN_COMMAND (error)
{
	char *text = BuildString (argc - 1, argv + 1);
	char *textcopy = copystring (text);
	delete[] text;
	I_Error (textcopy);
}
END_COMMAND (error)

BEGIN_COMMAND (dir)
{
	char dir[256], curdir[256];
	char *match;
	findstate_t c_file;
	long file;

	if (!getcwd (curdir, 256))
	{
		Printf (PRINT_HIGH, "Current path too long\n");
		return;
	}

	if (argc == 1 || chdir (argv[1]))
	{
		match = argc == 1 ? (char *)"./*" : argv[1];

		ExtractFilePath (match, dir);
		if (dir[0])
		{
			match += strlen (dir);
		}
		else
		{
			dir[0] = '.';
			dir[1] = '/';
			dir[2] = '\0';
		}
		if (!match[0])
			match = "*";

		if (chdir (dir))
		{
			Printf (PRINT_HIGH, "%s not found\n", dir);
			return;
		}
	}
	else
	{
		match = "*";
		strcpy (dir, argv[1]);
		if (dir[strlen(dir) - 1] != '/')
			strcat (dir, "/");
	}

	if ( (file = I_FindFirst (match, &c_file)) == -1)
		Printf (PRINT_HIGH, "Nothing matching %s%s\n", dir, match);
	else
	{
		Printf (PRINT_HIGH, "Listing of %s%s:\n", dir, match);
		do
		{
			if (I_FindAttr (&c_file) & FA_DIREC)
				Printf_Bold ("%s <dir>\n", I_FindName (&c_file));
			else
				Printf (PRINT_HIGH, "%s\n", I_FindName (&c_file));
		} while (I_FindNext (file, &c_file) == 0);
		I_FindClose (file);
	}

	chdir (curdir);
}
END_COMMAND (dir)

BEGIN_COMMAND (fov)
{
	if (argc != 2)
		Printf (PRINT_HIGH, "fov is %g\n", m_Instigator->player->fov);
	else
		m_Instigator->player->fov = atof (argv[1]);
}
END_COMMAND (fov)
