// Cajun bot console commands.
//
// [RH] Moved out of d_netcmd.c (in Cajun source), because they don't really
// belong there.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "b_bot.h"
#include "m_argv.h"
#include "doomstat.h"
#include "p_local.h"
#include "cmdlib.h"

CVAR (Int, bot_next_color, 11, 0)
CVAR (Bool, bot_observer, false, 0)

IMPLEMENT_POINTY_CLASS (DCajunMaster)
 DECLARE_POINTER (getspawned)
 DECLARE_POINTER (botinfo)
 DECLARE_POINTER (firstthing)
// DECLARE_POINTER (things)
 DECLARE_POINTER (body1)
 DECLARE_POINTER (body2)
END_POINTERS

CCMD (addbot)
{
	if (consoleplayer != Net_Arbitrator)
	{
		Printf ("Only player %d can add bots\n", Net_Arbitrator + 1);
		return;
	}

	if (argc > 2)
	{
		Printf ("addbot [botname] : add a bot to the game\n");
		return;
	}
/*
	if (argc == 3) //Used force colornum
	{
		color = atoi (argv[2]);
		if (color<0) color=0;
		if (color>10) color=10;
	}
*/
	if (argc > 1)
		bglobal.SpawnBot (argv[1]);
	else
		bglobal.SpawnBot (NULL);
}

void DCajunMaster::ClearPlayer (int i)
{
	if (players[i].mo)
	{
		players[i].mo->Destroy ();
		players[i].mo = NULL;
	}
	botinfo_t *bot = botinfo;
	while (bot && stricmp (players[i].userinfo.netname, bot->name))
		bot = bot->next;
	if (bot)
		bot->inuse = false;
	memset (&players[i], 0, sizeof(player_t));
	playeringame[i] = false;
}

CCMD (removebots)
{
	Net_WriteByte (DEM_KILLBOTS);
}

CCMD (freeze)
{
	if (!netgame)
	{
		if (bglobal.freeze)
		{
			bglobal.freeze = false;
			Printf ("Freeze mode off\n");
		}
		else
		{
			bglobal.freeze = true;
			Printf ("Freeze mode on\n");
		}
	}
}

CCMD (listbots)
{
	botinfo_t *thebot = bglobal.botinfo;
	int count = 0;

	while (thebot)
	{
		Printf ("%s%s\n", thebot->name, thebot->inuse ? " (active)" : "");
		thebot = thebot->next;
		count++;
	}
	Printf ("> %d bots\n", count);
}

FArchive &operator<< (FArchive &arc, botskill_t &skill)
{
	return arc << skill.aiming << skill.perfection << skill.reaction << skill.isp;
}
