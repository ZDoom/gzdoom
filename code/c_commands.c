#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "c_console.h"
#include "c_commands.h"
#include "c_dispatch.h"

#include "i_system.h"

#include "doomstat.h"
#include "d_englsh.h"
#include "sounds.h"
#include "s_sound.h"
#include "g_game.h"
#include "d_items.h"
#include "p_inter.h"
#include "z_zone.h"

extern FILE *Logfile;

cvar_t *sv_cheats;

struct CmdDispatcher CmdList[] = {
	"echo",						Cmd_Echo,
	"clear",					Cmd_Clear,
	"toggleconsole",			C_ToggleConsole,
	"centerview",				Cmd_CenterView,
	"pause",					Cmd_Pause,
	"gamma",					Cmd_Gamma,
	"kill",						Cmd_Kill,
	"sizedown",					Cmd_Sizedown,
	"sizeup",					Cmd_Sizeup,
	"impulse",					Cmd_Impulse,
	"alias",					Cmd_Alias,
	"cmdlist",					Cmd_Cmdlist,
	"unbind",					Cmd_Unbind,
	"unbindall",				Cmd_Unbindall,
	"bind",						Cmd_Bind,
	"dumpheap",					Cmd_DumpHeap,
	"exec",						Cmd_Exec,
	"gameversion",				Cmd_Gameversion,
	"get",						Cmd_Get,
	"toggle",					Cmd_Toggle,
	"give",						Cmd_Give,
	"god",						Cmd_God,
	"history",					Cmd_History,
	"idclev",					Cmd_idclev,
	"idclip",					Cmd_Noclip,
	"iddqd",					Cmd_God,
	"idmus",					Cmd_idmus,
	"idmypos",					Cmd_idmypos,
	"idspispopd",				Cmd_Noclip,
	"key",						Cmd_Key,
	"logfile",					Cmd_Logfile,
	"noclip",					Cmd_Noclip,
	"notarget",					Cmd_Notarget,
	"quit",						I_Quit,
	"set",						Cmd_Set,
	"vid_describecurrentdriver",Cmd_Vid_DescribeCurrentDriver,
	"vid_describecurrentmode",	Cmd_Vid_DescribeCurrentMode,
	"vid_describedrivers",		Cmd_Vid_DescribeDrivers,
	"vid_describemodes",		Cmd_Vid_DescribeModes,
	"stop",						Cmd_Stop,
	NULL
};

void C_InstallCommands (void)
{
	C_RegisterCommands (CmdList);
}

boolean CheckCheatmode (void)
{
	if (netgame) {
		Printf ("Cheats are not currently supported in netgames\n");
		return true;
	}

	if (((gameskill->value == sk_nightmare) || netgame) && (sv_cheats->value == 0.0)) {
		Printf ("You must run the server with '+set cheats 1' to enable this command.\n");
		return true;
	} else {
		return false;
	}
}

void Cmd_idmus (player_t *plyr, int argc, char **argv)
{
	if (argc > 1) {
		int musnum;

		musnum = atoi (argv[1]);
		if (musnum) {
			if (gamemode == commercial) {
				if (musnum < 1 || musnum > 35)
					Printf (STSTR_NOMUS);
				else
					S_ChangeMusic (S_music[mus_runnin + musnum - 1].name, 1);
			} else {
				musnum = (argv[1][0]-'1')*9 + (argv[1][1]-'1') + argv[1][2] * 64;
			  
				if (musnum > 31)
					Printf (STSTR_NOMUS);
				else
					S_ChangeMusic (S_music[mus_e1m1 + musnum].name, 1);
			}
		} else {
			S_ChangeMusic (argv[1], 1);
		}
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
	char	*msg;

	if (CheckCheatmode ())
		return;

	plyr->cheats ^= CF_GODMODE;
	if (plyr->cheats & CF_GODMODE) {
		msg = STSTR_DQDON;

		if (*argv[0] == 'i') {
			// iddqd compatibility

			if (plyr->mo)
				plyr->mo->health = 100;
	  
			plyr->health = 100;
		}
	} else {
		msg = STSTR_DQDOFF;
	}

	Printf (msg);
}

void Cmd_Notarget (player_t *plyr, int argc, char **argv)
{
	char	*msg;

	if (CheckCheatmode ())
		return;

	plyr->cheats ^= CF_NOTARGET;
	if (plyr->cheats & CF_NOTARGET) {
		msg = "notarget ON\n";
	} else {
		msg = "notarget OFF\n";
	}

	Printf (msg);
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

	plyr->cheats ^= CF_NOCLIP;
	
	if (plyr->cheats & CF_NOCLIP)
		Printf (STSTR_NCON);
	else
		Printf (STSTR_NCOFF);
}

void Cmd_idmypos (player_t *plyr, int argc, char **argv)
{
	extern cvar_t *idmypos;

	if (idmypos->value)
		SetCVar (idmypos, "0");
	else
		SetCVar (idmypos, "1");
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
		Printf (STSTR_CLEV);
      
		G_DeferedInitNew(gameskill->value, epsd, map);
	}
}

void Cmd_Give (player_t *plyr, int argc, char **argv)
{
	char *name;
	boolean giveall;
	int i;
	gitem_t *it;

	if (CheckCheatmode ())
		return;

	if (argc < 2)
		return;

	name = BuildString (argc - 1, argv + 1);

	if (stricmp (name, "all") == 0)
		giveall = true;
	else
		giveall = false;

	if (giveall || stricmp (argv[1], "health") == 0) {
		int h;

		if ((argc == 3) && (h = atoi (argv[2]))) {
			if (plyr->mo) {
				plyr->mo->health += h;
	  			plyr->health = plyr->mo->health;
			} else {
				plyr->health += h;
			}
		} else {
			if (plyr->mo)
				plyr->mo->health = 100;
	  
			plyr->health = 100;
		}

		if (!giveall)
			goto leavegive;
	}

	if (giveall || stricmp (name, "backpack") == 0) {
		if (!plyr->backpack) {
			for (i=0 ; i<NUMAMMO ; i++)
			plyr->maxammo[i] *= 2;
			plyr->backpack = true;
		}
		for (i=0 ; i<NUMAMMO ; i++)
			P_GiveAmmo (plyr, i, 1);

		if (!giveall)
			goto leavegive;
	}

	if (giveall || stricmp (name, "weapons") == 0) {
		for (i = 0; i<NUMWEAPONS; i++)
			plyr->weaponowned[i] = true;

		if (!giveall)
			goto leavegive;
	}

	if (giveall || stricmp (name, "ammo") == 0) {
		for (i=0;i<NUMAMMO;i++)
			plyr->ammo[i] = plyr->maxammo[i];

		if (!giveall)
			goto leavegive;
	}

	if (giveall || stricmp (name, "armor") == 0) {
		plyr->armorpoints = 200;
		plyr->armortype = 2;

		if (!giveall)
			goto leavegive;
	}

	if (giveall || stricmp (name, "keys") == 0) {
		for (i=0;i<NUMCARDS;i++)
			plyr->cards[i] = true;

		if (!giveall)
			goto leavegive;
	}

	if (giveall)
		goto leavegive;

	it = FindItem (name);
	if (!it) {
		it = FindItem (argv[1]);
		if (!it) {
			it = FindItemByClassname (argv[1]);
			if (!it) {
				Printf ("Unknown item\n");
				goto leavegive;
			}
		}
	}

	if (it->flags & IT_AMMO) {
		int howmuch;

		if (argc == 3)
			howmuch = atoi (argv[2]);
		else
			howmuch = it->quantity;

		P_GiveAmmo (plyr, it->offset, howmuch);
	} else if (it->flags & IT_WEAPON) {
		P_GiveWeapon (plyr, it->offset, 0);
	} else if (it->flags & IT_KEY) {
		P_GiveCard (plyr, it->offset);
	} else if (it->flags & IT_POWER) {
		P_GivePower (plyr, it->offset);
	} else if (it->flags & IT_ARMOR) {
		P_GiveArmor (plyr, it->offset);
	}

leavegive:
	if (name)
		free (name);
}

void Cmd_Gameversion (player_t *plyr, int argc, char **argv)
{
	Printf ("%d.%d : " __DATE__ "\n", VERSION / 100, VERSION % 100);
}

void Cmd_Exec (player_t *plyr, int argc, char **argv)
{
	FILE *f;
	char cmd[256], *end;

	if (argc < 2)
		return;

	if (f = fopen (argv[1], "r")) {
		while (fgets (cmd, 256, f)) {
			// lines beginning with ';' are ignored
			if (*cmd != ';') {
				end = cmd + strlen (cmd) - 1;
				if (*end == '\n')
					*end = 0;
				AddCommandString (cmd);
			}
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
		if (Logfile = fopen (argv[1], "w")) {
			Printf ("Log started: %s\n", timestr);
		} else {
			Printf ("Could not start log\n");
		}
	}
}