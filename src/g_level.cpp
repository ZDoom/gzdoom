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

#include "d_main.h"
#include "g_level.h"
#include "g_game.h"
#include "s_sound.h"
#include "d_event.h"
#include "m_random.h"
#include "engineerrors.h"
#include "doomstat.h"
#include "wi_stuff.h"
#include "filesystem.h"
#include "am_map.h"
#include "c_dispatch.h"
#include "i_interface.h"

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
#include "doommenu.h"
#include "a_sharedglobal.h"
#include "r_utility.h"
#include "p_spec.h"
#include "serializer_doom.h"
#include "vm.h"
#include "events.h"
#include "i_music.h"
#include "a_dynlight.h"
#include "p_conversation.h"
#include "p_effect.h"
#include "stringtable.h"
#include "c_buttons.h"
#include "screenjob.h"
#include "types.h"
#include "gstrings.h"

#include "gi.h"


#include "g_hub.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "i_time.h"
#include "p_maputl.h"
#include "s_music.h"
#include "fragglescript/t_script.h"

#include "texturemanager.h"

void STAT_StartNewGame(const char *lev);
void STAT_ChangeLevel(const char *newl, FLevelLocals *Level);
FString STAT_EpisodeName();

EXTERN_CVAR(Bool, save_formatted)
EXTERN_CVAR (Float, sv_gravity)
EXTERN_CVAR (Float, sv_aircontrol)
EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (String, playerclass)

extern uint8_t globalfreeze, globalchangefreeze;
int startpos = 0; // [RH] Support for multiple starts per level
int laststartpos = 0;

#define SNAP_ID			MAKE_ID('s','n','A','p')
#define DSNP_ID			MAKE_ID('d','s','N','p')
#define VIST_ID			MAKE_ID('v','i','S','t')
#define ACSD_ID			MAKE_ID('a','c','S','d')
#define RCLS_ID			MAKE_ID('r','c','L','s')
#define PCLS_ID			MAKE_ID('p','c','L','s')

void G_VerifySkill();
void I_UpdateWindowTitle();

CUSTOM_CVAR(Bool, gl_brightfog, false, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		if (Level->info == nullptr || Level->info->brightfog == -1) Level->brightfog = self;
	}
}

CUSTOM_CVAR(Bool, gl_lightadditivesurfaces, false, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		if (Level->info == nullptr || Level->info->lightadditivesurfaces == -1) Level->lightadditivesurfaces = self;
	}
}

CUSTOM_CVAR(Bool, gl_notexturefill, false, CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		if (Level->info == nullptr || Level->info->notexturefill == -1) Level->notexturefill = self;
	}
}

CUSTOM_CVAR(Int, gl_maplightmode, -1, CVAR_NOINITCALL | CVAR_CHEAT) // this is just for testing. -1 means 'inactive'
{
	if (self > 4 || self < -1) self = -1;
}

CUSTOM_CVARD(Int, gl_lightmode, 1, CVAR_ARCHIVE, "Select lighting mode. 2 is vanilla accurate, 1 is accurate to the ZDoom software renderer and 0 is a less demanding non-shader implementation")
{
	if (self < 0 || self > 2) self = 1;
}

ELightMode getRealLightmode(FLevelLocals* Level, bool for3d)
{
	// The rules are:
	// 1) if the map sets a proper light mode, it is taken unconditionally.
	if (Level->info->lightmode != ELightMode::NotSet) return Level->info->lightmode;
	// 2) if the user sets gl_maplightmode, this is being used.
	if (gl_maplightmode != -1) return (ELightMode)*gl_maplightmode;
	// 3) if not for 3D use lightmode Doom. This is for the automap where the software light modes do not work
	if (!for3d) return ELightMode::Doom;
	// otherwise use lightmode Doom or software lighting based on user preferences.
	if (gl_lightmode == 1) return ELightMode::ZDoomSoftware;
	else if (gl_lightmode == 2) return ELightMode::DoomSoftware;
	return ELightMode::Doom;
}

CVAR(Int, sv_alwaystally, 0, CVAR_SERVERINFO)

static FRandom pr_classchoice ("RandomPlayerClassChoice");

extern level_info_t TheDefaultLevelInfo;
extern bool timingdemo;

// Start time for timing demos
int starttime;


extern FString BackupSaveName;

bool savegamerestore;
int finishstate = FINISH_NoHub;

extern float mousex, mousey;
extern bool sendpause, sendsave, sendturn180, SendLand;

void *statcopy;					// for statistics driver

FLevelLocals level;			// info about current level
FLevelLocals *primaryLevel = &level;	// level for which to display the user interface.
FLevelLocals *currentVMLevel = &level;	// level which currently ticks. Used as global input to the VM and some functions called by it.
static PType* maprecordtype;


//=============================================================================
//
//
//
//=============================================================================

void Local_Job_Init()
{
	maprecordtype = NewPointer(NewStruct("MapRecord", nullptr, true));
}

//=============================================================================
//
//
//
//=============================================================================

static void CallCreateMapFunction(const char* qname, DObject* runner, level_info_t* map)
{
	auto func = LookupFunction(qname);
	if (func->Proto->ArgumentTypes.Size() == 1) return CallCreateFunction(qname, runner);	// accept functions without map parameter as well here.
	if (func->Proto->ArgumentTypes.Size() != 2) I_Error("Bad map-cutscene function %s. Must receive precisely two arguments.", qname);
	if (func->Proto->ArgumentTypes[0] != cutscene.runnerclasstype && func->Proto->ArgumentTypes[1] != maprecordtype)
		I_Error("Bad cutscene function %s. Must receive ScreenJobRunner and LevelInfo reference.", qname);
	VMValue val[2] = { runner, map };
	VMCall(func, val, 2, nullptr, 0);
}

//=============================================================================
//
//
//
//=============================================================================

bool CreateCutscene(CutsceneDef* cs, DObject* runner, level_info_t* map)
{
	if (cs->function.CompareNoCase("none") == 0)
		return true;	// play nothing but return as being validated
	if (cs->function.IsNotEmpty())
	{
		CallCreateMapFunction(cs->function.GetChars(), runner, map);
		return true;
	}
	else if (cs->video.IsNotEmpty())
	{
		AddGenericVideo(runner, cs->video, cs->GetSound(), cs->framespersec);
		return true;
	}
	return false;
}

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

void G_DeferedInitNew (FNewGameStartup *gs)
{
	if (gs->hasPlayerClass) playerclass = gs->PlayerClass.GetChars();
	d_mapname = AllEpisodes[gs->Episode].mEpisodeMap;
	d_skill = gs->Skill;
	CheckWarpTransMap (d_mapname, true);
	gameaction = ga_newgame2;
	finishstate = FINISH_NoHub;

	if (AllEpisodes[gs->Episode].mIntro.isdefined())
	{
		cutscene.runner = CreateRunner(false);
		GC::WriteBarrier(cutscene.runner);

		if (!CreateCutscene(&AllEpisodes[gs->Episode].mIntro, cutscene.runner, nullptr))
		{
			return;
		}

		cutscene.completion = [](bool) { gameaction = ga_newgame2; };
		if (!ScreenJobValidate())
		{
			DeleteScreenJob();
			cutscene.completion = nullptr;
			return;
		}
		gameaction = ga_intermission;
	}
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
		if (!strcmp(mapname, "*")) mapname = primaryLevel->MapName.GetChars();

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
		if (!strcmp(mapname, "*")) mapname = primaryLevel->MapName.GetChars();

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
		if (!P_CheckMapData(d_mapname.GetChars()))
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
	TThinkerIterator<AActor> it(primaryLevel, NAME_PlayerPawn, STAT_TRAVELLING, false);
	AActor *pawn, *next;

	next = it.Next();
	while ((pawn = next) != NULL)
	{
		next = it.Next();
		pawn->flags |= MF_NOSECTOR | MF_NOBLOCKMAP;
		pawn->Destroy();
	}
	if (primaryLevel->FraggleScriptThinker) primaryLevel->FraggleScriptThinker->Destroy();
	primaryLevel->FraggleScriptThinker = nullptr;

	// Destroy thinkers that may remain after change level failure
	// Usually, the list contains just a sentinel when such error occurred
	primaryLevel->Thinkers.DestroyThinkersInList(STAT_TRAVELLING);
	primaryLevel->ClientsideThinkers.DestroyThinkersInList(STAT_TRAVELLING); // This isn't currently supported, but maybe in the future

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
	G_InitNew (d_mapname.GetChars(), false);
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
	if (primaryLevel->info != nullptr)
		staticEventManager.WorldUnloaded(FString());	// [MK] don't pass the new map, as it's not a level transition

	UnlatchCVars ();
	if (!savegamerestore)
	{
		G_ClearHubInfo();
		G_ClearSnapshots ();
		P_RemoveDefereds ();

		// [RH] Mark all levels as not visited
		for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
			wadlevelinfos[i].flags = wadlevelinfos[i].flags & ~LEVEL_VISITED;

		auto redirectmap = FindLevelInfo(mapname);
		if (redirectmap->RedirectCVAR != NAME_None)
			redirectmap = redirectmap->CheckLevelRedirect();
		if (redirectmap && redirectmap->MapName.IsNotEmpty())
				mapname = redirectmap->MapName.GetChars();
	}

	G_VerifySkill();
	UnlatchCVars ();
	globalfreeze = globalchangefreeze = 0;
	for (auto Level : AllLevels())
	{
		Level->Thinkers.DestroyThinkersInList(STAT_STATIC);
		Level->ClientsideThinkers.DestroyThinkersInList(STAT_STATIC);
	}

	if (paused)
	{
		paused = 0;
		S_ResumeSound (false);
	}

	ST_CreateStatusBar(bTitleLevel);
	setsizeneeded = true;

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
		primaryLevel->time = 0;
		primaryLevel->maptime = 0;
		primaryLevel->totaltime = 0;
		primaryLevel->spawnindex = 0;

		primaryLevel->lightlists.wall_dlist.Clear();
		primaryLevel->lightlists.flat_dlist.Clear();

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

	//Added by MC: Initialize bots.
	if (!deathmatch)
	{
		primaryLevel->BotInfo.Init ();
	}

	if (bTitleLevel)
	{
		gamestate = GS_TITLELEVEL;
	}
	else if (gamestate != GS_STARTUP)
	{
		gamestate = GS_LEVEL;
	}
	
	if (!savegamerestore)
		startpos = laststartpos = 0;
	G_DoLoadLevel (mapname, startpos, false, !savegamerestore);

	if (!savegamerestore && (gameinfo.gametype == GAME_Strife || (SBarInfoScript[SCRIPT_CUSTOM] != nullptr && SBarInfoScript[SCRIPT_CUSTOM]->GetGameType() == GAME_Strife)))
	{
		// Set the initial quest log text for Strife.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				players[i].SetLogText("$TXT_FINDHELP");
		}
	}
}

//
// G_DoCompleted
//
static FString	nextlevel;
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

bool FLevelLocals::ShouldDoIntermission(cluster_info_t* nextcluster, cluster_info_t* thiscluster)
{
	// this is here to remove some code duplication

	if ((sv_alwaystally == 2) || (deathmatch))
		return true;

	if ((sv_alwaystally == 0) && (flags & LEVEL_NOINTERMISSION))
		return false;

	bool withinSameCluster = (nextcluster == thiscluster);
	bool clusterIsHub = (thiscluster->flags & CLUSTER_HUB);
	bool hubNoIntermission = !(thiscluster->flags & CLUSTER_ALLOWINTERMISSION);

	if (withinSameCluster && clusterIsHub && hubNoIntermission)
		return false;

	return true;
}

void FLevelLocals::ChangeLevel(const char *levelname, int position, int inflags, int nextSkill)
{
	if (!isPrimaryLevel()) return;	// only the primary level may exit.

	FString nextlevel;
	level_info_t *nextinfo = nullptr;

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
		if (!NextMap.Compare("enDSeQ",6))
		{
			nextlevel = NextMap;	// If there is already an end sequence please leave it alone!
		}
		else 
		{
			nextlevel.Format("enDSeQ%04x", gameinfo.DefaultEndSequence.GetIndex());
		}
	}
	else if (strncmp(levelname, "enDSeQ", 6) != 0)
	{
		FString reallevelname = levelname;
		CheckWarpTransMap(reallevelname, true);
		nextinfo = FindLevelInfo (reallevelname.GetChars(), false);
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
		NextSkill = (unsigned)nextSkill < AllSkills.Size() ? nextSkill : -1;

	if (inflags & CHANGELEVEL_NOINTERMISSION)
	{
		flags |= LEVEL_NOINTERMISSION;
	}

	cluster_info_t *thiscluster = FindClusterInfo (cluster);
	cluster_info_t *nextcluster = nextinfo? FindClusterInfo (nextinfo->cluster) : NULL;

	startpos = position;
	SetMusicVolume(1.0);
		
	if (nextinfo != NULL) 
	{
		if (thiscluster != nextcluster || (thiscluster && !(thiscluster->flags & CLUSTER_HUB)))
		{
			const bool doReset = dmflags3 & DF3_PISTOL_START;
			if (doReset || (nextinfo->flags2 & LEVEL2_RESETINVENTORY))
			{
				inflags |= CHANGELEVEL_RESETINVENTORY;
			}
			if (doReset || (nextinfo->flags2 & LEVEL2_RESETHEALTH))
			{
				inflags |= CHANGELEVEL_RESETHEALTH;
			}
		}
	}
	changeflags = inflags;

	BotInfo.End();	//Added by MC:

	// [RH] Give scripts a chance to do something
	unloading = true;
	Behaviors.StartTypedScripts (SCRIPT_Unloading, NULL, false, 0, true);
	// [ZZ] safe world unload
	for (auto Level : AllLevels())
	{
		// Todo: This must be exolicitly sandboxed!
		Level->localEventManager->WorldUnloaded(nextlevel);
	}
	// [ZZ] unsafe world unload (changemap != map)
	staticEventManager.WorldUnloaded(nextlevel);
	unloading = false;

	STAT_ChangeLevel(nextlevel.GetChars(), this);

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if (!ShouldDoIntermission(nextcluster, thiscluster))
			NoWipe = 35;
		D_DrawIcon = "TELEICON";
	}

	for(int i = 0; i < MAXPLAYERS; i++)
	{
		if (PlayerInGame(i))
		{
			player_t *player = Players[i];

			// Un-crouch all players here.
			player->Uncrouch();

			// If this is co-op, respawn any dead players now so they can
			// keep their inventory on the next map.
			if ((multiplayer || flags2 & LEVEL2_ALLOWRESPAWN || sv_singleplayerrespawn || !!G_SkillProperty(SKILLP_PlayerRespawn))
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
				DoReborn(i, true);
			}
		}
	}
	// Set global transition state.
	gameaction = ga_completed;
	::nextlevel = nextlevel;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, ChangeLevel)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_STRING(levelname);
	PARAM_INT(position);
	PARAM_INT(inflags);
	PARAM_INT(nextSkill);
	self->ChangeLevel(levelname.GetChars(), position, inflags, nextSkill);
	return 0;
}

//==========================================================================
//
//
//==========================================================================

const char *FLevelLocals::GetSecretExitMap()
{
	const char *nextmap = NextMap.GetChars();

	if (NextSecretMap.Len() > 0)
	{
		if (NextSecretMap.Compare("enDSeQ", 6) == 0 || P_CheckMapData(NextSecretMap.GetChars()))
		{
			nextmap = NextSecretMap.GetChars();
		}
	}
	return nextmap;
}

//==========================================================================
//
//
//==========================================================================


void FLevelLocals::ExitLevel (int position, bool keepFacing)
{
	flags3 |= LEVEL3_EXITNORMALUSED;
	ChangeLevel(NextMap.GetChars(), position, keepFacing ? CHANGELEVEL_KEEPFACING : 0);
}

static void LevelLocals_ExitLevel(FLevelLocals *self, int position, bool keepFacing)
{
	self->ExitLevel(position, keepFacing);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, ExitLevel, LevelLocals_ExitLevel)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(position);
	PARAM_INT(keepFacing);
	self->ExitLevel(position, keepFacing);
	return 0;
}

void FLevelLocals::SecretExitLevel (int position)
{
	flags3 |= LEVEL3_EXITSECRETUSED;
	ChangeLevel(GetSecretExitMap(), position, 0);
}

static void LevelLocals_SecretExitLevel(FLevelLocals *self, int position)
{
	self->SecretExitLevel(position);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, SecretExitLevel, LevelLocals_SecretExitLevel)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(position);
	self->SecretExitLevel(position);
	return 0;
}

//==========================================================================
//
//
//==========================================================================
static wbstartstruct_t staticWmInfo;

DIntermissionController* FLevelLocals::CreateIntermission()
{
	DIntermissionController* controller = nullptr;
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;

	if (flags & LEVEL_CHANGEMAPCHEAT)
		return nullptr;

	thiscluster = FindClusterInfo (cluster);

	bool endgame = strncmp (nextlevel.GetChars(), "enDSeQ", 6) == 0;
	if (endgame)
	{
		FName endsequence = ENamedName(strtoll(nextlevel.GetChars()+6, NULL, 16));
		// Strife needs a special case here to choose between good and sad ending. Bad is handled elsewhere.
		if (endsequence == NAME_Inter_Strife)
		{
			if (Players[0]->mo->FindInventory (NAME_QuestItem25) ||
				Players[0]->mo->FindInventory (NAME_QuestItem28))
			{
				endsequence = NAME_Inter_Strife_Good;
			}
			else
			{
				endsequence = NAME_Inter_Strife_Sad;
			}
		}

		auto ext = info->ExitMapTexts.CheckKey(flags3 & LEVEL3_EXITSECRETUSED ? NAME_Secret : NAME_Normal);
		if (ext != nullptr && (ext->mDefined & FExitText::DEF_TEXT))
		{
			controller = F_StartFinale(ext->mDefined & FExitText::DEF_MUSIC ? ext->mMusic.GetChars() : gameinfo.finaleMusic.GetChars(),
				ext->mDefined & FExitText::DEF_MUSIC ? ext->mOrder : gameinfo.finaleOrder,
				-1, 0,
				ext->mDefined & FExitText::DEF_BACKDROP ? ext->mBackdrop.GetChars() : gameinfo.FinaleFlat.GetChars(),
				ext->mText.GetChars(),
				false,
				ext->mDefined & FExitText::DEF_PIC,
				ext->mDefined & FExitText::DEF_LOOKUP,
				true, endsequence);
		}
		else if (!(info->flags2 & LEVEL2_NOCLUSTERTEXT))
		{
			controller = F_StartFinale(thiscluster->MessageMusic.GetChars(), thiscluster->musicorder,
				thiscluster->cdtrack, thiscluster->cdid,
				thiscluster->FinaleFlat.GetChars(), thiscluster->ExitText.GetChars(),
				thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
				thiscluster->flags & CLUSTER_FINALEPIC,
				thiscluster->flags & CLUSTER_LOOKUPEXITTEXT,
				true, endsequence);
		}
	}
	else if (!deathmatch)
	{
		FExitText *ext = nullptr;
		
		if (flags3 & LEVEL3_EXITSECRETUSED) ext = info->ExitMapTexts.CheckKey(NAME_Secret);
		else if (flags3 & LEVEL3_EXITNORMALUSED) ext = info->ExitMapTexts.CheckKey(NAME_Normal);
		if (ext == nullptr) ext = info->ExitMapTexts.CheckKey(nextlevel);

		if (ext != nullptr)
		{
			if ((ext->mDefined & FExitText::DEF_TEXT))
			{
				controller = F_StartFinale(ext->mDefined & FExitText::DEF_MUSIC ? ext->mMusic.GetChars() : gameinfo.finaleMusic.GetChars(),
					ext->mDefined & FExitText::DEF_MUSIC ? ext->mOrder : gameinfo.finaleOrder,
					-1, 0,
					ext->mDefined & FExitText::DEF_BACKDROP ? ext->mBackdrop.GetChars() : gameinfo.FinaleFlat.GetChars(),
					ext->mText.GetChars(),
					false,
					ext->mDefined & FExitText::DEF_PIC,
					ext->mDefined & FExitText::DEF_LOOKUP,
					false);
			}
			return controller;
		}

		nextcluster = FindClusterInfo (FindLevelInfo (nextlevel.GetChars())->cluster);

		if (nextcluster->cluster != cluster && !(info->flags2 & LEVEL2_NOCLUSTERTEXT))
		{
			// Only start the finale if the next level's cluster is different
			// than the current one and we're not in deathmatch.
			if (nextcluster->EnterText.IsNotEmpty())
			{
				controller = F_StartFinale (nextcluster->MessageMusic.GetChars(), nextcluster->musicorder,
					nextcluster->cdtrack, nextcluster->cdid,
					nextcluster->FinaleFlat.GetChars(), nextcluster->EnterText.GetChars(),
					nextcluster->flags & CLUSTER_ENTERTEXTINLUMP,
					nextcluster->flags & CLUSTER_FINALEPIC,
					nextcluster->flags & CLUSTER_LOOKUPENTERTEXT,
					false);
			}
			else if (thiscluster->ExitText.IsNotEmpty())
			{
				controller = F_StartFinale (thiscluster->MessageMusic.GetChars(), thiscluster->musicorder,
					thiscluster->cdtrack, nextcluster->cdid,
					thiscluster->FinaleFlat.GetChars(), thiscluster->ExitText.GetChars(),
					thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
					thiscluster->flags & CLUSTER_FINALEPIC,
					thiscluster->flags & CLUSTER_LOOKUPEXITTEXT,
					false);
			}
		}
	}
	return controller;
}

//=============================================================================
//
//
//
//=============================================================================

void RunIntermission(level_info_t* fromMap, level_info_t* toMap, DIntermissionController* intermissionScreen, DObject* statusScreen, std::function<void(bool)> completionf)
{
	cutscene.runner = CreateRunner(false);
	GC::WriteBarrier(cutscene.runner);
	cutscene.completion = std::move(completionf);
	
	// retrieve cluster relations for cluster-based cutscenes.
	cluster_info_t* fromcluster = nullptr, *tocluster = nullptr;
	if (fromMap) fromcluster = FindClusterInfo(fromMap->cluster);
	if (toMap) tocluster = FindClusterInfo(toMap->cluster);
	if (fromcluster == tocluster) fromcluster = tocluster = nullptr;

	if (fromMap)
	{
		if (!CreateCutscene(&fromMap->outro, cutscene.runner, fromMap))
		{
			if (fromcluster != nullptr) CreateCutscene(&fromcluster->outro, cutscene.runner, fromMap);
		}
	}

	auto func = LookupFunction("DoomCutscenes.BuildMapTransition");
	if (func == nullptr)
	{
		I_Error("Script function 'DoomCutscenes.BuildMapTransition' not found");
	}
	VMValue val[3] = { cutscene.runner, intermissionScreen, statusScreen };
	VMCall(func, val, 3, nullptr, 0);

	if (toMap)
	{
		if (!CreateCutscene(&toMap->intro, cutscene.runner, toMap))
		{
			if  (tocluster != nullptr) CreateCutscene(&tocluster->intro, cutscene.runner, toMap);
		}
	}

	if (!ScreenJobValidate())
	{
		DeleteScreenJob();
		if (cutscene.completion) cutscene.completion(false);
		cutscene.completion = nullptr;
		return;
	}
	gameaction = ga_intermission;
}

void G_DoCompleted (void)
{
	gameaction = ga_nothing;
	
	if (   gamestate == GS_DEMOSCREEN
		|| gamestate == GS_FULLCONSOLE
		|| gamestate == GS_STARTUP)
	{
		return;
	}
	
	if (gamestate == GS_TITLELEVEL)
	{
		G_DoLoadLevel (nextlevel, startpos, false, false);
		viewactive = true;
		return;
	}
	
	if (automapactive)
		AM_Stop ();

	// Close the conversation menu if open.
	P_FreeStrifeConversations ();

	bool playinter = primaryLevel->DoCompleted(nextlevel, staticWmInfo);
	S_StopAllChannels();
	for (auto Level : AllLevels())
	{
		SN_StopAllSequences(Level);
	}

	// todo: create end of level screenjob
	DObject* statusScreen = nullptr;
	DIntermissionController* intermissionScreen = nullptr;
	if (playinter)
	{
		// [RH] If you ever get a statistics driver operational, adapt this.
		//	if (statcopy)
		//		memcpy (statcopy, &wminfo, sizeof(wminfo));
		
		statusScreen = WI_Start (&staticWmInfo);
	}
	bool endgame = strncmp(nextlevel.GetChars(), "enDSeQ", 6) == 0;
	intermissionScreen = primaryLevel->CreateIntermission();
	auto nextinfo = !playinter || endgame? nullptr : FindLevelInfo(nextlevel.GetChars(), false);
	RunIntermission(primaryLevel->info, nextinfo, intermissionScreen, statusScreen, [=](bool)
	{
		if (!endgame) primaryLevel->WorldDone();
		else D_StartTitle();
	});
}

//==========================================================================
//
// Prepare the level to be exited and
// set up the wminfo struct for the coming intermission screen
//
//==========================================================================

bool FLevelLocals::DoCompleted (FString nextlevel, wbstartstruct_t &wminfo)
{
	int i;

	// [RH] Mark this level as having been visited
	if (!(flags & LEVEL_CHANGEMAPCHEAT))
		info->flags |= LEVEL_VISITED;
	
	uint32_t langtable[2] = {};
	wminfo.finished_ep = cluster - 1;
	wminfo.LName0 = TexMan.CheckForTexture(info->PName.GetChars(), ETextureType::MiscPatch);
	wminfo.thisname = info->LookupLevelName(&langtable[0]);	// re-get the name so we have more info about its origin.
	if (!wminfo.LName0.isValid() || !(info->flags3 & LEVEL3_HIDEAUTHORNAME)) wminfo.thisauthor = info->AuthorName;
	wminfo.current = MapName;

	if (deathmatch &&
		(*dmflags & DF_SAME_LEVEL) &&
		!(flags & LEVEL_CHANGEMAPCHEAT))
	{
		wminfo.next = MapName;
		wminfo.LName1 = wminfo.LName0;
		wminfo.nextname = wminfo.thisname;
		wminfo.nextauthor = wminfo.thisauthor;
	}
	else
	{
		level_info_t *nextinfo = FindLevelInfo (nextlevel.GetChars(), false);
		if (nextinfo == NULL || strncmp (nextlevel.GetChars(), "enDSeQ", 6) == 0)
		{
			wminfo.next = "";
			wminfo.LName1.SetInvalid();
			wminfo.nextname = "";
			wminfo.nextauthor = "";
		}
		else
		{
			wminfo.next = nextinfo->MapName;
			wminfo.LName1 = TexMan.CheckForTexture(nextinfo->PName.GetChars(), ETextureType::MiscPatch);
			wminfo.nextname = nextinfo->LookupLevelName(&langtable[1]);
			if (!wminfo.LName1.isValid() || !(nextinfo->flags3 & LEVEL3_HIDEAUTHORNAME)) wminfo.nextauthor = nextinfo->AuthorName;
		}
	}

	// This cannot use any common localization logic because it may not replace user content at all.
	// Unlike the menus, replacements here do not merely change the style but also the content.
	// On the other hand, the IWAD lumps may always be replaced with text, because they are the same style as the BigFont.
	if (gameinfo.flags & GI_IGNORETITLEPATCHES)
	{
		FTextureID *texids[] = { &wminfo.LName0, &wminfo.LName1 };
		for (int i = 0; i < 2; i++)
		{
			if (texids[i]->isValid() && langtable[i] != FStringTable::default_table)
			{
				FGameTexture *tex = TexMan.GetGameTexture(*texids[i]);
				if (tex != nullptr && !tex->isUserContent())
				{
					texids[i]->SetInvalid();
				}
			}
		}
	}

	CheckWarpTransMap (wminfo.next, true);
	nextlevel = wminfo.next;

	wminfo.next_ep = FindLevelInfo (wminfo.next.GetChars())->cluster - 1;
	wminfo.totalkills = killed_monsters;
	wminfo.maxkills = total_monsters;
	wminfo.maxitems = total_items;
	wminfo.maxsecret = total_secrets;
	wminfo.maxfrags = 0;
	wminfo.partime = TICRATE * partime;
	wminfo.sucktime = sucktime;
	wminfo.pnum = consoleplayer;
	wminfo.totaltime = totaltime;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		wminfo.plyr[i].skills = Players[i]->killcount;
		wminfo.plyr[i].sitems = Players[i]->itemcount;
		wminfo.plyr[i].ssecret = Players[i]->secretcount;
		wminfo.plyr[i].stime = time;
		memcpy (wminfo.plyr[i].frags, Players[i]->frags, sizeof(wminfo.plyr[i].frags));
		wminfo.plyr[i].fragcount = Players[i]->fragcount;
	}

	// [RH] If we're in a hub and staying within that hub, take a snapshot.
	//		If we're traveling to a new hub, take stuff from
	//		the player and clear the world vars. If this is just an
	//		ordinary cluster (not a hub), take stuff from the player, but
	//		leave the world vars alone.
	cluster_info_t *thiscluster = FindClusterInfo (cluster);
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
	G_LeavingHub(this, mode, thiscluster, &wminfo);

	// Do not allow playing sounds in here - they'd never be able to play properly.
	soundEngine->BlockNewSounds(true);
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{ // take away appropriate inventory
			G_PlayerFinishLevel (i, mode, changeflags);
		}
	}
	soundEngine->BlockNewSounds(false);

	if (mode == FINISH_SameHub)
	{ // Remember the level's state for re-entry.
		if (!(flags2 & LEVEL2_FORGETSTATE))
		{
			SnapshotLevel ();
			// Do not free any global strings this level might reference
			// while it's not loaded.
			Behaviors.LockLevelVarStrings(levelnum);
		}
		else
		{ // Make sure we don't have a snapshot lying around from before.
			info->Snapshot.Clean();
		}
	}
	else
	{ // Forget the states of all existing levels.
		G_ClearSnapshots ();

		if (mode == FINISH_NextHub)
		{ // Reset world variables for the new hub.
			P_ClearACSVars(false);
		}
		time = 0;
		maptime = 0;
		spawnindex = 0;
	}

	finishstate = mode;

	return ShouldDoIntermission(nextcluster, thiscluster);
}

//==========================================================================
//
//
//==========================================================================

class DAutosaver : public DThinker
{
	DECLARE_CLASS (DAutosaver, DThinker)
public:
	void Construct() {}
	void Tick ();
};

IMPLEMENT_CLASS(DAutosaver, false, false)

void DAutosaver::Tick ()
{
	Net_WriteInt8 (DEM_CHECKAUTOSAVE);
	Destroy ();
}

//==========================================================================
//
// G_DoLoadLevel 
//
//==========================================================================

extern gamestate_t 	wipegamestate; 
 
void G_DoLoadLevel(const FString &nextmapname, int position, bool autosave, bool newGame)
{
	gamestate_t oldgs = gamestate;

	// Here the new level needs to be allocated.
	primaryLevel->DoLoadLevel(nextmapname, position, autosave, newGame);

	// Reset the global state for the new level.
	if (wipegamestate == GS_LEVEL)
		wipegamestate = GS_FORCEWIPE;

	if (gamestate != GS_TITLELEVEL)
	{
		gamestate = GS_LEVEL;
	}

	gameaction = ga_nothing;

	// clear cmd building stuff
	buttonMap.ResetButtonStates();

	SendItemUse = nullptr;
	SendItemDrop = nullptr;
	mousex = mousey = 0;
	sendpause = sendsave = sendturn180 = SendLand = false;
	LocalViewAngle = 0;
	LocalViewPitch = 0;
	paused = 0;

	if (demoplayback || oldgs == GS_STARTUP || oldgs == GS_TITLELEVEL)
		C_HideConsole();

	C_FlushDisplay();
	P_ResetSightCounters(true);
	I_UpdateWindowTitle();
}

void FLevelLocals::DoLoadLevel(const FString &nextmapname, int position, bool autosave, bool newGame)
{
	MapName = nextmapname;
	int i;

	if (NextSkill >= 0)
	{
		UCVarValue val;
		val.Int = NextSkill;
		gameskill->ForceSet (val, CVAR_Int);
		NextSkill = -1;
	}

	if (position == -1)
		position = laststartpos;
	else
		laststartpos = position;

	Init();
	StatusBar->DetachAllMessages ();

	// Force 'teamplay' to 'true' if need be.
	if (flags2 & LEVEL2_FORCETEAMPLAYON)
		teamplay = true;

	// Force 'teamplay' to 'false' if need be.
	if (flags2 & LEVEL2_FORCETEAMPLAYOFF)
		teamplay = false;

	if (isPrimaryLevel())
	{
		FString mapname = nextmapname;
		mapname.ToUpper();
		Printf(PRINT_HIGH | PRINT_NONOTIFY, "\n" TEXTCOLOR_NORMAL "%s\n\n" TEXTCOLOR_BOLD "%s - %s\n\n", console_bar, mapname.GetChars(), LevelName.GetChars());
	}

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = TexMan.GetTextureID (gameinfo.SkyFlatName.GetChars(), ETextureType::Flat, FTextureManager::TEXMAN_Overridable);

	// [RH] Set up details about sky rendering
	InitSkyMap (this);

	for (i = 0; i < MAXPLAYERS; i++)
	{ 
		if (PlayerInGame(i) && (deathmatch || Players[i]->playerstate == PST_DEAD))
			Players[i]->playerstate = PST_ENTER;	// [BC]
		memset (Players[i]->frags,0,sizeof(Players[i]->frags));
		if (!(dmflags2 & DF2_YES_KEEPFRAGS) && (alwaysapplydmflags || deathmatch))
			Players[i]->fragcount = 0;
	}

	if (changeflags & CHANGELEVEL_NOMONSTERS)
	{
		flags2 |= LEVEL2_NOMONSTERS;
	}
	else
	{
		flags2 &= ~LEVEL2_NOMONSTERS;
	}
	if (changeflags & CHANGELEVEL_PRERAISEWEAPON)
	{
		flags2 |= LEVEL2_PRERAISEWEAPON;
	}

	maptime = 0;

	if (newGame)
	{
		staticEventManager.NewGame();
	}

	P_SetupLevel (this, position, newGame);




	//Added by MC: Initialize bots.
	if (deathmatch)
	{
		BotInfo.Init ();
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

	starttime = gametic;

	UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the 
	int pnumerr = FinishTravel ();

	if (!FromSnapshot)
	{
		for (int i = 0; i<MAXPLAYERS; i++)
		{
			if (PlayerInGame(i) && Players[i]->mo != nullptr)
				P_PlayerStartStomp(Players[i]->mo, !deathmatch);
		}
	}

	// For each player, if they are viewing through a player, make sure it is themselves.
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (PlayerInGame(ii))
		{
			if (Players[ii]->camera == nullptr || Players[ii]->camera->player != nullptr)
			{
				Players[ii]->camera = Players[ii]->mo;
			}

			if (savegamerestore)
			{
				continue;
			}

			const bool fromSnapshot = FromSnapshot;
			localEventManager->PlayerEntered(ii, fromSnapshot && finishstate == FINISH_SameHub);

			if (fromSnapshot)
			{
				// ENTER scripts are being handled when the player gets spawned, this cannot be changed due to its effect on voodoo dolls.
				Behaviors.StartTypedScripts(SCRIPT_Return, Players[ii]->mo, true);
			}
		}
	}

	if (FromSnapshot)
	{
		// [Nash] run REOPEN scripts upon map re-entry
		Behaviors.StartTypedScripts(SCRIPT_Reopen, NULL, false);
	}

	StatusBar->AttachToPlayer (&players[consoleplayer]);
	//      unsafe world load
	staticEventManager.WorldLoaded();
	//      regular world load (savegames are handled internally)
	localEventManager->WorldLoaded();
	DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.
	

	// [RH] Always save the game when entering a new 
	if (autosave && !savegamerestore && disableautosave < 1)
	{
		CreateThinker<DAutosaver>();
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

void FLevelLocals::WorldDone (void) 
{
	gameaction = ga_worlddone; 


	//Added by mc
	if (deathmatch)
	{
		BotInfo.RemoveAllBots(this, consoleplayer != Net_Arbitrator);
	}

}
 
DEFINE_ACTION_FUNCTION(FLevelLocals, WorldDone)
{
	// This is just a dummy to make old status screens happy.
	return 0;
}

//==========================================================================
//
// Started the game loaded into a map from the console. Run at least one tic
// so it can properly render out in net games.
//
//==========================================================================

void G_DoMapWarp()
{
	gameaction = ga_nothing;
	Net_ResetCommands(true);
	Net_SetWaiting();
}

//==========================================================================
//
//
//==========================================================================

void G_DoWorldDone (void) 
{		
	Net_ResetCommands(true);
	gamestate = GS_LEVEL;
	if (nextlevel.IsEmpty())
	{
		// Don't crash if no next map is given. Just repeat the current one.
		Printf ("No next map specified.\n");
		nextlevel = primaryLevel->MapName;
	}
	primaryLevel->StartTravel ();
	G_DoLoadLevel (nextlevel, startpos, true, false);
	gameaction = ga_nothing;
	viewactive = true; 
	Net_SetWaiting();
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

void FLevelLocals::StartTravel ()
{
	if (deathmatch)
		return;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AActor *pawn = Players[i]->mo;
			AActor *inv;
			Players[i]->camera = nullptr;

			// Only living players travel. Dead ones get a new body on the new level.
			if (Players[i]->health > 0)
			{
				pawn->UnlinkFromWorld (nullptr);
				pawn->UnlinkBehaviorsFromLevel();
				int tid = pawn->tid;	// Save TID
				pawn->SetTID(0);
				pawn->tid = tid;		// Restore TID (but no longer linked into the hash chain)
				pawn->ChangeStatNum (STAT_TRAVELLING);
				pawn->DeleteAttachedLights();

				for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
				{
					inv->ChangeStatNum (STAT_TRAVELLING);
					inv->UnlinkFromWorld (nullptr);
					inv->UnlinkBehaviorsFromLevel();
					inv->DeleteAttachedLights();
				}
			}
		}
	}

	BotInfo.StartTravel ();
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

int FLevelLocals::FinishTravel ()
{
	auto it = GetThinkerIterator<AActor>(NAME_PlayerPawn, STAT_TRAVELLING);
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

		start = PickPlayerStart(pnum, 0);
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
		pawndup = SpawnPlayer(start, pnum, SPF_TEMPPLAYER);
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
			PlayerPointerSubstitution (oldpawn, pawn, true);
			oldpawn->Destroy();
		}
		if (pawndup != NULL)
		{
			pawndup->Destroy();
		}
		pawn->LinkToWorld (nullptr);
		pawn->LinkBehaviorsToLevel();
		pawn->ClearInterpolation();
		pawn->ClearFOVInterpolation();
		const int tid = pawn->tid;	// Save TID (actor isn't linked into the hash chain yet)
		pawn->tid = 0;				// Reset TID
		pawn->SetTID(tid);			// Set TID (and link actor into the hash chain)
		pawn->SetState(pawn->SpawnState);
		pawn->player->SendPitchLimits();

		for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
		{
			inv->ChangeStatNum (STAT_INVENTORY);
			inv->LinkToWorld (nullptr);
			P_FindFloorCeiling(inv, FFCF_ONLYSPAWNPOS);
			inv->LinkBehaviorsToLevel();

			IFVIRTUALPTRNAME(inv, NAME_Inventory, Travelled)
			{
				VMValue params[1] = { inv };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		if (ib_compatflags & BCOMPATF_RESETPLAYERSPEED)
		{
			pawn->Speed = pawn->GetDefault()->Speed;
		}

		IFVIRTUALPTRNAME(pawn, NAME_PlayerPawn, Travelled)
		{
			VMValue params[1] = { pawn };
			VMCall(func, params, 1, nullptr, 0);
		}
		// [ZZ] we probably don't want to fire any scripts before all players are in, especially with runNow = true.
		pawns[pawnsnum++] = pawn;
	}

	BotInfo.FinishTravel ();

	// make sure that, after travelling has completed, no travelling thinkers are left.
	// Since this list is excluded from regular thinker cleaning, anything that may survive through here
	// will endlessly multiply and severely break the following savegames or just simply crash on broken pointers.
	Thinkers.DestroyThinkersInList(STAT_TRAVELLING);
	ClientsideThinkers.DestroyThinkersInList(STAT_TRAVELLING);
	return failnum;
}
 
//==========================================================================
//
//
//==========================================================================

FLevelLocals::FLevelLocals() : Behaviors(this), tagManager(this)
{
	// Make sure that these point to the right data all the time.
	// This will be needed for as long as it takes to completely separate global UI state from per-level play state.
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		Players[i] = &players[i];
	}
	localEventManager = new EventManager(this);
}

FLevelLocals::~FLevelLocals()
{
	if (localEventManager) delete localEventManager;
	if (aabbTree) delete aabbTree;
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::Init()
{
	P_InitParticles(this);
	P_ClearParticles(this);
	
	gravity = sv_gravity * 35/TICRATE;
	aircontrol = sv_aircontrol;
	AirControlChanged();
	teamdamage = ::teamdamage;
	flags = 0;
	flags2 = 0;
	flags3 = 0;
	ImpactDecalCount = 0;
	frozenstate = 0;

	info = FindLevelInfo (MapName.GetChars());

	skyspeed1 = info->skyspeed1;
	skyspeed2 = info->skyspeed2;
	skymistspeed = info->skymistspeed;
	skytexture1 = TexMan.GetTextureID(info->SkyPic1.GetChars(), ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	skytexture2 = TexMan.GetTextureID(info->SkyPic2.GetChars(), ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	skymisttexture = TexMan.GetTextureID(info->SkyMistPic.GetChars(), ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_ReturnFirst);
	fadeto = info->fadeto;
	cdtrack = info->cdtrack;
	cdid = info->cdid;
	FromSnapshot = false;
	if (fadeto == 0)
	{
		if (strnicmp (info->FadeTable.GetChars(), "COLORMAP", 8) != 0)
		{
			flags |= LEVEL_HASFADETABLE;
		}
	}


	globalcolormap = R_ColormapNumForName(info->CustomColorMap.GetChars());
	airsupply = info->airsupply*TICRATE;
	outsidefog = info->outsidefog;
	WallVertLight = info->WallVertLight*2;
	WallHorizLight = info->WallHorizLight*2;
	if (info->gravity != 0.f)
	{
		if (info->gravity == DBL_MAX) gravity = 0;
		else gravity = info->gravity * 35/TICRATE;
	}
	if (info->aircontrol != 0.f)
	{
		aircontrol = info->aircontrol;
	}
	if (info->teamdamage != 0.f)
	{
		teamdamage = info->teamdamage;
	}

	AirControlChanged ();

	cluster_info_t *clus = FindClusterInfo (info->cluster);

	partime = info->partime;
	sucktime = info->sucktime;
	cluster = info->cluster;
	clusterflags = clus ? clus->flags : 0;
	flags |= info->flags;
	flags2 |= info->flags2;
	flags3 |= info->flags3;
	levelnum = info->levelnum;
	LightningSound = info->LightningSound;
	Music = info->Music;
	musicorder = info->musicorder;
	MusicVolume = 1.f;
	HasHeightSecs = false;

	LevelName = info->LookupLevelName();
	NextMap = info->NextMap;
	NextSecretMap = info->NextSecretMap;
	F1Pic = info->F1Pic;
	AuthorName = info->AuthorName;
	hazardcolor = info->hazardcolor;
	hazardflash = info->hazardflash;
	
	// GL fog stuff modifiable by SetGlobalFogParameter.
	fogdensity = info->fogdensity;
	outsidefogdensity = info->outsidefogdensity;
	skyfog = info->skyfog;
	deathsequence = info->deathsequence;

	pixelstretch = info->pixelstretch;

	compatflags->Callback();
	compatflags2->Callback();

	DefaultEnvironment = info->DefaultEnvironment;

	brightfog = info->brightfog < 0? gl_brightfog : !!info->brightfog;
	lightadditivesurfaces = info->lightadditivesurfaces < 0 ? gl_lightadditivesurfaces : !!info->lightadditivesurfaces;
	notexturefill = info->notexturefill < 0 ? gl_notexturefill : !!info->notexturefill;
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
		lumpname.Format("E%01dM%01d", episode, level);
	}
	return lumpname;
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::AirControlChanged ()
{
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
				arc.AddString(nullptr, wi.MapName.GetChars());
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
				FStringf key("%d", i);
				arc(key.GetChars(), players[i].cls);
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

	for (unsigned j = 0; j < resf->EntryCount(); j++)
	{
		auto name = resf->getName(j);
		auto ptr = strstr(name, ".map.json");
		if (ptr != nullptr)
		{
			ptrdiff_t maplen = ptr - name;
			FString mapname(name, (size_t)maplen);
			i = FindLevelInfo(mapname.GetChars());
			if (i != nullptr)
			{
				i->Snapshot = resf->GetRawData(j);
			}
		}
		else
		{
			auto ptr = strstr(name, ".mapd.json");
			if (ptr != nullptr)
			{
				ptrdiff_t maplen = ptr - name;
				FString mapname(name, (size_t)maplen);
				TheDefaultLevelInfo.Snapshot = resf->GetRawData(j);
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
			auto i = FindLevelInfo(str.GetChars());
			if (i != nullptr) i->flags |= LEVEL_VISITED;
		}
		arc.EndArray();
	}

	arc.Array("randomclasses", SinglePlayerClass, MAXPLAYERS);

	if (arc.BeginObject("playerclasses"))
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			FStringf key("%d", i);
			arc(key.GetChars(), players[i].cls);
		}
		arc.EndObject();
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
					arc(wi.MapName.GetChars(), wi.deferred);
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
	GC::Mark(ClientSideACSThinker);
	GC::Mark(automap);
	GC::Mark(interpolator.Head);
	GC::Mark(SequenceListHead);
	GC::Mark(BotInfo.firstthing);
	GC::Mark(BotInfo.body1);
	GC::Mark(BotInfo.body2);
	if (localEventManager)
	{
		GC::Mark(localEventManager->FirstEventHandler);
		GC::Mark(localEventManager->LastEventHandler);
	}
	Thinkers.MarkRoots();
	ClientsideThinkers.MarkRoots();
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
		S_ChangeMusic(mus->first.GetChars(), mus->second);
	else if (info->InterMusic.IsNotEmpty())
		S_ChangeMusic(info->InterMusic.GetChars(), info->intermusicorder);
	else
		S_ChangeMusic(gameinfo.intermissionMusic.GetChars(), gameinfo.intermissionOrder);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, SetInterMusic)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_STRING(map);
	self->SetInterMusic(map.GetChars());
	return 0;
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::SetMusicVolume(float f)
{
	MusicVolume = f;
	I_SetMusicVolume(f);
}

//============================================================================
//
//
//
//============================================================================

int FLevelLocals::GetInfighting()
{
	if (flags2 & LEVEL2_TOTALINFIGHTING) return 1;
	if (flags2 & LEVEL2_NOINFIGHTING) return -1;
	return G_SkillProperty(SKILLP_Infight);
}

//============================================================================
//
// transfers the compatiblity flag for old PointOnLineSide to each line.
// This gets checked in a frequently called worker function and the closer
// this info is to the data this function works on, the better.
//
//============================================================================

void FLevelLocals::SetCompatLineOnSide(bool state)
{
	int on = (state && (i_compatflags2 & COMPATF2_POINTONLINE));
	if (on) for (auto &l : lines) l.flags |= ML_COMPATSIDE;
	else for (auto &l : lines) l.flags &= ~ML_COMPATSIDE;
}

int FLevelLocals::GetCompatibility(int mask)
{
	if (info == nullptr) return mask;
	else return (mask & ~info->compatmask) | (info->compatflags & info->compatmask);
}

int FLevelLocals::GetCompatibility2(int mask)
{
	return (info == nullptr) ? mask
		: (mask & ~info->compatmask2) | (info->compatflags2 & info->compatmask2);
}

void FLevelLocals::ApplyCompatibility()
{
	int old = i_compatflags;
	i_compatflags = GetCompatibility(compatflags) | ii_compatflags;
	if ((old ^ i_compatflags) & COMPATF_POLYOBJ)
	{
		ClearAllSubsectorLinks();
	}
}

void FLevelLocals::ApplyCompatibility2()
{
	i_compatflags2 = GetCompatibility2(compatflags2) | ii_compatflags2;
}

AActor* FLevelLocals::SelectActorFromTID(int tid, size_t index, AActor* defactor)
{
	if (tid == 0)
		return defactor;

	AActor* actor = nullptr;
	size_t cur = 0u;
	auto it = GetActorIterator(tid);
	while ((actor = it.Next()) != nullptr)
	{
		if (cur == index)
			return actor;
		++cur;
	}

	return nullptr;
}

//==========================================================================
// IsPointInMap
//
// Checks to see if a point is inside the void or not.
// Made by dpJudas, modified and implemented by Major Cooke
//==========================================================================

int IsPointInMap(FLevelLocals *Level, double x, double y, double z)
{
	// This uses the render nodes because those are guaranteed to be GL nodes, meaning all subsectors are closed.
	subsector_t *subsector = Level->PointInRenderSubsector(FLOAT2FIXED(x), FLOAT2FIXED(y));
	if (!subsector) return false;

	for (uint32_t i = 0; i < subsector->numlines; i++)
	{
		// Skip double sided lines.
		seg_t *seg = subsector->firstline + i;
		if (seg->backsector != nullptr || seg->linedef == nullptr) continue;

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


void FLevelLocals::SetMusic()
{
	S_ChangeMusic(Music.GetChars(), musicorder);
}


DEFINE_ACTION_FUNCTION(FLevelLocals, GetClusterName)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals)
	cluster_info_t* cluster = FindClusterInfo(self->cluster);
	FString retval;

	if (cluster)
	{
		if (cluster->flags & CLUSTER_LOOKUPNAME)
			retval = GStrings.GetString(cluster->ClusterName);
		else
			retval = cluster->ClusterName;
	}
	ACTION_RETURN_STRING(retval);
}

DEFINE_ACTION_FUNCTION(FLevelLocals, GetEpisodeName)
{
	// this is a bit of a crapshoot because ZDoom never assigned a level to an episode
	// and retroactively fixing this is not possible.
	// This will need some heuristics to assign a proper episode to each existing level.
	// Stuff for later. for now this just checks the STAT module for the currently running episode,
	// which should be fine unless cheating.
	ACTION_RETURN_STRING(GStrings.localize(STAT_EpisodeName().GetChars()));
}

