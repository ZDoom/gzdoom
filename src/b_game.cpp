/*******************************************
* B_game.h                                 *
* Description:                             *
* Misc things that has to do with the bot, *
* like it's spawning etc.                  *
* Makes the bot fit into game              *
*                                          *
*******************************************/
/*The files which are modified for Cajun Purpose
D_player.h (v0.85: added some variables)
D_netcmd.c (v0.71)
D_netcmd.h (v0.71)
D_main.c   (v0.71)
D_ticcmd.h (v0.71)
G_game.c   (v0.95: Too make demorecording work somewhat)
G_input.c  (v0.95: added some keycommands)
G_input.h  (v0.95)
P_mobj.c   (v0.95: changed much in the P_MobjThinker(), a little in P_SpawnPlayerMissile(), maybee something else )
P_mobj.h   (v0.95: Removed some unnecessary variables)
P_user.c   (v0.95: It's only one change maybee it already was there in 0.71)
P_inter.c  (v0.95: lot of changes)
P_pspr.c   (v0.71)
P_map.c    (v0.95: Test missile for bots)
P_tick.c   (v0.95: Freeze mode things only)
P_local.h  (v0.95: added> extern int tmsectortype)
Info.c     (v0.95: maybee same as 0.71)
Info.h     (v0.95: maybee same as 0.71)
M_menu.c   (v0.95: an extra menu in the key setup with the new commands)
R_main.c   (v0.95: Fix for bot's view)
wi_stuff.c (v0.97: To remove bots correct)

(v0.85) Removed all my code from: P_enemy.c
New file: b_move.c

******************************************
What I know has to be done. in near future.

- Do some hunting/fleeing functions.
- Make the roaming 100% flawfree.
- Fix all SIGSEVS (Below is known SIGSEVS)
      -Nada (but they might be there)
******************************************
Everything that is changed is marked (maybe commented) with "Added by MC"
*/

#include "doomdef.h"
#include "p_local.h"
#include "b_bot.h"
#include "g_game.h"
#include "m_random.h"
#include "z_zone.h"
#include "r_things.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "stats.h"
#include "m_misc.h"

//Externs
DCajunMaster bglobal;

cycle_t BotThinkCycles, BotSupportCycles;

static const char *BotConfigStrings[] =
{
	"name",
	"aiming",
	"perfection",
	"reaction",
	"isp",
	NULL
};

enum
{
	BOTCFG_NAME,
	BOTCFG_AIMING,
	BOTCFG_PERFECTION,
	BOTCFG_REACTION,
	BOTCFG_ISP
};

static bool waitingforspawn[MAXPLAYERS];


void G_DoReborn (int playernum);


//This function is called every tick (from g_game.c),
//send bots into thinking (+more).
void DCajunMaster::Main (int buf)
{
	int i;

	BotThinkCycles = 0;

	if (consoleplayer != Net_Arbitrator || demoplayback)
		return;

	if (gamestate != GS_LEVEL)
		return;

	m_Thinking = true;

	//Think for bots.
	if (botnum)
	{
		clock (BotThinkCycles);
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && players[i].mo && !freeze && players[i].isbot)
				Think (players[i].mo, &netcmds[i][buf]);
		}
		unclock (BotThinkCycles);
	}

	//Add new bots?
	if (wanted_botnum > botnum && !freeze)
	{
		if (t_join == ((wanted_botnum - botnum) * SPAWN_DELAY))
		{
            if (!Spawn (getspawned->GetArg (spawn_tries)))
				wanted_botnum--;
            spawn_tries++;
		}

		t_join--;
	}

	//Check if player should go observer. Or un observe
	if (bot_observer.value && !observer)
	{
		Printf (PRINT_HIGH, "%s is now observer\n", players[consoleplayer].userinfo.netname);
		observer = true;
		players[consoleplayer].mo->UnlinkFromWorld ();
		players[consoleplayer].mo->flags = MF_DROPOFF|MF_NOBLOCKMAP|MF_NOCLIP|MF_SHADOW|MF_NOTDMATCH|MF_NOGRAVITY;
		players[consoleplayer].mo->flags2 |= MF2_FLY;
		players[consoleplayer].mo->LinkToWorld ();
	}
	else if (!bot_observer.value && observer) //Go back
	{
		Printf (PRINT_HIGH, "%s returned to the fray\n", players[consoleplayer].userinfo.netname);
		observer = false;
		players[consoleplayer].mo->UnlinkFromWorld ();
		players[consoleplayer].mo->flags = MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH;
		players[consoleplayer].mo->flags2 &= ~MF2_FLY;
		players[consoleplayer].mo->LinkToWorld ();
	}

	m_Thinking = false;
}

void DCajunMaster::Init ()
{
	int i;

	botnum = 0;
	thingnum = 0;
	firstthing = NULL;
	itemsdone = false;
	spawn_tries = 0;
	freeze = false;
	observer = false;
	body1 = NULL;
	body2 = NULL;

	//Remove all bots upon each level start, they'll get spawned instead.
	playernumber = 0;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		waitingforspawn[i] = false;
		if (playeringame[i] && players[i].isbot)
		{
			playernumber++;
			CleanBotstuff (&players[i]);
			players[i].isbot = false;
			botingame[i] = false;
		}
	}

	if (ctf && teamplay.value==0)
		teamplay.Set (1); //Need teamplay for ctf. (which is not done yet)

	t_join = (wanted_botnum + 1) * SPAWN_DELAY; //The + is to let player get away before the bots come in.

	//Determine Combat distance for each weapon.
	if (deathmatch.value)
	{
		combatdst[wp_fist]         = 1       ;
		combatdst[wp_pistol]       = 25000000;
		combatdst[wp_shotgun]      = 24000000;
		combatdst[wp_chaingun]     = 27000000;
		combatdst[wp_missile]      = (SAFE_SELF_MISDIST*2);
		combatdst[wp_plasma]       = 27000000;
		combatdst[wp_bfg]          = 10000000;
		combatdst[wp_chainsaw]     = 1       ;
		combatdst[wp_supershotgun] = 15000000;
	}
	else
	{
		combatdst[wp_fist]         = 1       ;
		combatdst[wp_pistol]       = 15000000;
		combatdst[wp_shotgun]      = 14000000;
		combatdst[wp_chaingun]     = 17000000;
		combatdst[wp_missile]      = (SAFE_SELF_MISDIST*3/2);
		combatdst[wp_plasma]       = 17000000;
		combatdst[wp_bfg]          = 6000000;
		combatdst[wp_chainsaw]     = 1       ;
		combatdst[wp_supershotgun] = 10000000;
	}

	if (!savegamerestore)
		LoadBots ();
}

//Called on each level exit (from g_game.c).
void DCajunMaster::End ()
{
	int i;

	//Arrange wanted botnum and their names, so they can be spawned next level.
	if (getspawned)
		getspawned->FlushArgs ();
	else
		getspawned = new DArgs;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot)
		{
			getspawned->AppendArg (players[i].userinfo.netname);
			CleanBotstuff (&players[i]);
		}
	}
	wanted_botnum = botnum;
}



//Name can be optional, if = NULL
//then a random bot is spawned.
//If no bot with name = name found
//the function will CONS print an
//error message and will not spawn
//anything.
//The color parameter can be either a
//color (range from 0-10), or = NOCOLOR.
//The color parameter overides bots
//induvidual colors if not = NOCOLOR.

bool DCajunMaster::Spawn (const char *name, int color)
{
	int num=0;
	static bool red; //ctf, spawning helper, spawn first blue then a red ...
	int i;

	//COLORS
	static const char colors[11][17] =
	{
		"\\color\\40 cf 00",	//0  = Green
		"\\color\\b0 b0 b0",	//1  = Gray
		"\\color\\50 50 60",	//2  = Indigo
		"\\color\\8f 00 00",	//3  = Deep Red
		"\\color\\ff ff ff",	//4  = White
		"\\color\\ff af 3f",	//5  = Bright Brown
		"\\color\\bf 00 00",	//6  = Red
		"\\color\\00 00 ff",	//7  = Blue
		"\\color\\00 00 7f",	//8  = Dark Blue
		"\\color\\ff ff 00",	//9  = Yellow
		"\\color\\cf df 90"		//10 = Bleached Bone
	};

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] && !waitingforspawn[i])
		{
			playernumber = i;
			break;
		}
	}

	if (i == MAXPLAYERS)
	{
		Printf (PRINT_HIGH, "The maximum of %d players/bots has been reached\n", MAXPLAYERS);
		return false;
	}

	waitingforspawn[playernumber] = true;

	botinfo_t *thebot;

	if (name)
	{
		thebot = botinfo;

		// Check if exist or already in the game.
		while (thebot && stricmp (name, thebot->name))
			thebot = thebot->next;

		if (thebot == NULL)
		{
   		 	Printf (PRINT_HIGH, "couldn't find %s in %s\n", name, BOTFILENAME);
			return false;
		}
		else if (thebot->inuse)
		{
   		 	Printf (PRINT_HIGH, "%s is already in the thick\n", name);
			return false;
		}
	}
	else if (botnum < loaded_bots)
	{
		bool vacant = false;  //Spawn a random bot from bots.cfg if no name given.
		while (!vacant)
		{
			int rnum = (P_Random(pr_botspawn) % loaded_bots);
			thebot = botinfo;
			while (rnum)
				--rnum, thebot = thebot->next;
			if (!thebot->inuse)
				vacant = true;
		}
	}
	else
	{
		Printf (PRINT_HIGH, "Couldn't spawn bot; no bot left in %s\n", BOTFILENAME);
		return false;
	}

	Net_WriteByte (DEM_ADDBOT);
	Net_WriteByte (playernumber);
	{
		//Set color.
		if (color == NOCOLOR && bot_next_color.value < NOCOLOR && bot_next_color.value >= 0)
		{
			char concat[256];
			strcpy (concat, thebot->info);
			strcat (concat, colors[(int)bot_next_color.value]);
			Net_WriteString (concat);
		}
		else
		{
			Net_WriteString (thebot->info);
		}
	}

	players[playernumber].skill = thebot->skill;

#if 0
	if (ctf && color==NOCOLOR)
	{
		if (red)
		{
			players[bnum].redteam=true;
//			players[bnum].skincolor = 6;
			red=false;
		}
		else
		{
			players[bnum].redteam=false;
//			players[bnum].skincolor = 7;
			red=true;
		}
	}
#endif

	thebot->inuse = true;

	//Increment this.
	botnum++;

	return true;
}

void DCajunMaster::DoAddBot (int bnum, char *info)
{
	multiplayer = true; //Prevents cheating and so on, emulates real netgame (almost).
	players[bnum].isbot = true;
	playeringame[bnum] = true;
	players[bnum].mo = NULL;
	D_ReadUserInfoStrings (bnum, (byte **)&info, false);
	players[bnum].playerstate = PST_REBORN;
	botingame[bnum] = true;
    Printf (PRINT_HIGH, "%s joined the game\n", players[bnum].userinfo.netname);
    G_DoReborn (bnum);
	waitingforspawn[bnum] = false;
}

void DCajunMaster::RemoveAllBots (bool fromlist)
{
	int i;

	if (players[consoleplayer].camera &&
		players[consoleplayer].camera->player &&
		(!playeringame[players[consoleplayer].camera->player - players]
		 ||players[consoleplayer].camera->player->isbot))
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
		displayplayer = consoleplayer;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && botingame[i])
		{
			ClearPlayer (i);
		}
	}

	if (fromlist)
	{
		wanted_botnum = 0;
		for (i = 0; i < MAXPLAYERS; i++)
			waitingforspawn[i] = false;
	}
	botnum = 0;
}

//Clean the bot part of the player_t
//Used when bots are respawned or at level starts.
void DCajunMaster::CleanBotstuff (player_t *p)
{
	p->angle = ANG45;
	p->dest = NULL;
	p->enemy = NULL; //The dead meat.
	p->missile = NULL; //A threatening missile that needs to be avoided.
	p->mate = NULL;    //Friend (used for grouping in templay or coop.
	p->last_mate = NULL; //If bot's mate dissapeared (not if died) that mate is pointed to by this. Allows bot to roam to it if necessary.
	//Tickers
	p->t_active = 0; //Open door, lower lift stuff, door must open and lift must go down before bot does anything radical like try a stuckmove
	p->t_respawn = 0;
	p->t_strafe = 0;
	p->t_react = 0;
	//Misc bools
	p->isbot = true; //Important.
	p->first_shot = true; //Used for reaction skill.
	p->sleft = false; //If false, strafe is right.
	p->allround = false;
	p->redteam = false; //in ctf, if true this bot is on red team, else on blue..
	//Misc
	p->chat = c_none;//What bot will say one a tic.
}


//------------------
//Reads data for bot from
//a .bot file.
//The skills and other data should
//be arranged as follows in the bot file:
//
//{
// Name			bot's name
// Aiming		0-100
// Perfection	0-100
// Reaction		0-100
// Isp			0-100 (Instincts of Self Preservation)
// ???			any other valid userinfo strings can go here
//}

static void appendinfo (char *&front, const char *back)
{
	char *newstr;

	if (front)
	{
		int newlen = strlen (front) + strlen (back) + 2;
		newstr = new char[newlen];
		strcpy (newstr, front);
		delete[] front;
	}
	else
	{
		int newlen = strlen (back) + 2;
		newstr = new char[newlen];
	}
	strcat (newstr, "\\");
	strcat (newstr, back);
	front = newstr;
}

void DCajunMaster::ForgetBots ()
{
	botinfo_t *thebot = botinfo;

	while (thebot)
	{
		botinfo_t *next = thebot->next;
		delete[] thebot->name;
		delete[] thebot->info;
		delete thebot;
		thebot = next;
	}

	botinfo = NULL;
	loaded_bots = 0;
}

bool DCajunMaster::LoadBots ()
{
	bglobal.ForgetBots ();

#ifndef UNIX
	if (!FileExists ("zcajun/" BOTFILENAME))
	{
		DPrintf ("No " BOTFILENAME ", so no bots\n");
		return false;
	}
	else
		SC_OpenFile ("zcajun/" BOTFILENAME);
#else
	char *tmp = GetUserFile (BOTFILENAME);
	if (!FileExists (tmp))
	{
		if (!FileExists (SHARE_DIR BOTFILENAME))
		{
			DPrintf ("No " BOTFILENAME ", so no bots\n");
			return false;
		}
		else
			SC_OpenFile (SHARE_DIR BOTFILENAME);
	}
	else
	{
		SC_OpenFile (tmp);
		delete[] tmp;
	}
#endif

	while (SC_GetString ())
	{
		if (!SC_Compare ("{"))
		{
			SC_ScriptError ("Unexpected token '%s'\n", (const char **)&sc_String);
		}

		botinfo_t *newinfo = new botinfo_t;
		memset (newinfo, 0, sizeof(*newinfo));

		newinfo->info = copystring ("\\autoaim\\0");

		while (1)
		{
			SC_MustGetString ();
			if (SC_Compare ("}"))
				break;

			switch (SC_MatchString (BotConfigStrings))
			{
			case BOTCFG_NAME:
				SC_MustGetString ();
				appendinfo (newinfo->info, "name");
				appendinfo (newinfo->info, sc_String);
				newinfo->name = copystring (sc_String);
				break;

			case BOTCFG_AIMING:
				SC_MustGetNumber ();
				newinfo->skill.aiming = sc_Number;
				break;

			case BOTCFG_PERFECTION:
				SC_MustGetNumber ();
				newinfo->skill.perfection = sc_Number;
				break;

			case BOTCFG_REACTION:
				SC_MustGetNumber ();
				newinfo->skill.reaction = sc_Number;
				break;

			case BOTCFG_ISP:
				SC_MustGetNumber ();
				newinfo->skill.isp = sc_Number;
				break;

			default:
				appendinfo (newinfo->info, sc_String);
				SC_MustGetString ();
				appendinfo (newinfo->info, sc_String);
				break;
			}
		}
		newinfo->next = bglobal.botinfo;
		bglobal.botinfo = newinfo;
		bglobal.loaded_bots++;
	}
	SC_Close ();

	Printf (PRINT_HIGH, "%d bots read from %s\n", bglobal.loaded_bots, BOTFILENAME);

	return true;
}

BEGIN_STAT (bots)
{
	sprintf (out, "think = %04.1f ms  support = %04.1f ms",
		(double)BotThinkCycles * 1000 * SecondsPerCycle,
		(double)BotSupportCycles * 1000 * SecondsPerCycle);
}
END_STAT (bots)
