#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "c_consol.h"
#include "c_cmds.h"
#include "c_dispch.h"

#include "i_system.h"

#include "doomstat.h"
#include "d_englsh.h"
#include "sounds.h"
#include "s_sound.h"
#include "g_game.h"
#include "d_items.h"
#include "p_inter.h"
#include "z_zone.h"
#include "w_wad.h"

extern FILE *Logfile;

cvar_t *sv_cheats;

struct CmdDispatcher CmdList[] = {
	{ "spynext",				Cmd_SpyNext },
	{ "spyprev",				Cmd_SpyPrev },
	{ "messagemode",			Cmd_MessageMode },
	{ "say",					Cmd_Say },
	{ "limits",					Cmd_Limits },
	{ "screenshot",				Cmd_Screenshot },
	{ "vid_setmode",			Cmd_Vid_SetMode },
	{ "togglemap",				Cmd_Togglemap },
	{ "echo",					Cmd_Echo },
	{ "clear",					Cmd_Clear },
	{ "toggleconsole",			C_ToggleConsole },
	{ "centerview",				Cmd_CenterView },
	{ "pause",					Cmd_Pause },
	{ "setcolor",				Cmd_SetColor },
	{ "kill",					Cmd_Kill },
	{ "sizedown",				Cmd_Sizedown },
	{ "sizeup",					Cmd_Sizeup },
	{ "impulse",				Cmd_Impulse },
	{ "alias",					Cmd_Alias },
	{ "cmdlist",				Cmd_Cmdlist },
	{ "unbind",					Cmd_Unbind },
	{ "unbindall",				Cmd_Unbindall },
	{ "bind",					Cmd_Bind },
	{ "binddefaults",			Cmd_BindDefaults },
	{ "dumpheap",				Cmd_DumpHeap },
	{ "exec",					Cmd_Exec },
	{ "gameversion",			Cmd_Gameversion },
	{ "get",					Cmd_Get },
	{ "toggle",					Cmd_Toggle },
	{ "cvarlist",				Cmd_CvarList },
	{ "give",					Cmd_Give },
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
	{ "quit",					I_Quit },
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
	{ NULL }
};

void C_InstallCommands (void)
{
	C_RegisterCommands (CmdList);
}

BOOL CheckCheatmode (void)
{
	if (((gameskill->value == sk_nightmare) || netgame) && (sv_cheats->value == 0.0)) {
		Printf ("You must run the server with '+set cheats 1' to enable this command.\n");
		return true;
	} else {
		return false;
	}
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

void Cmd_idclev (player_t *plyr, int argc, char **argv)
{
	if (CheckCheatmode ())
		return;

	if ((argc > 1) && (*(argv[1] + 2) == 0) && *(argv[1] + 1) && *argv[1]) {
		int epsd, map;
		char *buf = argv[1];

		buf[0] -= '0';
		buf[1] -= '0';

		if (gamemode == commercial) {
			epsd = 1;
			map = buf[0]*10 + buf[1];
		} else {
			epsd = buf[0];
			map = buf[1];
		}

		// Catch invalid maps.
		if (epsd < 1)
			return;

		if (map < 1)
			return;
  
		if ((gamemode == retail) && ((epsd > 4) || (map > 9)))
			return;

		if ((gamemode == registered) && ((epsd > 3) || (map > 9)))
			return;

		if ((gamemode == shareware) && ((epsd > 1) || (map > 9)))
			return;

		if ((gamemode == commercial) && (( epsd > 1) || (map > 34)))
			return;

		// So be it.
		Printf ("%s\n", STSTR_CLEV);
      
		G_DeferedInitNew (CalcMapName (epsd, map));
	}
}

void Cmd_ChangeMap (player_t *plyr, int argc, char **argv)
{
	if (plyr != players && netgame) {
		Printf ("Only player 1 can change the map.\n");
		return;
	}

	if (argc > 1) {
		if (W_CheckNumForName (argv[1]) == -1) {
			Printf ("No map %s\n", argv[1]);
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

	plyr->message = STSTR_NOMUS;
	if (argc > 1) {
		if (gamemode == commercial) {
			l = atoi (argv[1]);
			if (l <= 99)
				map = CalcMapName (0, l);
			else {
				return;
			}
		} else {
			map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
		}

		if ( (info = FindLevelInfo (map)) ) {
			if (info->music[0]) {
				S_ChangeMusic (info->music, 1);
				plyr->message = STSTR_MUS;
			}
		}
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
	Printf ("%d.%d : " __DATE__ "\n", VERSION / 100, VERSION % 100);
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
			Printf ("Error parsing \"%s\"\n", argv[1]);

		fclose (f);
	} else
		Printf ("Could not open \"%s\"\n", argv[1]);
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
		Printf ("Log stopped: %s\n", timestr);
		fclose (Logfile);
		Logfile = NULL;
	}

	if (argc >= 2) {
		if ( (Logfile = fopen (argv[1], "w")) ) {
			Printf ("Log started: %s\n", timestr);
		} else {
			Printf ("Could not start log\n");
		}
	}
}

void Cmd_Limits (player_t *plyr, int argc, char **argv)
{
	extern int MaxDeathmatchStarts;
	extern int MaxPlats;
	extern int MaxCeilings;
	extern int MaxSpecialCross;
	extern int MaxDrawSegs;
	extern int MaxSegs;
	extern int MaxVisPlanes;
	extern int MaxVisSprites;
	extern int maxopenings;

	Printf_Bold ("Note that the following values are\n"
				 "dynamic and will increase as needed.\n\n");
	Printf ("MaxCeilings: %u\n", MaxCeilings);
	Printf ("MaxDeathmatchStarts: %u\n", MaxDeathmatchStarts);
	Printf ("MaxDrawSegs: %u\n", MaxDrawSegs);
	Printf ("MaxPlats: %u\n", MaxPlats);
	Printf ("MaxSegs: %u\n", MaxSegs);
	Printf ("MaxSpecialCross: %u\n", MaxSpecialCross);
	Printf ("MaxVisPlanes: %u\n", MaxVisPlanes);
	Printf ("MaxVisSprites: %u\n", MaxVisSprites);
}