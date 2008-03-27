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
#include "r_things.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "stats.h"
#include "m_misc.h"
#include "sbar.h"
#include "p_acs.h"
#include "teaminfo.h"

static FRandom pr_botspawn ("BotSpawn");

//Externs
DCajunMaster *bglobal;

cycle_t BotThinkCycles, BotSupportCycles, BotWTG;

static const char *BotConfigStrings[] =
{
	"name",
	"aiming",
	"perfection",
	"reaction",
	"isp",
	"team",
	NULL
};

enum
{
	BOTCFG_NAME,
	BOTCFG_AIMING,
	BOTCFG_PERFECTION,
	BOTCFG_REACTION,
	BOTCFG_ISP,
	BOTCFG_TEAM
};

static bool waitingforspawn[MAXPLAYERS];

DCajunMaster::DCajunMaster()
{
	firstthing = NULL;
	body1 = NULL;
	body2 = NULL;
	getspawned = NULL;
	memset(botingame, 0, sizeof(botingame));
	freeze = false;
	changefreeze = false;
	botnum = 0;
	botinfo = NULL;
	spawn_tries = 0;
	wanted_botnum = NULL;
	m_Thinking = false;
	ctf = false;
	loaded_bots = 0;
	t_join = 0;
	observer = false;
}

DCajunMaster::~DCajunMaster()
{
	ForgetBots();
	if (getspawned != NULL)
	{
		getspawned->Destroy();
		getspawned = NULL;
	}
	// FIXME: Make this object proper
	ObjectFlags |= OF_Cleanup | OF_YesReallyDelete;
}

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
            if (!SpawnBot (getspawned->GetArg (spawn_tries)))
				wanted_botnum--;
            spawn_tries++;
		}

		t_join--;
	}

	//Check if player should go observer. Or un observe
	if (bot_observer && !observer && !netgame)
	{
		Printf ("%s is now observer\n", players[consoleplayer].userinfo.netname);
		observer = true;
		players[consoleplayer].mo->UnlinkFromWorld ();
		players[consoleplayer].mo->flags = MF_DROPOFF|MF_NOBLOCKMAP|MF_NOCLIP|MF_NOTDMATCH|MF_NOGRAVITY|MF_FRIENDLY;
		players[consoleplayer].mo->flags2 |= MF2_FLY;
		players[consoleplayer].mo->LinkToWorld ();
	}
	else if (!bot_observer && observer && !netgame) //Go back
	{
		Printf ("%s returned to the fray\n", players[consoleplayer].userinfo.netname);
		observer = false;
		players[consoleplayer].mo->UnlinkFromWorld ();
		players[consoleplayer].mo->flags = MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY;
		players[consoleplayer].mo->flags2 &= ~MF2_FLY;
		players[consoleplayer].mo->LinkToWorld ();
	}

	m_Thinking = false;
}

void DCajunMaster::Init ()
{
	int i;

	botnum = 0;
	firstthing = NULL;
	spawn_tries = 0;
	freeze = false;
	observer = false;
	body1 = NULL;
	body2 = NULL;

	//Remove all bots upon each level start, they'll get spawned instead.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		waitingforspawn[i] = false;
		if (playeringame[i] && players[i].isbot)
		{
			CleanBotstuff (&players[i]);
			players[i].isbot = false;
			botingame[i] = false;
		}
	}

	if (ctf && teamplay == false)
		teamplay = true; //Need teamplay for ctf. (which is not done yet)

	t_join = (wanted_botnum + 1) * SPAWN_DELAY; //The + is to let player get away before the bots come in.

	if (botinfo == NULL)
	{
		LoadBots ();
	}
	else
	{
		botinfo_t *thebot = botinfo;

		while (thebot != NULL)
		{
			thebot->inuse = false;
			thebot = thebot->next;
		}
	}
}

//Called on each level exit (from g_game.c).
void DCajunMaster::End ()
{
	int i;

	//Arrange wanted botnum and their names, so they can be spawned next level.
	if (getspawned)
		getspawned->FlushArgs ();
	else
	{
		getspawned = new DArgs;
		GC::WriteBarrier(this, getspawned);
	}
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot)
		{
			if (deathmatch)
			{
				getspawned->AppendArg (players[i].userinfo.netname);
			}
			CleanBotstuff (&players[i]);
		}
	}
	if (deathmatch)
	{
		wanted_botnum = botnum;
	}
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

bool DCajunMaster::SpawnBot (const char *name, int color)
{
	int playernumber;

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

	for (playernumber = 0; playernumber < MAXPLAYERS; playernumber++)
	{
		if (!playeringame[playernumber] && !waitingforspawn[playernumber])
		{
			break;
		}
	}

	if (playernumber == MAXPLAYERS)
	{
		Printf ("The maximum of %d players/bots has been reached\n", MAXPLAYERS);
		return false;
	}

	botinfo_t *thebot;

	if (name)
	{
		thebot = botinfo;

		// Check if exist or already in the game.
		while (thebot && stricmp (name, thebot->name))
			thebot = thebot->next;

		if (thebot == NULL)
		{
   		 	Printf ("couldn't find %s in %s\n", name, BOTFILENAME);
			return false;
		}
		else if (thebot->inuse)
		{
   		 	Printf ("%s is already in the thick\n", name);
			return false;
		}
	}
	else if (botnum < loaded_bots)
	{
		bool vacant = false;  //Spawn a random bot from bots.cfg if no name given.
		while (!vacant)
		{
			int rnum = (pr_botspawn() % loaded_bots);
			thebot = botinfo;
			while (rnum)
				--rnum, thebot = thebot->next;
			if (!thebot->inuse)
				vacant = true;
		}
	}
	else
	{
		Printf ("Couldn't spawn bot; no bot left in %s\n", BOTFILENAME);
		return false;
	}

	waitingforspawn[playernumber] = true;

	Net_WriteByte (DEM_ADDBOT);
	Net_WriteByte (playernumber);
	{
		//Set color.
		char concat[512];
		strcpy (concat, thebot->info);
		if (color == NOCOLOR && bot_next_color < NOCOLOR && bot_next_color >= 0)
		{
			strcat (concat, colors[bot_next_color]);
		}
		if (TEAMINFO_IsValidTeam (thebot->lastteam))
		{ // Keep the bot on the same team when switching levels
			sprintf (concat+strlen(concat), "\\team\\%d\n", thebot->lastteam);
		}
		Net_WriteString (concat);
	}

	players[playernumber].skill = thebot->skill;

	thebot->inuse = true;

	//Increment this.
	botnum++;

	return true;
}

void DCajunMaster::DoAddBot (int bnum, char *info)
{
	BYTE *infob = (BYTE *)info;
	D_ReadUserInfoStrings (bnum, &infob, false);
	if (!deathmatch && playerstarts[bnum].type == 0)
	{
		Printf ("%s tried to join, but there was no player %d start\n",
			players[bnum].userinfo.netname, bnum+1);
		ClearPlayer (bnum, false);	// Make the bot inactive again
		if (botnum > 0)
		{
			botnum--;
		}
	}
	else
	{
		multiplayer = true; //Prevents cheating and so on; emulates real netgame (almost).
		players[bnum].isbot = true;
		playeringame[bnum] = true;
		players[bnum].mo = NULL;
		players[bnum].playerstate = PST_ENTER;
		botingame[bnum] = true;

		if (teamplay)
			Printf ("%s joined the %s team\n", players[bnum].userinfo.netname, teams[players[bnum].userinfo.team].name.GetChars ());
		else
			Printf ("%s joined the game\n", players[bnum].userinfo.netname);

		G_DoReborn (bnum, true);
		if (StatusBar != NULL)
		{
			StatusBar->MultiplayerChanged ();
		}
	}
	waitingforspawn[bnum] = false;
}

void DCajunMaster::RemoveAllBots (bool fromlist)
{
	int i, j;

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && botingame[i])
		{
			// If a player is looking through this bot's eyes, make him
			// look through his own eyes instead.
			for (j = 0; j < MAXPLAYERS; ++j)
			{
				if (i != j && playeringame[j] && !botingame[j])
				{
					if (players[j].camera == players[i].mo)
					{
						players[j].camera = players[j].mo;
						if (j == consoleplayer)
						{
							StatusBar->AttachToPlayer (players + j);
						}
					}
				}
			}
			ClearPlayer (i, !fromlist);
			FBehavior::StaticStartTypedScripts (SCRIPT_Disconnect, NULL, true, i);
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
		size_t newlen = strlen (front) + strlen (back) + 2;
		newstr = new char[newlen];
		strcpy (newstr, front);
		delete[] front;
	}
	else
	{
		size_t newlen = strlen (back) + 2;
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
	FScanner sc;
	FString tmp;
	bool gotteam = false;

	bglobal->ForgetBots ();
#ifndef unix
	tmp = progdir;
	tmp += "zcajun/" BOTFILENAME;
	if (!FileExists (tmp))
	{
		DPrintf ("No " BOTFILENAME ", so no bots\n");
		return false;
	}
#else
	tmp = GetUserFile (BOTFILENAME);
	if (!FileExists (tmp))
	{
		if (!FileExists (SHARE_DIR BOTFILENAME))
		{
			DPrintf ("No " BOTFILENAME ", so no bots\n");
			return false;
		}
		else
			sc.OpenFile (SHARE_DIR BOTFILENAME);
	}
#endif
	else
	{
		sc.OpenFile (tmp);
	}

	while (sc.GetString ())
	{
		if (!sc.Compare ("{"))
		{
			sc.ScriptError ("Unexpected token '%s'\n", sc.String);
		}

		botinfo_t *newinfo = new botinfo_t;
		bool gotclass = false;

		memset (newinfo, 0, sizeof(*newinfo));

		newinfo->info = copystring ("\\autoaim\\0\\movebob\\.25");

		for (;;)
		{
			sc.MustGetString ();
			if (sc.Compare ("}"))
				break;

			switch (sc.MatchString (BotConfigStrings))
			{
			case BOTCFG_NAME:
				sc.MustGetString ();
				appendinfo (newinfo->info, "name");
				appendinfo (newinfo->info, sc.String);
				newinfo->name = copystring (sc.String);
				break;

			case BOTCFG_AIMING:
				sc.MustGetNumber ();
				newinfo->skill.aiming = sc.Number;
				break;

			case BOTCFG_PERFECTION:
				sc.MustGetNumber ();
				newinfo->skill.perfection = sc.Number;
				break;

			case BOTCFG_REACTION:
				sc.MustGetNumber ();
				newinfo->skill.reaction = sc.Number;
				break;

			case BOTCFG_ISP:
				sc.MustGetNumber ();
				newinfo->skill.isp = sc.Number;
				break;

			case BOTCFG_TEAM:
				{
					char teamstr[16];
					BYTE teamnum;

					sc.MustGetString ();
					if (IsNum (sc.String))
					{
						teamnum = atoi (sc.String);
						if (!TEAMINFO_IsValidTeam (teamnum))
						{
							teamnum = TEAM_None;
						}
					}
					else
					{
						teamnum = TEAM_None;
						for (int i = 0; i < int(teams.Size()); ++i)
						{
							if (stricmp (teams[i].name, sc.String) == 0)
							{
								teamnum = i;
								break;
							}
						}
					}
					appendinfo (newinfo->info, "team");
					sprintf (teamstr, "%d", teamnum);
					appendinfo (newinfo->info, teamstr);
					gotteam = true;
					break;
				}

			default:
				if (stricmp (sc.String, "playerclass") == 0)
				{
					gotclass = true;
				}
				appendinfo (newinfo->info, sc.String);
				sc.MustGetString ();
				appendinfo (newinfo->info, sc.String);
				break;
			}
		}
		if (!gotclass)
		{ // Bots that don't specify a class get a random one
			appendinfo (newinfo->info, "playerclass");
			appendinfo (newinfo->info, "random");
		}
		if (!gotteam)
		{ // Same for bot teams
			appendinfo (newinfo->info, "team");
			appendinfo (newinfo->info, "255");
		}
		newinfo->next = bglobal->botinfo;
		newinfo->lastteam = TEAM_None;
		bglobal->botinfo = newinfo;
		bglobal->loaded_bots++;
	}
	Printf ("%d bots read from %s\n", bglobal->loaded_bots, BOTFILENAME);
	return true;
}

ADD_STAT (bots)
{
	FString out;
	out.Format ("think = %04.1f ms  support = %04.1f ms  wtg = %llu",
		(double)BotThinkCycles * 1000 * SecondsPerCycle,
		(double)BotSupportCycles * 1000 * SecondsPerCycle,
		BotWTG);
	return out;
}
