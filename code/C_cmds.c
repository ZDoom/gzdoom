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
#include "c_consol.h"
#include "c_cmds.h"
#include "c_dispch.h"

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

extern FILE *Logfile;

cvar_t *sv_cheats;

static void cmd_toggleconsole (player_t *plyr, int argc, char **argv)
{
	C_ToggleConsole();
}

struct CmdDispatcher CmdList[] = {
	{ "playdemo",				Cmd_PlayDemo },
	{ "timedemo",				Cmd_TimeDemo },
	{ "error",					Cmd_Error },
	{ "endgame",				Cmd_Endgame },
	{ "mem",					Cmd_Mem },
	{ "pings",					Cmd_Pings },
	{ "skins",					Cmd_Skins },
	{ "turn180",				Cmd_Turn180 },
	{ "puke",					Cmd_Puke },
	{ "spynext",				Cmd_SpyNext },
	{ "spyprev",				Cmd_SpyPrev },
	{ "messagemode",			Cmd_MessageMode },
	{ "say",					Cmd_Say },
	{ "messagemode2",			Cmd_MessageMode2 },
	{ "say_team",				Cmd_Say_Team },
	{ "limits",					Cmd_Limits },
	{ "screenshot",				Cmd_Screenshot },
	{ "vid_setmode",			Cmd_Vid_SetMode },
	{ "togglemap",				Cmd_Togglemap },
	{ "echo",					Cmd_Echo },
	{ "clear",					Cmd_Clear },
	{ "toggleconsole",			cmd_toggleconsole },
	{ "centerview",				Cmd_CenterView },
	{ "pause",					Cmd_Pause },
	{ "setcolor",				Cmd_SetColor },
	{ "kill",					Cmd_Kill },
	{ "sizedown",				Cmd_Sizedown },
	{ "sizeup",					Cmd_Sizeup },
	{ "impulse",				Cmd_Impulse },
	{ "weapnext",				Cmd_WeapNext },
	{ "weapprev",				Cmd_WeapPrev },
	{ "alias",					Cmd_Alias },
	{ "cmdlist",				Cmd_Cmdlist },
	{ "unbind",					Cmd_Unbind },
	{ "unbindall",				Cmd_Unbindall },
	{ "undoublebind",			Cmd_UnDoubleBind },
	{ "doublebind",				Cmd_DoubleBind },
	{ "bind",					Cmd_Bind },
	{ "binddefaults",			Cmd_BindDefaults },
	{ "dumpheap",				Cmd_DumpHeap },
	{ "exec",					Cmd_Exec },
	{ "gameversion",			Cmd_Gameversion },
	{ "get",					Cmd_Get },
	{ "toggle",					Cmd_Toggle },
	{ "cvarlist",				Cmd_CvarList },
	{ "give",					Cmd_Give },
	{ "chase",					Cmd_Chase },
	{ "god",					Cmd_God },
	{ "history",				Cmd_History },
	{ "idclev",					Cmd_idclev },
	{ "changemap",				Cmd_ChangeMap },
	{ "map",					Cmd_Map },
	{ "idclip",					Cmd_Noclip },
	{ "iddqd",					Cmd_God },
	{ "changemus",				Cmd_ChangeMus },
	{ "idmus",					Cmd_idmus },
	{ "idspispopd",				Cmd_Noclip },
	{ "key",					Cmd_Key },
	{ "logfile",				Cmd_Logfile },
	{ "noclip",					Cmd_Noclip },
	{ "notarget",				Cmd_Notarget },
	{ "quit",					Cmd_Quit },
	{ "set",					Cmd_Set },
	{ "menu_main",				Cmd_Menu_Main },
	{ "menu_load",				Cmd_Menu_Load },
	{ "menu_save",				Cmd_Menu_Save },
	{ "menu_help",				Cmd_Menu_Help },
	{ "quicksave",				Cmd_Quicksave },
	{ "quickload",				Cmd_Quickload },
	{ "menu_endgame",			Cmd_Menu_Endgame },
	{ "menu_quit",				Cmd_Menu_Quit },
	{ "menu_game",				Cmd_Menu_Game },
	{ "menu_options",			Cmd_Menu_Options },
	{ "menu_display",			Cmd_Menu_Display },
	{ "menu_keys",				Cmd_Menu_Keys },
	{ "menu_player",			Cmd_Menu_Player },
	{ "menu_video",				Cmd_Menu_Video },
	{ "menu_gameplay",			Cmd_Menu_Gameplay },
	{ "bumpgamma",				Cmd_Bumpgamma },
	{ "togglemessages",			Cmd_ToggleMessages },
	{ "stop",					Cmd_Stop },
	{ "soundlist",				Cmd_Soundlist },
	{ "soundlinks",				Cmd_Soundlinks },
	{ "dir",					Cmd_Dir },
	{ NULL }
};

void C_InstallCommands (void)
{
	C_RegisterCommands (CmdList);
}

BOOL CheckCheatmode (void)
{
	if (((gameskill->value == sk_nightmare) || netgame) && (sv_cheats->value == 0.0)) {
		Printf (PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return true;
	} else {
		return false;
	}
}

void Cmd_Quit (player_t *plyr, int argc, char **argv)
{
	exit (0);
}

void Cmd_ChangeMus (player_t *plyr, int argc, char **argv)
{
	if (argc > 1) {
		S_ChangeMusic (argv[1], 1);
	}
}

/*
==================
Cmd_God

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God (player_t *plyr, int argc, char **argv)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	if (*argv[0] == 'i')
		Net_WriteByte (CHT_IDDQD);
	else
		Net_WriteByte (CHT_GOD);
}

void Cmd_Notarget (player_t *plyr, int argc, char **argv)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOTARGET);
}

/*
==================
Cmd_Noclip

argv(0) noclip
==================
*/
void Cmd_Noclip (player_t *plyr, int argc, char **argv)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_NOCLIP);
}

extern cvar_t *chasedemo;

void Cmd_Chase (player_t *plyr, int argc, char **argv)
{
	if (demoplayback) {
		int i;

		if (chasedemo->value) {
			SetCVarFloat (chasedemo, 0);
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats &= ~CF_CHASECAM;
		} else {
			SetCVarFloat (chasedemo, 1);
			for (i = 0; i < MAXPLAYERS; i++)
				players[i].cheats |= CF_CHASECAM;
		}
	} else {
		if (CheckCheatmode ())
			return;

		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_CHASECAM);
	}
}

void Cmd_idclev (player_t *plyr, int argc, char **argv)
{
	if (CheckCheatmode ())
		return;

	if ((argc > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1]) {
		int epsd, map;
		char *buf = argv[1];
		char *mapname;

		buf[0] -= '0';
		buf[1] -= '0';

		if (gameinfo.flags & GI_MAPxx) {
			epsd = 1;
			map = buf[0]*10 + buf[1];
		} else {
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

void Cmd_ChangeMap (player_t *plyr, int argc, char **argv)
{
	if (plyr != players && netgame) {
		Printf (PRINT_HIGH, "Only player 1 can change the map.\n");
		return;
	}

	if (argc > 1) {
		if (W_CheckNumForName (argv[1]) == -1) {
			Printf (PRINT_HIGH, "No map %s\n", argv[1]);
		} else {
			Net_WriteByte (DEM_CHANGEMAP);
			Net_WriteString (argv[1]);
		}
	}
}

void Cmd_idmus (player_t *plyr, int argc, char **argv)
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

void Cmd_Give (player_t *plyr, int argc, char **argv)
{
	char *name;

	if (CheckCheatmode ())
		return;

	if (argc < 2)
		return;

	if ( (name = BuildString (argc - 1, argv + 1)) ) {
		Net_WriteByte (DEM_GIVECHEAT);
		Net_WriteString (name);
		free (name);
	}
}

void Cmd_Gameversion (player_t *plyr, int argc, char **argv)
{
	Printf (PRINT_HIGH, "%d.%d : " __DATE__ "\n", VERSION / 100, VERSION % 100);
}

void Cmd_Exec (player_t *plyr, int argc, char **argv)
{
	FILE *f;
	char cmd[4096], *end, *comment;

	if (argc < 2)
		return;

	if ( (f = fopen (argv[1], "r")) ) {
		while (fgets (cmd, 256, f)) {
			// Comments begin with //
			comment = strstr (cmd, "//");
			if (comment)
				*comment = 0;

			end = cmd + strlen (cmd) - 1;
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

void Cmd_DumpHeap (player_t *plyr, int argc, char **argv)
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

void Cmd_Logfile (player_t *plyr, int argc, char **argv)
{
	time_t clock;
	char *timestr;

	time (&clock);
	timestr = asctime (localtime (&clock));

	if (Logfile) {
		Printf (PRINT_HIGH, "Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argc >= 2) {
		if ( (Logfile = fopen (argv[1], "w")) ) {
			Printf (PRINT_HIGH, "Log started: %s\n", timestr);
		} else {
			Printf (PRINT_HIGH, "Could not start log\n");
		}
	}
}

void Cmd_Limits (player_t *plyr, int argc, char **argv)
{
	extern int MaxDeathmatchStarts;
	extern int MaxSpecialCross;
	extern int MaxDrawSegs;
	extern int MaxSegs;
	extern int MaxVisSprites;
	extern int maxopenings;

	Printf (PRINT_HIGH, "Note that the following values are\n"
						"dynamic and will increase as needed.\n\n");
	Printf (PRINT_HIGH, "MaxDeathmatchStarts: %u\n", MaxDeathmatchStarts);
	Printf (PRINT_HIGH, "MaxDrawSegs: %u\n", MaxDrawSegs);
	Printf (PRINT_HIGH, "MaxSegs: %u\n", MaxSegs);
	Printf (PRINT_HIGH, "MaxSpecialCross: %u\n", MaxSpecialCross);
	Printf (PRINT_HIGH, "MaxVisSprites: %u\n", MaxVisSprites);
	Printf (PRINT_HIGH, "MaxOpeninings: %u\n", maxopenings);
}

BOOL P_StartScript (void *who, void *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always);
void Cmd_Puke (player_t *plyr, int argc, char **argv)
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
		P_StartScript (plyr->mo, NULL, script, level.mapname, 0, arg0, arg1, arg2, false);
	}
}

void Cmd_Error (player_t *plyr, int argc, char **argv)
{
	char *text = BuildString (argc - 1, argv + 1);
	char *textcopy = Z_Malloc (strlen (text) + 1, PU_LEVEL, 0);
	strcpy (textcopy, text);
	free (text);
	I_Error (textcopy);
}

void Cmd_Dir (player_t *plyr, int argc, char **argv)
{
	char dir[256], curdir[256];
	char *match;
	findstate_t c_file;
	long file;

	if (!getcwd (curdir, 256)) {
		Printf (PRINT_HIGH, "Current path too long\n");
		return;
	}

	if (argc == 1 || chdir (argv[1])) {
		match = argc == 1 ? "./*" : argv[1];

		ExtractFilePath (match, dir);
		if (dir[0]) {
			match += strlen (dir);
		} else {
			dir[0] = '.';
			dir[1] = '/';
			dir[2] = '\0';
		}
		if (!match[0])
			match = "*";

		if (chdir (dir)) {
			Printf (PRINT_HIGH, "%s not found\n", dir);
			return;
		}
	} else {
		match = "*";
		strcpy (dir, argv[1]);
		if (dir[strlen(dir) - 1] != '/')
			strcat (dir, "/");
	}

	if ( (file = I_FindFirst (match, &c_file)) == -1)
		Printf (PRINT_HIGH, "Nothing matching %s%s\n", dir, match);
	else {
		Printf (PRINT_HIGH, "Listing of %s%s:\n", dir, match);
		do {
			if (I_FindAttr (&c_file) & FA_DIREC)
				Printf_Bold ("%s <dir>\n", I_FindName (&c_file));
			else
				Printf (PRINT_HIGH, "%s\n", I_FindName (&c_file));
		} while (I_FindNext (file, &c_file) == 0);
		I_FindClose (file);
	}

	chdir (curdir);
}
