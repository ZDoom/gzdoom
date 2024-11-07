/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999 Martin Colberg
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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
#include "doomstat.h"
#include "cmdlib.h"
#include "m_misc.h"
#include "sbar.h"
#include "p_acs.h"
#include "teaminfo.h"
#include "d_net.h"
#include "d_netinf.h"
#include "d_player.h"
#include "events.h"
#include "vm.h"
#include "g_levellocals.h"

#if !defined _WIN32 && !defined __APPLE__
#include "i_system.h"  // for SHARE_DIR
#endif // !_WIN32 && !__APPLE__

static FRandom pr_botspawn ("BotSpawn");

cycle_t BotThinkCycles, BotSupportCycles;
int BotWTG;

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

FCajunMaster::~FCajunMaster()
{
	ForgetBots();
}

//This function is called every tick (from g_game.c).
void FCajunMaster::Main(FLevelLocals *Level)
{
	BotThinkCycles.Reset();

	if (demoplayback || gamestate != GS_LEVEL || consoleplayer != Net_Arbitrator)
		return;

	//Add new bots?
	if (wanted_botnum > botnum && !Level->isFrozen())
	{
		if (t_join == ((wanted_botnum - botnum) * SPAWN_DELAY))
		{
			if (!SpawnBot (getspawned[spawn_tries].GetChars()))
				wanted_botnum--;
			spawn_tries++;
		}

		t_join--;
	}

	//Check if player should go observer. Or un observe
	FLinkContext ctx;
}

void FCajunMaster::Init ()
{
	botnum = 0;
	firstthing = nullptr;
	spawn_tries = 0;
	body1 = nullptr;
	body2 = nullptr;

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
			thebot->inuse = BOTINUSE_No;
			thebot = thebot->next;
		}
	}
}

//Called on each level exit (from g_game.c).
void FCajunMaster::End ()
{
	int i;

	//Arrange wanted botnum and their names, so they can be spawned next level.
	getspawned.Clear();
	if (deathmatch)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (players[i].Bot != NULL)
			{
				getspawned.Push(players[i].userinfo.GetName());
			}
		}

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
//individual colors if not = NOCOLOR.

bool FCajunMaster::SpawnBot (const char *name, int color)
{
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

	botinfo_t *thebot = botinfo;
	int botshift = 0;

	if (name)
	{
		// Check if exist or already in the game.
		while (thebot && thebot->Name.CompareNoCase(name))
		{
			botshift++;
			thebot = thebot->next;
		}

		if (thebot == NULL)
		{
   		 	Printf ("couldn't find %s in %s\n", name, BOTFILENAME);
			return false;
		}
		else if (thebot->inuse == BOTINUSE_Waiting)
		{
			return false;
		}
		else if (thebot->inuse == BOTINUSE_Yes)
		{
   		 	Printf ("%s is already in the thick\n", name);
			return false;
		}
	}
	else
	{
		//Spawn a random bot from bots.cfg if no name given.
		TArray<botinfo_t *> BotInfoAvailable;

		while (thebot)
		{
			if (thebot->inuse == BOTINUSE_No)
				BotInfoAvailable.Push (thebot);

			thebot = thebot->next;
		}

		if (BotInfoAvailable.Size () == 0)
		{
			Printf ("Couldn't spawn bot; no bot left in %s\n", BOTFILENAME);
			return false;
		}

		thebot = BotInfoAvailable[pr_botspawn() % BotInfoAvailable.Size ()];

		botinfo_t *thebot2 = botinfo;
		while (thebot2)
		{
			if (thebot == thebot2)
				break;

			botshift++;
			thebot2 = thebot2->next;
		}
	}

	thebot->inuse = BOTINUSE_Waiting;

	Net_WriteInt8 (DEM_ADDBOT);
	Net_WriteInt8 (botshift);
	{
		//Set color.
		FString concat = thebot->Info;
		if (color == NOCOLOR && bot_next_color < NOCOLOR && bot_next_color >= 0)
		{
			concat << colors[bot_next_color];
		}
		if (FTeam::IsValid (thebot->lastteam))
		{ // Keep the bot on the same team when switching levels
			concat.AppendFormat("\\team\\%d\n", thebot->lastteam);
		}
		Net_WriteString (concat.GetChars());
	}
	Net_WriteInt8(thebot->skill.aiming);
	Net_WriteInt8(thebot->skill.perfection);
	Net_WriteInt8(thebot->skill.reaction);
	Net_WriteInt8(thebot->skill.isp);

	return true;
}

void FCajunMaster::TryAddBot (FLevelLocals *Level, uint8_t **stream, int player)
{
	int botshift = ReadInt8 (stream);
	char *info = ReadString (stream);
	botskill_t skill;
	skill.aiming = ReadInt8 (stream);
	skill.perfection = ReadInt8 (stream);
	skill.reaction = ReadInt8 (stream);
	skill.isp = ReadInt8 (stream);

	botinfo_t *thebot = NULL;

	if (consoleplayer == player)
	{
		thebot = botinfo;

		while (botshift > 0)
		{
			thebot = thebot->next;
			botshift--;
		}
	}

	if (DoAddBot (Level,(uint8_t *)info, skill))
	{
		//Increment this.
		botnum++;

		if (thebot != NULL)
		{
			thebot->inuse = BOTINUSE_Yes;
		}
	}
	else
	{
		if (thebot != NULL)
		{
			thebot->inuse = BOTINUSE_No;
		}
	}

	delete[] info;
}

bool FCajunMaster::DoAddBot (FLevelLocals *Level, uint8_t *info, botskill_t skill)
{
	int bnum;

	for (bnum = 0; bnum < MAXPLAYERS; bnum++)
	{
		if (!playeringame[bnum])
		{
			break;
		}
	}

	if (bnum == MAXPLAYERS)
	{
		Printf ("The maximum of %d players/bots has been reached\n", MAXPLAYERS);
		return false;
	}

	D_ReadUserInfoStrings (bnum, &info, false);

	multiplayer = true; //Prevents cheating and so on; emulates real netgame (almost).
	players[bnum].Bot = Level->CreateThinker<DBot>();
	players[bnum].Bot->player = &players[bnum];
	players[bnum].Bot->skill = skill;
	playeringame[bnum] = true;
	players[bnum].mo = NULL;
	players[bnum].playerstate = PST_ENTER;

	if (teamplay)
		Printf ("%s joined the %s team\n", players[bnum].userinfo.GetName(), Teams[players[bnum].userinfo.GetTeam()].GetName());
	else
		Printf ("%s joined the game\n", players[bnum].userinfo.GetName());

	Level->DoReborn (bnum, true);
	Level->localEventManager->PlayerEntered(bnum, false);
	return true;
}

void FCajunMaster::RemoveAllBots (FLevelLocals *Level, bool fromlist)
{
	int i, j;

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (Level->Players[i]->Bot != nullptr)
		{
			// If a player is looking through this bot's eyes, make him
			// look through his own eyes instead.
			for (j = 0; j < MAXPLAYERS; ++j)
			{
				if (i != j && Level->PlayerInGame(j) && Level->Players[j]->Bot == nullptr)
				{
					if (Level->Players[j]->camera == Level->Players[i]->mo)
					{
						Level->Players[j]->camera = Level->Players[j]->mo;
						if (Level->isConsolePlayer(Level->Players[j]->mo))
						{
							StatusBar->AttachToPlayer (Level->Players[j]);
						}
					}
				}
			}
			// [ZZ] run event hook
			Level->localEventManager->PlayerDisconnected(i);
			Level->Behaviors.StartTypedScripts (SCRIPT_Disconnect, Level->Players[i]->mo, true, i, true);
			ClearPlayer (i, !fromlist);
		}
	}

	if (fromlist)
	{
		wanted_botnum = 0;
	}
	botnum = 0;
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

static void appendinfo (FString &front, const char *back)
{
	front << "\\" << back;
}

void FCajunMaster::ForgetBots ()
{
	botinfo_t *thebot = botinfo;

	while (thebot)
	{
		botinfo_t *next = thebot->next;
		delete thebot;
		thebot = next;
	}

	botinfo = NULL;
}

#if defined _WIN32 || defined __APPLE__

FString M_GetCajunPath(const char* botfilename)
{
	FString path;

	path << progdir << "zcajun/" << botfilename;
	if (!FileExists(path))
	{
		path = "";
	}
	return path;
}

#else

FString M_GetCajunPath(const char* botfilename)
{
	FString path;

	// Check first in $HOME/.config/zdoom/botfilename.
	path = GetUserFile(botfilename);
	if (!FileExists(path))
	{
		// Then check in SHARE_DIR/botfilename.
		path = SHARE_DIR;
		path << botfilename;
		if (!FileExists(path))
		{
			path = "";
		}
	}
	return path;
}

#endif

bool FCajunMaster::LoadBots ()
{
	FScanner sc;
	FString tmp;
	bool gotteam = false;
	int loaded_bots = 0;

	ForgetBots ();
	tmp = M_GetCajunPath(BOTFILENAME);
	if (tmp.IsEmpty())
	{
		DPrintf (DMSG_ERROR, "No " BOTFILENAME ", so no bots\n");
		return false;
	}
	if (!sc.OpenFile(tmp.GetChars()))
	{
		Printf("Unable to open %s. So no bots\n", tmp.GetChars());
		return false;
	}

	while (sc.GetString ())
	{
		if (!sc.Compare ("{"))
		{
			sc.ScriptError ("Unexpected token '%s'\n", sc.String);
		}

		botinfo_t *newinfo = new botinfo_t;
		bool gotclass = false;

		newinfo->Info = "\\autoaim\\0\\movebob\\.25";

		for (;;)
		{
			sc.MustGetString ();
			if (sc.Compare ("}"))
				break;

			switch (sc.MatchString (BotConfigStrings))
			{
			case BOTCFG_NAME:
				sc.MustGetString ();
				appendinfo (newinfo->Info, "name");
				appendinfo (newinfo->Info, sc.String);
				newinfo->Name = sc.String;
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
					uint8_t teamnum;

					sc.MustGetString ();
					if (IsNum (sc.String))
					{
						teamnum = atoi (sc.String);
						if (!FTeam::IsValid (teamnum))
						{
							teamnum = TEAM_NONE;
						}
					}
					else
					{
						teamnum = TEAM_NONE;
						for (unsigned int i = 0; i < Teams.Size(); ++i)
						{
							if (stricmp (Teams[i].GetName (), sc.String) == 0)
							{
								teamnum = i;
								break;
							}
						}
					}
					appendinfo (newinfo->Info, "team");
					mysnprintf (teamstr, countof(teamstr), "%d", teamnum);
					appendinfo (newinfo->Info, teamstr);
					gotteam = true;
					break;
				}

			default:
				if (stricmp (sc.String, "playerclass") == 0)
				{
					gotclass = true;
				}
				appendinfo (newinfo->Info, sc.String);
				sc.MustGetString ();
				appendinfo (newinfo->Info, sc.String);
				break;
			}
		}
		if (!gotclass)
		{ // Bots that don't specify a class get a random one
			appendinfo (newinfo->Info, "playerclass");
			appendinfo (newinfo->Info, "random");
		}
		if (!gotteam)
		{ // Same for bot teams
			appendinfo (newinfo->Info, "team");
			appendinfo (newinfo->Info, "255");
		}
		newinfo->next = botinfo;
		newinfo->lastteam = TEAM_NONE;
		botinfo = newinfo;
		loaded_bots++;
	}
	Printf ("%d bots read from %s\n", loaded_bots, BOTFILENAME);
	return true;
}

ADD_STAT (bots)
{
	FString out;
	out.Format ("think = %04.1f ms  support = %04.1f ms  wtg = %d",
		BotThinkCycles.TimeMS(), BotSupportCycles.TimeMS(),
		BotWTG);
	return out;
}
