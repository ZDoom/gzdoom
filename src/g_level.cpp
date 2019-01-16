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
#include "v_video.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "p_saveg.h"
#include "p_acs.h"
#include "d_protocol.h"
#include "v_text.h"
#include "s_sndseq.h"
#include "b_bot.h"
#include "sbar.h"
#include "a_lightning.h"
#include "version.h"
#include "sbarinfo.h"
#include "p_lnspec.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_netinf.h"
#include "menu/menu.h"
#include "a_sharedglobal.h"
#include "r_renderer.h"
#include "r_utility.h"
#include "p_spec.h"
#include "serializer.h"
#include "vm.h"
#include "events.h"
#include "i_music.h"
#include "a_dynlight.h"
#include "p_conversation.h"
#include "p_effect.h"

#include "gi.h"

#include "g_levellocals.h"
#include "actorinlines.h"
#include "i_time.h"
#include "p_maputl.h"

// Compatibility glue to emulate removed features.
FLevelLocals emptyLevelPlaceholderForZScript;
FLevelLocals *levelForZScript = &emptyLevelPlaceholderForZScript;
bool globalfreeze;
DEFINE_GLOBAL(globalfreeze);
DEFINE_GLOBAL_NAMED(levelForZScript, level);

DEFINE_ACTION_FUNCTION(FGameSession, __GetCompatibilityLevel)
{
	ACTION_RETURN_POINTER(levelForZScript);
}


void STAT_StartNewGame(TArray<OneLevel> &LevelData, const char *lev);
void STAT_ChangeLevel(TArray<OneLevel> &LevelData, const char *newl, FLevelLocals *Level);
void STAT_Serialize(TArray<OneLevel> &LevelData, FSerializer &file);

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

CUSTOM_CVAR(Bool, gl_brightfog, false, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	ForAllLevels([&](FLevelLocals *Level)
	{
		if (Level->info == nullptr || Level->info->brightfog == -1) Level->brightfog = self;
	});
}

CUSTOM_CVAR(Bool, gl_lightadditivesurfaces, false, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	ForAllLevels([&](FLevelLocals *Level)
	{
		if (Level->info == nullptr || Level->info->lightadditivesurfaces == -1) Level->lightadditivesurfaces = self;
	});
}

CUSTOM_CVAR(Bool, gl_notexturefill, false, CVAR_NOINITCALL)
{
	ForAllLevels([&](FLevelLocals *Level)
	{
		if (Level->info == nullptr || Level->info->notexturefill == -1) Level->notexturefill = self;
	});
}

CUSTOM_CVAR(Int, gl_lightmode, 3, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	int newself = self;
	if (newself > 8) newself = 16;	// use 8 and 16 for software lighting to avoid conflicts with the bit mask
	else if (newself > 4) newself = 8;
	else if (newself < 0) newself = 0;
	if (self != newself) self = newself;	// This recursively calls this handler again.
	else ForAllLevels([&](FLevelLocals *Level)
	{
		if ((Level->info == nullptr || Level->info->lightmode == ELightMode::NotSet)) Level->lightMode = (ELightMode)*self;
	});
}



static FRandom pr_classchoice ("RandomPlayerClassChoice");

extern bool timingdemo;

// Start time for timing demos
int starttime;


extern FString BackupSaveName;

bool savegamerestore;
int finishstate = FINISH_NoHub;

extern int mousex, mousey;
extern bool sendpause, sendsave, sendturn180, SendLand;

void *statcopy;					// for statistics driver

FGameSession *currentSession = nullptr;

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
		if (!strcmp(mapname, "*"))
		{
			if (who == nullptr)
			{
				Printf("Player is not in a level that can be restarted.\n");
				return;
			}
			mapname = who->Level->MapName.GetChars();
		}

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

UNSAFE_CCMD(recordmap)
{
	if (netgame)
	{
		Printf("You cannot record a new game while in a netgame.\n");
		return;
	}
	if (argv.argc() > 2)
	{
		const char *mapname = argv[2];
		
		if (!strcmp(mapname, "*"))
		{
			if (who == nullptr)
			{
				Printf("Player is not in a level that can be restarted.\n");
				return;
			}
			mapname = who->Level->MapName.GetChars();
		}

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

UNSAFE_CCMD (open)
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

	ForAllLevels([](FLevelLocals *Level)
	{
		// Destory all old player refrences that may still exist
		TThinkerIterator<AActor> it(Level, NAME_PlayerPawn, STAT_TRAVELLING);
		AActor *pawn, *next;
		
		next = it.Next();
		while ((pawn = next) != NULL)
		{
			next = it.Next();
			pawn->flags |= MF_NOSECTOR | MF_NOBLOCKMAP;
			pawn->Destroy();
		}
	});

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
		saved_ui.TransferFrom(p->userinfo);
		const int chasecam = p->cheats & CF_CHASECAM;
		const bool settings_controller = p->settings_controller;
		p->~player_t();
		::new(p) player_t;
		p->settings_controller = settings_controller;
		p->cheats |= chasecam;
		p->playerstate = PST_DEAD;
		p->userinfo.TransferFrom(saved_ui);
		playeringame[i] = false;
	}
	BackupSaveName = "";
	consoleplayer = 0;
	currentSession->NextSkill = -1;
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

void FGameSession::InitPlayerClasses ()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		SinglePlayerClass[i] = players[i].userinfo.GetPlayerClassNum();
		if (SinglePlayerClass[i] < 0 || !playeringame[i])
		{
			SinglePlayerClass[i] = (pr_classchoice()) % PlayerClasses.Size ();
		}
		players[i].cls = nullptr;
		players[i].CurrentPlayerClass = SinglePlayerClass[i];
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
	if (currentSession->Levelinfo.Size())
		E_WorldUnloadedUnsafe();

	if (!savegamerestore)
	{
		currentSession->Reset();
	}

	UnlatchCVars ();
	G_VerifySkill();
	UnlatchCVars ();
	Thinkers.DestroyThinkersInList(STAT_STATIC);

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
		currentSession->time = 0;
		currentSession->totaltime = 0;

		if (!multiplayer || !deathmatch)
		{
			currentSession->InitPlayerClasses ();
		}

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_ENTER;	// [BC]

		STAT_StartNewGame(currentSession->Statistics, mapname);
	}

	usergame = !bTitleLevel;		// will be set false if a demo
	paused = 0;
	demoplayback = false;
	automapactive = false;
	viewactive = true;

	//Added by MC: Initialize bots.
	if (!deathmatch)
	{
		bglobal.Init ();
	}

	if (bTitleLevel)
	{
		gamestate = GS_TITLELEVEL;
	}
	else if (gamestate != GS_STARTUP)
	{
		gamestate = GS_LEVEL;
	}
	
	G_DoLoadLevel (mapname, 0, false, !savegamerestore);
}

//
// G_DoCompleted
//
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

void G_ChangeLevel(FLevelLocals *OldLevel, const char *levelname, int position, int flags, int nextSkill)
{
	level_info_t *nextinfo = NULL;

	if (OldLevel != currentSession->Levelinfo[0])
	{
		// Level exit must be initiated from the primary level.
		return;
	}
	if (unloading)
	{
		Printf (TEXTCOLOR_RED "Unloading scripts cannot exit the level again.\n");
		return;
	}
	if (gameaction == ga_completed && !(currentSession->Levelinfo[0]->i_compatflags2 & COMPATF2_MULTIEXIT))	// do not exit multiple times.
	{
		return;
	}

	FString nextlevel;
	if (levelname == nullptr || *levelname == 0)
	{
		// end the game
		levelname = nullptr;
		if (!OldLevel->NextMap.Compare("enDSeQ",6))
		{
			nextlevel = OldLevel->NextMap;	// If there is already an end sequence please leave it alone!
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
		currentSession->NextSkill = nextSkill;

	if (flags & CHANGELEVEL_NOINTERMISSION)
	{
		OldLevel->flags |= LEVEL_NOINTERMISSION;
	}

	cluster_info_t *thiscluster = FindClusterInfo (OldLevel->cluster);
	cluster_info_t *nextcluster = nextinfo? FindClusterInfo (nextinfo->cluster) : NULL;

	currentSession->nextlevel = nextlevel;
	currentSession->nextstartpos = position;
	gameaction = ga_completed;
	currentSession->SetMusicVolume(1.0);
		
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
	OldLevel->Behaviors.StartTypedScripts (OldLevel, SCRIPT_Unloading, NULL, false, 0, true);
	// [ZZ] safe world unload
	E_WorldUnloaded();
	// [ZZ] unsafe world unload (changemap != map)
	E_WorldUnloadedUnsafe();
	unloading = false;

	STAT_ChangeLevel(currentSession->Statistics, nextlevel, OldLevel);

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if ((OldLevel->flags & LEVEL_NOINTERMISSION) || ((nextcluster == thiscluster) && !(thiscluster->flags & CLUSTER_ALLOWINTERMISSION)))
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
			if ((multiplayer || OldLevel->flags2 & LEVEL2_ALLOWRESPAWN || sv_singleplayerrespawn || !!G_SkillProperty(SKILLP_PlayerRespawn))
				&& !deathmatch && player->playerstate == PST_DEAD)
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

const char *G_GetExitMap(FLevelLocals *Level)
{
	return Level->NextMap;
}

const char *G_GetSecretExitMap(FLevelLocals *Level)
{
	const char *nextmap = Level->NextMap;

	if (Level->NextSecretMap.Len() > 0)
	{
		if (P_CheckMapData(Level->NextSecretMap))
		{
			nextmap = Level->NextSecretMap;
		}
	}
	return nextmap;
}

//==========================================================================
//
// The flags here must always be on the primary map.
//
//==========================================================================

void G_ExitLevel (FLevelLocals *Level, int position, bool keepFacing)
{
	Level->flags3 |= LEVEL3_EXITNORMALUSED;
	G_ChangeLevel(Level, G_GetExitMap(Level), position, keepFacing ? CHANGELEVEL_KEEPFACING : 0, -1);
}

void G_SecretExitLevel (FLevelLocals *Level, int position)
{
	Level->flags3 |= LEVEL3_EXITSECRETUSED;
	G_ChangeLevel(Level, G_GetSecretExitMap(Level), position, 0, -1);
}

//==========================================================================
//
//
//==========================================================================

void G_DoCompleted ()
{
	int i;
	auto Level = currentSession->Levelinfo[0];

	gameaction = ga_nothing;

	if (   gamestate == GS_DEMOSCREEN
		|| gamestate == GS_FULLCONSOLE
		|| gamestate == GS_STARTUP)
	{
		return;
	}

	if (gamestate == GS_TITLELEVEL)
	{
		G_DoLoadLevel (currentSession->nextlevel, currentSession->nextstartpos, false, false);
		currentSession->nextlevel = "";
		currentSession->nextstartpos = 0;
		viewactive = true;
		return;
	}

	// [RH] Mark this level as having been visited
	if (!(Level->flags & LEVEL_CHANGEMAPCHEAT))
	{
		currentSession->Visited.Insert(Level->MapName, true);
	}

	if (automapactive)
		AM_Stop ();
	
	// Close the conversation menu if open.
	P_FreeStrifeConversations ();

	wminfo.finished_ep = Level->cluster - 1;
	wminfo.LName0 = TexMan.CheckForTexture(Level->info->PName, ETextureType::MiscPatch);
	wminfo.current = Level->MapName;

	if (deathmatch &&
		(dmflags & DF_SAME_LEVEL) &&
		!(Level->flags & LEVEL_CHANGEMAPCHEAT))
	{
		wminfo.next = Level->MapName;
		wminfo.LName1 = wminfo.LName0;
	}
	else
	{
		level_info_t *nextinfo = FindLevelInfo (currentSession->nextlevel, false);
		if (nextinfo == NULL || strncmp (currentSession->nextlevel, "enDSeQ", 6) == 0)
		{
			wminfo.next = currentSession->nextlevel;
			wminfo.LName1.SetInvalid();
		}
		else
		{
			wminfo.next = nextinfo->MapName;
			wminfo.LName1 = TexMan.CheckForTexture(nextinfo->PName, ETextureType::MiscPatch);
		}
	}

	CheckWarpTransMap (wminfo.next, true);
	currentSession->nextlevel = wminfo.next;

	wminfo.next_ep = FindLevelInfo (wminfo.next)->cluster - 1;
	wminfo.maxkills = Level->total_monsters;
	wminfo.maxitems = Level->total_items;
	wminfo.maxsecret = Level->total_secrets;
	wminfo.maxfrags = 0;
	wminfo.partime = TICRATE * Level->partime;
	wminfo.sucktime = Level->sucktime;
	wminfo.pnum = consoleplayer;
	wminfo.totaltime = currentSession->totaltime;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		wminfo.plyr[i].skills = players[i].killcount;
		wminfo.plyr[i].sitems = players[i].itemcount;
		wminfo.plyr[i].ssecret = players[i].secretcount;
		wminfo.plyr[i].stime = currentSession->time;
		memcpy (wminfo.plyr[i].frags, players[i].frags
				, sizeof(wminfo.plyr[i].frags));
		wminfo.plyr[i].fragcount = players[i].fragcount;
	}

	// [RH] If we're in a hub and staying within that hub, take a snapshot
	//		of the map. If we're traveling to a new hub, take stuff from
	//		the player and clear the world vars. If this is just an
	//		ordinary cluster (not a hub), take stuff from the player, but
	//		leave the world vars alone.
	cluster_info_t *thiscluster = FindClusterInfo (Level->cluster);
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
	currentSession->LeavingHub(mode, thiscluster, &wminfo, Level);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{ // take away appropriate inventory
			G_PlayerFinishLevel (i, mode, changeflags);
		}
	}

	if (mode == FINISH_SameHub)
	{ // Remember the level's state for re-entry.
		if (!(Level->flags2 & LEVEL2_FORGETSTATE))
		{
			G_SnapshotLevel ();
			// Do not free any global strings this level (or any sublevel) might reference
			// while it's not loaded.
			ForAllLevels([](FLevelLocals *Level)
			{
				Level->Behaviors.LockLevelVarStrings(Level);
			});
		}
		else
		{ // Make sure we don't have a snapshot lying around from before.
			currentSession->RemoveSnapshot(Level->MapName);
		}
	}
	else
	{ // Forget the states of all existing levels.
		G_ClearSnapshots ();

		if (mode == FINISH_NextHub)
		{ // Reset world variables for the new hub.
			P_ClearACSVars(false);
		}
		currentSession->time = 0;
	}

	finishstate = mode;

	if (!deathmatch &&
		((Level->flags & LEVEL_NOINTERMISSION) ||
		((nextcluster == thiscluster) && (thiscluster->flags & CLUSTER_HUB) && !(thiscluster->flags & CLUSTER_ALLOWINTERMISSION))))
	{
		G_WorldDone (Level);
		return;
	}

	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

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
	DAutosaver() : DThinker(nullptr) {}
};

IMPLEMENT_CLASS(DAutosaver, false, false)

void DAutosaver::Tick ()
{
	Net_WriteByte (DEM_CHECKAUTOSAVE);
	Destroy ();
}


//==========================================================================
//
//
//
//==========================================================================

void InitGlobalState()
{
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
}

//==========================================================================
//
// G_DoLoadLevel 
//
//==========================================================================

extern gamestate_t 	wipegamestate; 
 
void G_DoLoadLevel (const FString &nextlevel, int position, bool autosave, bool newGame)
{
	auto levelinfo = FindLevelInfo(nextlevel);
	TArray<level_info_t *> MapSet;

	MapSet.Push(levelinfo);
	//MapSet.Append(levelinfo->SubLevels);
	
	static int lastposition = 0;
	gamestate_t oldgs = gamestate;
	int i;

	if (currentSession->NextSkill >= 0)
	{
		UCVarValue val;
		val.Int = currentSession->NextSkill;
		gameskill.ForceSet (val, CVAR_Int);
		currentSession->NextSkill = -1;
	}

	if (position == -1)
		position = lastposition;
	else
		lastposition = position;

	StatusBar->DetachAllMessages ();
	currentSession->Levelinfo.DeleteAndClear();
	levelForZScript = &emptyLevelPlaceholderForZScript;
	GC::FullGC();	// really get rid of all the data we just deleted.
	
	// Force 'teamplay' to 'true' if need be.
	if (levelinfo->flags2 & LEVEL2_FORCETEAMPLAYON)
		teamplay = true;

	// Force 'teamplay' to 'false' if need be.
	if (levelinfo->flags2 & LEVEL2_FORCETEAMPLAYOFF)
		teamplay = false;

	FString levelname = levelinfo->LookupLevelName();
	FString mapname = levelinfo->MapName;
	mapname.ToLower();
	
	Printf (
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"
			TEXTCOLOR_BOLD "%s - %s\n\n",
			mapname.GetChars(), levelname.GetChars());

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
	skyflatnum = TexMan.GetTextureID (gameinfo.SkyFlatName, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && (deathmatch || players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_ENTER;	// [BC]
		memset (players[i].frags, 0, sizeof(players[i].frags));
		if (!(dmflags2 & DF2_YES_KEEPFRAGS) && (alwaysapplydmflags || deathmatch))
			players[i].fragcount = 0;
	}

	globalfreeze = false;

	// Set up all needed levels.
	for(auto &linfo : MapSet)
	{
		FLevelLocals *Level = new FLevelLocals;
		auto pos = currentSession->Levelinfo.Push(Level);
		if (pos == 0) levelForZScript = currentSession->Levelinfo[0];

		Level->InitLevelLocals (linfo, pos == 0);
	
		if (changeflags & CHANGELEVEL_NOMONSTERS)
		{
			Level->flags2 |= LEVEL2_NOMONSTERS;
		}
		else
		{
			Level->flags2 &= ~LEVEL2_NOMONSTERS;
		}
		if (changeflags & CHANGELEVEL_PRERAISEWEAPON)
		{
			Level->flags2 |= LEVEL2_PRERAISEWEAPON;
		}
	}
	
	if (newGame)
	{
		E_NewGame(EventHandlerType::Global);
	}

	// Load all levels.
	ForAllLevels([&](FLevelLocals *Level)
	{
		P_SetupLevel (Level, Level->MapName, position, newGame);
		AM_LevelInit(Level);

		// [RH] Start lightning, if MAPINFO tells us to
		if (Level->flags & LEVEL_STARTLIGHTNING)
		{
			P_StartLightning (Level);
		}
	});

	// Init global state after all levels have been loaded.
	InitGlobalState();

	// Restore the state of the levels
	G_UnSnapshotLevel (currentSession->Levelinfo, !savegamerestore);

	globalfreeze = !!(currentSession->isFrozen() & 2);
	
	int pnumerr = G_FinishTravel (currentSession->Levelinfo[0]);

	for (int i = 0; i<MAXPLAYERS; i++)
	{
		// Be prepared for players being on different levels by checking each one's level separately.
		// (This is very unlikely to ever become an issue, though.)
		if (playeringame[i] && players[i].mo != nullptr && !players[i].mo->Level->FromSnapshot)
			P_PlayerStartStomp(players[i].mo);
	}
	
	// For each player, if they are viewing through a player, make sure it is themselves.
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			if (players[i].camera == nullptr || players[i].camera->player != nullptr)
			{
				players[i].camera = players[i].mo;
			}

			if (savegamerestore)
			{
				continue;
			}

			auto Level = players[i].mo->Level;
			const bool fromSnapshot = Level->FromSnapshot;
			E_PlayerEntered(i, fromSnapshot && finishstate == FINISH_SameHub);

			if (fromSnapshot)
			{
				// ENTER scripts are being handled when the player gets spawned, this cannot be changed due to its effect on voodoo dolls.
				Level->Behaviors.StartTypedScripts(Level, SCRIPT_Return, players[i].mo, true);
			}
		}
	}

	ForAllLevels([&](FLevelLocals *Level)
	{
		if (Level->FromSnapshot)
		{
			// [Nash] run REOPEN scripts upon map re-entry
			Level->Behaviors.StartTypedScripts(Level, SCRIPT_Reopen, NULL, false);
		}
	});

	StatusBar->AttachToPlayer (&players[consoleplayer]);
	//      unsafe world load
	E_WorldLoadedUnsafe();
	//      regular world load (savegames are handled internally)
	E_WorldLoaded();
	
	ForAllLevels([&](FLevelLocals *Level)
	{
		P_DoDeferedScripts (Level);	// [RH] Do script actions that were triggered on another map.
	});
	
	if (demoplayback || oldgs == GS_STARTUP || oldgs == GS_TITLELEVEL)
		C_HideConsole ();

	C_FlushDisplay ();

	// [RH] Always save the game when entering a new level.
	if (autosave && !savegamerestore && disableautosave < 1)
	{
		DAutosaver GCCNOWARN *dummy = CreateThinker<DAutosaver>();
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

void G_WorldDone (FLevelLocals *Level)
{ 
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;

	gameaction = ga_worlddone; 

	if (Level->flags & LEVEL_CHANGEMAPCHEAT)
		return;

	thiscluster = FindClusterInfo (Level->cluster);

	auto nextlevel = Level->NextMap;
	
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

		auto ext = Level->info->ExitMapTexts.CheckKey(Level->flags3 & LEVEL3_EXITSECRETUSED ? NAME_Secret : NAME_Normal);
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
		const FExitText *ext = nullptr;
		
		if (Level->flags3 & LEVEL3_EXITSECRETUSED) ext = Level->info->ExitMapTexts.CheckKey(NAME_Secret);
		else if (Level->flags3 & LEVEL3_EXITNORMALUSED) ext = Level->info->ExitMapTexts.CheckKey(NAME_Normal);
		if (ext == nullptr) ext = Level->info->ExitMapTexts.CheckKey(nextlevel);

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

		if (nextcluster->cluster != Level->cluster && !deathmatch)
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
 
DEFINE_ACTION_FUNCTION(FGameSession, WorldDone)
{
	PARAM_SELF_STRUCT_PROLOGUE(FGameSession);
	G_WorldDone(self->Levelinfo[0]);
	return 0;
}

//==========================================================================
//
//
//==========================================================================

void G_DoWorldDone (void) 
{		 
	gamestate = GS_LEVEL;
	if (currentSession->nextlevel.IsEmpty())
	{
		// Don't crash if no next map is given.
		I_Error ("No next map specified.\n");
	}
	G_StartTravel ();
	G_DoLoadLevel (currentSession->nextlevel, currentSession->nextstartpos, true, false);
	currentSession->nextlevel = "";
	currentSession->nextstartpos = 0;

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
			AActor *inv;
			players[i].camera = nullptr;

			// Only living players travel. Dead ones get a new body on the new map.
			if (players[i].health > 0)
			{
				pawn->UnlinkFromWorld (nullptr);
				int tid = pawn->tid;	// Save TID
				pawn->RemoveFromHash ();
				pawn->tid = tid;		// Restore TID (but no longer linked into the hash chain)
				pawn->ChangeStatNum (STAT_TRAVELLING);
				pawn->DeleteAttachedLights();

				for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
				{
					inv->ChangeStatNum (STAT_TRAVELLING);
					inv->UnlinkFromWorld (nullptr);
					inv->DeleteAttachedLights();
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

int G_FinishTravel (FLevelLocals *Level)
{
	TThinkerIterator<AActor> it (Level, NAME_PlayerPawn, STAT_TRAVELLING);
	AActor *pawn, *pawndup, *oldpawn, *next;
	AActor *inv;
	FPlayerStart *start;
	int pnum;
	int failnum = 0;

	// 
	AActor* pawns[MAXPLAYERS];
	int pawnsnum = 0;

	next = it.Next ();
	while ( (pawn = next) != NULL)
	{
		next = it.Next ();
		pnum = int(pawn->player - players);
		pawn->ChangeStatNum (STAT_PLAYER);
		pawndup = pawn->player->mo;
		assert (pawn != pawndup);

		start = G_PickPlayerStart(Level, pnum, 0);
		if (start == NULL)
		{
			if (pawndup != nullptr)
			{
				Printf(TEXTCOLOR_RED "No player %d start to travel to!\n", pnum + 1);
				// Move to the coordinates this player had when they left the map.
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
		pawndup = P_SpawnPlayer(Level, start, pnum, SPF_TEMPPLAYER);
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
			pawn->waterdepth = pawndup->waterdepth;
		}
		else if (failnum == 0)	// In the failure case this may run into some undefined data.
		{
			P_FindFloorCeiling(pawn);
		}
		pawn->target = nullptr;
		pawn->lastenemy = nullptr;
		pawn->player->mo = pawn;
		pawn->player->camera = pawn;
		pawn->player->viewheight = pawn->player->DefaultViewHeight();
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

			IFVIRTUALPTRNAME(inv, NAME_Inventory, Travelled)
			{
				VMValue params[1] = { inv };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		if (pawn->Level->ib_compatflags & BCOMPATF_RESETPLAYERSPEED)
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
	Thinkers.DestroyThinkersInList(STAT_TRAVELLING);
	return failnum;
}
 
//==========================================================================
//
//
//
//==========================================================================

FLevelLocals::~FLevelLocals()
{
	SN_StopAllSequences(this);
	ClearAllSubsectorLinks(); // can't be done as part of the polyobj deletion process.
	Thinkers.DestroyAllThinkers();

	// delete allocated data in the level arrays.
	if (sectors.Size() > 0)
	{
		delete[] sectors[0].e;
	}
	for (auto &sub : subsectors)
	{
		if (sub.BSP != nullptr) delete sub.BSP;
	}

	// also clear the render data
	for (auto &sub : subsectors)
	{
		for (int j = 0; j < 2; j++)
		{
			if (sub.portalcoverage[j].subsectors != nullptr)
			{
				delete[] sub.portalcoverage[j].subsectors;
				sub.portalcoverage[j].subsectors = nullptr;
			}
		}
	}

	for (auto &pb : PolyBlockMap)
	{
		polyblock_t *link = pb;
		while (link != nullptr)
		{
			polyblock_t *next = link->next;
			delete link;
			link = next;
		}
	}
}


//==========================================================================
//
//
//==========================================================================

void FLevelLocals::InitLevelLocals (const level_info_t *info, bool isprimary)
{
	this->info = info;
	MapName = info->MapName;

	// Set up the two default portals.
	sectorPortals.Resize(2);
	// The first entry must always be the default skybox. This is what every sector gets by default.
	memset(&sectorPortals[0], 0, sizeof(sectorPortals[0]));
	sectorPortals[0].mType = PORTS_SKYVIEWPOINT;
	sectorPortals[0].mFlags = PORTSF_SKYFLATONLY;
	// The second entry will be the default sky. This is for forcing a regular sky through the skybox picker
	memset(&sectorPortals[1], 0, sizeof(sectorPortals[0]));
	sectorPortals[1].mType = PORTS_SKYVIEWPOINT;
	sectorPortals[1].mFlags = PORTSF_SKYFLATONLY;


	// Session data should be moved out of here later!
	currentSession->F1Pic = info->F1Pic;
	currentSession->MusicVolume = 1.f;


	P_InitParticles(this);
	P_ClearParticles(this);
	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any

	gravity = sv_gravity * 35/TICRATE;
	aircontrol = sv_aircontrol;
	teamdamage = ::teamdamage;
	flags = info->flags;
	flags2 = info->flags2;
	flags3 = info->flags3;
	freeze = false;
	changefreeze = false;
	maptime = 0;
	spawnindex = 0;
	starttime = gametic;


	skyspeed1 = info->skyspeed1;
	skyspeed2 = info->skyspeed2;
	skytexture1 = TexMan.GetTextureID(info->SkyPic1, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	skytexture2 = TexMan.GetTextureID(info->SkyPic2, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	InitSkyMap(this);
	fadeto = info->fadeto;
	FromSnapshot = false;
	if (fadeto == 0)
	{
		if (strnicmp (info->FadeTable, "COLORMAP", 8) != 0)
		{
			flags |= LEVEL_HASFADETABLE;
		}
	}
	airsupply = info->airsupply*TICRATE;
	outsidefog = info->outsidefog;
	WallVertLight = info->WallVertLight*2;
	WallHorizLight = info->WallHorizLight*2;
	if (info->gravity != 0.f)
	{
		gravity = info->gravity * 35/TICRATE;
	}
	if (info->teamdamage != 0.f)
	{
		teamdamage = info->teamdamage;
	}

	ChangeAirControl(info->aircontrol != 0.f? info->aircontrol : *sv_aircontrol);

	cluster_info_t *clus = FindClusterInfo (info->cluster);

	partime = info->partime;
	sucktime = info->sucktime;
	cluster = info->cluster;
	clusterflags = clus ? clus->flags : 0;
	levelnum = info->levelnum;
	Music = info->Music;
	musicorder = info->musicorder;
	HasHeightSecs = false;

	LevelName = info->LookupLevelName();
	NextMap = info->NextMap;
	NextSecretMap = info->NextSecretMap;
	hazardcolor = info->hazardcolor;
	hazardflash = info->hazardflash;
	
	// GL fog stuff modifiable by SetGlobalFogParameter.
	fogdensity = info->fogdensity;
	outsidefogdensity = info->outsidefogdensity;
	skyfog = info->skyfog;
	deathsequence = info->deathsequence;

	pixelstretch = info->pixelstretch;

	// Fixme: This should not process all other levels again.
	compatflags.Callback();
	compatflags2.Callback();

	DefaultEnvironment = info->DefaultEnvironment;

	lightMode = info->lightmode == ELightMode::NotSet? (ELightMode)*gl_lightmode : info->lightmode;
	brightfog = info->brightfog < 0? gl_brightfog : !!info->brightfog;
	lightadditivesurfaces = info->lightadditivesurfaces < 0 ? gl_lightadditivesurfaces : !!info->lightadditivesurfaces;
	notexturefill = info->notexturefill < 0 ? gl_notexturefill : !!info->notexturefill;

	// This may only be done for the primary level in a set!
	if (isprimary) FLightDefaults::SetAttenuationForLevel(this);
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

void FLevelLocals::ChangeAirControl(double newval)
{
	aircontrol = newval;
	if (aircontrol <= 1/256.)
	{
		airfriction = 1.;
	}
	else
	{
		// Friction is inversely proportional to the amount of control
		airfriction = aircontrol * -0.0941 + 1.0004;
	}
}

//==========================================================================
//
//
//==========================================================================

void G_ClearSnapshots (void)
{
	currentSession->ClearSnapshots();
	// Since strings are only locked when snapshotting a level, unlock them
	// all now, since we got rid of all the snapshots that cared about them.
	GlobalACSStrings.UnlockAll();
}

//==========================================================================
//
//
//==========================================================================

void FGameSession::SerializeACSDefereds(FSerializer &arc)
{
	if (arc.isWriting())
	{
		if (DeferredScripts.CountUsed() == 0) return;
		decltype(DeferredScripts)::Iterator it(DeferredScripts);
		decltype(DeferredScripts)::Pair *pair;

		if (arc.BeginObject("deferred"))
		{
			while (it.NextPair(pair))
			{
				arc(pair->Key, pair->Value);
			}
		}
		arc.EndObject();
	}
	else
	{
		FString MapName;

		DeferredScripts.Clear();

		if (arc.BeginObject("deferred"))
		{
			const char *key;

			while ((key = arc.GetKey()))
			{
				TArray<acsdefered_t> deferred;
				arc(nullptr, deferred);
				DeferredScripts.Insert(key, std::move(deferred));
			}
			arc.EndObject();
		}
	}
}

//==========================================================================
//
//
//==========================================================================

void FGameSession::SerializeVisited(FSerializer &arc)
{
	if (arc.isWriting())
	{
		decltype(Visited)::Iterator it(Visited);
		decltype(Visited)::Pair *pair;

		if (arc.BeginArray("visited"))
		{
			while (it.NextPair(pair))
			{
				// Write out which levels have been visited
				arc.AddString(nullptr, pair->Key);
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
	else
	{
		if (arc.BeginArray("visited"))
		{
			for (int s = arc.ArraySize(); s > 0; s--)
			{
				FString str;
				arc(nullptr, str);
				Visited.Insert(str, true);
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
}


//==========================================================================
//
// 
//
//==========================================================================

void FGameSession::SerializeSession(FSerializer &arc)
{
	if (arc.BeginObject("session"))
	{
		arc("f1pic", F1Pic)
			("musicvolume", MusicVolume)
			("totaltime", totaltime)
			("time", time)
			("frozenstate", frozenstate)
			("hubinfo", hubdata)
			("nextskill", NextSkill);

		SerializeACSDefereds(arc);
		SerializeVisited(arc);
		STAT_Serialize(Statistics, arc);
		if (arc.isReading()) P_ReadACSVars(arc);
		else P_WriteACSVars(arc);
		arc.EndObject();
	}
}


//==========================================================================
//
// Archives the current level
//
//==========================================================================

void G_SnapshotLevel ()
{
	// Create snapshots of all active levels.
	ForAllLevels([](FLevelLocals *Level)
	{
		// first remove the old snapshot, if it exists.
		currentSession->RemoveSnapshot(Level->MapName);
		
		FSerializer arc;
		if (arc.OpenWriter(save_formatted))
		{
			SaveVersion = SAVEVER;
			arc.SetLevel(Level);
			G_SerializeLevel(arc, Level, false);
			currentSession->Snapshots.Insert(Level->MapName, arc.GetCompressedOutput());
			arc.SetLevel(nullptr);
		}
	});
}

//==========================================================================
//
// Unarchives the current level based on its snapshot
// The level should have already been loaded and setup.
//
//==========================================================================

void G_UnSnapshotLevel (const TArray<FLevelLocals *> &levels, bool hubLoad)
{
	for (auto &Level : levels)
	{
		auto snapshot = currentSession->Snapshots.CheckKey(Level->MapName);
		if (snapshot == nullptr)
		{
			continue;
		}

		if (Level->info->isValid())
		{
			FSerializer arc;
			if (!arc.OpenReader(snapshot))
			{
				I_Error("Failed to load savegame");
				return;
			}
			
			arc.SetLevel(Level);
			G_SerializeLevel (arc, Level, hubLoad);
			arc.SetLevel(nullptr);
			Level->FromSnapshot = true;

			TThinkerIterator<AActor> it(Level, NAME_PlayerPawn);
			AActor *pawn;
			
			while ((pawn = it.Next()) != 0)
			{
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
		currentSession->RemoveSnapshot(Level->MapName);
		// Unlock ACS global strings that were locked when the snapshot was made.
		Level->Behaviors.UnlockLevelVarStrings(Level->levelnum);
	}
}

//==========================================================================
//
//
//==========================================================================

void G_WriteSnapshots(TArray<FString> &filenames, TArray<FCompressedBuffer> &buffers)
{
	decltype(currentSession->Snapshots)::Iterator it(currentSession->Snapshots);
	decltype(currentSession->Snapshots)::Pair *pair;
	
	while (it.NextPair(pair))
	{
		if (pair->Value.mCompressedSize > 0)
		{
			FStringf filename("%s.map.json", pair->Key.GetChars());
			filename.ToLower();
			filenames.Push(filename);
			buffers.Push(pair->Value);
		}
	}
}

//==========================================================================
//
//
//==========================================================================

void G_ReadSnapshots(FResourceFile *resf)
{
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
				FName mapname(resl->FullName.GetChars(), (size_t)maplen, false);
				currentSession->Snapshots.Insert(mapname, resl->GetRawData());
			}
		}
	}
}

//==========================================================================
//
//
//==========================================================================

CCMD(listsnapshots)
{
	decltype(currentSession->Snapshots)::Iterator it(currentSession->Snapshots);
	decltype(currentSession->Snapshots)::Pair *pair;
	
	while (it.NextPair(pair))
	{
		FCompressedBuffer *snapshot = &pair->Value;
		if (snapshot->mBuffer != nullptr)
		{
			Printf("%s (%u -> %u bytes)\n", pair->Key.GetChars(), snapshot->mCompressedSize, snapshot->mSize);
		}
	}
}

//==========================================================================
//
// This object is responsible for marking sectors during the propagate
// stage. In case there are many, many sectors, it lets us break them
// up instead of marking them all at once.
//
//==========================================================================

class DSectorMarker : public DObject
{
	enum
	{
		SECTORSTEPSIZE = 32,
		POLYSTEPSIZE = 120,
		SIDEDEFSTEPSIZE = 240
	};
	DECLARE_CLASS(DSectorMarker, DObject)
public:
	DSectorMarker(FLevelLocals *l) : Level(l), SecNum(0),PolyNum(0),SideNum(0) {}
	size_t PropagateMark();
	FLevelLocals *Level;
	int SecNum;
	int PolyNum;
	int SideNum;
};

IMPLEMENT_CLASS(DSectorMarker, true, false)

//==========================================================================
//
// DSectorMarker :: PropagateMark
//
// Propagates marks across a few sectors and reinserts itself into the
// gray list if it didn't do them all.
//
//==========================================================================

size_t DSectorMarker::PropagateMark()
{
	int i;
	int marked = 0;
	bool moretodo = false;
	int numsectors = Level->sectors.Size();
	
	for (i = 0; i < SECTORSTEPSIZE && SecNum + i < numsectors; ++i)
	{
		sector_t *sec = &Level->sectors[SecNum + i];
		GC::Mark(sec->SoundTarget);
		GC::Mark(sec->SecActTarget);
		GC::Mark(sec->floordata);
		GC::Mark(sec->ceilingdata);
		GC::Mark(sec->lightingdata);
		for(int j = 0; j < 4; j++) GC::Mark(sec->interpolations[j]);
	}
	marked += i * sizeof(sector_t);
	if (SecNum + i < numsectors)
	{
		SecNum += i;
		moretodo = true;
	}
	
	if (!moretodo && Level->Polyobjects.Size() > 0)
	{
		for (i = 0; i < POLYSTEPSIZE && PolyNum + i < (int)Level->Polyobjects.Size(); ++i)
		{
			GC::Mark(Level->Polyobjects[PolyNum + i].interpolation);
		}
		marked += i * sizeof(FPolyObj);
		if (PolyNum + i < (int)Level->Polyobjects.Size())
		{
			PolyNum += i;
			moretodo = true;
		}
	}
	if (!moretodo && Level->sides.Size() > 0)
	{
		for (i = 0; i < SIDEDEFSTEPSIZE && SideNum + i < (int)Level->sides.Size(); ++i)
		{
			side_t *side = &Level->sides[SideNum + i];
			for (int j = 0; j < 3; j++) GC::Mark(side->textures[j].interpolation);
		}
		marked += i * sizeof(side_t);
		if (SideNum + i < (int)Level->sides.Size())
		{
			SideNum += i;
			moretodo = true;
		}
	}
	// If there are more items to mark, put ourself back into the gray list.
	if (moretodo)
	{
		Black2Gray();
		GCNext = GC::Gray;
		GC::Gray = this;
	}
	return marked;
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::Mark()
{
	if (SectorMarker == nullptr && (sectors.Size() > 0 || Polyobjects.Size() > 0 || sides.Size() > 0))
	{
		SectorMarker = Create<DSectorMarker>(this);
	}
	else if (sectors.Size() == 0 && Polyobjects.Size() == 0 && sides.Size() == 0)
	{
		SectorMarker = nullptr;
	}
	else
	{
		SectorMarker->SecNum = 0;
	}

	GC::Mark(SectorMarker);
	GC::Mark(SpotState);
	GC::Mark(FraggleScriptThinker);
	GC::Mark(ACSThinker);
	GC::Mark(interpolator.Head);
	GC::Mark(SequenceListHead);
	Thinkers.MarkRoots();

	canvasTextureInfo.Mark();
	for (auto &c : CorpseQueue)
	{
		GC::Mark(c);
	}
	for (auto &s : sectorPortals)
	{
		GC::Mark(s.mSkybox);
	}
	// Mark dead bodies.
	for (auto &p : bodyque)
	{
		GC::Mark(p);
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
	auto mus = info->MapInterMusic.CheckKey(nextmap);
	if (mus != nullptr)
		S_ChangeMusic(mus->first, mus->second);
	else if (info->InterMusic.IsNotEmpty())
		S_ChangeMusic(info->InterMusic, info->intermusicorder);
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

void FLevelLocals::SetMusic()
{
	if (info->cdtrack == 0 || !S_ChangeCDMusic(info->cdtrack, info->cdid))
		S_ChangeMusic(Music, musicorder);
}
//==========================================================================
//
//
//==========================================================================

void FGameSession::SetMusicVolume(float f)
{
	if (currentSession) currentSession->MusicVolume = f;
	I_SetMusicVolume(f);
}

//==========================================================================
// IsPointInMap
//
// Checks to see if a point is inside the void or not.
// Made by dpJudas, modified and implemented by Major Cooke
//==========================================================================


int IsPointInMap(FLevelLocals *Level, double x, double y, double z)
{
	subsector_t *subsector = R_PointInSubsector(Level, FLOAT2FIXED(x), FLOAT2FIXED(y));
	if (!subsector) return false;

	for (uint32_t i = 0; i < subsector->numlines; i++)
	{
		// Skip single sided lines.
		seg_t *seg = subsector->firstline + i;
		if (seg->backsector != nullptr)	continue;

		divline_t dline;
		P_MakeDivline(seg->linedef, &dline);
		bool pol = P_PointOnDivlineSide(x, y, &dline) < 1;
		if (!pol) return false;
	}

	double ceilingZ = subsector->sector->ceilingplane.ZatPoint(x, y);
	if (z > ceilingZ) return false;

	double floorZ = subsector->sector->floorplane.ZatPoint(x, y);
	if (z < floorZ) return false;

	return true;
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, IsPointInMap, IsPointInMap)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_BOOL(IsPointInMap(self, x, y, z));
}

template <typename T>
inline T VecDiff(FLevelLocals *Level, const T& v1, const T& v2)
{
	T result = v2 - v1;

	if (Level->subsectors.Size() > 0)
	{
		const sector_t *const sec1 = P_PointInSector(Level, v1);
		const sector_t *const sec2 = P_PointInSector(Level, v2);

		if (nullptr != sec1 && nullptr != sec2)
		{
			result += Level->Displacements.getOffset(sec2->PortalGroup, sec1->PortalGroup);
		}
	}

	return result;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, Vec2Diff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	ACTION_RETURN_VEC2(VecDiff(self, DVector2(x1, y1), DVector2(x2, y2)));
}

DEFINE_ACTION_FUNCTION(FLevelLocals, Vec3Diff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(x1);
	PARAM_FLOAT(y1);
	PARAM_FLOAT(z1);
	PARAM_FLOAT(x2);
	PARAM_FLOAT(y2);
	PARAM_FLOAT(z2);
	ACTION_RETURN_VEC3(VecDiff(self, DVector3(x1, y1, z1), DVector3(x2, y2, z2)));
}

DEFINE_ACTION_FUNCTION(FLevelLocals, SphericalCoords)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_FLOAT(viewpointX);
	PARAM_FLOAT(viewpointY);
	PARAM_FLOAT(viewpointZ);
	PARAM_FLOAT(targetX);
	PARAM_FLOAT(targetY);
	PARAM_FLOAT(targetZ);
	PARAM_ANGLE(viewYaw);
	PARAM_ANGLE(viewPitch);
	PARAM_BOOL(absolute);

	DVector3 viewpoint(viewpointX, viewpointY, viewpointZ);
	DVector3 target(targetX, targetY, targetZ);
	auto vecTo = absolute ? target - viewpoint : VecDiff(self, viewpoint, target);

	ACTION_RETURN_VEC3(DVector3(
		deltaangle(vecTo.Angle(), viewYaw).Degrees,
		deltaangle(-vecTo.Pitch(), viewPitch).Degrees,
		vecTo.Length()
	));
}


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
		ForAllLevels([&](FLevelLocals *Level) {
			Level->skyfog = MAX(0, (int)strtoull(argv[1], NULL, 0));
		});
	}
}


//==========================================================================
//
// ZScript counterpart to ACS ChangeSky, uses TextureIDs
//
//==========================================================================
DEFINE_ACTION_FUNCTION(FLevelLocals, ChangeSky)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(sky1);
	PARAM_INT(sky2);
	self->skytexture1 = FSetTextureID(sky1);
	self->skytexture2 = FSetTextureID(sky2);
	InitSkyMap(self);
	return 0;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, StartIntermission)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_NAME(seq);
	PARAM_INT(state);
	F_StartIntermission(seq, (uint8_t)state);
	return 0;
}
