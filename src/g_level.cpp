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
#include "a_strifeglobal.h"
#include "r_data/colormaps.h"
#include "farchive.h"
#include "r_renderer.h"

#include "gi.h"

#include "g_hub.h"

void STAT_StartNewGame(const char *lev);
void STAT_ChangeLevel(const char *newl);


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
		try
		{
			if (!P_CheckMapData(argv[1]))
			{
				Printf ("No map %s\n", argv[1]);
			}
			else
			{
				G_DeferedInitNew (argv[1]);
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
		Printf ("Usage: map <map name>\n");
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
			gameaction = ga_newgame2;
			d_skill = -1;
		}
	}
	else
	{
		Printf ("Usage: open <map file>\n");
	}
}


//==========================================================================
//
//
//==========================================================================

void G_NewInit ()
{
	int i;

	G_ClearSnapshots ();
	ST_SetNeedRefresh();
	netgame = false;
	multiplayer = false;
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
	EGameSpeed oldSpeed;
	bool wantFast;
	int i;

	G_ClearHubInfo();
	if (!savegamerestore)
	{
		G_ClearSnapshots ();
		P_RemoveDefereds ();

		// [RH] Mark all levels as not visited
		for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
			wadlevelinfos[i].flags = wadlevelinfos[i].flags & ~LEVEL_VISITED;
	}

	UnlatchCVars ();
	G_VerifySkill();
	UnlatchCVars ();

	if (paused)
	{
		paused = 0;
		S_ResumeSound (false);
	}

	if (StatusBar != NULL)
	{
		StatusBar->Destroy();
		StatusBar = NULL;
	}
	if (bTitleLevel)
	{
		StatusBar = new DBaseStatusBar (0);
	}
	else if (SBarInfoScript[SCRIPT_CUSTOM] != NULL)
	{
		int cstype = SBarInfoScript[SCRIPT_CUSTOM]->GetGameType();

		//Did the user specify a "base"
		if(cstype == GAME_Strife)
		{
			StatusBar = CreateStrifeStatusBar();
		}
		else if(cstype == GAME_Any) //Use the default, empty or custom.
		{
			StatusBar = CreateCustomStatusBar(SCRIPT_CUSTOM);
		}
		else
		{
			StatusBar = CreateCustomStatusBar(SCRIPT_DEFAULT);
		}
	}
	if (StatusBar == NULL)
	{
		if (gameinfo.gametype & (GAME_DoomChex|GAME_Heretic|GAME_Hexen))
		{
			StatusBar = CreateCustomStatusBar (SCRIPT_DEFAULT);
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			StatusBar = CreateStrifeStatusBar ();
		}
		else
		{
			StatusBar = new DBaseStatusBar (0);
		}
	}
	GC::WriteBarrier(StatusBar);
	StatusBar->AttachToPlayer (&players[consoleplayer]);
	StatusBar->NewGame ();
	setsizeneeded = true;

	if (gameinfo.gametype == GAME_Strife || (SBarInfoScript[SCRIPT_CUSTOM] != NULL && SBarInfoScript[SCRIPT_CUSTOM]->GetGameType() == GAME_Strife))
	{
		// Set the initial quest log text for Strife.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].SetLogText ("Find help");
		}
	}

	// [RH] If this map doesn't exist, bomb out
	if (!P_CheckMapData(mapname))
	{
		I_Error ("Could not find map %s\n", mapname);
	}

	oldSpeed = GameSpeed;
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

	if (mapname != level.mapname)
	{
		strcpy (level.mapname, mapname);
	}
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

void G_ChangeLevel(const char *levelname, int position, int flags, int nextSkill)
{
	level_info_t *nextinfo = NULL;

	if (unloading)
	{
		Printf (TEXTCOLOR_RED "Unloading scripts cannot exit the level again.\n");
		return;
	}

	if (levelname == NULL || *levelname == 0)
	{
		// end the game
		levelname = NULL;
		if (!strncmp(level.nextmap, "enDSeQ",6))
		{
			levelname = level.nextmap;	// If there is already an end sequence please leave it alone!
		}
		else 
		{
			nextlevel.Format("enDSeQ%04x", int(gameinfo.DefaultEndSequence));
		}
	}
	else if (strncmp(levelname, "enDSeQ", 6) != 0)
	{
		nextinfo = FindLevelInfo (levelname, false);
		if (nextinfo != NULL)
		{
			level_info_t *nextredir = nextinfo->CheckLevelRedirect();
			if (nextredir != NULL)
			{
				nextinfo = nextredir;
				levelname = nextinfo->mapname;
			}
		}
	}

	if (levelname != NULL) nextlevel = levelname;

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
	unloading = false;

	STAT_ChangeLevel(nextlevel);

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if ((level.flags & LEVEL_NOINTERMISSION) || (nextcluster == thiscluster))
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
			if ((multiplayer || level.flags2 & LEVEL2_ALLOWRESPAWN) && !deathmatch && player->playerstate == PST_DEAD)
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
	return level.nextmap;
}

const char *G_GetSecretExitMap()
{
	const char *nextmap = level.nextmap;

	if (level.secretmap[0] != 0)
	{
		if (P_CheckMapData(level.secretmap))
		{
			nextmap = level.secretmap;
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
	G_ChangeLevel(G_GetExitMap(), position, keepFacing ? CHANGELEVEL_KEEPFACING : 0);
}

void G_SecretExitLevel (int position) 
{
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

	if (gamestate == GS_TITLELEVEL)
	{
		strncpy (level.mapname, nextlevel, 255);
		G_DoLoadLevel (startpos, false);
		startpos = 0;
		viewactive = true;
		return;
	}

	// [RH] Mark this level as having been visited
	if (!(level.flags & LEVEL_CHANGEMAPCHEAT))
		FindLevelInfo (level.mapname)->flags |= LEVEL_VISITED;

	if (automapactive)
		AM_Stop ();

	wminfo.finished_ep = level.cluster - 1;
	wminfo.LName0 = TexMan[TexMan.CheckForTexture(level.info->pname, FTexture::TEX_MiscPatch)];
	wminfo.current = level.mapname;

	if (deathmatch &&
		(dmflags & DF_SAME_LEVEL) &&
		!(level.flags & LEVEL_CHANGEMAPCHEAT))
	{
		wminfo.next = level.mapname;
		wminfo.LName1 = wminfo.LName0;
	}
	else
	{
		level_info_t *nextinfo = FindLevelInfo (nextlevel, false);
		if (nextinfo == NULL || strncmp (nextlevel, "enDSeQ", 6) == 0)
		{
			wminfo.next = nextlevel;
			wminfo.LName1 = NULL;
		}
		else
		{
			wminfo.next = nextinfo->mapname;
			wminfo.LName1 = TexMan[TexMan.CheckForTexture(nextinfo->pname, FTexture::TEX_MiscPatch)];
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
		wminfo.plyr[i].in = playeringame[i];
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
			level.info->ClearSnapshot();
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

	if (!deathmatch &&
		((level.flags & LEVEL_NOINTERMISSION) ||
		((nextcluster == thiscluster) && (thiscluster->flags & CLUSTER_HUB))))
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

IMPLEMENT_CLASS (DAutosaver)

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

	Printf (
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"
			TEXTCOLOR_BOLD "%s - %s\n\n",
			level.mapname, level.LevelName.GetChars());

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
	sky1texture = TexMan.GetTexture (level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
	sky2texture = TexMan.GetTexture (level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);

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
	P_SetupLevel (level.mapname, position);

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
			starttime = I_GetTime (false);
			firstTime = false;
		}
	}

	level.starttime = gametic;
	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	G_FinishTravel ();
	// For each player, if they are viewing through a player, make sure it is themselves.
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (playeringame[ii] && (players[ii].camera == NULL || players[ii].camera->player != NULL))
		{
			players[ii].camera = players[ii].mo;
		}
	}
	StatusBar->AttachToPlayer (&players[consoleplayer]);
	P_DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.
	
	if (demoplayback || oldgs == GS_STARTUP || oldgs == GS_TITLELEVEL)
		C_HideConsole ();

	C_FlushDisplay ();

	// [RH] Always save the game when entering a new level.
	if (autosave && !savegamerestore && disableautosave < 1)
	{
		DAutosaver GCCNOWARN *dummy = new DAutosaver;
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
		FName endsequence = ENamedName(strtol(nextlevel.GetChars()+6, NULL, 16));
		// Strife needs a special case here to choose between good and sad ending. Bad is handled elsewherw.
		if (endsequence == NAME_Inter_Strife)
		{
			if (players[0].mo->FindInventory (QuestItemClasses[24]) ||
				players[0].mo->FindInventory (QuestItemClasses[27]))
			{
				endsequence = NAME_Inter_Strife_Good;
			}
			else
			{
				endsequence = NAME_Inter_Strife_Sad;
			}
		}

		F_StartFinale (thiscluster->MessageMusic, thiscluster->musicorder,
			thiscluster->cdtrack, thiscluster->cdid,
			thiscluster->FinaleFlat, thiscluster->ExitText,
			thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
			thiscluster->flags & CLUSTER_FINALEPIC,
			thiscluster->flags & CLUSTER_LOOKUPEXITTEXT,
			true, endsequence);
	}
	else
	{
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
		strncpy (level.mapname, nextlevel, 255);
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
				pawn->UnlinkFromWorld ();
				P_DelSector_List ();
				int tid = pawn->tid;	// Save TID
				pawn->RemoveFromHash ();
				pawn->tid = tid;		// Restore TID (but no longer linked into the hash chain)
				pawn->ChangeStatNum (STAT_TRAVELLING);

				for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
				{
					inv->ChangeStatNum (STAT_TRAVELLING);
					inv->UnlinkFromWorld ();
					P_DelSector_List ();
				}
			}
		}
	}
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

void G_FinishTravel ()
{
	TThinkerIterator<APlayerPawn> it (STAT_TRAVELLING);
	APlayerPawn *pawn, *pawndup, *oldpawn, *next;
	AInventory *inv;

	next = it.Next ();
	while ( (pawn = next) != NULL)
	{
		next = it.Next ();
		pawn->ChangeStatNum (STAT_PLAYER);
		pawndup = pawn->player->mo;
		assert (pawn != pawndup);
		if (pawndup == NULL)
		{ // Oh no! there was no start for this player!
			pawn->flags |= MF_NOSECTOR|MF_NOBLOCKMAP;
			pawn->Destroy ();
		}
		else
		{
			oldpawn = pawndup;

			// The player being spawned here is a short lived dummy and
			// must not start any ENTER script or big problems will happen.
			pawndup = P_SpawnPlayer (&playerstarts[pawn->player - players], int(pawn->player - players), SPF_TEMPPLAYER);
			if (!(changeflags & CHANGELEVEL_KEEPFACING))
			{
				pawn->angle = pawndup->angle;
				pawn->pitch = pawndup->pitch;
			}
			pawn->x = pawndup->x;
			pawn->y = pawndup->y;
			pawn->z = pawndup->z;
			pawn->velx = pawndup->velx;
			pawn->vely = pawndup->vely;
			pawn->velz = pawndup->velz;
			pawn->Sector = pawndup->Sector;
			pawn->floorz = pawndup->floorz;
			pawn->ceilingz = pawndup->ceilingz;
			pawn->dropoffz = pawndup->dropoffz;
			pawn->floorsector = pawndup->floorsector;
			pawn->floorpic = pawndup->floorpic;
			pawn->ceilingsector = pawndup->ceilingsector;
			pawn->ceilingpic = pawndup->ceilingpic;
			pawn->floorclip = pawndup->floorclip;
			pawn->waterlevel = pawndup->waterlevel;
			pawn->target = NULL;
			pawn->lastenemy = NULL;
			pawn->player->mo = pawn;
			pawn->player->camera = pawn;
			DObject::StaticPointerSubstitution (oldpawn, pawn);
			oldpawn->Destroy();
			pawndup->Destroy ();
			pawn->LinkToWorld ();
			pawn->AddToHash ();
			pawn->SetState(pawn->SpawnState);
			pawn->player->SendPitchLimits();

			for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
			{
				inv->ChangeStatNum (STAT_INVENTORY);
				inv->LinkToWorld ();
				inv->Travelled ();
			}
			if (ib_compatflags & BCOMPATF_RESETPLAYERSPEED)
			{
				pawn->Speed = pawn->GetDefault()->Speed;
			}
			if (level.FromSnapshot)
			{
				FBehavior::StaticStartTypedScripts (SCRIPT_Return, pawn, true);
			}
		}
	}
}
 
//==========================================================================
//
//
//==========================================================================

void G_InitLevelLocals ()
{
	level_info_t *info;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any
	NormalLight.Maps = realcolormaps;

	// [BB] Instead of just setting the color, we also have to reset Desaturate and build the lights.
	NormalLight.ChangeColor (PalEntry (255, 255, 255), 0);

	level.gravity = sv_gravity * 35/TICRATE;
	level.aircontrol = (fixed_t)(sv_aircontrol * 65536.f);
	level.teamdamage = teamdamage;
	level.flags = 0;
	level.flags2 = 0;

	info = FindLevelInfo (level.mapname);

	level.info = info;
	level.skyspeed1 = info->skyspeed1;
	level.skyspeed2 = info->skyspeed2;
	strncpy (level.skypic2, info->skypic2, 8);
	level.fadeto = info->fadeto;
	level.cdtrack = info->cdtrack;
	level.cdid = info->cdid;
	level.FromSnapshot = false;
	if (level.fadeto == 0)
	{
		R_SetDefaultColormap (info->fadetable);
		if (strnicmp (info->fadetable, "COLORMAP", 8) != 0)
		{
			level.flags |= LEVEL_HASFADETABLE;
		}
		/*
	}
	else
	{
		NormalLight.ChangeFade (level.fadeto);
		*/
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
		level.aircontrol = (fixed_t)(info->aircontrol * 65536.f);
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
	level.levelnum = info->levelnum;
	level.Music = info->Music;
	level.musicorder = info->musicorder;

	level.LevelName = level.info->LookupLevelName();
	strncpy (level.nextmap, info->nextmap, 10);
	level.nextmap[10] = 0;
	strncpy (level.secretmap, info->secretmap, 10);
	level.secretmap[10] = 0;
	strncpy (level.skypic1, info->skypic1, 8);
	level.skypic1[8] = 0;
	if (!level.skypic2[0])
		strncpy (level.skypic2, level.skypic1, 8);
	level.skypic2[8] = 0;

	compatflags.Callback();
	compatflags2.Callback();

	NormalLight.ChangeFade (level.fadeto);

	level.DefaultEnvironment = info->DefaultEnvironment;
	level.DefaultSkybox = NULL;
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
	return !(level.flags & LEVEL_JUMP_NO);
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
	return !(level.flags & LEVEL_CROUCH_NO);
}

//==========================================================================
//
//
//==========================================================================

bool FLevelLocals::IsFreelookAllowed() const
{
	if (level.flags & LEVEL_FREELOOK_NO)
		return false;
	if (level.flags & LEVEL_FREELOOK_YES)
		return true;
	return !(dmflags & DF_NO_FREELOOK);
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
	if (level.aircontrol <= 256)
	{
		level.airfriction = FRACUNIT;
	}
	else
	{
		// Friction is inversely proportional to the amount of control
		float fric = ((float)level.aircontrol/65536.f) * -0.0941f + 1.0004f;
		level.airfriction = (fixed_t)(fric * 65536.f);
	}
}

//==========================================================================
//
//
//==========================================================================

void G_SerializeLevel (FArchive &arc, bool hubLoad)
{
	int i = level.totaltime;
	
	Renderer->StartSerialize(arc);

	arc << level.flags
		<< level.flags2
		<< level.fadeto
		<< level.found_secrets
		<< level.found_items
		<< level.killed_monsters
		<< level.gravity
		<< level.aircontrol
		<< level.teamdamage
		<< level.maptime
		<< i;

	if (SaveVersion >= 3313)
	{
		arc << level.nextmusic;
	}

	// Hub transitions must keep the current total time
	if (!hubLoad)
		level.totaltime = i;

	if (arc.IsStoring ())
	{
		arc.WriteName (level.skypic1);
		arc.WriteName (level.skypic2);
	}
	else
	{
		strncpy (level.skypic1, arc.ReadName(), 8);
		strncpy (level.skypic2, arc.ReadName(), 8);
		sky1texture = TexMan.GetTexture (level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
		sky2texture = TexMan.GetTexture (level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
		R_InitSkyMap ();
	}

	G_AirControlChanged ();

	BYTE t;

	// Does this level have scrollers?
	if (arc.IsStoring ())
	{
		t = level.Scrolls ? 1 : 0;
		arc << t;
	}
	else
	{
		arc << t;
		if (level.Scrolls)
		{
			delete[] level.Scrolls;
			level.Scrolls = NULL;
		}
		if (t)
		{
			level.Scrolls = new FSectorScrollValues[numsectors];
			memset (level.Scrolls, 0, sizeof(level.Scrolls)*numsectors);
		}
	}

	FBehavior::StaticSerializeModuleStates (arc);
	if (arc.IsLoading()) interpolator.ClearInterpolations();
	P_SerializeThinkers (arc, hubLoad);
	P_SerializeWorld (arc);
	P_SerializePolyobjs (arc);
	P_SerializeSubsectors(arc);
	StatusBar->Serialize (arc);

	if (SaveVersion >= 4222)
	{ // This must be done *after* thinkers are serialized.
		arc << level.DefaultSkybox;
	}

	arc << level.total_monsters << level.total_items << level.total_secrets;

	// Does this level have custom translations?
	FRemapTable *trans;
	WORD w;
	if (arc.IsStoring ())
	{
		for (unsigned int i = 0; i < translationtables[TRANSLATION_LevelScripted].Size(); ++i)
		{
			trans = translationtables[TRANSLATION_LevelScripted][i];
			if (trans != NULL && !trans->IsIdentity())
			{
				w = WORD(i);
				arc << w;
				trans->Serialize(arc);
			}
		}
		w = 0xffff;
		arc << w;
	}
	else
	{
		while (arc << w, w != 0xffff)
		{
			trans = translationtables[TRANSLATION_LevelScripted].GetVal(w);
			if (trans == NULL)
			{
				trans = new FRemapTable;
				translationtables[TRANSLATION_LevelScripted].SetVal(w, trans);
			}
			trans->Serialize(arc);
		}
	}

	// This must be saved, too, of course!
	FCanvasTextureInfo::Serialize (arc);
	AM_SerializeMarkers(arc);

	P_SerializePlayers (arc, hubLoad);
	P_SerializeSounds (arc);
	if (arc.IsLoading())
	{
		for (i = 0; i < numsectors; i++)
		{
			P_Recalculate3DFloors(&sectors[i]);
		}
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != NULL)
			{
				players[i].mo->SetupWeaponSlots();
			}
		}
	}
	Renderer->EndSerialize(arc);
}

//==========================================================================
//
// Archives the current level
//
//==========================================================================

void G_SnapshotLevel ()
{
	if (level.info->snapshot)
		delete level.info->snapshot;

	if (level.info->isValid())
	{
		level.info->snapshotVer = SAVEVER;
		level.info->snapshot = new FCompressedMemFile;
		level.info->snapshot->Open ();

		FArchive arc (*level.info->snapshot);

		SaveVersion = SAVEVER;
		G_SerializeLevel (arc, false);
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
	if (level.info->snapshot == NULL)
		return;

	if (level.info->isValid())
	{
		SaveVersion = level.info->snapshotVer;
		level.info->snapshot->Reopen ();
		FArchive arc (*level.info->snapshot);
		if (hubLoad)
			arc.SetHubTravel ();
		G_SerializeLevel (arc, hubLoad);
		arc.Close ();
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
					if (playeringame[i] && players[i].morphTics && players[i].mo->tracer == pawn)
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
	}
	// No reason to keep the snapshot around once the level's been entered.
	level.info->ClearSnapshot();
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

static void writeMapName (FArchive &arc, const char *name)
{
	BYTE size;
	if (name[7] != 0)
	{
		size = 8;
	}
	else
	{
		size = (BYTE)strlen (name);
	}
	arc << size;
	arc.Write (name, size);
}

//==========================================================================
//
//
//==========================================================================

static void writeSnapShot (FArchive &arc, level_info_t *i)
{
	arc << i->snapshotVer;
	writeMapName (arc, i->mapname);
	i->snapshot->Serialize (arc);
}

//==========================================================================
//
//
//==========================================================================

void G_WriteSnapshots (FILE *file)
{
	unsigned int i;

	for (i = 0; i < wadlevelinfos.Size(); i++)
	{
		if (wadlevelinfos[i].snapshot)
		{
			FPNGChunkArchive arc (file, SNAP_ID);
			writeSnapShot (arc, (level_info_t *)&wadlevelinfos[i]);
		}
	}
	if (TheDefaultLevelInfo.snapshot != NULL)
	{
		FPNGChunkArchive arc (file, DSNP_ID);
		writeSnapShot(arc, &TheDefaultLevelInfo);
	}

	FPNGChunkArchive *arc = NULL;
	
	// Write out which levels have been visited
	for (i = 0; i < wadlevelinfos.Size(); ++i)
	{
		if (wadlevelinfos[i].flags & LEVEL_VISITED)
		{
			if (arc == NULL)
			{
				arc = new FPNGChunkArchive (file, VIST_ID);
			}
			writeMapName (*arc, wadlevelinfos[i].mapname);
		}
	}

	if (arc != NULL)
	{
		BYTE zero = 0;
		*arc << zero;
		delete arc;
	}

	// Store player classes to be used when spawning a random class
	if (multiplayer)
	{
		FPNGChunkArchive arc2 (file, RCLS_ID);
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			SBYTE cnum = SinglePlayerClass[i];
			arc2 << cnum;
		}
	}

	// Store player classes that are currently in use
	FPNGChunkArchive arc3 (file, PCLS_ID);
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		BYTE pnum;
		if (playeringame[i])
		{
			pnum = i;
			arc3 << pnum;
			arc3.UserWriteClass (players[i].cls);
		}
		pnum = 255;
		arc3 << pnum;
	}
}

//==========================================================================
//
//
//==========================================================================

void G_ReadSnapshots (PNGHandle *png)
{
	DWORD chunkLen;
	BYTE namelen;
	char mapname[256];
	level_info_t *i;

	G_ClearSnapshots ();

	chunkLen = (DWORD)M_FindPNGChunk (png, SNAP_ID);
	while (chunkLen != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), SNAP_ID, chunkLen);
		DWORD snapver;

		arc << snapver;
		arc << namelen;
		arc.Read (mapname, namelen);
		mapname[namelen] = 0;
		i = FindLevelInfo (mapname);
		i->snapshotVer = snapver;
		i->snapshot = new FCompressedMemFile;
		i->snapshot->Serialize (arc);
		chunkLen = (DWORD)M_NextPNGChunk (png, SNAP_ID);
	}

	chunkLen = (DWORD)M_FindPNGChunk (png, DSNP_ID);
	if (chunkLen != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), DSNP_ID, chunkLen);
		DWORD snapver;

		arc << snapver;
		arc << namelen;
		arc.Read (mapname, namelen);
		TheDefaultLevelInfo.snapshotVer = snapver;
		TheDefaultLevelInfo.snapshot = new FCompressedMemFile;
		TheDefaultLevelInfo.snapshot->Serialize (arc);
	}

	chunkLen = (DWORD)M_FindPNGChunk (png, VIST_ID);
	if (chunkLen != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), VIST_ID, chunkLen);

		arc << namelen;
		while (namelen != 0)
		{
			arc.Read (mapname, namelen);
			mapname[namelen] = 0;
			i = FindLevelInfo (mapname);
			i->flags |= LEVEL_VISITED;
			arc << namelen;
		}
	}

	chunkLen = (DWORD)M_FindPNGChunk (png, RCLS_ID);
	if (chunkLen != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), PCLS_ID, chunkLen);
		SBYTE cnum;

		for (DWORD j = 0; j < chunkLen; ++j)
		{
			arc << cnum;
			SinglePlayerClass[j] = cnum;
		}
	}

	chunkLen = (DWORD)M_FindPNGChunk (png, PCLS_ID);
	if (chunkLen != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), RCLS_ID, chunkLen);
		BYTE pnum;

		arc << pnum;
		while (pnum != 255)
		{
			arc.UserReadClass (players[pnum].cls);
			arc << pnum;
		}
	}
	png->File->ResetFilePtr();
}

//==========================================================================

CCMD(listsnapshots)
{
	for (unsigned i = 0; i < wadlevelinfos.Size(); ++i)
	{
		FCompressedMemFile *snapshot = wadlevelinfos[i].snapshot;
		if (snapshot != NULL)
		{
			unsigned int comp, uncomp;
			snapshot->GetSizes(comp, uncomp);
			Printf("%s (%u -> %u bytes)\n", wadlevelinfos[i].mapname, comp, uncomp);
		}
	}
}

//==========================================================================
//
//
//==========================================================================

static void writeDefereds (FArchive &arc, level_info_t *i)
{
	writeMapName (arc, i->mapname);
	arc << i->defered;
}

//==========================================================================
//
//
//==========================================================================

void P_WriteACSDefereds (FILE *file)
{
	FPNGChunkArchive *arc = NULL;

	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
	{
		if (wadlevelinfos[i].defered)
		{
			if (arc == NULL)
			{
				arc = new FPNGChunkArchive (file, ACSD_ID);
			}
			writeDefereds (*arc, (level_info_t *)&wadlevelinfos[i]);
		}
	}

	if (arc != NULL)
	{
		// Signal end of defereds
		BYTE zero = 0;
		*arc << zero;
		delete arc;
	}
}

//==========================================================================
//
//
//==========================================================================

void P_ReadACSDefereds (PNGHandle *png)
{
	BYTE namelen;
	char mapname[256];
	size_t chunklen;

	P_RemoveDefereds ();

	if ((chunklen = M_FindPNGChunk (png, ACSD_ID)) != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), ACSD_ID, chunklen);

		arc << namelen;
		while (namelen)
		{
			arc.Read (mapname, namelen);
			mapname[namelen] = 0;
			level_info_t *i = FindLevelInfo (mapname);
			if (i == NULL)
			{
				I_Error ("Unknown map '%s' in savegame", mapname);
			}
			arc << i->defered;
			arc << namelen;
		}
	}
	png->File->ResetFilePtr();
}


//==========================================================================
//
//
//==========================================================================

void FLevelLocals::Tick ()
{
	// Reset carry sectors
	if (Scrolls != NULL)
	{
		memset (Scrolls, 0, sizeof(*Scrolls)*numsectors);
	}
}

//==========================================================================
//
//
//==========================================================================

void FLevelLocals::AddScroller (DScroller *scroller, int secnum)
{
	if (secnum < 0)
	{
		return;
	}
	if (Scrolls == NULL)
	{
		Scrolls = new FSectorScrollValues[numsectors];
		memset (Scrolls, 0, sizeof(*Scrolls)*numsectors);
	}
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
		MapData *map = P_OpenMapData(info->mapname, true);

		if (map != NULL)
		{
			Printf("%s: '%s' (%s)\n", info->mapname, info->LookupLevelName().GetChars(),
				Wads.GetWadName(Wads.GetLumpFile(map->lumpnum)));
			delete map;
		}
	}
}




