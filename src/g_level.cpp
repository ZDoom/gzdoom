#include "d_main.h"
#include "m_alloc.h"
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
#include "p_local.h"
#include "r_sky.h"
#include "c_console.h"
#include "f_finale.h"
#include "dstrings.h"
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

#include "gi.h"
#include "minilzo.h"

#define lioffset(x)		myoffsetof(level_pwad_info_t,x)
#define cioffset(x)		myoffsetof(cluster_info_t,x)

static level_info_t *FindDefLevelInfo (char *mapname);
static cluster_info_t *FindDefClusterInfo (int cluster);

extern int timingdemo;

// Start time for timing demos
int starttime;


// ACS variables with world scope
int WorldVars[NUM_WORLDVARS];


extern BOOL netdemo;
BOOL savegamerestore;

extern int mousex, mousey, joyxmove, joyymove, Impulse;
extern BOOL sendpause, sendsave, sendcenterview;

void *statcopy;					// for statistics driver

level_locals_t level;			// info about current level

static level_pwad_info_t *wadlevelinfos;
static cluster_info_t *wadclusterinfos;
static int numwadlevelinfos = 0;
static int numwadclusterinfos = 0;

BOOL HexenHack;



static const char *MapInfoTopLevel[] =
{
	"map",
	"defaultmap",
	"clusterdef",
	NULL
};

enum
{
	MITL_MAP,
	MITL_DEFAULTMAP,
	MITL_CLUSTERDEF
};

static const char *MapInfoMapLevel[] =
{
	"levelnum",
	"next",
	"secretnext",
	"cluster",
	"sky1",
	"sky2",
	"fade",
	"outsidefog",
	"titlepatch",
	"par",
	"music",
	"nointermission",
	"doublesky",
	"nosoundclipping",
	"allowmonstertelefrags",
	"map07special",
	"baronspecial",
	"cyberdemonspecial",
	"spidermastermindspecial",
	"specialaction_exitlevel",
	"specialaction_opendoor",
	"specialaction_lowerfloor",
	"lightning",
	"fadetable",
	"evenlighting",
	"noautosequences",
	"forcenoskystretch",
	"allowfreelook",
	"nofreelook",
	"allowjump",
	"nojump",
	"cdtrack",
	"cd_start_track",
	"cd_end1_track",
	"cd_end2_track",
	"cd_end3_track",
	"cd_intermission_track",
	"cd_title_track",
	"warptrans",
	NULL
};

enum EMIType
{
	MITYPE_IGNORE,
	MITYPE_EATNEXT,
	MITYPE_INT,
	MITYPE_COLOR,
	MITYPE_MAPNAME,
	MITYPE_LUMPNAME,
	MITYPE_SKY,
	MITYPE_SETFLAG,
	MITYPE_SCFLAGS,
	MITYPE_CLUSTER,
	MITYPE_STRING,
	MITYPE_CSTRING
};

struct MapInfoHandler
{
	EMIType type;
	DWORD data1, data2;
}
MapHandlers[] =
{
	{ MITYPE_INT,		lioffset(levelnum), 0 },
	{ MITYPE_MAPNAME,	lioffset(nextmap), 0 },
	{ MITYPE_MAPNAME,	lioffset(secretmap), 0 },
	{ MITYPE_CLUSTER,	lioffset(cluster), 0 },
	{ MITYPE_SKY,		lioffset(skypic1), lioffset(skyspeed1) },
	{ MITYPE_SKY,		lioffset(skypic2), lioffset(skyspeed2) },
	{ MITYPE_COLOR,		lioffset(fadeto), 0 },
	{ MITYPE_COLOR,		lioffset(outsidefog), 0 },
	{ MITYPE_LUMPNAME,	lioffset(pname), 0 },
	{ MITYPE_INT,		lioffset(partime), 0 },
	{ MITYPE_LUMPNAME,	lioffset(music), 0 },
	{ MITYPE_SETFLAG,	LEVEL_NOINTERMISSION, 0 },
	{ MITYPE_SETFLAG,	LEVEL_DOUBLESKY, 0 },
	{ MITYPE_SETFLAG,	LEVEL_NOSOUNDCLIPPING, 0 },
	{ MITYPE_SETFLAG,	LEVEL_MONSTERSTELEFRAG, 0 },
	{ MITYPE_SETFLAG,	LEVEL_MAP07SPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_BRUISERSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_CYBORGSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_SPIDERSPECIAL, 0 },
	{ MITYPE_SCFLAGS,	0, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_SCFLAGS,	LEVEL_SPECOPENDOOR, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_SCFLAGS,	LEVEL_SPECLOWERFLOOR, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_IGNORE,	0, 0 },		// lightning
	{ MITYPE_LUMPNAME,	lioffset(fadetable), 0 },
	{ MITYPE_SETFLAG,	LEVEL_EVENLIGHTING, 0 },
	{ MITYPE_SETFLAG,	LEVEL_SNDSEQTOTALCTRL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_FORCENOSKYSTRETCH, 0 },
	{ MITYPE_SCFLAGS,	LEVEL_FREELOOK_YES, ~LEVEL_FREELOOK_NO },
	{ MITYPE_SCFLAGS,	LEVEL_FREELOOK_NO, ~LEVEL_FREELOOK_YES },
	{ MITYPE_SCFLAGS,	LEVEL_JUMP_YES, ~LEVEL_JUMP_NO },
	{ MITYPE_SCFLAGS,	LEVEL_JUMP_NO, ~LEVEL_JUMP_YES },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 }
};

static const char *MapInfoClusterLevel[] =
{
	"entertext",
	"exittext",
	"music",
	"flat",
	"hub",
	NULL
};

MapInfoHandler ClusterHandlers[] =
{
	{ MITYPE_STRING,	cioffset(entertext), 0 },
	{ MITYPE_STRING,	cioffset(exittext), 0 },
	{ MITYPE_CSTRING,	cioffset(messagemusic), 8 },
	{ MITYPE_LUMPNAME,	cioffset(finaleflat), 0 },
	{ MITYPE_SETFLAG,	CLUSTER_HUB, 0 }
};

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_pwad_info_t *levelinfo,
							   cluster_info_t *clusterinfo,
							   DWORD levelflags);

static int FindWadLevelInfo (char *name)
{
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
		if (!strnicmp (name, wadlevelinfos[i].mapname, 8))
			return i;
		
	return -1;
}

static int FindWadClusterInfo (int cluster)
{
	int i;

	for (i = 0; i < numwadclusterinfos; i++)
		if (wadclusterinfos[i].cluster == cluster)
			return i;
		
	return -1;
}

static void SetLevelDefaults (level_pwad_info_t *levelinfo)
{
	memset (levelinfo, 0, sizeof(*levelinfo));
	levelinfo->snapshot = NULL;
	levelinfo->outsidefog = 0xff000000;
	strncpy (levelinfo->fadetable, "COLORMAP", 8);
}

//
// G_ParseMapInfo
// Parses the MAPINFO lumps of all loaded WADs and generates
// data for wadlevelinfos and wadclusterinfos.
//
void G_ParseMapInfo (void)
{
	int lump, lastlump = 0;
	level_pwad_info_t defaultinfo;
	level_pwad_info_t *levelinfo;
	int levelindex;
	cluster_info_t *clusterinfo;
	int clusterindex;
	DWORD levelflags;

	while ((lump = W_FindLump ("MAPINFO", &lastlump)) != -1)
	{
		SetLevelDefaults (&defaultinfo);
		SC_OpenLumpNum (lump, "MAPINFO");
		
		while (SC_GetString ())
		{
			switch (SC_MustMatchString (MapInfoTopLevel))
			{
			case MITL_DEFAULTMAP:
				SetLevelDefaults (&defaultinfo);
				ParseMapInfoLower (MapHandlers, MapInfoMapLevel, &defaultinfo, NULL, 0);
				break;

			case MITL_MAP:		// map <MAPNAME> <Nice Name>
				levelflags = defaultinfo.flags;
				SC_MustGetString ();
				if (IsNum (sc_String))
				{	// MAPNAME is a number, assume a Hexen wad
					int map = atoi (sc_String);
					sprintf (sc_String, "MAP%02d", map);
					SKYFLATNAME[5] = 0;
					HexenHack = true;
					// Hexen levels are automatically nointermission
					// and even lighting and no auto sound sequences
					levelflags |= LEVEL_NOINTERMISSION
								| LEVEL_EVENLIGHTING
								| LEVEL_SNDSEQTOTALCTRL;
				}
				levelindex = FindWadLevelInfo (sc_String);
				if (levelindex == -1)
				{
					levelindex = numwadlevelinfos++;
					wadlevelinfos = (level_pwad_info_t *)Realloc (wadlevelinfos, sizeof(level_pwad_info_t)*numwadlevelinfos);
				}
				levelinfo = wadlevelinfos + levelindex;
				memcpy (levelinfo, &defaultinfo, sizeof(*levelinfo));
				uppercopy (levelinfo->mapname, sc_String);
				SC_MustGetString ();
				ReplaceString (&levelinfo->level_name, sc_String);
				// Set up levelnum now so that the Teleport_NewMap specials
				// in hexen.wad work without modification.
				if (!strnicmp (levelinfo->mapname, "MAP", 3) && levelinfo->mapname[5] == 0)
				{
					int mapnum = atoi (levelinfo->mapname + 3);

					if (mapnum >= 1 && mapnum <= 99)
						levelinfo->levelnum = mapnum;
				}
				ParseMapInfoLower (MapHandlers, MapInfoMapLevel, levelinfo, NULL, levelflags);
				break;

			case MITL_CLUSTERDEF:	// clusterdef <clusternum>
				SC_MustGetNumber ();
				clusterindex = FindWadClusterInfo (sc_Number);
				if (clusterindex == -1)
				{
					clusterindex = numwadclusterinfos++;
					wadclusterinfos = (cluster_info_t *)Realloc (wadclusterinfos, sizeof(cluster_info_t)*numwadclusterinfos);
					memset (wadclusterinfos + clusterindex, 0, sizeof(cluster_info_t));
				}
				clusterinfo = wadclusterinfos + clusterindex;
				clusterinfo->cluster = sc_Number;
				ParseMapInfoLower (ClusterHandlers, MapInfoClusterLevel, NULL, clusterinfo, 0);
				break;
			}
		}
		SC_Close ();
	}
}

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_pwad_info_t *levelinfo,
							   cluster_info_t *clusterinfo,
							   DWORD flags)
{
	int entry;
	MapInfoHandler *handler;
	char *string;
	byte *info;

	info = levelinfo ? (byte *)levelinfo : (byte *)clusterinfo;

	while (SC_GetString ())
	{
		if (SC_MatchString (MapInfoTopLevel) != -1)
		{
			SC_UnGet ();
			break;
		}
		entry = SC_MustMatchString (strings);
		handler = handlers + entry;
		switch (handler->type)
		{
		case MITYPE_IGNORE:
			break;

		case MITYPE_EATNEXT:
			SC_MustGetString ();
			break;

		case MITYPE_INT:
			SC_MustGetNumber ();
			*((int *)(info + handler->data1)) = sc_Number;
			break;

		case MITYPE_COLOR:
			SC_MustGetString ();
			string = V_GetColorStringByName (sc_String);
			if (string)
			{
				*((DWORD *)(info + handler->data1)) =
					V_GetColorFromString (NULL, string);
				delete[] string;
			}
			else
			{
				*((DWORD *)(info + handler->data1)) =
									V_GetColorFromString (NULL, sc_String);
			}
			break;

		case MITYPE_MAPNAME:
			SC_MustGetString ();
			if (IsNum (sc_String))
			{
				int map = atoi (sc_String);
				sprintf (sc_String, "MAP%02d", map);
			}
			strncpy ((char *)(info + handler->data1), sc_String, 8);
			break;

		case MITYPE_LUMPNAME:
			SC_MustGetString ();
			uppercopy ((char *)(info + handler->data1), sc_String);
			break;

		case MITYPE_SKY:
			SC_MustGetString ();	// get texture name;
			uppercopy ((char *)(info + handler->data1), sc_String);
			SC_MustGetFloat ();		// get scroll speed
			if (HexenHack)
			{
				*((fixed_t *)(info + handler->data2)) = sc_Number << 8;
			}
			else
			{
				*((fixed_t *)(info + handler->data2)) = (fixed_t)(sc_Float * 65536.0f);
			}
			break;

		case MITYPE_SETFLAG:
			flags |= handler->data1;
			break;

		case MITYPE_SCFLAGS:
			flags = (flags & handler->data2) | handler->data1;
			break;

		case MITYPE_CLUSTER:
			SC_MustGetNumber ();
			*((int *)(info + handler->data1)) = sc_Number;
			if (HexenHack)
			{
				cluster_info_t *cluster = FindClusterInfo (sc_Number);
				if (cluster)
					cluster->flags |= CLUSTER_HUB;
			}
			break;

		case MITYPE_STRING:
			SC_MustGetString ();
			ReplaceString ((char **)(info + handler->data1), sc_String);
			break;

		case MITYPE_CSTRING:
			SC_MustGetString ();
			strncpy ((char *)(info + handler->data1), sc_String, handler->data2);
			*((char *)(info + handler->data1 + handler->data2)) = '\0';
			break;
		}
	}
	if (levelinfo)
		levelinfo->flags = flags;
	else
		clusterinfo->flags = flags;
}

static void zapDefereds (acsdefered_t *def)
{
	while (def) {
		acsdefered_t *next = def->next;
		Z_Free (def);
		def = next;
	}
}

void P_RemoveDefereds (void)
{
	int i;

	// Remove any existing defereds
	for (i = 0; i < numwadlevelinfos; i++)
		if (wadlevelinfos[i].defered) {
			zapDefereds (wadlevelinfos[i].defered);
			wadlevelinfos[i].defered = NULL;
		}

	for (i = 0; LevelInfos[i].level_name; i++)
		if (LevelInfos[i].defered) {
			zapDefereds (LevelInfos[i].defered);
			LevelInfos[i].defered = NULL;
		}
}
//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//
static char d_mapname[9];

void G_DeferedInitNew (char *mapname)
{
	strncpy (d_mapname, mapname, 8);
	gameaction = ga_newgame;
}

BEGIN_COMMAND (map)
{
	if (argc > 1)
	{
		if (W_CheckNumForName (argv[1]) == -1)
			Printf (PRINT_HIGH, "No map %s\n", argv[1]);
		else
			G_DeferedInitNew (argv[1]);
	}
}
END_COMMAND (map)

void G_DoNewGame (void)
{
	if (demoplayback)
	{
		C_RestoreCVars ();
		demoplayback = false;
		D_SetupUserInfo ();
	}
	netdemo = false;
	netgame = false;
	multiplayer = false;
//	deathmatch = 0;
	{
		int i;

		for (i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
	}
//	respawnparm = false;
//	fastparm = 0;
//	nomonsters = false;
	consoleplayer = 0;
	G_InitNew (d_mapname);
	gameaction = ga_nothing;
}

void G_InitNew (char *mapname)
{
	static BOOL isFast;
	BOOL wantFast;
	int i;

	// [RH] Remove all particles
	R_ClearParticles ();

	if (!savegamerestore)
		G_ClearSnapshots ();

	// [RH] Mark all levels as not visited
	if (!savegamerestore)
	{
		for (i = 0; i < numwadlevelinfos; i++)
			wadlevelinfos[i].flags &= ~LEVEL_VISITED;

		for (i = 0; LevelInfos[i].mapname[0]; i++)
			LevelInfos[i].flags &= ~LEVEL_VISITED;
	}

	UnlatchCVars ();

	if (gameskill.value > sk_nightmare)
		gameskill.Set (sk_nightmare);
	else if (gameskill.value < sk_baby)
		gameskill.Set (sk_baby);

	UnlatchCVars ();

	if (paused)
	{
		paused = false;
		S_ResumeSound ();
	}

	// [RH] If this map doesn't exist, bomb out
	if (W_CheckNumForName (mapname) == -1)
	{
		I_Error ("Could not find map %s\n", mapname);
	}

	if ((gameskill.value == sk_nightmare) || (dmflags & DF_MONSTERS_RESPAWN) )
		respawnmonsters = true;
	else
		respawnmonsters = false;

	wantFast = (dmflags & DF_FAST_MONSTERS) || (gameskill.value == sk_nightmare);
	if (wantFast != isFast)
	{
		if (wantFast)
		{
			for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
				states[i].tics >>= 1;
			mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
			mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
			mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
		}
		else
		{
			for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
				states[i].tics <<= 1;
			mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
			mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
			mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
		}
		isFast = wantFast;
	}

	if (!savegamerestore)
	{
		M_ClearRandom ();
		memset (WorldVars, 0, sizeof(WorldVars));
		level.time = 0;

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_REBORN;
	}

	usergame = true;				// will be set false if a demo
	paused = false;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	BorderNeedRefresh = true;

	strncpy (level.mapname, mapname, 8);
	G_DoLoadLevel (0);
}

//
// G_DoCompleted
//
BOOL 			secretexit;
static int		startpos;	// [RH] Support for multiple starts per level
extern BOOL		NoWipe;		// [RH] Don't wipe when travelling in hubs

// [RH] The position parameter to these next three functions should
//		match the first parameter of the single player start spots
//		that should appear in the next map.
static void goOn (int position)
{
	cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
	cluster_info_t *nextcluster = FindClusterInfo (FindLevelInfo (level.nextmap)->cluster);


	startpos = position;
	gameaction = ga_completed;
	bglobal.End();	//Added by MC:

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if ((level.flags & LEVEL_NOINTERMISSION) || (nextcluster == thiscluster))
			NoWipe = 4;
		D_DrawIcon = "TELEICON";
	}
}

void G_ExitLevel (int position)
{
	secretexit = false;
	goOn (position);
}

// Here's for the german edition.
void G_SecretExitLevel (int position) 
{
	// [RH] Check for secret levels is done in 
	//		G_DoCompleted()

	secretexit = true;
	goOn (position);
}

void G_DoCompleted (void) 
{
	int i; 

	gameaction = ga_nothing;

	// [RH] Mark this level as having been visited
	if (!(level.flags & LEVEL_CHANGEMAPCHEAT))
		FindLevelInfo (level.mapname)->flags |= LEVEL_VISITED;

	if (automapactive)
		AM_Stop ();

	wminfo.epsd = level.cluster - 1;		// Only used for DOOM I.
	strncpy (wminfo.lname0, level.info->pname, 8);
	strncpy (wminfo.current, level.mapname, 8);

	if (deathmatch.value &&
		(dmflags & DF_SAME_LEVEL) &&
		!(level.flags & LEVEL_CHANGEMAPCHEAT)) {
		strncpy (wminfo.next, level.mapname, 8);
		strncpy (wminfo.lname1, level.info->pname, 8);
	} else {
		wminfo.next[0] = 0;
		if (secretexit) {
			if (W_CheckNumForName (level.secretmap) != -1) {
				strncpy (wminfo.next, level.secretmap, 8);
				strncpy (wminfo.lname1, FindLevelInfo (level.secretmap)->pname, 8);
			} else {
				secretexit = false;
			}
		}
		if (!wminfo.next[0]) {
			strncpy (wminfo.next, level.nextmap, 8);
			strncpy (wminfo.lname1, FindLevelInfo (level.nextmap)->pname, 8);
		}
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
		wminfo.plyr[i].fragcount = players[i].fragcount;
	}

	// [RH] If we're in a hub and staying within that hub, take a snapshot
	//		of the level. If we're traveling to a new hub, take stuff from
	//		the player and clear the world vars. If this is just an
	//		ordinary cluster (not a hub), take stuff from the player, but
	//		leave the world vars alone.
	{
		cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
		cluster_info_t *nextcluster = FindClusterInfo (FindLevelInfo (level.nextmap)->cluster);

		if (thiscluster != nextcluster ||
			deathmatch.value ||
			!(thiscluster->flags & CLUSTER_HUB)) {
			for (i=0 ; i<MAXPLAYERS ; i++)
				if (playeringame[i])
					G_PlayerFinishLevel (i);	// take away cards and stuff

				if (nextcluster->flags & CLUSTER_HUB) {
					memset (WorldVars, 0, sizeof(WorldVars));
					P_RemoveDefereds ();
					G_ClearSnapshots ();
				}
		} else {
			G_SnapshotLevel ();
		}
		if (!(nextcluster->flags & CLUSTER_HUB) || !(thiscluster->flags & CLUSTER_HUB))
			level.time = 0;	// Reset time to zero if not entering/staying in a hub

		if (!deathmatch.value &&
			((level.flags & LEVEL_NOINTERMISSION) ||
			((nextcluster == thiscluster) && (thiscluster->flags & CLUSTER_HUB)))) {
			G_WorldDone ();
			return;
		}
	}

	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

// [RH] If you ever get a statistics driver operational, adapt this.
//	if (statcopy)
//		memcpy (statcopy, &wminfo, sizeof(wminfo));
		
	WI_Start (&wminfo);
}

//
// G_DoLoadLevel 
//
extern gamestate_t 	wipegamestate; 
extern float BaseBlendA;
 
void G_DoLoadLevel (int position) 
{ 
	static int lastposition = 0;
	gamestate_t oldgs = gamestate;
	int i;

	if (position == -1)
		position = lastposition;
	else
		lastposition = position;

	G_InitLevelLocals ();

	Printf (PRINT_HIGH, 
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n"
			TEXTCOLOR_BOLD "%s\n\n",
			level.level_name);

	if (wipegamestate == GS_LEVEL) 
		wipegamestate = GS_FORCEWIPE;

	gamestate = GS_LEVEL; 
	
	if (demoplayback || oldgs == GS_STARTUP)
		C_HideConsole ();

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = R_FlatNumForName ( SKYFLATNAME );

	// DOOM determines the sky texture to be used
	// depending on the current episode, and the game version.
	// [RH] Fetch sky parameters from level_locals_t.
	sky1texture = R_TextureNumForName (level.skypic1);
	sky2texture = R_TextureNumForName (level.skypic2);

	// [RH] Set up details about sky rendering
	R_InitSkyMap ();

	for (i = 0; i < MAXPLAYERS; i++) 
	{ 
		if (playeringame[i] && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_REBORN; 
		memset (players[i].frags,0,sizeof(players[i].frags)); 
		players[i].fragcount = 0;
	}

	// initialize the msecnode_t freelist.					phares 3/25/98
	// any nodes in the freelist are gone by now, cleared
	// by Z_FreeTags() when the previous level ended or player
	// died.

	{
		extern msecnode_t *headsecnode; // phares 3/25/98
		headsecnode = NULL;

		// [RH] Need to prevent the AActor destructor from trying to
		//		free the nodes
		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) )
			actor->touching_sectorlist = NULL;
	}

	SN_StopAllSequences ();
	P_SetupLevel (level.mapname, position);	 
	displayplayer = consoleplayer;				// view the guy you are playing
	ST_Start();		// [RH] Make sure status bar knows who we are
	gameaction = ga_nothing; 
	Z_CheckHeap ();
	
	// clear cmd building stuff
	Impulse = 0;
	for (i = 0; i < NUM_ACTIONS; i++)
		if (i != ACTION_MLOOK && i != ACTION_KLOOK)
			Actions[i] = 0;
	joyxmove = joyymove = 0; 
	mousex = mousey = 0; 
	sendpause = sendsave = paused = sendcenterview = false; 

	//Added by MC: Initialize bots.
	bglobal.Init ();

	if (timingdemo) {
		static BOOL firstTime = true;

		if (firstTime) {
			starttime = I_GetTimePolled ();
			firstTime = false;
		}
	}

	level.starttime = I_GetTime ();
	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	P_DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.

	C_FlushDisplay ();
}


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;

	gameaction = ga_worlddone; 

	if (level.flags & LEVEL_CHANGEMAPCHEAT)
		return;

	thiscluster = FindClusterInfo (level.cluster);
	if (!strncmp (level.nextmap, "EndGame", 7)) {
		F_StartFinale (thiscluster->messagemusic, thiscluster->finaleflat, thiscluster->exittext);
	} else {
		if (!secretexit)
			nextcluster = FindClusterInfo (FindLevelInfo (level.nextmap)->cluster);
		else
			nextcluster = FindClusterInfo (FindLevelInfo (level.secretmap)->cluster);

		if (nextcluster->cluster != level.cluster && !deathmatch.value) {
			// Only start the finale if the next level's cluster is different
			// than the current one and we're not in deathmatch.
			if (nextcluster->entertext) {
				F_StartFinale (nextcluster->messagemusic, nextcluster->finaleflat, nextcluster->entertext);
			} else if (thiscluster->exittext) {
				F_StartFinale (thiscluster->messagemusic, thiscluster->finaleflat, thiscluster->exittext);
			}
		}
	}
} 
 
void G_DoWorldDone (void) 
{		 
	gamestate = GS_LEVEL;
	if (wminfo.next[0] == 0) {
		// Don't die if no next map is given,
		// just repeat the current one.
		Printf (PRINT_HIGH, "No next map specified.\n");
	} else {
		strncpy (level.mapname, wminfo.next, 8);
	}
	G_DoLoadLevel (startpos);
	startpos = 0;
	gameaction = ga_nothing; 
	viewactive = true; 
} 
 

extern dyncolormap_t NormalLight;

void G_InitLevelLocals ()
{
	int oldfade = level.fadeto;
	level_info_t *info;
	int i;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any
	NormalLight.maps = realcolormaps;

	if ((i = FindWadLevelInfo (level.mapname)) > -1) {
		level_pwad_info_t *pinfo = wadlevelinfos + i;

		level.info = (level_info_t *)pinfo;
		level.skyspeed1 = pinfo->skyspeed1;
		level.skyspeed2 = pinfo->skyspeed2;
		info = (level_info_t *)pinfo;
		strncpy (level.skypic2, pinfo->skypic2, 8);
		level.fadeto = pinfo->fadeto;
		if (level.fadeto) {
			NormalLight.maps = DefaultPalette->maps.colormaps;
		} else {
			R_SetDefaultColormap (pinfo->fadetable);
		}
		level.outsidefog = pinfo->outsidefog;
		level.flags |= LEVEL_DEFINEDINMAPINFO;
	} else {
		info = FindDefLevelInfo (level.mapname);
		level.info = info;
		level.skyspeed1 =  level.skyspeed2 = 0;
		level.fadeto = 0;
		level.outsidefog = 0xff000000;	// 0xff000000 signals not to handle it special
		level.skypic2[0] = 0;
		R_SetDefaultColormap ("COLORMAP");
	}

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
		if (!level.skypic2[0])
			strncpy (level.skypic2, level.skypic1, 8);
	} else {
		level.partime = level.cluster = 0;
		strcpy (level.level_name, "Unnamed");
		level.nextmap[0] =
			level.secretmap[0] =
			level.music[0] = 0;
			strncpy (level.skypic1, "SKY1", 8);
			strncpy (level.skypic2, "SKY1", 8);
		level.flags = 0;
		level.levelnum = 1;
	}

	{
		int clear = 0, set = 0;
		char buf[16];

		if (level.flags & LEVEL_JUMP_YES)
			clear = DF_NO_JUMP;
		if (level.flags & LEVEL_JUMP_NO)
			set = DF_NO_JUMP;
		if (level.flags & LEVEL_FREELOOK_YES)
			clear |= DF_NO_FREELOOK;
		if (level.flags & LEVEL_FREELOOK_NO)
			set |= DF_NO_FREELOOK;

		dmflags &= ~clear;
		dmflags |= set;
		sprintf (buf, "%d", dmflags);
		cvar_set ("dmflags", buf);
	}

	memset (level.vars, 0, sizeof(level.vars));

	if (oldfade != level.fadeto)
		RefreshPalettes ();
}

char *CalcMapName (int episode, int level)
{
	static char lumpname[9];

	if (gameinfo.flags & GI_MAPxx)
	{
		sprintf (lumpname, "MAP%02d", level);
	}
	else
	{
		lumpname[0] = 'E';
		lumpname[1] = '0' + episode;
		lumpname[2] = 'M';
		lumpname[3] = '0' + level;
		lumpname[4] = 0;
	}
	return lumpname;
}

static level_info_t *FindDefLevelInfo (char *mapname)
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

level_info_t *FindLevelInfo (char *mapname)
{
	int i;

	if ((i = FindWadLevelInfo (mapname)) > -1)
		return (level_info_t *)(wadlevelinfos + i);
	else
		return FindDefLevelInfo (mapname);
}

level_info_t *FindLevelByNum (int num)
{
	{
		int i;

		for (i = 0; i < numwadlevelinfos; i++)
			if (wadlevelinfos[i].levelnum == num)
				return (level_info_t *)(wadlevelinfos + i);
	}
	{
		level_info_t *i = LevelInfos;
		while (i->level_name) {
			if (i->levelnum == num)
				return i;
			i++;
		}
		return NULL;
	}
}

static cluster_info_t *FindDefClusterInfo (int cluster)
{
	cluster_info_t *i;

	i = ClusterInfos;
	while (i->cluster && i->cluster != cluster)
		i++;

	return i;
}

cluster_info_t *FindClusterInfo (int cluster)
{
	int i;

	if ((i = FindWadClusterInfo (cluster)) > -1)
		return wadclusterinfos + i;
	else
		return FindDefClusterInfo (cluster);
}

void G_SetLevelStrings (void)
{
	char temp[8];
	char *namepart;
	int i, start;

	temp[0] = '0';
	temp[1] = ':';
	temp[2] = 0;
	for (i = 65; i < 101; i++) {		// HUSTR_E1M1 .. HUSTR_E4M9
		if (temp[0] < '9')
			temp[0]++;
		else
			temp[0] = '1';

		if ( (namepart = strstr (Strings[i].string, temp)) ) {
			namepart += 2;
			while (*namepart && *namepart <= ' ')
				namepart++;
		} else {
			namepart = Strings[i].string;
		}

		ReplaceString (&LevelInfos[i-65].level_name, namepart);
	}

	for (i = 0; i < 4; i++)
		ReplaceString (&ClusterInfos[i].exittext, Strings[221+i].string);

	if (gamemission == pack_plut)
		start = 133;		// PHUSTR_1
	else if (gamemission == pack_tnt)
		start = 165;		// THUSTR_1
	else
		start = 101;		// HUSTR_1

	for (i = 0; i < 32; i++) {
		sprintf (temp, "%d:", i + 1);
		if ( (namepart = strstr (Strings[i+start].string, temp)) ) {
			namepart += strlen (temp);
			while (*namepart && *namepart <= ' ')
				namepart++;
		} else {
			namepart = Strings[i+start].string;
		}
		ReplaceString (&LevelInfos[36+i].level_name, namepart);
	}

	if (gamemission == pack_plut)
		start = 231;		// P1TEXT
	else if (gamemission == pack_tnt)
		start = 237;		// T1TEXT
	else
		start = 225;		// C1TEXT

	for (i = 0; i < 4; i++)
		ReplaceString (&ClusterInfos[4 + i].exittext, Strings[start+i].string);
	for (; i < 6; i++)
		ReplaceString (&ClusterInfos[4 + i].entertext, Strings[start+i].string);

	if (level.info)
		strncpy (level.level_name, level.info->level_name, 63);
}


void G_SerializeLevel (FArchive &arc, bool hubLoad)
{
	int i;

	if (arc.IsStoring ())
	{
		arc << level.flags
			<< level.fadeto
			<< level.found_secrets
			<< level.found_items
			<< level.killed_monsters;

		for (i = 0; i < NUM_MAPVARS; i++)
			arc << level.vars[i];
	}
	else
	{
		arc >> level.flags
			>> level.fadeto
			>> level.found_secrets
			>> level.found_items
			>> level.killed_monsters;

		for (i = 0; i < NUM_MAPVARS; i++)
			arc >> level.vars[i];
	}
	P_SerializeThinkers (arc, hubLoad);
	P_SerializeWorld (arc);
	P_SerializePolyobjs (arc);
	P_SerializeSounds (arc);
	if (!hubLoad)
		P_SerializePlayers (arc);
}

// Archives the current level
void G_SnapshotLevel ()
{
	if (level.info->snapshot)
		delete level.info->snapshot;

	level.info->snapshot = new FLZOMemFile;
	level.info->snapshot->Open ();

	FArchive arc (*level.info->snapshot);

	G_SerializeLevel (arc, false);
}

// Unarchives the current level based on its snapshot
// The level should have already been loaded and setup.
void G_UnSnapshotLevel (bool hubLoad)
{
	if (level.info->snapshot == NULL)
		return;

	level.info->snapshot->Reopen ();
	FArchive arc (*level.info->snapshot);
	if (hubLoad)
		arc.SetHubTravel ();
	G_SerializeLevel (arc, hubLoad);
	arc.Close ();
	// No reason to keep the snapshot around once the level's been entered.
	delete level.info->snapshot;
	level.info->snapshot = NULL;
}

void G_ClearSnapshots (void)
{
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
		if (wadlevelinfos[i].snapshot)
		{
			delete wadlevelinfos[i].snapshot;
			wadlevelinfos[i].snapshot = NULL;
		}

	for (i = 0; LevelInfos[i].level_name; i++)
		if (LevelInfos[i].snapshot)
		{
			delete LevelInfos[i].snapshot;
			LevelInfos[i].snapshot = NULL;
		}
}

static void writeSnapShot (FArchive &arc, level_info_t *i)
{
	arc.Write (i->mapname, 8);
	i->snapshot->Serialize (arc);
}

void G_SerializeSnapshots (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		int i;

		for (i = 0; i < numwadlevelinfos; i++)
			if (wadlevelinfos[i].snapshot)
				writeSnapShot (arc, &wadlevelinfos[i]);

		for (i = 0; LevelInfos[i].level_name; i++)
			if (LevelInfos[i].snapshot)
				writeSnapShot (arc, &LevelInfos[i]);

		// Signal end of snapshots
		arc << (char)0;
	}
	else
	{
		char mapname[8];

		G_ClearSnapshots ();

		arc >> mapname[0];
		while (mapname[0])
		{
			arc.Read (&mapname[1], 7);
			level_info_t *i = FindLevelInfo (mapname);
			i->snapshot = new FLZOMemFile;
			i->snapshot->Serialize (arc);
			arc >> mapname[0];
		}
	}
}


static void writeDefereds (FArchive &arc, level_info_t *i)
{
	arc.Write (i->mapname, 8);
	arc << i->defered;
}

void P_SerializeACSDefereds (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		int i;

		for (i = 0; i < numwadlevelinfos; i++)
			if (wadlevelinfos[i].defered)
				writeDefereds (arc, &wadlevelinfos[i]);

		for (i = 0; LevelInfos[i].level_name; i++)
			if (LevelInfos[i].defered)
				writeDefereds (arc, &LevelInfos[i]);

		// Signal end of defereds
		arc << (char)0;
	}
	else
	{
		char mapname[8];

		P_RemoveDefereds ();

		arc >> mapname[0];
		while (mapname[0])
		{
			arc.Read (&mapname[1], 7);
			level_info_t *i = FindLevelInfo (mapname);
			if (i == NULL)
			{
				char name[9];

				strncpy (name, mapname, 8);
				name[8] = 0;
				I_Error ("Unknown map '%s' in savegame", name);
			}
			arc >> i->defered;
			arc >> mapname[0];
		}
	}
}


// Static level info from original game
// The level names and cluster messages get filled in
// by G_SetLevelStrings().

level_info_t LevelInfos[] = {
	// Registered/Retail Episode 1
	{
		"E1M1",
		1,
		NULL,
		"WILV00",
		"E1M2",
		"E1M9",
		30,
		"SKY1",
		"D_E1M1",
		0,
		1
	},
	{
		"E1M2",
		2,
		NULL,
		"WILV01",
		"E1M3",
		"E1M9",
		75,
		"SKY1",
		"D_E1M2",
		0,
		1
	},
	{
		"E1M3",
		3,
		NULL,
		"WILV02",
		"E1M4",
		"E1M9",
		120,
		"SKY1",
		"D_E1M3",
		0,
		1
	},
	{
		"E1M4",
		4,
		NULL,	
		"WILV03",
		"E1M5",
		"E1M9",
		90,
		"SKY1",
		"D_E1M4",
		0,
		1
	},
	{
		"E1M5",
		5,
		NULL,
		"WILV04",
		"E1M6",
		"E1M9",
		165,
		"SKY1",
		"D_E1M5",
		0,
		1
	},
	{
		"E1M6",
		6,
		NULL,
		"WILV05",
		"E1M7",
		"E1M9",
		180,
		"SKY1",
		"D_E1M6",
		0,
		1
	},
	{
		"E1M7",
		7,
		NULL,
		"WILV06",
		"E1M8",
		"E1M9",
		180,
		"SKY1",
		"D_E1M7",
		0,
		1
	},
	{
		"E1M8",
		8,
		NULL,
		"WILV07",
		{ 'E','n','d','G','a','m','e','1' },
		"E1M9",
		30,
		"SKY1",
		"D_E1M8",
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_BRUISERSPECIAL|LEVEL_SPECLOWERFLOOR,
		1
	},
	{
		"E1M9",
		9,
		NULL,
		"WILV08",
		"E1M4",
		"E1M4",
		165,
		"SKY1",
		"D_E1M9",
		0,
		1
	},

	// Registered/Retail Episode 2
	{
		"E2M1",
		11,
		NULL,
		"WILV10",
		"E2M2",
		"E2M9",
		90,
		"SKY2",
		"D_E2M1",
		0,
		2
	},

	{
		"E2M2",
		12,
		NULL,
		"WILV11",
		"E2M3",
		"E2M9",
		90,
		"SKY2",
		"D_E2M2",
		0,
		2
	},
	{
		"E2M3",
		13,
		NULL,
		"WILV12",
		"E2M4",
		"E2M9",
		90,
		"SKY2",
		"D_E2M3",
		0,
		2
	},
	{
		"E2M4",
		14,
		NULL,
		"WILV13",
		"E2M5",
		"E2M9",
		120,
		"SKY2",
		"D_E2M4",
		0,
		2
	},
	{
		"E2M5",
		15,
		NULL,
		"WILV14",
		"E2M6",
		"E2M9",
		90,
		"SKY2",
		"D_E2M5",
		0,
		2
	},
	{
		"E2M6",
		16,
		NULL,
		"WILV15",
		"E2M7",
		"E2M9",
		360,
		"SKY2",
		"D_E2M6",
		0,
		2
	},
	{
		"E2M7",
		17,
		NULL,
		"WILV16",
		"E2M8",
		"E2M9",
		240,
		"SKY2",
		"D_E2M7",
		0,
		2
	},
	{
		"E2M8",
		18,
		NULL,
		"WILV17",
		{ 'E','n','d','G','a','m','e','2' },
		"E2M9",
		30,
		"SKY2",
		"D_E2M8",
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_CYBORGSPECIAL,
		2
	},
	{
		"E2M9",
		19,
		NULL,
		"WILV18",
		"E2M6",
		"E2M6",
		170,
		"SKY2",
		"D_E2M9",
		0,
		2
	},

	// Registered/Retail Episode 3

	{
		"E3M1",
		21,
		NULL,
		"WILV20",
		"E3M2",
		"E3M9",
		90,
		"SKY3",
		"D_E3M1",
		0,
		3
	},
	{
		"E3M2",
		22,
		NULL,
		"WILV21",
		"E3M3",
		"E3M9",
		45,
		"SKY3",
		"D_E3M2",
		0,
		3
	},
	{
		"E3M3",
		23,
		NULL,
		"WILV22",
		"E3M4",
		"E3M9",
		90,
		"SKY3",
		"D_E3M3",
		0,
		3
	},
	{
		"E3M4",
		24,
		NULL,
		"WILV23",
		"E3M5",
		"E3M9",
		150,
		"SKY3",
		"D_E3M4",
		0,
		3
	},
	{
		"E3M5",
		25,
		NULL,
		"WILV24",
		"E3M6",
		"E3M9",
		90,
		"SKY3",
		"D_E3M5",
		0,
		3
	},
	{
		"E3M6",
		26,
		NULL,
		"WILV25",
		"E3M7",
		"E3M9",
		90,
		"SKY3",
		"D_E3M6",
		0,
		3
	},
	{
		"E3M7",
		27,
		NULL,
		"WILV26",
		"E3M8",
		"E3M9",
		165,
		"SKY3",
		"D_E3M7",
		0,
		3
	},
	{
		"E3M8",
		28,
		NULL,
		"WILV27",
		{ 'E','n','d','G','a','m','e','3' },
		"E3M9",
		30,
		"SKY3",
		"D_E3M8",
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPIDERSPECIAL,
		3
	},
	{
		"E3M9",
		29,
		NULL,
		"WILV28",
		"E3M7",
		"E3M7",
		135,
		"SKY3",
		"D_E3M9",
		0,
		3
	},

	// Retail Episode 4
	{
		"E4M1",
		31,
		NULL,
		"WILV30",
		"E4M2",
		"E4M9",
		0,
		"SKY4",
		"D_E3M4",
		0,
		4
	},
	{
		"E4M2",
		32,
		NULL,
		"WILV31",
		"E4M3",
		"E4M9",
		0,
		"SKY4",
		"D_E3M2",
		0,
		4
	},
	{
		"E4M3",
		33,
		NULL,
		"WILV32",
		"E4M4",
		"E4M9",
		0,
		"SKY4",
		"D_E3M3",
		0,
		4
	},
	{
		"E4M4",
		34,
		NULL,
		"WILV33",
		"E4M5",
		"E4M9",
		0,
		"SKY4",
		"D_E1M5",
		0,
		4
	},
	{
		"E4M5",
		35,
		NULL,
		"WILV34",
		"E4M6",
		"E4M9",
		0,
		"SKY4",
		"D_E2M7",
		0,
		4
	},
	{
		"E4M6",
		36,
		NULL,
		"WILV35",
		"E4M7",
		"E4M9",
		0,
		"SKY4",
		"D_E2M4",
		LEVEL_CYBORGSPECIAL|LEVEL_SPECOPENDOOR,
		4
	},
	{
		"E4M7",
		37,
		NULL,
		"WILV36",
		"E4M8",
		"E4M9",
		0,
		"SKY4",
		"D_E2M6",
		0,
		4
	},
	{
		"E4M8",
		38,
		NULL,
		"WILV37",
		{ 'E','n','d','G','a','m','e','4' },
		"E4M9",
		0,
		"SKY4",
		"D_E2M5",
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPIDERSPECIAL|LEVEL_SPECLOWERFLOOR,
		4
	},
	{
		"E4M9",
		39,
		NULL,
		"WILV38",
		"E4M3",
		"E4M3",
		0,
		"SKY4",
		"D_E1M9",
		0,
		4
	},

	// DOOM 2 Levels

	{
		"MAP01",
		1,
		NULL,
		"CWILV00",
		"MAP02",
		"MAP02",
		30,
		"SKY1",
		{ 'D','_','R','U','N','N','I','N' },
		0,
		5
	},
	{
		"MAP02",
		2,
		NULL,
		"CWILV01",
		"MAP03",
		"MAP03",
		90,
		"SKY1",
		{ 'D','_','S','T','A','L','K','S' },
		0,
		5
	},
	{
		"MAP03",
		3,
		NULL,
		"CWILV02",
		"MAP04",
		"MAP04",
		120,
		"SKY1",
		{ 'D','_','C','O','U','N','T','D' },
		0,
		5
	},
	{
		"MAP04",
		4,
		NULL,
		"CWILV03",
		"MAP05",
		"MAP05",
		120,
		"SKY1",
		{ 'D','_','B','E','T','W','E','E' },
		0,
		5
	},
	{
		"MAP05",
		5,
		NULL,
		"CWILV04",
		"MAP06",
		"MAP06",
		90,
		"SKY1",
		"D_DOOM",
		0,
		5
	},
	{
		"MAP06",
		6,
		NULL,
		"CWILV05",
		"MAP07",
		"MAP07",
		150,
		"SKY1",
		{ 'D','_','T','H','E','_','D','A' },
		0,
		5
	},
	{
		"MAP07",
		7,
		NULL,
		"CWILV06",
		"MAP08",
		"MAP08",
		120,
		"SKY1",
		"D_SHAWN",
		LEVEL_MAP07SPECIAL,
		6
	},
	{
		"MAP08",
		8,
		NULL,
		"CWILV07",
		"MAP09",
		"MAP09",
		120,
		"SKY1",
		{ 'D','_','D','D','T','B','L','U' },
		0,
		6
	},
	{
		"MAP09",
		9,
		NULL,
		"CWILV08",
		"MAP10",
		"MAP10",
		270,
		"SKY1",
		{ 'D','_','I','N','_','C','I','T' },
		0,
		6
	},
	{
		"MAP10",
		10,
		NULL,
		"CWILV09",
		"MAP11",
		"MAP11",
		90,
		"SKY1",
		"D_DEAD",
		0,
		6
	},
	{
		"MAP11",
		11,
		NULL,
		"CWILV10",
		"MAP12",
		"MAP12",
		210,
		"SKY1",
		{ 'D','_','S','T','L','K','S','2' },
		0,
		6
	},
	{
		"MAP12",
		12,
		NULL,
		"CWILV11",
		"MAP13",
		"MAP13",
		150,
		"SKY2",
		{ 'D','_','T','H','E','D','A','2' },
		0,
		7
	},
	{
		"MAP13",
		13,
		NULL,
		"CWILV12",
		"MAP14",
		"MAP14",
		150,
		"SKY2",
		"D_DOOM2",
		0,
		7
	},
	{
		"MAP14",
		14,
		NULL,
		"CWILV13",
		"MAP15",
		"MAP15",
		150,
		"SKY2",
		{ 'D','_','D','D','T','B','L','2' },
		0,
		7
	},
	{
		"MAP15",
		15,
		NULL,
		"CWILV14",
		"MAP16",
		"MAP31",
		210,
		"SKY2",
		{ 'D','_','R','U','N','N','I','2' },
		0,
		7
	},
	{
		"MAP16",
		16,
		NULL,
		"CWILV15",
		"MAP17",
		"MAP17",
		150,
		"SKY2",
		"D_DEAD2",
		0,
		7
	},
	{
		"MAP17",
		17,
		NULL,
		"CWILV16",
		"MAP18",
		"MAP18",
		420,
		"SKY2",
		{ 'D','_','S','T','L','K','S','3' },
		0,
		7
	},
	{
		"MAP18",
		18,
		NULL,
		"CWILV17",
		"MAP19",
		"MAP19",
		150,
		"SKY2",
		{ 'D','_','R','O','M','E','R','O' },
		0,
		7
	},
	{
		"MAP19",
		19,
		NULL,
		"CWILV18",
		"MAP20",
		"MAP20",
		210,
		"SKY2",
		{ 'D','_','S','H','A','W','N','2' },
		0,
		7
	},
	{
		"MAP20",
		20,
		NULL,
		"CWILV19",
		"MAP21",
		"MAP21",
		150,
		"SKY2",
		{ 'D','_','M','E','S','S','A','G' },
		0,
		7
	},
	{
		"MAP21",
		21,
		NULL,
		"CWILV20",
		"MAP22",
		"MAP22",
		240,
		"SKY3",
		{ 'D','_','C','O','U','N','T','2' },
		0,
		8
	},
	{
		"MAP22",
		22,
		NULL,
		"CWILV21",
		"MAP23",
		"MAP23",
		150,
		"SKY3",
		{ 'D','_','D','D','T','B','L','3' },
		0,
		8
	},
	{
		"MAP23",
		23,
		NULL,
		"CWILV22",
		"MAP24",
		"MAP24",
		180,
		"SKY3",
		"D_AMPIE",
		0,
		8
	},
	{
		"MAP24",
		24,
		NULL,
		"CWILV23",
		"MAP25",
		"MAP25",
		150,
		"SKY3",
		{ 'D','_','T','H','E','D','A','3' },
		0,
		8
	},
	{
		"MAP25",
		25,
		NULL,
		"CWILV24",
		"MAP26",
		"MAP26",
		150,
		"SKY3",
		{ 'D','_','A','D','R','I','A','N' },
		0,
		8
	},
	{
		"MAP26",
		26,
		NULL,
		"CWILV25",
		"MAP27",
		"MAP27",
		300,
		"SKY3",
		{ 'D','_','M','E','S','S','G','2' },
		0,
		8
	},
	{
		"MAP27",
		27,
		NULL,
		"CWILV26",
		"MAP28",
		"MAP28",
		330,
		"SKY3",
		{ 'D','_','R','O','M','E','R','2' },
		0,
		8
	},
	{
		"MAP28",
		28,
		NULL,
		"CWILV27",
		"MAP29",
		"MAP29",
		420,
		"SKY3",
		"D_TENSE",
		0,
		8
	},
	{
		"MAP29",
		29,
		NULL,
		"CWILV28",
		"MAP30",
		"MAP30",
		300,
		"SKY3",
		{ 'D','_','S','H','A','W','N','3' },
		0,
		8
	},
	{
		"MAP30",
		30,
		NULL,
		"CWILV29",
		{ 'E','n','d','G','a','m','e','C' },
		{ 'E','n','d','G','a','m','e','C' },
		180,
		"SKY3",
		{ 'D','_','O','P','E','N','I','N' },
		LEVEL_MONSTERSTELEFRAG,
		8
	},
	{
		"MAP31",
		31,
		NULL,
		"CWILV30",
		"MAP16",
		"MAP32",
		120,
		"SKY3",
		"D_EVIL",
		0,
		9
	},
	{
		"MAP32",
		32,
		NULL,
		"CWILV31",
		"MAP16",
		"MAP16",
		30,
		"SKY3",
		{ 'D','_','U','L','T','I','M','A' },
		0,
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
		{ 'F','L','O','O','R','4','_','8' },
		NULL,
		NULL
	},
	{
		2,		// DOOM Episode 2
		"D_VICTOR",
		"SFLR6_1",
		NULL,
		NULL
	},
	{
		3,		// DOOM Episode 3
		"D_VICTOR",
		"MFLR8_4",
		NULL,
		NULL
	},
	{
		4,		// DOOM Episode 4
		"D_VICTOR",
		"MFLR8_3",
		NULL,
		NULL
	},
	{
		5,		// DOOM II first cluster (up thru level 6)
		"D_READ_M",
		"SLIME16",
		NULL,
		NULL
	},
	{
		6,		// DOOM II second cluster (up thru level 11)
		"D_READ_M",
		"RROCK14",
		NULL,
		NULL
	},
	{
		7,		// DOOM II third cluster (up thru level 20)
		"D_READ_M",
		"RROCK07",
		NULL,
		NULL
	},
	{
		8,		// DOOM II fourth cluster (up thru level 30)
		"D_READ_M",
		"RROCK17",
		NULL,
		NULL
	},
	{
		9,		// DOOM II fifth cluster (level 31)
		"D_READ_M",
		"RROCK13",
		NULL,
		NULL
	},
	{
		10,		// DOOM II sixth cluster (level 32)
		"D_READ_M",
		"RROCK19",
		NULL,
		NULL
	},
	{
		0		// End-of-clusters marker
	}
};






