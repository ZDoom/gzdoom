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

//CVAR (bot_flag_return_time, "1000", CVAR_ARCHIVE)
CVAR (bot_next_color, "11", 0)
CVAR (weapondrop, "0", CVAR_SERVERINFO)
//CVAR (bot_allow_duds, "0", CVAR_ARCHIVE)
//CVAR (bot_maxcorpses, "0", CVAR_ARCHIVE)
CVAR (bot_observer, "0", 0)
//CVAR (bot_watersplash, "0", CVAR_ARCHIVE)
//CVAR (bot_chat, "1", CVAR_ARCHIVE)

IMPLEMENT_POINTY_CLASS (DCajunMaster, DObject)
 DECLARE_POINTER (getspawned)
 DECLARE_POINTER (botinfo)
 DECLARE_POINTER (firstthing)
// DECLARE_POINTER (things)
 DECLARE_POINTER (body1)
 DECLARE_POINTER (body2)
END_POINTERS

BEGIN_COMMAND (addbot)
{
	if (consoleplayer != Net_Arbitrator)
	{
		Printf (PRINT_HIGH, "Only player %d can add bots\n", Net_Arbitrator + 1);
		return;
	}

	if (argc > 2)
	{
		Printf (PRINT_HIGH, "addbot [botname] : add a bot to the game\n");
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
		bglobal.Spawn (argv[1]);
	else
		bglobal.Spawn (NULL);
}
END_COMMAND (addbot)

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

BEGIN_COMMAND (removebots)
{
	Net_WriteByte (DEM_KILLBOTS);
}
END_COMMAND (removebots)

BEGIN_COMMAND (freeze)
{
	if (!netgame)
	{
		if (bglobal.freeze)
		{
			bglobal.freeze = false;
			Printf (PRINT_HIGH, "Freeze mode off\n");
		}
		else
		{
			bglobal.freeze = true;
			Printf (PRINT_HIGH, "Freeze mode on\n");
		}
	}
}
END_COMMAND (freeze)

BEGIN_COMMAND (listbots)
{
	botinfo_t *thebot = bglobal.botinfo;
	int count = 0;

	while (thebot)
	{
		Printf (PRINT_HIGH, "%s%s\n", thebot->name, thebot->inuse ? " (active)" : "");
		thebot = thebot->next;
		count++;
	}
	Printf (PRINT_HIGH, "> %d bots\n", count);
}
END_COMMAND (listbots)

FArchive &operator<< (FArchive &arc, botskill_t &skill)
{
	return arc << skill.aiming << skill.perfection << skill.reaction << skill.isp;
}
FArchive &operator>> (FArchive &arc, botskill_t &skill)
{
	return arc >> skill.aiming >> skill.perfection >> skill.reaction >> skill.isp;
}
