#include "g_level.h"
#include "g_game.h"
#include "s_sound.h"
#include "d_event.h"
#include "m_random.h"
#include "doomstat.h"
#include "wi_stuff.h"
#include "r_data.h"
#include "w_wad.h"
#include "am_map.h"
#include "c_dispatch.h"
#include "z_zone.h"
#include "i_system.h"
#include "p_setup.h"
#include "r_sky.h"
#include "c_console.h"
#include "f_finale.h"

extern boolean netdemo;

extern int mousex, mousey, joyxmove, joyymove, Impulse;
extern boolean sendpause, sendsave, sendcenterview;

void *statcopy;					// for statistics driver

level_locals_t level;			// info about current level

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//
static skill_t	d_skill;
static char		d_mapname[9];

void G_DeferedInitNew (skill_t skill, char *mapname)
{
	d_skill = skill;
	strncpy (d_mapname, mapname, 8);
	gameaction = ga_newgame;
}


void G_DoNewGame (void)
{
	demoplayback = false;
	netdemo = false;
	netgame = false;
	deathmatch = 0;
	// playeringame[1] = playeringame[2] = playeringame[3] = 0;
	{
		int i;

		for (i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
	}
	respawnparm = false;
	fastparm = 0;
	nomonsters = false;
	consoleplayer = 0;
	G_InitNew (d_skill, d_mapname);
	gameaction = ga_nothing;
}

// The sky texture to be used instead of the F_SKY1 dummy.
extern	int 	skytexture;


void G_InitNew (skill_t skill, char *mapname)
{
	int 			i;

	if (paused)
	{
		paused = false;
		S_ResumeSound ();
	}


	if (skill > sk_nightmare)
		skill = sk_nightmare;


	// [RH] If this map doesn't exist, use something that should
	if (W_CheckNumForName (mapname) == -1) {
		mapname = CalcMapName (1, 1);
	}
	M_ClearRandom ();

	if (skill == sk_nightmare || respawnparm )
		respawnmonsters = true;
	else
		respawnmonsters = false;

	if (fastparm || (skill == sk_nightmare && gameskill->value != sk_nightmare) )
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
			states[i].tics >>= 1;
		mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
	}
	else if (skill != sk_nightmare && gameskill->value == sk_nightmare)
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
			states[i].tics <<= 1;
		mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
	}


	// force players to be initialized upon first level load
	for (i=0 ; i<MAXPLAYERS ; i++)
		players[i].playerstate = PST_REBORN;

	usergame = true;				// will be set false if a demo
	paused = false;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	SetCVarFloat (gameskill, skill);
 
	viewactive = true;

	strncpy (level.mapname, mapname, 8);
	G_DoLoadLevel ();
}

//
// G_DoCompleted
//
boolean 		secretexit;
extern char*	pagename;
 
void G_ExitLevel (void)
{
	secretexit = false;
	gameaction = ga_completed;
}

// Here's for the german edition.
void G_SecretExitLevel (void) 
{
	// IF NO WOLF3D LEVELS, NO SECRET EXIT!
	if ( (gamemode == commercial)
	  && (W_CheckNumForName("map31")<0))
		secretexit = false;
	else
		secretexit = true;
	gameaction = ga_completed;
}

void G_DoCompleted (void) 
{
	int 			i; 

	gameaction = ga_nothing;

	for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i])
			G_PlayerFinishLevel (i);		// take away cards and stuff 

	if (automapactive)
		AM_Stop ();

	if (level.flags & LEVEL_NOINTERMISSION) {
		G_WorldDone ();
		return;
	}

	wminfo.didsecret = players[consoleplayer].didsecret;
	wminfo.epsd = level.cluster -1;
	wminfo.last = level.levelnum -1;
	
	strncpy (wminfo.lname0, level.info->pname, 8);
	wminfo.next[0] = 0;
	if (secretexit) {
		if (W_CheckNumForName (level.secretmap) != -1) {
			strncpy (wminfo.next, level.secretmap, 8);
			strncpy (wminfo.lname1, FindLevelInfo (level.secretmap)->pname, 8);
		}
	}
	if (!wminfo.next[0]) {
		strncpy (wminfo.next, level.nextmap, 8);
		strncpy (wminfo.lname1, FindLevelInfo (level.nextmap)->pname, 8);
	}
	wminfo.maxkills = level.total_monsters;
	wminfo.maxitems = level.total_items;
	wminfo.maxsecret = level.total_secrets;
	wminfo.maxfrags = 0;
	wminfo.partime = TICRATE * level.partime;
	wminfo.pnum = consoleplayer;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		wminfo.plyr[i].in = playeringame[i];
		wminfo.plyr[i].skills = players[i].killcount;
		wminfo.plyr[i].sitems = players[i].itemcount;
		wminfo.plyr[i].ssecret = players[i].secretcount;
		wminfo.plyr[i].stime = level.time;
		memcpy (wminfo.plyr[i].frags, players[i].frags
				, sizeof(wminfo.plyr[i].frags));
	}

	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

	if (statcopy)
		memcpy (statcopy, &wminfo, sizeof(wminfo));
		
	WI_Start (&wminfo);
}

//
// G_DoLoadLevel 
//
extern	gamestate_t 	wipegamestate; 
 
void G_DoLoadLevel (void) 
{ 
	int i; 

	G_InitLevelLocals ();

	Printf ("\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n%s\n\n",
			level.level_name);
	
	C_HideConsole ();
	C_FlushDisplay ();

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = R_FlatNumForName ( SKYFLATNAME );

	// DOOM determines the sky texture to be used
	// depending on the current episode, and the game version.
	// [RH] Fetch sky from level_locals_t
	skytexture = R_TextureNumForName (level.skypic1);

	if (wipegamestate == GS_LEVEL) 
		wipegamestate = -1; 			// force a wipe 

	gamestate = GS_LEVEL; 

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		if (playeringame[i] && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_REBORN; 
		memset (players[i].frags,0,sizeof(players[i].frags)); 
	} 
				 
	P_SetupLevel (level.mapname, 0, gameskill->value);	 
	displayplayer = consoleplayer;				// view the guy you are playing    
	gameaction = ga_nothing; 
	Z_CheckHeap ();
	
	// clear cmd building stuff
	Impulse = 0;
	Actions &= ACTION_MLOOK | ACTION_KLOOK;
	joyxmove = joyymove = 0; 
	mousex = mousey = 0; 
	sendpause = sendsave = paused = sendcenterview = false; 
}


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;

	gameaction = ga_worlddone; 

	if (level.flags & LEVEL_SECRET) 
		players[consoleplayer].didsecret = true; 

	thiscluster = FindClusterInfo (level.cluster);
	if (!strncmp (level.nextmap, "EndGame", 7)) {
		F_StartFinale (thiscluster->messagemusic, thiscluster->finaleflat, thiscluster->exittext);
	} else {
		nextcluster = FindClusterInfo (FindLevelInfo (level.nextmap)->cluster);

		if (nextcluster->cluster != level.cluster) {
			// Only start the finale if the next level's cluster is different than the current one.
			if (nextcluster->entertext) {
				F_StartFinale (nextcluster->messagemusic, nextcluster->finaleflat, nextcluster->exittext);
			} else if (thiscluster->exittext) {
				F_StartFinale (thiscluster->messagemusic, thiscluster->finaleflat, thiscluster->exittext);
			}
		}
	}
} 
 
void G_DoWorldDone (void) 
{		 
	gamestate = GS_LEVEL;
	strncpy (level.mapname, wminfo.next, 8);
	G_DoLoadLevel (); 
	gameaction = ga_nothing; 
	viewactive = true; 
} 
 


void G_InitLevelLocals ()
{
	level_info_t *info;

	info = FindLevelInfo (level.mapname);

	level.info = info;
	level.starttime = I_GetTime ();
	if (info->level_name) {
		level.partime = info->partime;
		level.cluster = info->cluster;
		level.flags = info->flags;
		level.levelnum = info->levelnum;

		strncpy (level.level_name, info->level_name, 63);
		strncpy (level.nextmap, info->nextmap, 8);
		strncpy (level.secretmap, info->secretmap, 8);
		strncpy (level.music, info->music, 8);
		strncpy (level.skypic1, info->skypic1, 8);
		strncpy (level.skypic2, info->skypic2, 8);
	} else {
		level.partime = level.cluster = 0;
		strcpy (level.level_name, "Unnamed");
		level.nextmap[0] =
			level.secretmap[0] =
			level.music[0] = 0;
			strcpy (level.skypic1, "SKY1");
			strcpy (level.skypic2, "SKY1");
		level.flags = 0;
		level.levelnum = 1;
	}
}

char *CalcMapName (int episode, int level)
{
	static char lumpname[9];

	if (gamemode == commercial) {
		sprintf (lumpname, "MAP%02d", level);
	} else {
		lumpname[0] = 'E';
		lumpname[1] = '0' + episode;
		lumpname[2] = 'M';
		lumpname[3] = '0' + level;
		lumpname[4] = 0;
	}
	return lumpname;
}

level_info_t *FindLevelInfo (char *mapname)
{
	level_info_t *i;

	i = LevelInfos;
	while (i->level_name) {
		if (!strnicmp (i->mapname, mapname, 8))
			break;
		i++;
	}
	return i;
}

cluster_info_t *FindClusterInfo (int cluster)
{
	cluster_info_t *i;

	i = ClusterInfos;
	while (i->cluster && i->cluster != cluster)
		i++;

	return i;
}

// Static level info from original game
// Default to English strings for level names;
// localization code can change them to something else.

level_info_t LevelInfos[] = {
	// Registered/Retail Episode 1
	{
		"E1M1",
		1,
		"Hangar",
		"WILV00",
		"E1M2",
		"E1M9",
		30,
		"SKY1",
		"SKY1",
		"D_E1M1",
		0,
		1
	},
	{
		"E1M2",
		2,
		"Nuclear Plant",
		"WILV01",
		"E1M3",
		"E1M9",
		75,
		"SKY1",
		"SKY1",
		"D_E1M2",
		0,
		1
	},
	{
		"E1M3",
		3,
		"Toxin Refinery",
		"WILV02",
		"E1M4",
		"E1M9",
		120,
		"SKY1",
		"SKY1",
		"D_E1M3",
		0,
		1
	},
	{
		"E1M4",
		4,
		"Command Control",	
		"WILV03",
		"E1M5",
		"E1M9",
		90,
		"SKY1",
		"SKY1",
		"D_E1M4",
		0,
		1
	},
	{
		"E1M5",
		5,
		"Phobos Lab",
		"WILV04",
		"E1M6",
		"E1M9",
		165,
		"SKY1",
		"SKY1",
		"D_E1M5",
		0,
		1
	},
	{
		"E1M6",
		6,
		"Central Processing",
		"WILV05",
		"E1M7",
		"E1M9",
		180,
		"SKY1",
		"SKY1",
		"D_E1M6",
		0,
		1
	},
	{
		"E1M7",
		7,
		"Computer Station",
		"WILV06",
		"E1M8",
		"E1M9",
		180,
		"SKY1",
		"SKY1",
		"D_E1M7",
		0,
		1
	},
	{
		"E1M8",
		8,
		"Phobos Anomaly",
		"WILV07",
		"EndGame1",
		"E1M9",
		30,
		"SKY1",
		"SKY1",
		"D_E1M8",
		LEVEL_NOINTERMISSION,
		1
	},
	{
		"E1M9",
		9,
		"Military Base",
		"WILV08",
		"E1M4",
		"E1M4",
		165,
		"SKY1",
		"SKY1",
		"D_E1M9",
		LEVEL_SECRET,
		1
	},

	// Registered/Retail Episode 2
	{
		"E2M1",
		11,
		"Deimos Anomaly",
		"WILV10",
		"E2M2",
		"E2M9",
		90,
		"SKY2",
		"SKY2",
		"D_E2M1",
		0,
		2
	},

	{
		"E2M2",
		12,
		"Containment Area",
		"WILV11",
		"E2M3",
		"E2M9",
		90,
		"SKY2",
		"SKY2",
		"D_E2M2",
		0,
		2
	},
	{
		"E2M3",
		13,
		"Refinery",
		"WILV12",
		"E2M4",
		"E2M9",
		90,
		"SKY2",
		"SKY2",
		"D_E2M3",
		0,
		2
	},
	{
		"E2M4",
		14,
		"Deimos Lab",
		"WILV13",
		"E2M5",
		"E2M9",
		120,
		"SKY2",
		"SKY2",
		"D_E2M4",
		0,
		2
	},
	{
		"E2M5",
		15,
		"Command Center",
		"WILV14",
		"E2M6",
		"E2M9",
		90,
		"SKY2",
		"SKY2",
		"D_E2M5",
		0,
		2
	},
	{
		"E2M6",
		16,
		"Halls of the Damned",
		"WILV15",
		"E2M7",
		"E2M9",
		360,
		"SKY2",
		"SKY2",
		"D_E2M6",
		0,
		2
	},
	{
		"E2M7",
		17,
		"Spawning Vats",
		"WILV16",
		"E2M8",
		"E2M9",
		240,
		"SKY2",
		"SKY2",
		"D_E2M7",
		0,
		2
	},
	{
		"E2M8",
		18,
		"Tower of Babel",
		"WILV17",
		"EndGame2",
		"E2M9",
		30,
		"SKY2",
		"SKY2",
		"D_E2M8",
		LEVEL_NOINTERMISSION,
		2
	},
	{
		"E2M9",
		19,
		"Fortress of Mystery",
		"WILV18",
		"E2M6",
		"E2M6",
		170,
		"SKY2",
		"SKY2",
		"D_E2M9",
		LEVEL_SECRET,
		2
	},

	// Registered/Retail Episode 3

	{
		"E3M1",
		21,
		"Hell Keep",
		"WILV20",
		"E3M2",
		"E3M9",
		90,
		"SKY3",
		"SKY3",
		"D_E3M1",
		0,
		3
	},
	{
		"E3M2",
		22,
		"Slough of Despair",
		"WILV21",
		"E3M3",
		"E3M9",
		45,
		"SKY3",
		"SKY3",
		"D_E3M2",
		0,
		3
	},
	{
		"E3M3",
		23,
		"Pandemonium",
		"WILV22",
		"E3M4",
		"E3M9",
		90,
		"SKY3",
		"SKY3",
		"D_E3M3",
		0,
		3
	},
	{
		"E3M4",
		24,
		"House of Pain",
		"WILV23",
		"E3M5",
		"E3M9",
		150,
		"SKY3",
		"SKY3",
		"D_E3M4",
		0,
		3
	},
	{
		"E3M5",
		25,
		"Unholy Cathedral",
		"WILV24",
		"E3M6",
		"E3M9",
		90,
		"SKY3",
		"SKY3",
		"D_E3M5",
		0,
		3
	},
	{
		"E3M6",
		26,
		"Mt. Erebus",
		"WILV25",
		"E3M7",
		"E3M9",
		90,
		"SKY3",
		"SKY3",
		"D_E3M6",
		0,
		3
	},
	{
		"E3M7",
		27,
		"Limbo",
		"WILV26",
		"E3M8",
		"E3M9",
		165,
		"SKY3",
		"SKY3",
		"D_E3M7",
		0,
		3
	},
	{
		"E3M8",
		28,
		"Dis",
		"WILV27",
		"EndGame3",
		"E3M9",
		30,
		"SKY3",
		"SKY3",
		"D_E3M8",
		LEVEL_NOINTERMISSION,
		3
	},
	{
		"E3M9",
		29,
		"Warrens",
		"WILV28",
		"E3M7",
		"E3M7",
		135,
		"SKY3",
		"SKY3",
		"D_E3M9",
		LEVEL_SECRET,
		3
	},

	// Retail Episode 4
	{
		"E4M1",
		31,
		"Hell Beneath",
		"WILV30",
		"E4M2",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E3M4",
		0,
		4
	},
	{
		"E4M2",
		32,
		"Perfect Hatred",
		"WILV31",
		"E4M3",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E3M2",
		0,
		4
	},
	{
		"E4M3",
		33,
		"Sever The Wicked",
		"WILV32",
		"E4M4",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E3M3",
		0,
		4
	},
	{
		"E4M4",
		34,
		"Unruly Evil",
		"WILV33",
		"E4M5",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E1M5",
		0,
		4
	},
	{
		"E4M5",
		35,
		"They Will Repent",
		"WILV34",
		"E4M6",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E2M7",
		0,
		4
	},
	{
		"E4M6",
		36,
		"Against Thee Wickedly",
		"WILV35",
		"E4M7",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E2M4",
		0,
		4
	},
	{
		"E4M7",
		37,
		"And Hell Followed",
		"WILV36",
		"E4M8",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E2M6",
		0,
		4
	},
	{
		"E4M8",
		38,
		"Unto The Cruel",
		"WILV37",
		"EndGame4",
		"E4M9",
		0,
		"SKY4",
		"SKY4",
		"D_E2M5",
		LEVEL_NOINTERMISSION,
		4
	},
	{
		"E4M9",
		39,
		"Fear",
		"WILV38",
		"E4M3",
		"E4M3",
		0,
		"SKY4",
		"SKY4",
		"D_E1M9",
		LEVEL_SECRET,
		4
	},

	// DOOM 2 Levels

	{
		"MAP01",
		1,
		"entryway",
		"CWILV00",
		"MAP02",
		"MAP02",
		30,
		"SKY1",
		"SKY1",
		"D_RUNNIN",
		0,
		5
	},
	{
		"MAP02",
		2,
		"underhalls",
		"CWILV01",
		"MAP03",
		"MAP03",
		90,
		"SKY1",
		"SKY1",
		"D_STALKS",
		0,
		5
	},
	{
		"MAP03",
		3,
		"the gantlet",
		"CWILV02",
		"MAP04",
		"MAP04",
		120,
		"SKY1",
		"SKY1",
		"D_COUNTD",
		0,
		5
	},
	{
		"MAP04",
		4,
		"the focus",
		"CWILV03",
		"MAP05",
		"MAP05",
		120,
		"SKY1",
		"SKY1",
		"D_BETWEE",
		0,
		5
	},
	{
		"MAP05",
		5,
		"the waste tunnels",
		"CWILV04",
		"MAP06",
		"MAP06",
		90,
		"SKY1",
		"SKY1",
		"D_DOOM",
		0,
		5
	},
	{
		"MAP06",
		6,
		"the crusher",
		"CWILV05",
		"MAP07",
		"MAP07",
		150,
		"SKY1",
		"SKY1",
		"D_THE_DA",
		0,
		5
	},
	{
		"MAP07",
		7,
		"dead simple",
		"CWILV06",
		"MAP08",
		"MAP08",
		120,
		"SKY1",
		"SKY1",
		"D_SHAWN",
		0,
		6
	},
	{
		"MAP08",
		8,
		"tricks and traps",
		"CWILV07",
		"MAP09",
		"MAP09",
		120,
		"SKY1",
		"SKY1",
		"D_DDTBLU",
		0,
		6
	},
	{
		"MAP09",
		9,
		"the pit",
		"CWILV08",
		"MAP10",
		"MAP10",
		270,
		"SKY1",
		"SKY1",
		"D_IN_CIT",
		0,
		6
	},
	{
		"MAP10",
		10,
		"refueling base",
		"CWILV09",
		"MAP11",
		"MAP11",
		90,
		"SKY1",
		"SKY1",
		"D_DEAD",
		0,
		6
	},
	{
		"MAP11",
		11,
		"'o' of destruction!",
		"CWILV10",
		"MAP12",
		"MAP12",
		210,
		"SKY1",
		"SKY1",
		"D_STLKS2",
		0,
		6
	},
	{
		"MAP12",
		12,
		"the factory",
		"CWILV11",
		"MAP13",
		"MAP13",
		150,
		"SKY2",
		"SKY2",
		"D_THEDA2",
		0,
		7
	},
	{
		"MAP13",
		13,
		"downtown",
		"CWILV12",
		"MAP14",
		"MAP14",
		150,
		"SKY2",
		"SKY2",
		"D_DOOM2",
		0,
		7
	},
	{
		"MAP14",
		14,
		"the inmost dens",
		"CWILV13",
		"MAP15",
		"MAP15",
		150,
		"SKY2",
		"SKY2",
		"D_DDTBL2",
		0,
		7
	},
	{
		"MAP15",
		15,
		"industrial zone",
		"CWILV14",
		"MAP16",
		"MAP31",
		210,
		"SKY2",
		"SKY2",
		"D_RUNNI2",
		0,
		7
	},
	{
		"MAP16",
		16,
		"suburbs",
		"CWILV15",
		"MAP17",
		"MAP17",
		150,
		"SKY2",
		"SKY2",
		"D_DEAD2",
		0,
		7
	},
	{
		"MAP17",
		17,
		"tenements",
		"CWILV16",
		"MAP18",
		"MAP18",
		420,
		"SKY2",
		"SKY2",
		"D_STLKS3",
		0,
		7
	},
	{
		"MAP18",
		18,
		"the courtyard",
		"CWILV17",
		"MAP19",
		"MAP19",
		150,
		"SKY2",
		"SKY2",
		"D_ROMERO",
		0,
		7
	},
	{
		"MAP19",
		19,
		"the citadel",
		"CWILV18",
		"MAP20",
		"MAP20",
		210,
		"SKY2",
		"SKY2",
		"D_SHAWN2",
		0,
		7
	},
	{
		"MAP20",
		20,
		"gotcha!",
		"CWILV19",
		"MAP21",
		"MAP21",
		150,
		"SKY2",
		"SKY2",
		"D_MESSAG",
		0,
		7
	},
	{
		"MAP21",
		21,
		"nirvana",
		"CWILV20",
		"MAP22",
		"MAP22",
		240,
		"SKY3",
		"SKY3",
		"D_COUNT2",
		0,
		8
	},
	{
		"MAP22",
		22,
		"the catacombs",
		"CWILV21",
		"MAP23",
		"MAP23",
		150,
		"SKY3",
		"SKY3",
		"D_DDTBL3",
		0,
		8
	},
	{
		"MAP23",
		23,
		"barrels o' fun",
		"CWILV22",
		"MAP24",
		"MAP24",
		180,
		"SKY3",
		"SKY3",
		"D_AMPIE",
		0,
		8
	},
	{
		"MAP24",
		24,
		"the chasm",
		"CWILV23",
		"MAP25",
		"MAP25",
		150,
		"SKY3",
		"SKY3",
		"D_THEDA3",
		0,
		8
	},
	{
		"MAP25",
		25,
		"bloodfalls",
		"CWILV24",
		"MAP26",
		"MAP26",
		150,
		"SKY3",
		"SKY3",
		"D_ADRIAN",
		0,
		8
	},
	{
		"MAP26",
		26,
		"the abandoned mines",
		"CWILV25",
		"MAP27",
		"MAP27",
		300,
		"SKY3",
		"SKY3",
		"D_MESSG2",
		0,
		8
	},
	{
		"MAP27",
		27,
		"monster condo",
		"CWILV26",
		"MAP28",
		"MAP28",
		330,
		"SKY3",
		"SKY3",
		"D_ROMER2",
		0,
		8
	},
	{
		"MAP28",
		28,
		"the spirit world",
		"CWILV27",
		"MAP29",
		"MAP29",
		420,
		"SKY3",
		"SKY3",
		"D_TENSE",
		0,
		8
	},
	{
		"MAP29",
		29,
		"the living end",
		"CWILV28",
		"MAP30",
		"MAP30",
		300,
		"SKY3",
		"SKY3",
		"D_SHAWN3",
		0,
		8
	},
	{
		"MAP30",
		30,
		"icon of sin",
		"CWILV29",
		"EndGameC",
		"EndGameC",
		180,
		"SKY3",
		"SKY3",
		"D_OPENIN",
		0,
		8
	},
	{
		"MAP31",
		31,
		"wolfenstein",
		"CWILV30",
		"MAP16",
		"MAP32",
		120,
		"SKY3",
		"SKY3",
		"D_EVIL",
		LEVEL_SECRET,
		9
	},
	{
		"MAP32",
		32,
		"grosse",
		"CWILV31",
		"MAP16",
		"MAP16",
		30,
		"SKY3",
		"SKY3",
		"D_ULTIMA",
		LEVEL_SECRET,
		10
	},
	{
		"",
		0,
		NULL,
	}
};


// Episode/Cluster information
cluster_info_t ClusterInfos[] = {
	{
		1,		// DOOM Episode 1
		"D_VICTOR",
		"FLOOR4_8",
		"Once you beat the big badasses and\n"\
		"clean out the moon base you're supposed\n"\
		"to win, aren't you? Aren't you? Where's\n"\
		"your fat reward and ticket home? What\n"\
		"the hell is this? It's not supposed to\n"\
		"end this way!\n"\
		"\n" \
		"It stinks like rotten meat, but looks\n"\
		"like the lost Deimos base.  Looks like\n"\
		"you're stuck on The Shores of Hell.\n"\
		"The only way out is through.\n"\
		"\n"\
		"To continue the DOOM experience, play\n"\
		"The Shores of Hell and its amazing\n"\
		"sequel, Inferno!\n",
		NULL
	},
	{
		2,		// DOOM Episode 2
		"D_VICTOR",
		"SFLR6_1",
		"You've done it! The hideous cyber-\n"\
		"demon lord that ruled the lost Deimos\n"\
		"moon base has been slain and you\n"\
		"are triumphant! But ... where are\n"\
		"you? You clamber to the edge of the\n"\
		"moon and look down to see the awful\n"\
		"truth.\n" \
		"\n"\
		"Deimos floats above Hell itself!\n"\
		"You've never heard of anyone escaping\n"\
		"from Hell, but you'll make the bastards\n"\
		"sorry they ever heard of you! Quickly,\n"\
		"you rappel down to  the surface of\n"\
		"Hell.\n"\
		"\n" \
		"Now, it's on to the final chapter of\n"\
		"DOOM! -- Inferno.",
		NULL
	},
	{
		3,		// DOOM Episode 3
		"D_VICTOR",
		"MFLR8_4",
		"The loathsome spiderdemon that\n"\
		"masterminded the invasion of the moon\n"\
		"bases and caused so much death has had\n"\
		"its ass kicked for all time.\n"\
		"\n"\
		"A hidden doorway opens and you enter.\n"\
		"You've proven too tough for Hell to\n"\
		"contain, and now Hell at last plays\n"\
		"fair -- for you emerge from the door\n"\
		"to see the green fields of Earth!\n"\
		"Home at last.\n" \
		"\n"\
		"You wonder what's been happening on\n"\
		"Earth while you were battling evil\n"\
		"unleashed. It's good that no Hell-\n"\
		"spawn could have come through that\n"\
		"door with you ...",
		NULL
	},
	{
		4,		// DOOM Episode 4
		"D_VICTOR",
		"MFLR8_3",
		"the spider mastermind must have sent forth\n"\
		"its legions of hellspawn before your\n"\
		"final confrontation with that terrible\n"\
		"beast from hell.  but you stepped forward\n"\
		"and brought forth eternal damnation and\n"\
		"suffering upon the horde as a true hero\n"\
		"would in the face of something so evil.\n"\
		"\n"\
		"besides, someone was gonna pay for what\n"\
		"happened to daisy, your pet rabbit.\n"\
		"\n"\
		"but now, you see spread before you more\n"\
		"potential pain and gibbitude as a nation\n"\
		"of demons run amok among our cities.\n"\
		"\n"\
		"next stop, hell on earth!",
		NULL
	},
	{
		5,		// DOOM II first cluster (up thru level 6)
		"D_READ_M",
		"SLIME16",
		"YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n" \
		"STARPORT. BUT SOMETHING IS WRONG. THE\n" \
		"MONSTERS HAVE BROUGHT THEIR OWN REALITY\n" \
		"WITH THEM, AND THE STARPORT'S TECHNOLOGY\n" \
		"IS BEING SUBVERTED BY THEIR PRESENCE.\n" \
		"\n"\
		"AHEAD, YOU SEE AN OUTPOST OF HELL, A\n" \
		"FORTIFIED ZONE. IF YOU CAN GET PAST IT,\n" \
		"YOU CAN PENETRATE INTO THE HAUNTED HEART\n" \
		"OF THE STARBASE AND FIND THE CONTROLLING\n" \
		"SWITCH WHICH HOLDS EARTH'S POPULATION\n" \
		"HOSTAGE.",
		NULL
	},
	{
		6,		// DOOM II second cluster (up thru level 11)
		"D_READ_M",
		"RROCK14",
		"YOU HAVE WON! YOUR VICTORY HAS ENABLED\n" \
		"HUMANKIND TO EVACUATE EARTH AND ESCAPE\n"\
		"THE NIGHTMARE.  NOW YOU ARE THE ONLY\n"\
		"HUMAN LEFT ON THE FACE OF THE PLANET.\n"\
		"CANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\n"\
		"AND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\n"\
		"YOU SIT BACK AND WAIT FOR DEATH, CONTENT\n"\
		"THAT YOU HAVE SAVED YOUR SPECIES.\n"\
		"\n"\
		"BUT THEN, EARTH CONTROL BEAMS DOWN A\n"\
		"MESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\n"\
		"THE SOURCE OF THE ALIEN INVASION. IF YOU\n"\
		"GO THERE, YOU MAY BE ABLE TO BLOCK THEIR\n"\
		"ENTRY.  THE ALIEN BASE IS IN THE HEART OF\n"\
		"YOUR OWN HOME CITY, NOT FAR FROM THE\n"\
		"STARPORT.\" SLOWLY AND PAINFULLY YOU GET\n"\
		"UP AND RETURN TO THE FRAY.",
		NULL
	},
	{
		7,		// DOOM II third cluster (up thru level 20)
		"D_READ_M",
		"RROCK07",
		"YOU ARE AT THE CORRUPT HEART OF THE CITY,\n"\
		"SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n"\
		"YOU SEE NO WAY TO DESTROY THE CREATURES'\n"\
		"ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n"\
		"TEETH AND PLUNGE THROUGH IT.\n"\
		"\n"\
		"THERE MUST BE A WAY TO CLOSE IT ON THE\n"\
		"OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n"\
		"GOT TO GO THROUGH HELL TO GET TO IT?",
		NULL
	},
	{
		8,		// DOOM II fourth cluster (up thru level 30)
		"D_READ_M",
		"RROCK17",
		"THE HORRENDOUS VISAGE OF THE BIGGEST\n"\
		"DEMON YOU'VE EVER SEEN CRUMBLES BEFORE\n"\
		"YOU, AFTER YOU PUMP YOUR ROCKETS INTO\n"\
		"HIS EXPOSED BRAIN. THE MONSTER SHRIVELS\n"\
		"UP AND DIES, ITS THRASHING LIMBS\n"\
		"DEVASTATING UNTOLD MILES OF HELL'S\n"\
		"SURFACE.\n"\
		"\n"\
		"YOU'VE DONE IT. THE INVASION IS OVER.\n"\
		"EARTH IS SAVED. HELL IS A WRECK. YOU\n"\
		"WONDER WHERE BAD FOLKS WILL GO WHEN THEY\n"\
		"DIE, NOW. WIPING THE SWEAT FROM YOUR\n"\
		"FOREHEAD YOU BEGIN THE LONG TREK BACK\n"\
		"HOME. REBUILDING EARTH OUGHT TO BE A\n"\
		"LOT MORE FUN THAN RUINING IT WAS.\n",
		NULL
	},
	{
		9,		// DOOM II fifth cluster (level 31)
		"D_READ_M",
		"RROCK13",
		NULL,
		"CONGRATULATIONS, YOU'VE FOUND THE SECRET\n"\
		"LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n"\
		"HUMANS, RATHER THAN DEMONS. YOU WONDER\n"\
		"WHO THE INMATES OF THIS CORNER OF HELL\n"\
		"WILL BE."
	},
	{
		10,		// DOOM II sixth cluster (level 32)
		"D_READ_M",
		"RROCK19",
		NULL,
		"CONGRATULATIONS, YOU'VE FOUND THE\n"\
		"SUPER SECRET LEVEL!  YOU'D BETTER\n"\
		"BLAZE THROUGH THIS ONE!\n"
	},
	{
		0		// End-of-clusters marker
	}
};






