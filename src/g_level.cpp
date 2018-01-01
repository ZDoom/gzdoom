/*
** g_level.cpp
** controls movement between levels
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

#include <assert.h>
#include "templates.h"
#include "d_main.h"
#include "g_level.h"
#include "g_game.h"
#include "s_sound.h"
#include "d_event.h"
#include "m_random.h"
#include "doomerrors.h"
#include "doomstat.h"
#include "wi_stuff.h"
#include "w_wad.h"
#include "am_map.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "p_setup.h"
#include "p_local.h"
#include "r_sky.h"
#include "c_console.h"
#include "intermission/intermission.h"
#include "gstrings.h"
#include "v_video.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "p_saveg.h"
#include "p_acs.h"
#include "d_protocol.h"
#include "v_text.h"
#include "s_sndseq.h"
#include "b_bot.h"
#include "sc_man.h"
#include "sbar.h"
#include "a_lightning.h"
#include "m_png.h"
#include "m_random.h"
#include "version.h"
#include "statnums.h"
#include "sbarinfo.h"
#include "r_data/r_translate.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_netinf.h"
#include "v_palette.h"
#include "menu/menu.h"
#include "a_sharedglobal.h"
#include "r_data/colormaps.h"
#include "r_renderer.h"
#include "r_utility.h"
#include "p_spec.h"
#include "serializer.h"
#include "vm.h"
#include "events.h"
#include "dobjgc.h"

#include "gi.h"

#include "g_hub.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "vm.h"
#include "i_time.h"

#include <string.h>

void STAT_StartNewGame(const char *lev);
void STAT_ChangeLevel(const char *newl);

EXTERN_CVAR(Bool, save_formatted)
EXTERN_CVAR (Float, sv_gravity)
EXTERN_CVAR (Float, sv_aircontrol)
EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (String, playerclass)

#define SNAP_ID			MAKE_ID('s','n','A','p')
#define DSNP_ID			MAKE_ID('d','s','N','p')
#define VIST_ID			MAKE_ID('v','i','S','t')
#define ACSD_ID			MAKE_ID('a','c','S','d')
#define RCLS_ID			MAKE_ID('r','c','L','s')
#define PCLS_ID			MAKE_ID('p','c','L','s')

void G_VerifySkill();


static FRandom pr_classchoice ("RandomPlayerClassChoice");

extern level_info_t TheDefaultLevelInfo;
extern bool timingdemo;

// Start time for timing demos
int starttime;


extern FString BackupSaveName;

bool savegamerestore;
int finishstate = FINISH_NoHub;

extern int mousex, mousey;
extern bool sendpause, sendsave, sendturn180, SendLand;

void *statcopy;					// for statistics driver

FLevelLocals level;			// info about current level


//==========================================================================
//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, playeringame[] should be set.
//
//==========================================================================

static FString d_mapname;
static int d_skill=-1;

void G_DeferedInitNew (const char *mapname, int newskill)
{
	d_mapname = mapname;
	d_skill = newskill;
	CheckWarpTransMap (d_mapname, true);
	gameaction = ga_newgame2;
}

void G_DeferedInitNew (FGameStartup *gs)
{
	if (gs->PlayerClass != NULL) playerclass = gs->PlayerClass;
	d_mapname = AllEpisodes[gs->Episode].mEpisodeMap;
	d_skill = gs->Skill;
	CheckWarpTransMap (d_mapname, true);
	gameaction = ga_newgame2;
	finishstate = FINISH_NoHub;
}

//==========================================================================
//
//
//==========================================================================

CCMD (map)
{
	if (netgame)
	{
		Printf ("Use " TEXTCOLOR_BOLD "changemap" TEXTCOLOR_NORMAL " instead. " TEXTCOLOR_BOLD "Map"
				TEXTCOLOR_NORMAL " is for single-player only.\n");
		return;
	}
	if (argv.argc() > 1)
	{
		const char *mapname = argv[1];
		if (!strcmp(mapname, "*")) mapname = level.MapName.GetChars();

		try
		{
			if (!P_CheckMapData(mapname))
			{
				Printf ("No map %s\n", mapname);
			}
			else
			{
				if (argv.argc() > 2 && stricmp(argv[2], "coop") == 0)
				{
					deathmatch = false;
					multiplayernext = true;
				}
				else if (argv.argc() > 2 && stricmp(argv[2], "dm") == 0)
				{
					deathmatch = true;
					multiplayernext = true;
				}
				G_DeferedInitNew (mapname);
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
		Printf ("Usage: map <map name> [coop|dm]\n");
	}
}

//==========================================================================
//
//
//==========================================================================

CCMD(recordmap)
{
	if (netgame)
	{
		Printf("You cannot record a new game while in a netgame.");
		return;
	}
	if (argv.argc() > 2)
	{
		const char *mapname = argv[2];
		if (!strcmp(mapname, "*")) mapname = level.MapName.GetChars();

		try
		{
			if (!P_CheckMapData(mapname))
			{
				Printf("No map %s\n", mapname);
			}
			else
			{
				if (argv.argc() > 3 && stricmp(argv[3], "coop") == 0)
				{
					deathmatch = false;
					multiplayernext = true;
				}
				else if (argv.argc() > 3 && stricmp(argv[3], "dm") == 0)
				{
					deathmatch = true;
					multiplayernext = true;
				}
				G_DeferedInitNew(mapname);
				gameaction = ga_recordgame;
				newdemoname = argv[1];
				newdemomap = mapname;
			}
		}
		catch (CRecoverableError &error)
		{
			if (error.GetMessage())
				Printf("%s", error.GetMessage());
		}
	}
	else
	{
		Printf("Usage: recordmap <filename> <map name> [coop|dm]\n");
	}
}

//==========================================================================
//
//
//==========================================================================

CCMD (open)
{
	if (netgame)
	{
		Printf ("You cannot use open in multiplayer games.\n");
		return;
	}
	if (argv.argc() > 1)
	{
		d_mapname = "file:";
		d_mapname += argv[1];
		if (!P_CheckMapData(d_mapname))
		{
			Printf ("No map %s\n", d_mapname.GetChars());
		}
		else
		{
			if (argv.argc() > 2 && stricmp(argv[2], "coop") == 0)
			{
				deathmatch = false;
				multiplayernext = true;
			}
			else if (argv.argc() > 2 && stricmp(argv[2], "dm") == 0)
			{
				deathmatch = true;
				multiplayernext = true;
			}
			gameaction = ga_newgame2;
			d_skill = -1;
		}
	}
	else
	{
		Printf ("Usage: open <map file> [coop|dm]\n");
	}
}


//==========================================================================
//
//
//==========================================================================

void G_NewInit ()
{
	int i;

	// Destory all old player refrences that may still exist
	TThinkerIterator<APlayerPawn> it(STAT_TRAVELLING);
	APlayerPawn *pawn, *next;

	next = it.Next();
	while ((pawn = next) != NULL)
	{
		next = it.Next();
		pawn->flags |= MF_NOSECTOR | MF_NOBLOCKMAP;
		pawn->Destroy();
	}

	G_ClearSnapshots ();
	netgame = false;
	multiplayer = multiplayernext;
	multiplayernext = false;
	if (demoplayback)
	{
		C_RestoreCVars ();
		demoplayback = false;
		D_SetupUserInfo ();
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		player_t *p = &players[i];
		userinfo_t saved_ui;
		saved_ui.TransferFrom(players[i].userinfo);
		int chasecam = p->cheats & CF_CHASECAM;
		p->~player_t();
		::new(p) player_t;
		players[i].cheats |= chasecam;
		players[i].playerstate = PST_DEAD;
		playeringame[i] = 0;
		players[i].userinfo.TransferFrom(saved_ui);
	}
	BackupSaveName = "";
	consoleplayer = 0;
	NextSkill = -1;
}

//==========================================================================
//
//
//==========================================================================

void G_DoNewGame (void)
{
	G_NewInit ();
	playeringame[consoleplayer] = 1;
	if (d_skill != -1)
	{
		gameskill = d_skill;
	}
	G_InitNew (d_mapname, false);
	gameaction = ga_nothing;
}

//==========================================================================
//
// Initializes player classes in case they are random.
// This gets called at the start of a new game, and the classes
// chosen here are used for the remainder of a single-player
// or coop game. These are ignored for deathmatch.
//
//==========================================================================


static void InitPlayerClasses ()
{
	if (!savegamerestore)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			SinglePlayerClass[i] = players[i].userinfo.GetPlayerClassNum();
			if (SinglePlayerClass[i] < 0 || !playeringame[i])
			{
				SinglePlayerClass[i] = (pr_classchoice()) % PlayerClasses.Size ();
			}
			players[i].cls = NULL;
			players[i].CurrentPlayerClass = SinglePlayerClass[i];
		}
	}
}

//==========================================================================
//
//
//==========================================================================

void G_InitNew (const char *mapname, bool bTitleLevel)
{
	bool wantFast;
	int i;

	// did we have any level before?
	if (level.info != nullptr)
		E_WorldUnloadedUnsafe();

	if (!savegamerestore)
	{
		G_ClearHubInfo();
		G_ClearSnapshots ();
		P_RemoveDefereds ();

		// [RH] Mark all levels as not visited
		for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
			wadlevelinfos[i].flags = wadlevelinfos[i].flags & ~LEVEL_VISITED;
	}

	UnlatchCVars ();
	G_VerifySkill();
	UnlatchCVars ();
	DThinker::DestroyThinkersInList(STAT_STATIC);

	if (paused)
	{
		paused = 0;
		S_ResumeSound (false);
	}

	ST_CreateStatusBar(bTitleLevel);
	setsizeneeded = true;

	if (gameinfo.gametype == GAME_Strife || (SBarInfoScript[SCRIPT_CUSTOM] != NULL && SBarInfoScript[SCRIPT_CUSTOM]->GetGameType() == GAME_Strife))
	{
		// Set the initial quest log text for Strife.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				players[i].SetLogText ("Find help");
		}
	}

	// [RH] If this map doesn't exist, bomb out
	if (!P_CheckMapData(mapname))
	{
		I_Error ("Could not find map %s\n", mapname);
	}

	wantFast = !!G_SkillProperty(SKILLP_FastMonsters);
	GameSpeed = wantFast ? SPEED_Fast : SPEED_Normal;

	if (!savegamerestore)
	{
		if (!netgame && !demorecording && !demoplayback)
		{
			// [RH] Change the random seed for each new single player game
			// [ED850] The demo already sets the RNG.
			rngseed = use_staticrng ? staticrngseed : (rngseed + 1);
		}
		FRandom::StaticClearRandom ();
		P_ClearACSVars(true);
		level.time = 0;
		level.maptime = 0;
		level.totaltime = 0;

		if (!multiplayer || !deathmatch)
		{
			InitPlayerClasses ();
		}

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_ENTER;	// [BC]

		STAT_StartNewGame(mapname);
	}

	usergame = !bTitleLevel;		// will be set false if a demo
	paused = 0;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	V_SetBorderNeedRefresh();

	//Added by MC: Initialize bots.
	if (!deathmatch)
	{
		bglobal.Init ();
	}

	level.MapName = mapname;
	if (bTitleLevel)
	{
		gamestate = GS_TITLELEVEL;
	}
	else if (gamestate != GS_STARTUP)
	{
		gamestate = GS_LEVEL;
	}
	G_DoLoadLevel (0, false);
}

//
// G_DoCompleted
//
static FString	nextlevel;
static int		startpos;	// [RH] Support for multiple starts per level
extern int		NoWipe;		// [RH] Don't wipe when travelling in hubs
static int		changeflags;
static bool		unloading;

//==========================================================================
//
// [RH] The position parameter to these next three functions should
//		match the first parameter of the single player start spots
//		that should appear in the next map.
//
//==========================================================================

EXTERN_CVAR(Bool, sv_singleplayerrespawn)

void G_ChangeLevel(const char *levelname, int position, int flags, int nextSkill)
{
	level_info_t *nextinfo = NULL;

	if (unloading)
	{
		Printf (TEXTCOLOR_RED "Unloading scripts cannot exit the level again.\n");
		return;
	}
	if (gameaction == ga_completed && !(i_compatflags2 & COMPATF2_MULTIEXIT))	// do not exit multiple times.
	{
		return;
	}

	if (levelname == NULL || *levelname == 0)
	{
		// end the game
		levelname = NULL;
		if (!level.NextMap.Compare("enDSeQ",6))
		{
			nextlevel = level.NextMap;	// If there is already an end sequence please leave it alone!
		}
		else 
		{
			nextlevel.Format("enDSeQ%04x", int(gameinfo.DefaultEndSequence));
		}
	}
	else if (strncmp(levelname, "enDSeQ", 6) != 0)
	{
		FString reallevelname = levelname;
		CheckWarpTransMap(reallevelname, true);
		nextinfo = FindLevelInfo (reallevelname, false);
		if (nextinfo != NULL)
		{
			level_info_t *nextredir = nextinfo->CheckLevelRedirect();
			if (nextredir != NULL)
			{
				nextinfo = nextredir;
			}
			nextlevel = nextinfo->MapName;
		}
		else
		{
			nextlevel = levelname;
		}
	}
	else
	{
		nextlevel = levelname;
	}

	if (nextSkill != -1)
		NextSkill = nextSkill;

	if (flags & CHANGELEVEL_NOINTERMISSION)
	{
		level.flags |= LEVEL_NOINTERMISSION;
	}

	cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
	cluster_info_t *nextcluster = nextinfo? FindClusterInfo (nextinfo->cluster) : NULL;

	startpos = position;
	gameaction = ga_completed;
		
	if (nextinfo != NULL) 
	{
		if (thiscluster != nextcluster || (thiscluster && !(thiscluster->flags & CLUSTER_HUB)))
		{
			if (nextinfo->flags2 & LEVEL2_RESETINVENTORY)
			{
				flags |= CHANGELEVEL_RESETINVENTORY;
			}
			if (nextinfo->flags2 & LEVEL2_RESETHEALTH)
			{
				flags |= CHANGELEVEL_RESETHEALTH;
			}
		}
	}
	changeflags = flags;

	bglobal.End();	//Added by MC:

	// [RH] Give scripts a chance to do something
	unloading = true;
	FBehavior::StaticStartTypedScripts (SCRIPT_Unloading, NULL, false, 0, true);
	// [ZZ] safe world unload
	E_WorldUnloaded();
	// [ZZ] unsafe world unload (changemap != map)
	E_WorldUnloadedUnsafe();
	unloading = false;

	STAT_ChangeLevel(nextlevel);

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if ((level.flags & LEVEL_NOINTERMISSION) || ((nextcluster == thiscluster) && !(thiscluster->flags & CLUSTER_ALLOWINTERMISSION)))
			NoWipe = 35;
		D_DrawIcon = "TELEICON";
	}

	for(int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			player_t *player = &players[i];

			// Un-crouch all players here.
			player->Uncrouch();

			// If this is co-op, respawn any dead players now so they can
			// keep their inventory on the next map.
			if ((multiplayer || level.flags2 & LEVEL2_ALLOWRESPAWN || sv_singleplayerrespawn) && !deathmatch && player->playerstate == PST_DEAD)
			{
				// Copied from the end of P_DeathThink [[
				player->cls = NULL;		// Force a new class if the player is using a random class
				player->playerstate = PST_REBORN;
				if (player->mo->special1 > 2)
				{
					player->mo->special1 = 0;
				}
				// ]]
				G_DoReborn(i, false);
			}
		}
	}
}

//==========================================================================
//
//
//==========================================================================

const char *G_GetExitMap()
{
	return level.NextMap;
}

const char *G_GetSecretExitMap()
{
	const char *nextmap = level.NextMap;

	if (level.NextSecretMap.Len() > 0)
	{
		if (P_CheckMapData(level.NextSecretMap))
		{
			nextmap = level.NextSecretMap;
		}
	}
	return nextmap;
}

//==========================================================================
//
//
//==========================================================================

void G_ExitLevel (int position, bool keepFacing)
{
	level.flags3 |= LEVEL3_EXITNORMALUSED;
	G_ChangeLevel(G_GetExitMap(), position, keepFacing ? CHANGELEVEL_KEEPFACING : 0);
}

void G_SecretExitLevel (int position) 
{
	level.flags3 |= LEVEL3_EXITSECRETUSED;
	G_ChangeLevel(G_GetSecretExitMap(), position, 0);
}

//==========================================================================
//
//
//==========================================================================

void G_DoCompleted (void)
{
	int i; 

	gameaction = ga_nothing;

	if (   gamestate == GS_DEMOSCREEN
		|| gamestate == GS_FULLCONSOLE
		|| gamestate == GS_STARTUP)
	{
		return;
	}

	if (gamestate == GS_TITLELEVEL)
	{
		level.MapName = nextlevel;
		G_DoLoadLevel (startpos, false);
		startpos = 0;
		viewactive = true;
		return;
	}

	// [RH] Mark this level as having been visited
	if (!(level.flags & LEVEL_CHANGEMAPCHEAT))
		FindLevelInfo (level.MapName)->flags |= LEVEL_VISITED;

	if (automapactive)
		AM_Stop ();

	wminfo.finished_ep = level.cluster - 1;
	wminfo.LName0 = TexMan.CheckForTexture(level.info->PName, FTexture::TEX_MiscPatch);
	wminfo.current = level.MapName;

	if (deathmatch &&
		(dmflags & DF_SAME_LEVEL) &&
		!(level.flags & LEVEL_CHANGEMAPCHEAT))
	{
		wminfo.next = level.MapName;
		wminfo.LName1 = wminfo.LName0;
	}
	else
	{
		level_info_t *nextinfo = FindLevelInfo (nextlevel, false);
		if (nextinfo == NULL || strncmp (nextlevel, "enDSeQ", 6) == 0)
		{
			wminfo.next = nextlevel;
			wminfo.LName1.SetInvalid();
		}
		else
		{
			wminfo.next = nextinfo->MapName;
			wminfo.LName1 = TexMan.CheckForTexture(nextinfo->PName, FTexture::TEX_MiscPatch);
		}
	}

	CheckWarpTransMap (wminfo.next, true);
	nextlevel = wminfo.next;

	wminfo.next_ep = FindLevelInfo (wminfo.next)->cluster - 1;
	wminfo.maxkills = level.total_monsters;
	wminfo.maxitems = level.total_items;
	wminfo.maxsecret = level.total_secrets;
	wminfo.maxfrags = 0;
	wminfo.partime = TICRATE * level.partime;
	wminfo.sucktime = level.sucktime;
	wminfo.pnum = consoleplayer;
	wminfo.totaltime = level.totaltime;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		wminfo.plyr[i].skills = players[i].killcount;
		wminfo.plyr[i].sitems = players[i].itemcount;
		wminfo.plyr[i].ssecret = players[i].secretcount;
		wminfo.plyr[i].stime = level.time;
		memcpy (wminfo.plyr[i].frags, players[i].frags
				, sizeof(wminfo.plyr[i].frags));
		wminfo.plyr[i].fragcount = players[i].fragcount;
	}

	// [RH] If we're in a hub and staying within that hub, take a snapshot
	//		of the level. If we're traveling to a new hub, take stuff from
	//		the player and clear the world vars. If this is just an
	//		ordinary cluster (not a hub), take stuff from the player, but
	//		leave the world vars alone.
	cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
	cluster_info_t *nextcluster = FindClusterInfo (wminfo.next_ep+1);	// next_ep is cluster-1
	EFinishLevelType mode;

	if (thiscluster != nextcluster || deathmatch ||
		!(thiscluster->flags & CLUSTER_HUB))
	{
		if (nextcluster->flags & CLUSTER_HUB)
		{
			mode = FINISH_NextHub;
		}
		else
		{
			mode = FINISH_NoHub;
		}
	}
	else
	{
		mode = FINISH_SameHub;
	}

	// Intermission stats for entire hubs
	G_LeavingHub(mode, thiscluster, &wminfo);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{ // take away appropriate inventory
			G_PlayerFinishLevel (i, mode, changeflags);
		}
	}

	if (mode == FINISH_SameHub)
	{ // Remember the level's state for re-entry.
		if (!(level.flags2 & LEVEL2_FORGETSTATE))
		{
			G_SnapshotLevel ();
			// Do not free any global strings this level might reference
			// while it's not loaded.
			FBehavior::StaticLockLevelVarStrings();
		}
		else
		{ // Make sure we don't have a snapshot lying around from before.
			level.info->Snapshot.Clean();
		}
	}
	else
	{ // Forget the states of all existing levels.
		G_ClearSnapshots ();

		if (mode == FINISH_NextHub)
		{ // Reset world variables for the new hub.
			P_ClearACSVars(false);
		}
		level.time = 0;
		level.maptime = 0;
	}

	finishstate = mode;

	if (!deathmatch &&
		((level.flags & LEVEL_NOINTERMISSION) ||
		((nextcluster == thiscluster) && (thiscluster->flags & CLUSTER_HUB) && !(thiscluster->flags & CLUSTER_ALLOWINTERMISSION))))
	{
		G_WorldDone ();
		return;
	}

	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

// [RH] If you ever get a statistics driver operational, adapt this.
//	if (statcopy)
//		memcpy (statcopy, &wminfo, sizeof(wminfo));

	WI_Start (&wminfo);
}

//==========================================================================
//
//
//==========================================================================

class DAutosaver : public DThinker
{
	DECLARE_CLASS (DAutosaver, DThinker)
public:
	void Tick ();
};

IMPLEMENT_CLASS(DAutosaver, false, false)

void DAutosaver::Tick ()
{
	Net_WriteByte (DEM_CHECKAUTOSAVE);
	Destroy ();
}

//==========================================================================
//
// G_DoLoadLevel 
//
//==========================================================================

extern gamestate_t 	wipegamestate; 
 
void G_DoLoadLevel (int position, bool autosave)
{ 
	static int lastposition = 0;
	gamestate_t oldgs = gamestate;
	int i;

	if (NextSkill >= 0)
	{
		UCVarValue val;
		val.Int = NextSkill;
		gameskill.ForceSet (val, CVAR_Int);
		NextSkill = -1;
	}

	if (position == -1)
		position = lastposition;
	else
		lastposition = position;

	G_InitLevelLocals ();
	StatusBar->DetachAllMessages ();

	// Force 'teamplay' to 'true' if need be.
	if (level.flags2 & LEVEL2_FORCETEAMPLAYON)
		teamplay = true;

	// Force 'teamplay' to 'false' if need be.
	if (level.flags2 & LEVEL2_FORCETEAMPLAYOFF)
		teamplay = false;

	FString mapname = level.MapName;
	mapname.ToLower();
	Printf (
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"
			TEXTCOLOR_BOLD "%s - %s\n\n",
			mapname.GetChars(), level.LevelName.GetChars());

	if (wipegamestate == GS_LEVEL)
		wipegamestate = GS_FORCEWIPE;

	if (gamestate != GS_TITLELEVEL)
	{
		gamestate = GS_LEVEL; 
	}

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = TexMan.GetTexture (gameinfo.SkyFlatName, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

	// DOOM determines the sky texture to be used
	// depending on the current episode and the game version.
	// [RH] Fetch sky parameters from FLevelLocals.
	sky1texture = level.skytexture1;
	sky2texture = level.skytexture2;

	// [RH] Set up details about sky rendering
	R_InitSkyMap ();

	for (i = 0; i < MAXPLAYERS; i++)
	{ 
		if (playeringame[i] && (deathmatch || players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_ENTER;	// [BC]
		memset (players[i].frags,0,sizeof(players[i].frags));
		if (!(dmflags2 & DF2_YES_KEEPFRAGS) && (alwaysapplydmflags || deathmatch))
			players[i].fragcount = 0;
	}

	if (changeflags & CHANGELEVEL_NOMONSTERS)
	{
		level.flags2 |= LEVEL2_NOMONSTERS;
	}
	else
	{
		level.flags2 &= ~LEVEL2_NOMONSTERS;
	}
	if (changeflags & CHANGELEVEL_PRERAISEWEAPON)
	{
		level.flags2 |= LEVEL2_PRERAISEWEAPON;
	}

	level.maptime = 0;

	P_SetupLevel (level.MapName, position);

	AM_LevelInit();

	// [RH] Start lightning, if MAPINFO tells us to
	if (level.flags & LEVEL_STARTLIGHTNING)
	{
		P_StartLightning ();
	}

	gameaction = ga_nothing; 

	// clear cmd building stuff
	ResetButtonStates ();

	SendItemUse = NULL;
	SendItemDrop = NULL;
	mousex = mousey = 0; 
	sendpause = sendsave = sendturn180 = SendLand = false;
	LocalViewAngle = 0;
	LocalViewPitch = 0;
	paused = 0;

	//Added by MC: Initialize bots.
	if (deathmatch)
	{
		bglobal.Init ();
	}

	if (timingdemo)
	{
		static bool firstTime = true;

		if (firstTime)
		{
			starttime = I_GetTime ();
			firstTime = false;
		}
	}

	level.starttime = gametic;

	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	int pnumerr = G_FinishTravel ();
	// For each player, if they are viewing through a player, make sure it is themselves.
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (playeringame[ii])
		{
			if (players[ii].camera == NULL || players[ii].camera->player != NULL)
			{
				players[ii].camera = players[ii].mo;
			}

			if (savegamerestore)
			{
				continue;
			}

			const bool fromSnapshot = level.FromSnapshot;
			E_PlayerEntered(ii, fromSnapshot && finishstate == FINISH_SameHub);

			if (fromSnapshot)
			{
				// ENTER scripts are being handled when the player gets spawned, this cannot be changed due to its effect on voodoo dolls.
				FBehavior::StaticStartTypedScripts(SCRIPT_Return, players[ii].mo, true);
			}
		}
	}

	if (level.FromSnapshot)
	{
		// [Nash] run REOPEN scripts upon map re-entry
		FBehavior::StaticStartTypedScripts(SCRIPT_Reopen, NULL, false);
	}

	StatusBar->AttachToPlayer (&players[consoleplayer]);
	//      unsafe world load
	E_WorldLoadedUnsafe();
	//      regular world load (savegames are handled internally)
	E_WorldLoaded();
	P_DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.
	
	if (demoplayback || oldgs == GS_STARTUP || oldgs == GS_TITLELEVEL)
		C_HideConsole ();

	C_FlushDisplay ();

	// [RH] Always save the game when entering a new level.
	if (autosave && !savegamerestore && disableautosave < 1)
	{
		DAutosaver GCCNOWARN *dummy = Create<DAutosaver>();
	}
	if (pnumerr > 0)
	{
		I_Error("no start for player %d found.", pnumerr);
	}
}


//==========================================================================
//
// G_WorldDone 
//
//==========================================================================

void G_WorldDone (void) 
{ 
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;

	gameaction = ga_worlddone; 

	if (level.flags & LEVEL_CHANGEMAPCHEAT)
		return;

	thiscluster = FindClusterInfo (level.cluster);

	if (strncmp (nextlevel, "enDSeQ", 6) == 0)
	{
		FName endsequence = ENamedName(strtoll(nextlevel.GetChars()+6, NULL, 16));
		// Strife needs a special case here to choose between good and sad ending. Bad is handled elsewhere.
		if (endsequence == NAME_Inter_Strife)
		{
			if (players[0].mo->FindInventory (NAME_QuestItem25) ||
				players[0].mo->FindInventory (NAME_QuestItem28))
			{
				endsequence = NAME_Inter_Strife_Good;
			}
			else
			{
				endsequence = NAME_Inter_Strife_Sad;
			}
		}

		auto ext = level.info->ExitMapTexts.CheckKey(level.flags3 & LEVEL3_EXITSECRETUSED ? NAME_Secret : NAME_Normal);
		if (ext != nullptr && (ext->mDefined & FExitText::DEF_TEXT))
		{
			F_StartFinale(ext->mDefined & FExitText::DEF_MUSIC ? ext->mMusic : gameinfo.finaleMusic,
				ext->mDefined & FExitText::DEF_MUSIC ? ext->mOrder : gameinfo.finaleOrder,
				-1, 0,
				ext->mDefined & FExitText::DEF_BACKDROP ? ext->mBackdrop : gameinfo.FinaleFlat,
				ext->mText,
				false,
				ext->mDefined & FExitText::DEF_PIC,
				ext->mDefined & FExitText::DEF_LOOKUP,
				true, endsequence);
		}
		else
		{
			F_StartFinale(thiscluster->MessageMusic, thiscluster->musicorder,
				thiscluster->cdtrack, thiscluster->cdid,
				thiscluster->FinaleFlat, thiscluster->ExitText,
				thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
				thiscluster->flags & CLUSTER_FINALEPIC,
				thiscluster->flags & CLUSTER_LOOKUPEXITTEXT,
				true, endsequence);
		}
	}
	else
	{
		FExitText *ext = nullptr;
		
		if (level.flags3 & LEVEL3_EXITSECRETUSED) ext = level.info->ExitMapTexts.CheckKey(NAME_Secret);
		else if (level.flags3 & LEVEL3_EXITNORMALUSED) ext = level.info->ExitMapTexts.CheckKey(NAME_Normal);
		if (ext == nullptr) ext = level.info->ExitMapTexts.CheckKey(nextlevel);

		if (ext != nullptr)
		{
			if ((ext->mDefined & FExitText::DEF_TEXT))
			{
				F_StartFinale(ext->mDefined & FExitText::DEF_MUSIC ? ext->mMusic : gameinfo.finaleMusic,
					ext->mDefined & FExitText::DEF_MUSIC ? ext->mOrder : gameinfo.finaleOrder,
					-1, 0,
					ext->mDefined & FExitText::DEF_BACKDROP ? ext->mBackdrop : gameinfo.FinaleFlat,
					ext->mText,
					false,
					ext->mDefined & FExitText::DEF_PIC,
					ext->mDefined & FExitText::DEF_LOOKUP,
					false);
			}
			return;
		}

		nextcluster = FindClusterInfo (FindLevelInfo (nextlevel)->cluster);

		if (nextcluster->cluster != level.cluster && !deathmatch)
		{
			// Only start the finale if the next level's cluster is different
			// than the current one and we're not in deathmatch.
			if (nextcluster->EnterText.IsNotEmpty())
			{
				F_StartFinale (nextcluster->MessageMusic, nextcluster->musicorder,
					nextcluster->cdtrack, nextcluster->cdid,
					nextcluster->FinaleFlat, nextcluster->EnterText,
					nextcluster->flags & CLUSTER_ENTERTEXTINLUMP,
					nextcluster->flags & CLUSTER_FINALEPIC,
					nextcluster->flags & CLUSTER_LOOKUPENTERTEXT,
					false);
			}
			else if (thiscluster->ExitText.IsNotEmpty())
			{
				F_StartFinale (thiscluster->MessageMusic, thiscluster->musicorder,
					thiscluster->cdtrack, nextcluster->cdid,
					thiscluster->FinaleFlat, thiscluster->ExitText,
					thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
					thiscluster->flags & CLUSTER_FINALEPIC,
					thiscluster->flags & CLUSTER_LOOKUPEXITTEXT,
					false);
			}
		}
	}
} 
 
DEFINE_ACTION_FUNCTION(FLevelLocals, WorldDone)
{
	G_WorldDone();
	return 0;
}

//==========================================================================
//
//
//==========================================================================

void G_DoWorldDone (void) 
{		 
	gamestate = GS_LEVEL;
	if (wminfo.next[0] == 0)
	{
		// Don't crash if no next map is given. Just repeat the current one.
		Printf ("No next map specified.\n");
	}
	else
	{
		level.MapName = nextlevel;
	}
	G_StartTravel ();
	G_DoLoadLevel (startpos, true);
	startpos = 0;
	gameaction = ga_nothing;
	viewactive = true; 
}

//==========================================================================
//
// G_StartTravel
//
// Moves players (and eventually their inventory) to a different statnum,
// so they will not be destroyed when switching levels. This only applies
// to real players, not voodoo dolls.
//
//==========================================================================

void G_StartTravel ()
{
	if (deathmatch)
		return;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AActor *pawn = players[i].mo;
			AInventory *inv;
			players[i].camera = NULL;

			// Only living players travel. Dead ones get a new body on the new level.
			if (players[i].health > 0)
			{
				pawn->UnlinkFromWorld (nullptr);
				int tid = pawn->tid;	// Save TID
				pawn->RemoveFromHash ();
				pawn->tid = tid;		// Restore TID (but no longer linked into the hash chain)
				pawn->ChangeStatNum (STAT_TRAVELLING);

				for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
				{
					inv->ChangeStatNum (STAT_TRAVELLING);
					inv->UnlinkFromWorld (nullptr);
				}
			}
		}
	}

	bglobal.StartTravel ();
}

//==========================================================================
//
// G_FinishTravel
//
// Moves any travelling players so that they occupy their newly-spawned
// copies' locations, destroying the new players in the process (because
// they are really fake placeholders to show where the travelling players
// should go).
//
//==========================================================================

int G_FinishTravel ()
{
	TThinkerIterator<APlayerPawn> it (STAT_TRAVELLING);
	APlayerPawn *pawn, *pawndup, *oldpawn, *next;
	AInventory *inv;
	FPlayerStart *start;
	int pnum;
	int failnum = 0;

	// 
	APlayerPawn* pawns[MAXPLAYERS];
	int pawnsnum = 0;

	next = it.Next ();
	while ( (pawn = next) != NULL)
	{
		next = it.Next ();
		pnum = int(pawn->player - players);
		pawn->ChangeStatNum (STAT_PLAYER);
		pawndup = pawn->player->mo;
		assert (pawn != pawndup);

		start = G_PickPlayerStart(pnum, 0);
		if (start == NULL)
		{
			if (pawndup != nullptr)
			{
				Printf(TEXTCOLOR_RED "No player %d start to travel to!\n", pnum + 1);
				// Move to the coordinates this player had when they left the level.
				pawn->SetXYZ(pawndup->Pos());
			}
			else
			{
				// Could not find a start for this player at all. This really should never happen but if it does, let's better abort.
				if (failnum == 0) failnum = pnum + 1;
			}
		}
		oldpawn = pawndup;

		// The player being spawned here is a short lived dummy and
		// must not start any ENTER script or big problems will happen.
		pawndup = P_SpawnPlayer(start, pnum, SPF_TEMPPLAYER);
		if (pawndup != NULL)
		{
			if (!(changeflags & CHANGELEVEL_KEEPFACING))
			{
				pawn->Angles = pawndup->Angles;
			}
			pawn->SetXYZ(pawndup->Pos());
			pawn->Vel = pawndup->Vel;
			pawn->Sector = pawndup->Sector;
			pawn->floorz = pawndup->floorz;
			pawn->ceilingz = pawndup->ceilingz;
			pawn->dropoffz = pawndup->dropoffz;
			pawn->floorsector = pawndup->floorsector;
			pawn->floorpic = pawndup->floorpic;
			pawn->floorterrain = pawndup->floorterrain;
			pawn->ceilingsector = pawndup->ceilingsector;
			pawn->ceilingpic = pawndup->ceilingpic;
			pawn->Floorclip = pawndup->Floorclip;
			pawn->waterlevel = pawndup->waterlevel;
		}
		else if (failnum == 0)	// In the failure case this may run into some undefined data.
		{
			P_FindFloorCeiling(pawn);
		}
		pawn->target = NULL;
		pawn->lastenemy = NULL;
		pawn->player->mo = pawn;
		pawn->player->camera = pawn;
		pawn->player->viewheight = pawn->ViewHeight;
		pawn->flags2 &= ~MF2_BLASTED;
		if (oldpawn != nullptr)
		{
			DObject::StaticPointerSubstitution (oldpawn, pawn);
			oldpawn->Destroy();
		}
		if (pawndup != NULL)
		{
			pawndup->Destroy();
		}
		pawn->LinkToWorld (nullptr);
		pawn->ClearInterpolation();
		pawn->AddToHash ();
		pawn->SetState(pawn->SpawnState);
		pawn->player->SendPitchLimits();

		for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
		{
			inv->ChangeStatNum (STAT_INVENTORY);
			inv->LinkToWorld (nullptr);

			IFVIRTUALPTR(inv, AInventory, Travelled)
			{
				VMValue params[1] = { inv };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		if (ib_compatflags & BCOMPATF_RESETPLAYERSPEED)
		{
			pawn->Speed = pawn->GetDefault()->Speed;
		}
		// [ZZ] we probably don't want to fire any scripts before all players are in, especially with runNow = true.
		pawns[pawnsnum++] = pawn;
	}

	bglobal.FinishTravel ();

	// make sure that, after travelling has completed, no travelling thinkers are left.
	// Since this list is excluded from regular thinker cleaning, anything that may survive through here
	// will endlessly multiply and severely break the following savegames or just simply crash on broken pointers.
	DThinker::DestroyThinkersInList(STAT_TRAVELLING);
	return failnum;
}
 
//==========================================================================
//
//
//==========================================================================

void G_InitLevelLocals ()
{
	level_info_t *info;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any

	level.gravity = sv_gravity * 35/TICRATE;
	level.aircontrol = sv_aircontrol;
	level.teamdamage = teamdamage;
	level.flags = 0;
	level.flags2 = 0;
	level.flags3 = 0;

	info = FindLevelInfo (level.MapName);

	level.info = info;
	level.skyspeed1 = info->skyspeed1;
	level.skyspeed2 = info->skyspeed2;
	level.skytexture1 = TexMan.GetTexture(info->SkyPic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	level.skytexture2 = TexMan.GetTexture(info->SkyPic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	level.fadeto = info->fadeto;
	level.cdtrack = info->cdtrack;
	level.cdid = info->cdid;
	level.FromSnapshot = false;
	if (level.fadeto == 0)
	{
		if (strnicmp (info->FadeTable, "COLORMAP", 8) != 0)
		{
			level.flags |= LEVEL_HASFADETABLE;
		}
	}
	level.airsupply = info->airsupply*TICRATE;
	level.outsidefog = info->outsidefog;
	level.WallVertLight = info->WallVertLight*2;
	level.WallHorizLight = info->WallHorizLight*2;
	if (info->gravity != 0.f)
	{
		level.gravity = info->gravity * 35/TICRATE;
	}
	if (info->aircontrol != 0.f)
	{
		level.aircontrol = info->aircontrol;
	}
	if (info->teamdamage != 0.f)
	{
		level.teamdamage = info->teamdamage;
	}

	G_AirControlChanged ();

	cluster_info_t *clus = FindClusterInfo (info->cluster);

	level.partime = info->partime;
	level.sucktime = info->sucktime;
	level.cluster = info->cluster;
	level.clusterflags = clus ? clus->flags : 0;
	level.flags |= info->flags;
	level.flags2 |= info->flags2;
	level.flags3 |= info->flags3;
	level.levelnum = info->levelnum;
	level.Music = info->Music;
	level.musicorder = info->musicorder;

	level.LevelName = level.info->LookupLevelName();
	level.NextMap = info->NextMap;
	level.NextSecretMap = info->NextSecretMap;
	level.F1Pic = info->F1Pic;
	level.hazardcolor = info->hazardcolor;
	level.hazardflash = info->hazardflash;
	
	// GL fog stuff modifiable by SetGlobalFogParameter.
	level.fogdensity = info->fogdensity;
	level.outsidefogdensity = info->outsidefogdensity;
	level.skyfog = info->skyfog;

	level.pixelstretch = info->pixelstretch;

	compatflags.Callback();
	compatflags2.Callback();

	level.DefaultEnvironment = info->DefaultEnvironment;
}

//==========================================================================
//
//
//==========================================================================

bool FLevelLocals::IsJumpingAllowed() const
{
	if (dmflags & DF_NO_JUMP)
		return false;
	if (dmflags & DF_YES_JUMP)
		return true;
	return !(flags & LEVEL_JUMP_NO);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, IsJumpingAllowed)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	ACTION_RETURN_BOOL(self->IsJumpingAllowed());
}


//==========================================================================
//
//
//==========================================================================

bool FLevelLocals::IsCrouchingAllowed() const
{
	if (dmflags & DF_NO_CROUCH)
		return false;
	if (dmflags & DF_YES_CROUCH)
		return true;
	return !(flags & LEVEL_CROUCH_NO);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, IsCrouchingAllowed)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	ACTION_RETURN_BOOL(self->IsCrouchingAllowed());
}

//==========================================================================
//
//
//==========================================================================

bool FLevelLocals::IsFreelookAllowed() const
{
	if (dmflags & DF_NO_FREELOOK)
		return false;
	if (dmflags & DF_YES_FREELOOK)
		return true;
	return !(flags & LEVEL_FREELOOK_NO);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, IsFreelookAllowed)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	ACTION_RETURN_BOOL(self->IsFreelookAllowed());
}


//==========================================================================
//
//
//==========================================================================

FString CalcMapName (int episode, int level)
{
	FString lumpname;

	if (gameinfo.flags & GI_MAPxx)
	{
		lumpname.Format("MAP%02d", level);
	}
	else
	{
		lumpname = "";
		lumpname << 'E' << ('0' + episode) << 'M' << ('0' + level);
	}
	return lumpname;
}

//==========================================================================
//
//
//==========================================================================

void G_AirControlChanged ()
{
	if (level.aircontrol <= 1/256.)
	{
		level.airfriction = 1.;
	}
	else
	{
		// Friction is inversely proportional to the amount of control
		level.airfriction = level.aircontrol * -0.0941 + 1.0004;
	}
}

//==========================================================================
//
// Archives the current level
//
//==========================================================================

void G_SnapshotLevel ()
{
	level.info->Snapshot.Clean();

	if (level.info->isValid())
	{
		FSerializer arc;

		if (arc.OpenWriter(save_formatted))
		{
			SaveVersion = SAVEVER;
			G_SerializeLevel(arc, false);
			level.info->Snapshot = arc.GetCompressedOutput();
		}
	}
}

//==========================================================================
//
// Unarchives the current level based on its snapshot
// The level should have already been loaded and setup.
//
//==========================================================================

void G_UnSnapshotLevel (bool hubLoad)
{
	if (level.info->Snapshot.mBuffer == nullptr)
		return;

	if (level.info->isValid())
	{
		FSerializer arc;
		if (!arc.OpenReader(&level.info->Snapshot))
		{
			I_Error("Failed to load savegame");
			return;
		}

		G_SerializeLevel (arc, hubLoad);
		level.FromSnapshot = true;

		TThinkerIterator<APlayerPawn> it;
		APlayerPawn *pawn, *next;

		next = it.Next();
		while ((pawn = next) != 0)
		{
			next = it.Next();
			if (pawn->player == NULL || pawn->player->mo == NULL || !playeringame[pawn->player - players])
			{
				int i;

				// If this isn't the unmorphed original copy of a player, destroy it, because it's extra.
				for (i = 0; i < MAXPLAYERS; ++i)
				{
					if (playeringame[i] && players[i].morphTics && players[i].mo->alternative == pawn)
					{
						break;
					}
				}
				if (i == MAXPLAYERS)
				{
					pawn->Destroy ();
				}
			}
		}
		arc.Close();
	}
	// No reason to keep the snapshot around once the level's been entered.
	level.info->Snapshot.Clean();
	if (hubLoad)
	{
		// Unlock ACS global strings that were locked when the snapshot was made.
		FBehavior::StaticUnlockLevelVarStrings();
	}
}

//==========================================================================
//
//
//==========================================================================

void G_WriteSnapshots(TArray<FString> &filenames, TArray<FCompressedBuffer> &buffers)
{
	unsigned int i;
	FString filename;

	for (i = 0; i < wadlevelinfos.Size(); i++)
	{
		if (wadlevelinfos[i].Snapshot.mCompressedSize > 0)
		{
			filename.Format("%s.map.json", wadlevelinfos[i].MapName.GetChars());
			filename.ToLower();
			filenames.Push(filename);
			buffers.Push(wadlevelinfos[i].Snapshot);
		}
	}
	if (TheDefaultLevelInfo.Snapshot.mCompressedSize > 0)
	{
		filename.Format("%s.mapd.json", TheDefaultLevelInfo.MapName.GetChars());
		filename.ToLower();
		filenames.Push(filename);
		buffers.Push(TheDefaultLevelInfo.Snapshot);
	}
}

//==========================================================================
//
//
//==========================================================================

void G_WriteVisited(FSerializer &arc)
{
	if (arc.BeginArray("visited"))
	{
		// Write out which levels have been visited
		for (auto & wi : wadlevelinfos)
		{
			if (wi.flags & LEVEL_VISITED)
			{
				arc.AddString(nullptr, wi.MapName);
			}
		}
		arc.EndArray();
	}

	// Store player classes to be used when spawning a random class
	if (multiplayer)
	{
		arc.Array("randomclasses", SinglePlayerClass, MAXPLAYERS);
	}

	if (arc.BeginObject("playerclasses"))
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				FString key;
				key.Format("%d", i);
				arc(key, players[i].cls);
			}
		}
		arc.EndObject();
	}
}

//==========================================================================
//
//
//==========================================================================

void G_ReadSnapshots(FResourceFile *resf)
{
	FString MapName;
	level_info_t *i;

	G_ClearSnapshots();

	for (unsigned j = 0; j < resf->LumpCount(); j++)
	{
		FResourceLump * resl = resf->GetLump(j);
		if (resl != nullptr)
		{
			auto ptr = strstr(resl->FullName, ".map.json");
			if (ptr != nullptr)
			{
				ptrdiff_t maplen = ptr - resl->FullName.GetChars();
				FString mapname(resl->FullName.GetChars(), (size_t)maplen);
				i = FindLevelInfo(mapname);
				if (i != nullptr)
				{
					i->Snapshot = resl->GetRawData();
				}
			}
			else
			{
				auto ptr = strstr(resl->FullName, ".mapd.json");
				if (ptr != nullptr)
				{
					ptrdiff_t maplen = ptr - resl->FullName.GetChars();
					FString mapname(resl->FullName.GetChars(), (size_t)maplen);
					TheDefaultLevelInfo.Snapshot = resl->GetRawData();
				}
			}
		}
	}
}

//==========================================================================
//
//
//==========================================================================

void G_ReadVisited(FSerializer &arc)
{
	if (arc.BeginArray("visited"))
	{
		for (int s = arc.ArraySize(); s > 0; s--)
		{
			FString str;
			arc(nullptr, str);
			auto i = FindLevelInfo(str);
			if (i != nullptr) i->flags |= LEVEL_VISITED;
		}
		arc.EndArray();
	}

	arc.Array("randomclasses", SinglePlayerClass, MAXPLAYERS);

	if (arc.BeginObject("playerclasses"))
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			FString key;
			key.Format("%d", i);
			arc(key, players[i].cls);
		}
		arc.EndObject();
	}
}

//==========================================================================
//
//
//==========================================================================

CCMD(listsnapshots)
{
	for (unsigned i = 0; i < wadlevelinfos.Size(); ++i)
	{
		FCompressedBuffer *snapshot = &wadlevelinfos[i].Snapshot;
		if (snapshot->mBuffer != nullptr)
		{
			Printf("%s (%u -> %u bytes)\n", wadlevelinfos[i].MapName.GetChars(), snapshot->mCompressedSize, snapshot->mSize);
		}
	}
}

//==========================================================================
//
//
//==========================================================================

void P_WriteACSDefereds (FSerializer &arc)
{
	bool found = false;

	// only write this stuff if needed
	for (auto &wi : wadlevelinfos)
	{
		if (wi.deferred.Size() > 0)
		{
			found = true;
			break;
		}
	}
	if (found && arc.BeginObject("deferred"))
	{
		for (auto &wi : wadlevelinfos)
		{
			if (wi.deferred.Size() > 0)
			{
				if (wi.deferred.Size() > 0)
				{
					arc(wi.MapName, wi.deferred);
				}
			}
		}
		arc.EndObject();
	}
}

//==========================================================================
//
//
//==========================================================================

void P_ReadACSDefereds (FSerializer &arc)
{
	FString MapName;
	
	P_RemoveDefereds ();

	if (arc.BeginObject("deferred"))
	{
		const char *key;

		while ((key = arc.GetKey()))
		{
			level_info_t *i = FindLevelInfo(key);
			if (i == NULL)
			{
				I_Error("Unknown map '%s' in savegame", key);
			}
			arc(nullptr, i->deferred);
		}
		arc.EndObject();
	}
}


//==========================================================================
//
//
//==========================================================================

void FLevelLocals::Tick ()
{
	// Reset carry sectors
	if (Scrolls.Size() > 0)
	{
		memset (&Scrolls[0], 0, sizeof(Scrolls[0])*Scrolls.Size());
	}
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::AddScroller (int secnum)
{
	if (secnum < 0)
	{
		return;
	}
	if (Scrolls.Size() == 0)
	{
		Scrolls.Resize(sectors.Size());
		memset(&Scrolls[0], 0, sizeof(Scrolls[0])*Scrolls.Size());
	}
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::SetInterMusic(const char *nextmap)
{
	auto mus = level.info->MapInterMusic.CheckKey(nextmap);
	if (mus != nullptr)
		S_ChangeMusic(mus->first, mus->second);
	else if (level.info->InterMusic.IsNotEmpty())
		S_ChangeMusic(level.info->InterMusic, level.info->intermusicorder);
	else
		S_ChangeMusic(gameinfo.intermissionMusic.GetChars(), gameinfo.intermissionOrder);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, SetInterMusic)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_STRING(map);
	self->SetInterMusic(map);
	return 0;
}

//==========================================================================
//
//
//==========================================================================

template <typename T>
inline T VecDiff(const T& v1, const T& v2)
{
	T result = v2 - v1;

	if (level.subsectors.Size() > 0)
	{
		const sector_t *const sec1 = P_PointInSector(v1);
		const sector_t *const sec2 = P_PointInSector(v2);

		if (nullptr != sec1 && nullptr != sec2)
		{
			result += Displacements.getOffset(sec2->PortalGroup, sec1->PortalGroup);
		}
	}

	return result;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, Vec2Diff)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	ACTION_RETURN_VEC2(VecDiff(DVector2(x1, y1), DVector2(x2, y2)));
}

DEFINE_ACTION_FUNCTION(FLevelLocals, Vec3Diff)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(z1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	PARAM_FLOAT(z2);
	ACTION_RETURN_VEC3(VecDiff(DVector3(x1, y1, z1), DVector3(x2, y2, z2)));
}

//==========================================================================
//
//
//
//==========================================================================
DEFINE_GLOBAL(level);
DEFINE_FIELD(FLevelLocals, sectors)
DEFINE_FIELD(FLevelLocals, lines)
DEFINE_FIELD(FLevelLocals, sides)
DEFINE_FIELD(FLevelLocals, vertexes)
DEFINE_FIELD(FLevelLocals, sectorPortals)
DEFINE_FIELD(FLevelLocals, time)
DEFINE_FIELD(FLevelLocals, maptime)
DEFINE_FIELD(FLevelLocals, totaltime)
DEFINE_FIELD(FLevelLocals, starttime)
DEFINE_FIELD(FLevelLocals, partime)
DEFINE_FIELD(FLevelLocals, sucktime)
DEFINE_FIELD(FLevelLocals, cluster)
DEFINE_FIELD(FLevelLocals, clusterflags)
DEFINE_FIELD(FLevelLocals, levelnum)
DEFINE_FIELD(FLevelLocals, LevelName)
DEFINE_FIELD(FLevelLocals, MapName)
DEFINE_FIELD(FLevelLocals, NextMap)
DEFINE_FIELD(FLevelLocals, NextSecretMap)
DEFINE_FIELD(FLevelLocals, F1Pic)
DEFINE_FIELD(FLevelLocals, maptype)
DEFINE_FIELD(FLevelLocals, Music)
DEFINE_FIELD(FLevelLocals, musicorder)
DEFINE_FIELD(FLevelLocals, total_secrets)
DEFINE_FIELD(FLevelLocals, found_secrets)
DEFINE_FIELD(FLevelLocals, total_items)
DEFINE_FIELD(FLevelLocals, found_items)
DEFINE_FIELD(FLevelLocals, total_monsters)
DEFINE_FIELD(FLevelLocals, killed_monsters)
DEFINE_FIELD(FLevelLocals, gravity)
DEFINE_FIELD(FLevelLocals, aircontrol)
DEFINE_FIELD(FLevelLocals, airfriction)
DEFINE_FIELD(FLevelLocals, airsupply)
DEFINE_FIELD(FLevelLocals, teamdamage)
DEFINE_FIELD(FLevelLocals, fogdensity)
DEFINE_FIELD(FLevelLocals, outsidefogdensity)
DEFINE_FIELD(FLevelLocals, skyfog)
DEFINE_FIELD(FLevelLocals, pixelstretch)
DEFINE_FIELD_BIT(FLevelLocals, flags, noinventorybar, LEVEL_NOINVENTORYBAR)
DEFINE_FIELD_BIT(FLevelLocals, flags, monsterstelefrag, LEVEL_MONSTERSTELEFRAG)
DEFINE_FIELD_BIT(FLevelLocals, flags, actownspecial, LEVEL_ACTOWNSPECIAL)
DEFINE_FIELD_BIT(FLevelLocals, flags, sndseqtotalctrl, LEVEL_SNDSEQTOTALCTRL)
DEFINE_FIELD_BIT(FLevelLocals, flags2, allmap, LEVEL2_ALLMAP)
DEFINE_FIELD_BIT(FLevelLocals, flags2, missilesactivateimpact, LEVEL2_MISSILESACTIVATEIMPACT)
DEFINE_FIELD_BIT(FLevelLocals, flags2, monsterfallingdamage, LEVEL2_MONSTERFALLINGDAMAGE)
DEFINE_FIELD_BIT(FLevelLocals, flags2, checkswitchrange, LEVEL2_CHECKSWITCHRANGE)
DEFINE_FIELD_BIT(FLevelLocals, flags2, polygrind, LEVEL2_POLYGRIND)
DEFINE_FIELD_BIT(FLevelLocals, flags2, allowrespawn, LEVEL2_ALLOWRESPAWN)
DEFINE_FIELD_BIT(FLevelLocals, flags2, nomonsters, LEVEL2_NOMONSTERS)
DEFINE_FIELD_BIT(FLevelLocals, flags2, frozen, LEVEL2_FROZEN)
DEFINE_FIELD_BIT(FLevelLocals, flags2, infinite_flight, LEVEL2_INFINITE_FLIGHT)
DEFINE_FIELD_BIT(FLevelLocals, flags2, no_dlg_freeze, LEVEL2_CONV_SINGLE_UNFREEZE)

//==========================================================================
//
// Lists all currently defined maps
//
//==========================================================================

CCMD(listmaps)
{
	for(unsigned i = 0; i < wadlevelinfos.Size(); i++)
	{
		level_info_t *info = &wadlevelinfos[i];
		MapData *map = P_OpenMapData(info->MapName, true);

		if (map != NULL)
		{
			Printf("%s: '%s' (%s)\n", info->MapName.GetChars(), info->LookupLevelName().GetChars(),
				Wads.GetWadName(Wads.GetLumpFile(map->lumpnum)));
			delete map;
		}
	}
}

//==========================================================================
//
// For testing sky fog sheets
//
//==========================================================================
CCMD(skyfog)
{
	if (argv.argc()>1)
	{
		level.skyfog = MAX(0, (int)strtoull(argv[1], NULL, 0));
	}
}

