/*
** g_level.cpp
** Parses MAPINFO and controls movement between levels
**
**---------------------------------------------------------------------------
** Copyright 1998-2003 Randy Heit
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

#include "templates.h"
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
#include "i_system.h"
#include "p_setup.h"
#include "p_local.h"
#include "r_sky.h"
#include "c_console.h"
#include "f_finale.h"
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
#include "m_menu.h"

#include "gi.h"


EXTERN_CVAR (Float, sv_gravity)
EXTERN_CVAR (Float, sv_aircontrol)
EXTERN_CVAR (Int, disableautosave)

#define lioffset(x)		myoffsetof(level_info_t,x)
#define cioffset(x)		myoffsetof(cluster_info_t,x)

#define SNAP_ID			MAKE_ID('s','n','A','p')
#define VIST_ID			MAKE_ID('v','i','S','t')
#define ACSD_ID			MAKE_ID('a','c','S','d')
#define RCLS_ID			MAKE_ID('r','c','L','s')
#define PCLS_ID			MAKE_ID('p','c','L','s')

static int FindEndSequence (int type, const char *picname);
static void SetEndSequence (char *nextmap, int type);
static void InitPlayerClasses ();
static void ParseEpisodeInfo ();
static void G_DoParseMapInfo (int lump);
static void SetLevelNum (level_info_t *info, int num);

static FRandom pr_classchoice ("RandomPlayerClassChoice");

TArray<EndSequence> EndSequences;

static const char Musics1[48][9] =
{
	"D_E1M1",		"D_E1M2",		"D_E1M3",		"D_E1M4",		"D_E1M5",
	"D_E1M6",		"D_E1M7",		"D_E1M8",		"D_E1M9",		"D_E2M1",
	"D_E2M2",		"D_E2M3",		"D_E2M4",		"D_E2M5",		"D_E2M6",
	"D_E2M7",		"D_E2M8",		"D_E2M9",		"D_E3M1",		"D_E3M2",
	"D_E3M3",		"D_E3M4",		"D_E3M5",		"D_E3M6",		"D_E3M7",
	"D_E3M8",		"D_E3M9",		"D_E3M4",		"D_E3M2",		"D_E3M3",
	"D_E1M5",		"D_E2M7",		"D_E2M4",		"D_E2M6",		"D_E2M5",
	"D_E1M9",		"MUS_E2M1",		"MUS_E2M2",		"MUS_E2M3",		"MUS_E2M4",
	"MUS_E1M4",		"MUS_E2M6",		"MUS_E2M7",		"MUS_E2M8",		"MUS_E2M9",
	"MUS_E3M2",		"MUS_E3M3",		"MUS_E1M6"
};

static const char Musics2[36][9] =
{
	"MUS_E1M1",		"MUS_E1M2",		"MUS_E1M3",		"MUS_E1M4",		"MUS_E1M5",
	"MUS_E1M6",		"MUS_E1M7",		"MUS_E1M8",		"MUS_E1M9",		"MUS_E2M1",
	"MUS_E2M2",		"MUS_E2M3",		"MUS_E2M4",		"MUS_E1M4",		"MUS_E2M6",
	"MUS_E2M7",		"MUS_E2M8",		"MUS_E2M9",		"MUS_E1M1",		"MUS_E3M2",
	"MUS_E3M3",		"MUS_E1M6",		"MUS_E1M3",		"MUS_E1M2",		"MUS_E1M5",
	"MUS_E1M9",		"MUS_E2M6",		"MUS_E1M6",		"MUS_E1M2",		"MUS_E1M3",
	"MUS_E1M4",		"MUS_E1M5",		"MUS_E1M1",		"MUS_E1M7",		"MUS_E1M8",
	"MUS_E1M9"
};

static const char Musics3[32][9] =
{
	"D_RUNNIN",		"D_STALKS",		"D_COUNTD",		"D_BETWEE",		"D_DOOM",
	"D_THE_DA",		"D_SHAWN",		"D_DDTBLU",		"D_IN_CIT",		"D_DEAD",
	"D_STLKS2",		"D_THEDA2",		"D_DOOM2",		"D_DDTBL2",		"D_RUNNI2",
	"D_DEAD2",		"D_STLKS3",		"D_ROMERO",		"D_SHAWN2",		"D_MESSAG",
	"D_COUNT2",		"D_DDTBL3",		"D_AMPIE",		"D_THEDA3",		"D_ADRIAN",
	"D_MESSG2",		"D_ROMER2",		"D_TENSE",		"D_SHAWN3",		"D_OPENIN",
	"D_EVIL",		"D_ULTIMA"
};

static const char Musics4[15][9] =
{
	"D_VICTOR",		"D_VICTOR",		"D_VICTOR",		"D_VICTOR",		"D_READ_M",
	"D_READ_M",		"D_READ_M",		"D_READ_M",		"D_READ_M",		"D_READ_M",
	"MUS_CPTD",		"MUS_CPTD",		"MUS_CPTD",		"MUS_CPTD",		"MUS_CPTD"
};

extern int timingdemo;

// Start time for timing demos
int starttime;


// ACS variables with world scope
SDWORD ACS_WorldVars[NUM_WORLDVARS];
TAutoGrowArray<SDWORD> ACS_WorldArrays[NUM_WORLDVARS];

// ACS variables with global scope
SDWORD ACS_GlobalVars[NUM_GLOBALVARS];
TAutoGrowArray<SDWORD> ACS_GlobalArrays[NUM_GLOBALVARS];

extern BOOL netdemo;
extern char BackupSaveName[PATH_MAX];

BOOL savegamerestore;

extern int mousex, mousey;
extern bool sendpause, sendsave, sendcenterview, sendturn180, SendLand;
extern BYTE SendWeaponSlot, SendWeaponChoice;
extern int SendItemSelect;
extern artitype_t SendItemUse;
extern artitype_t LocalSelectedItem;

void *statcopy;					// for statistics driver

level_locals_t level;			// info about current level

static cluster_info_t *wadclusterinfos;
static int numwadclusterinfos = 0;
level_info_t *wadlevelinfos;
int numwadlevelinfos = 0;

BOOL HexenHack;

static level_info_t TheDefaultLevelInfo = { "", 0, "", "", "", "SKY1", 0, 0, 0, NULL, "Unnamed", "COLORMAP", +8, -8 };

static cluster_info_t TheDefaultClusterInfo = { 0 };



static const char *MapInfoTopLevel[] =
{
	"map",
	"defaultmap",
	"clusterdef",
	"episode",
	"clearepisodes",
	NULL
};

enum
{
	MITL_MAP,
	MITL_DEFAULTMAP,
	MITL_CLUSTERDEF,
	MITL_EPISODE,
	MITL_CLEAREPISODES
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
	"intermission",
	"doublesky",
	"nosoundclipping",
	"allowmonstertelefrags",
	"map07special",
	"baronspecial",
	"cyberdemonspecial",
	"spidermastermindspecial",
	"minotaurspecial",
	"dsparilspecial",
	"ironlichspecial",
	"specialaction_exitlevel",
	"specialaction_opendoor",
	"specialaction_lowerfloor",
	"specialaction_killmonsters",
	"lightning",
	"fadetable",
	"evenlighting",
	"noautosequences",
	"forcenoskystretch",
	"allowfreelook",
	"nofreelook",
	"allowjump",
	"nojump",
	"fallingdamage",		// Hexen falling damage
	"oldfallingdamage",		// Lesser ZDoom falling damage
	"forcefallingdamage",	// Skull Tag compatibility name for oldfallingdamage
	"nofallingdamage",
	"cdtrack",
	"cdid",
	"cd_start_track",
	"cd_end1_track",
	"cd_end2_track",
	"cd_end3_track",
	"cd_intermission_track",
	"cd_title_track",
	"warptrans",
	"vertwallshade",
	"horizwallshade",
	"gravity",
	"aircontrol",
	"filterstarts",
	"activateowndeathspecials",
	"killeractivatesdeathspecials",
	"noinventorybar",
	NULL
};

enum EMIType
{
	MITYPE_EATNEXT,
	MITYPE_IGNORE,
	MITYPE_INT,
	MITYPE_FLOAT,
	MITYPE_HEX,
	MITYPE_COLOR,
	MITYPE_MAPNAME,
	MITYPE_LUMPNAME,
	MITYPE_SKY,
	MITYPE_SETFLAG,
	MITYPE_CLRFLAG,
	MITYPE_SCFLAGS,
	MITYPE_CLUSTER,
	MITYPE_STRING,
	MITYPE_MUSIC,
	MITYPE_RELLIGHT,
	MITYPE_CLRBYTES
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
	{ MITYPE_MUSIC,		lioffset(music), lioffset(musicorder) },
	{ MITYPE_SETFLAG,	LEVEL_NOINTERMISSION, 0 },
	{ MITYPE_CLRFLAG,	LEVEL_NOINTERMISSION, 0 },
	{ MITYPE_SETFLAG,	LEVEL_DOUBLESKY, 0 },
	{ MITYPE_IGNORE,	0, 0 },	// was nosoundclipping
	{ MITYPE_SETFLAG,	LEVEL_MONSTERSTELEFRAG, 0 },
	{ MITYPE_SETFLAG,	LEVEL_MAP07SPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_BRUISERSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_CYBORGSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_SPIDERSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_MINOTAURSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_SORCERER2SPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_HEADSPECIAL, 0 },
	{ MITYPE_SCFLAGS,	0, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_SCFLAGS,	LEVEL_SPECOPENDOOR, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_SCFLAGS,	LEVEL_SPECLOWERFLOOR, ~LEVEL_SPECACTIONSMASK },
	{ MITYPE_SETFLAG,	LEVEL_SPECKILLMONSTERS, 0 },
	{ MITYPE_SETFLAG,	LEVEL_STARTLIGHTNING, 0 },
	{ MITYPE_LUMPNAME,	lioffset(fadetable), 0 },
	{ MITYPE_CLRBYTES,	lioffset(WallVertLight), lioffset(WallHorizLight) },
	{ MITYPE_SETFLAG,	LEVEL_SNDSEQTOTALCTRL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_FORCENOSKYSTRETCH, 0 },
	{ MITYPE_SCFLAGS,	LEVEL_FREELOOK_YES, ~LEVEL_FREELOOK_NO },
	{ MITYPE_SCFLAGS,	LEVEL_FREELOOK_NO, ~LEVEL_FREELOOK_YES },
	{ MITYPE_SCFLAGS,	LEVEL_JUMP_YES, ~LEVEL_JUMP_NO },
	{ MITYPE_SCFLAGS,	LEVEL_JUMP_NO, ~LEVEL_JUMP_YES },
	{ MITYPE_SCFLAGS,	LEVEL_FALLDMG_HX, ~LEVEL_FALLDMG_ZD },
	{ MITYPE_SCFLAGS,	LEVEL_FALLDMG_ZD, ~LEVEL_FALLDMG_HX },
	{ MITYPE_SCFLAGS,	LEVEL_FALLDMG_ZD, ~LEVEL_FALLDMG_HX },
	{ MITYPE_SCFLAGS,	0, ~(LEVEL_FALLDMG_ZD|LEVEL_FALLDMG_HX) },
	{ MITYPE_INT,		lioffset(cdtrack), 0 },
	{ MITYPE_HEX,		lioffset(cdid), 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_INT,		lioffset(WarpTrans), 0 },
	{ MITYPE_RELLIGHT,	lioffset(WallVertLight), 0 },
	{ MITYPE_RELLIGHT,	lioffset(WallHorizLight), 0 },
	{ MITYPE_FLOAT,		lioffset(gravity), 0 },
	{ MITYPE_FLOAT,		lioffset(aircontrol), 0 },
	{ MITYPE_SETFLAG,	LEVEL_FILTERSTARTS, 0 },
	{ MITYPE_SETFLAG,	LEVEL_ACTOWNSPECIAL, 0 },
	{ MITYPE_CLRFLAG,	LEVEL_ACTOWNSPECIAL, 0 },
	{ MITYPE_SETFLAG,	LEVEL_NOINVENTORYBAR, 0 },
};

static const char *MapInfoClusterLevel[] =
{
	"entertext",
	"exittext",
	"music",
	"flat",
	"hub",
	"cdtrack",
	"cdid",
	"entertextislump",
	"exittextislump",
	NULL
};

MapInfoHandler ClusterHandlers[] =
{
	{ MITYPE_STRING,	cioffset(entertext), 0 },
	{ MITYPE_STRING,	cioffset(exittext), 0 },
	{ MITYPE_MUSIC,		cioffset(messagemusic), cioffset(musicorder) },
	{ MITYPE_LUMPNAME,	cioffset(finaleflat), 0 },
	{ MITYPE_SETFLAG,	CLUSTER_HUB, 0 },
	{ MITYPE_INT,		cioffset(cdtrack), 0 },
	{ MITYPE_HEX,		cioffset(cdid), 0 },
	{ MITYPE_SETFLAG,	CLUSTER_ENTERTEXTINLUMP, 0 },
	{ MITYPE_SETFLAG,	CLUSTER_EXITTEXTINLUMP, 0 }
};

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_info_t *levelinfo,
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

static void SetLevelDefaults (level_info_t *levelinfo)
{
	memset (levelinfo, 0, sizeof(*levelinfo));
	levelinfo->snapshot = NULL;
	levelinfo->outsidefog = 0xff000000;
	levelinfo->WallHorizLight = -8;
	levelinfo->WallVertLight = +8;
	strncpy (levelinfo->fadetable, "COLORMAP", 8);
}

//
// G_ParseMapInfo
// Parses the MAPINFO lumps of all loaded WADs and generates
// data for wadlevelinfos and wadclusterinfos.
//
void G_ParseMapInfo ()
{
	int lump, lastlump = 0;

	// Parse the default MAPINFO for the current game.
	switch (gameinfo.gametype)
	{
	case GAME_Doom:
		switch (gamemission)
		{
		case doom:
			G_DoParseMapInfo (Wads.GetNumForName ("D1INFO"));
			break;
		case pack_plut:
			G_DoParseMapInfo (Wads.GetNumForName ("PLUTINFO"));
			break;
		case pack_tnt:
			G_DoParseMapInfo (Wads.GetNumForName ("TNTINFO"));
			break;
		default:
			G_DoParseMapInfo (Wads.GetNumForName ("D2INFO"));
			break;
		}
		break;

	case GAME_Heretic:
		G_DoParseMapInfo (Wads.GetNumForName ("HERINFO"));
		break;

	case GAME_Hexen:
		// Hexen already has a MAPINFO.
		break;

	case GAME_Strife:
		G_DoParseMapInfo (Wads.GetNumForName ("STRFINFO"));
		break;

	default:
		break;
	}

	// Parse any extra MAPINFOs.
	GStrings.LoadNames ();
	while ((lump = Wads.FindLump ("MAPINFO", &lastlump)) != -1)
	{
		G_DoParseMapInfo (lump);
	}
	GStrings.FlushNames ();
	EndSequences.ShrinkToFit ();

	if (EpiDef.numitems == 0)
	{
		I_FatalError ("You cannot use clearepisodes in a MAPINFO if you do not define any new episodes after it.");
	}
}

static void G_DoParseMapInfo (int lump)
{
	level_info_t defaultinfo;
	level_info_t *levelinfo;
	int levelindex;
	cluster_info_t *clusterinfo;
	int clusterindex;
	DWORD levelflags;

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
			{	// MAPNAME is a number; assume a Hexen wad
				int map = atoi (sc_String);
				sprintf (sc_String, "MAP%02d", map);
				HexenHack = true;
				// Hexen levels are automatically nointermission,
				// no auto sound sequences, falling damage, and
				// monsters activate their own specials.
				levelflags |= LEVEL_NOINTERMISSION
							| LEVEL_SNDSEQTOTALCTRL
							| LEVEL_FALLDMG_HX
							| LEVEL_ACTOWNSPECIAL;
			}
			levelindex = FindWadLevelInfo (sc_String);
			if (levelindex == -1)
			{
				levelindex = numwadlevelinfos++;
				wadlevelinfos = (level_info_t *)Realloc (wadlevelinfos, sizeof(level_info_t)*numwadlevelinfos);
			}
			levelinfo = wadlevelinfos + levelindex;
			memcpy (levelinfo, &defaultinfo, sizeof(*levelinfo));
			if (HexenHack)
			{
				levelinfo->WallHorizLight = levelinfo->WallVertLight = 0;
			}
			uppercopy (levelinfo->mapname, sc_String);
			SC_MustGetString ();
			if (SC_Compare ("lookup"))
			{
				const char *thename;
				int strnum;

				SC_MustGetString ();
				strnum = GStrings.FindString (sc_String);
				if (strnum < 0)
				{
					thename = sc_String;
				}
				else
				{
					const char *lookedup = GStrings(strnum);
					char checkstring[32];

					// Strip out the header from the localized string
					if (levelinfo->mapname[0] == 'E' && levelinfo->mapname[2] == 'M')
					{
						sprintf (checkstring, "%s: ", levelinfo->mapname);
					}
					else if (levelinfo->mapname[0] == 'M' && levelinfo->mapname[1] == 'A' &&
						levelinfo->mapname[2] == 'P')
					{
						sprintf (checkstring, "level %d: ", atoi(levelinfo->mapname + 3));
					}
					thename = strstr (lookedup, checkstring);
					if (thename == NULL)
					{
						thename = lookedup;
					}
					else
					{
						thename += strlen (checkstring);
					}
				}
				ReplaceString (&levelinfo->level_name, thename);
			}
			else
			{
				ReplaceString (&levelinfo->level_name, sc_String);
			}
			// Set up levelnum now so that you can use Teleport_NewMap specials
			// to teleport to maps with standard names without needing a levelnum.
			if (!strnicmp (levelinfo->mapname, "MAP", 3) && levelinfo->mapname[5] == 0)
			{
				int mapnum = atoi (levelinfo->mapname + 3);

				if (mapnum >= 1 && mapnum <= 99)
					levelinfo->levelnum = mapnum;
			}
			else if (levelinfo->mapname[0] == 'E' &&
				levelinfo->mapname[1] >= '0' && levelinfo->mapname[1] <= '9' &&
				levelinfo->mapname[2] == 'M' &&
				levelinfo->mapname[3] >= '0' && levelinfo->mapname[3] <= '9')
			{
				int epinum = levelinfo->mapname[1] - '1';
				int mapnum = levelinfo->mapname[3] - '0';
				levelinfo->levelnum = epinum*10 + mapnum;
			}
			ParseMapInfoLower (MapHandlers, MapInfoMapLevel, levelinfo, NULL, levelflags);
			SetLevelNum (levelinfo, levelinfo->levelnum);	// Wipe out matching levelnums from other maps.
			if (levelinfo->pname[0] != 0 && TexMan.CheckForTexture (levelinfo->pname, FTexture::TEX_MiscPatch) < 0)
			{
				int lumpnum = Wads.CheckNumForName (levelinfo->pname);
				if (lumpnum >= 0)
				{
					TexMan.CreateTexture (lumpnum, FTexture::TEX_MiscPatch);
				}
				else
				{
					levelinfo->pname[0] = 0;
				}
			}
			break;

		case MITL_CLUSTERDEF:	// clusterdef <clusternum>
			SC_MustGetNumber ();
			clusterindex = FindWadClusterInfo (sc_Number);
			if (clusterindex == -1)
			{
				clusterindex = numwadclusterinfos++;
				wadclusterinfos = (cluster_info_t *)Realloc (wadclusterinfos, sizeof(cluster_info_t)*numwadclusterinfos);
				clusterinfo = wadclusterinfos + clusterindex;
			}
			else
			{
				clusterinfo = wadclusterinfos + clusterindex;
				if (clusterinfo->entertext != NULL)
				{
					delete[] clusterinfo->entertext;
				}
				if (clusterinfo->exittext != NULL)
				{
					delete[] clusterinfo->exittext;
				}
				if (clusterinfo->messagemusic != NULL)
				{
					delete[] clusterinfo->messagemusic;
				}
			}
			memset (clusterinfo, 0, sizeof(cluster_info_t));
			clusterinfo->cluster = sc_Number;
			ParseMapInfoLower (ClusterHandlers, MapInfoClusterLevel, NULL, clusterinfo, 0);
			break;

		case MITL_EPISODE:
			ParseEpisodeInfo ();
			break;

		case MITL_CLEAREPISODES:
			for (int i = 0; i < EpiDef.numitems; ++i)
			{
				delete[] const_cast<char *>(EpisodeMenu[i].name);
				EpisodeMenu[i].name = NULL;
			}
			EpiDef.numitems = 0;
			break;
		}
	}
	SC_Close ();
}

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_info_t *levelinfo,
							   cluster_info_t *clusterinfo,
							   DWORD flags)
{
	int entry;
	MapInfoHandler *handler;
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
		case MITYPE_EATNEXT:
			SC_MustGetString ();
			break;

		case MITYPE_IGNORE:
			break;

		case MITYPE_INT:
			SC_MustGetNumber ();
			*((int *)(info + handler->data1)) = sc_Number;
			break;

		case MITYPE_FLOAT:
			SC_MustGetFloat ();
			*((float *)(info + handler->data1)) = sc_Float;
			break;

		case MITYPE_HEX:
			SC_MustGetString ();
			*((int *)(info + handler->data1)) = strtoul (sc_String, NULL, 16);
			break;

		case MITYPE_COLOR:
			SC_MustGetString ();
			*((DWORD *)(info + handler->data1)) = V_GetColor (NULL, sc_String);
			break;

		case MITYPE_MAPNAME: {
			EndSequence newSeq;
			bool useseq = false;

			SC_MustGetString ();
			if (IsNum (sc_String))
			{
				int map = atoi (sc_String);

				if (HexenHack)
				{
					sprintf (sc_String, "&wt@%02d", map);
				}
				else
				{
					sprintf (sc_String, "MAP%02d", map);
				}
			}
			if (strnicmp (sc_String, "EndGame", 7) == 0)
			{
				int type;
				switch (sc_String[7])
				{
				case '1':	type = END_Pic1;		break;
				case '2':	type = END_Pic2;		break;
				case '3':	type = END_Bunny;		break;
				case 'C':	type = END_Cast;		break;
				case 'W':	type = END_Underwater;	break;
				default:	type = END_Pic3;		break;
				}
				newSeq.EndType = type;
				useseq = true;
			}
			else if (SC_Compare ("endpic"))
			{
				SC_MustGetString ();
				newSeq.EndType = END_Pic;
				strcpy (newSeq.PicName, "endseq");
				useseq = true;
			}
			else if (SC_Compare ("endbunny"))
			{
				newSeq.EndType = END_Bunny;
				useseq = true;
			}
			else if (SC_Compare ("endcast"))
			{
				newSeq.EndType = END_Cast;
				useseq = true;
			}
			else if (SC_Compare ("enddemon"))
			{
				newSeq.EndType = END_Demon;
				useseq = true;
			}
			else if (SC_Compare ("endchess"))
			{
				newSeq.EndType = END_Chess;
				useseq = true;
			}
			else if (SC_Compare ("endunderwater"))
			{
				newSeq.EndType = END_Underwater;
				useseq = true;
			}
			else
			{
				strncpy ((char *)(info + handler->data1), sc_String, 8);
			}
			if (useseq)
			{
				int seqnum = FindEndSequence (newSeq.EndType, newSeq.PicName);
				if (seqnum == -1)
				{
					seqnum = (int)EndSequences.Push (newSeq);
				}
				strcpy ((char *)(info + handler->data1), "enDSeQ");
				*((WORD *)(info + handler->data1 + 6)) = (WORD)seqnum;
			}
			break;
		  }

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

		case MITYPE_CLRFLAG:
			flags &= ~handler->data1;
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
			if (SC_Compare ("lookup"))
			{
				SC_MustGetString ();
				int strnum = GStrings.FindString (sc_String);
				if (strnum >= 0)
				{
					ReplaceString ((char **)(info + handler->data1), GStrings(strnum));
					break;
				}
			}
			ReplaceString ((char **)(info + handler->data1), sc_String);
			break;

		case MITYPE_MUSIC:
			SC_MustGetString ();
			{
				char *colon = strchr (sc_String, ':');
				if (colon)
				{
					*colon = 0;
				}
				ReplaceString ((char **)(info + handler->data1), sc_String);
				*((int *)(info + handler->data2)) = colon ? atoi (colon + 1) : 0;
			}
			break;

		case MITYPE_RELLIGHT:
			SC_MustGetNumber ();
			*((SBYTE *)(info + handler->data1)) = (SBYTE)clamp (sc_Number / 2, -128, 127);
			break;

		case MITYPE_CLRBYTES:
			*((BYTE *)(info + handler->data1)) = 0;
			*((BYTE *)(info + handler->data2)) = 0;
			break;
		}
	}
	if (levelinfo)
	{
		levelinfo->flags = flags;
	}
	else
	{
		clusterinfo->flags = flags;
	}
}

// Episode definitions start with the header "episode <start-map>"
// and then can be followed by any of the following:
//
// name "Episode name as text"
// picname "Picture to display the episode name"
// key "Shortcut key for the menu"
// remove

static void ParseEpisodeInfo ()
{
	int i;
	char map[9];
	char *pic = NULL;
	bool picisgfx = false;	// Shut up, GCC!!!!
	bool remove = false;
	char key = 0;
	bool addedgfx = false;

	// Get map name
	SC_MustGetString ();
	uppercopy (map, sc_String);
	map[8] = 0;

	SC_MustGetString ();
	do
	{
		if (SC_Compare ("name"))
		{
			SC_MustGetString ();
			ReplaceString (&pic, sc_String);
			picisgfx = false;
		}
		else if (SC_Compare ("picname"))
		{
			SC_MustGetString ();
			ReplaceString (&pic, sc_String);
			picisgfx = true;
		}
		else if (SC_Compare ("remove"))
		{
			remove = true;
		}
		else if (SC_Compare ("key"))
		{
			SC_MustGetString ();
			key = sc_String[0];
		}
		else
		{
			SC_UnGet ();
			break;
		}
	}
	while (SC_GetString ());

	for (i = 0; i < EpiDef.numitems; ++i)
	{
		if (strncmp (EpisodeMaps[i], map, 8) == 0)
		{
			break;
		}
	}

	if (remove)
	{
		// If the remove property is given for an episode, remove it.
		if (i < EpiDef.numitems)
		{
			if (i+1 < EpiDef.numitems)
			{
				memmove (&EpisodeMaps[i], &EpisodeMaps[i+1],
					sizeof(EpisodeMaps[0])*(EpiDef.numitems - i - 1));
				memmove (&EpisodeMenu[i], &EpisodeMenu[i+1],
					sizeof(EpisodeMenu[0])*(EpiDef.numitems - i - 1));
			}
			EpiDef.numitems--;
		}
	}
	else
	{
		if (pic == NULL)
		{
			pic = copystring (map);
			picisgfx = false;
		}

		if (i == EpiDef.numitems)
		{
			if (EpiDef.numitems == MAX_EPISODES)
			{
				i = EpiDef.numitems - 1;
			}
			else
			{
				i = EpiDef.numitems++;
			}
		}
		else
		{
			delete[] const_cast<char *>(EpisodeMenu[i].name);
		}

		EpisodeMenu[i].name = pic;
		EpisodeMenu[i].alphaKey = tolower(key);
		EpisodeMenu[i].fulltext = !picisgfx;
		strncpy (EpisodeMaps[i], map, 8);

		if (picisgfx)
		{
			if (TexMan.CheckForTexture (pic, FTexture::TEX_MiscPatch, 0) == -1)
			{
				TexMan.AddPatch (pic);
				addedgfx = true;
			}
		}
	}
}

static int FindEndSequence (int type, const char *picname)
{
	size_t i, num;

	num = EndSequences.Size ();
	for (i = 0; i < num; i++)
	{
		if (EndSequences[i].EndType == type &&
			(type != END_Pic || stricmp (EndSequences[i].PicName, picname) == 0))
		{
			return (int)i;
		}
	}
	return -1;
}

static void SetEndSequence (char *nextmap, int type)
{
	int seqnum;

	seqnum = FindEndSequence (type, NULL);
	if (seqnum == -1)
	{
		EndSequence newseq;
		newseq.EndType = type;
		memset (newseq.PicName, 0, sizeof(newseq.PicName));
		seqnum = (int)EndSequences.Push (newseq);
	}
	strcpy (nextmap, "enDSeQ");
	*((WORD *)(nextmap + 6)) = (WORD)seqnum;
}

void G_SetForEndGame (char *nextmap)
{
	if (gameinfo.gametype == GAME_Hexen)
	{
		SetEndSequence (nextmap, END_Chess);
	}
	else if (gamemode == commercial)
	{
		SetEndSequence (nextmap, END_Cast);
	}
	else
	{ // The ExMx games actually have different ends based on the episode,
	  // but I want to keep this simple.
		SetEndSequence (nextmap, END_Pic1);
	}
}

level_info_t *FindLevelByWarpTrans (int num)
{
	int i;

	for (i = numwadlevelinfos - 1; i >= 0; --i)
		if (wadlevelinfos[i].WarpTrans == num)
			return (level_info_t *)(wadlevelinfos + i);

	return NULL;
}

static void zapDefereds (acsdefered_t *def)
{
	while (def)
	{
		acsdefered_t *next = def->next;
		delete def;
		def = next;
	}
}

void P_RemoveDefereds (void)
{
	int i;

	// Remove any existing defereds
	for (i = 0; i < numwadlevelinfos; i++)
	{
		if (wadlevelinfos[i].defered)
		{
			zapDefereds (wadlevelinfos[i].defered);
			wadlevelinfos[i].defered = NULL;
		}
	}
}

bool CheckWarpTransMap (char mapname[9], bool substitute)
{
	if (mapname[0] == '&' && mapname[1] == 'w' &&
		mapname[2] == 't' && mapname[3] == '@')
	{
		level_info_t *lev = FindLevelByWarpTrans (atoi (mapname + 4));
		if (lev != NULL)
		{
			strncpy (mapname, lev->mapname, 8);
			mapname[8] = 0;
			return true;
		}
		else if (substitute)
		{
			mapname[0] = 'M';
			mapname[1] = 'A';
			mapname[2] = 'P';
			mapname[3] = mapname[4];
			mapname[4] = mapname[5];
			mapname[5] = 0;
		}
	}
	return false;
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, playeringame[] should be set.
//
static char d_mapname[9];

void G_DeferedInitNew (char *mapname)
{
	strncpy (d_mapname, mapname, 8);
	CheckWarpTransMap (d_mapname, true);
	gameaction = ga_newgame2;
}

CCMD (map)
{
	if (argv.argc() > 1)
	{
		if (Wads.CheckNumForName (argv[1]) == -1)
			Printf ("No map %s\n", argv[1]);
		else
			G_DeferedInitNew (argv[1]);
	}
	else
	{
		Printf ("Usage: map <map name>\n");
	}
}

void G_NewInit ()
{
	G_ClearSnapshots ();
	SB_state = screen->GetPageCount ();
	netgame = false;
	netdemo = false;
	multiplayer = false;
	if (demoplayback)
	{
		C_RestoreCVars ();
		demoplayback = false;
		D_SetupUserInfo ();
	}
	LocalSelectedItem = arti_none;
	memset (playeringame, 0, sizeof(playeringame));
	BackupSaveName[0] = 0;
	consoleplayer = 0;
	NextSkill = -1;
}

void G_DoNewGame (void)
{
	G_NewInit ();
	playeringame[consoleplayer] = 1;
	G_InitNew (d_mapname);
	gameaction = ga_nothing;
}

void G_InitNew (char *mapname)
{
	EGameSpeed oldSpeed;
	bool wantFast;
	int i;

	if (!savegamerestore)
		G_ClearSnapshots ();

	// [RH] Mark all levels as not visited
	if (!savegamerestore)
	{
		for (i = 0; i < numwadlevelinfos; i++)
			wadlevelinfos[i].flags &= ~LEVEL_VISITED;
	}

	UnlatchCVars ();

	if (gameskill > sk_nightmare)
		gameskill = sk_nightmare;
	else if (gameskill < sk_baby)
		gameskill = sk_baby;

	UnlatchCVars ();

	if (paused)
	{
		paused = 0;
		S_ResumeSound ();
	}

	StatusBar->NewGame ();

	// [RH] If this map doesn't exist, bomb out
	if (Wads.CheckNumForName (mapname) == -1)
	{
		I_Error ("Could not find map %s\n", mapname);
	}

	respawnmonsters =
		((gameinfo.gametype == GAME_Doom && gameskill == sk_nightmare)
		|| (dmflags & DF_MONSTERS_RESPAWN));

	oldSpeed = GameSpeed;
	wantFast = (dmflags & DF_FAST_MONSTERS) || (gameskill == sk_nightmare);
	GameSpeed = wantFast ? SPEED_Fast : SPEED_Normal;

	if (oldSpeed != GameSpeed)
	{
		FActorInfo::StaticSpeedSet ();
	}

	if (!savegamerestore)
	{
		if (!demoplayback)
		{
			if (!netgame)
			{ // [RH] Change the random seed for each new single player game
				rngseed = rngseed*3/2;
			}
			FRandom::StaticClearRandom ();
		}
		memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
		memset (ACS_GlobalVars, 0, sizeof(ACS_GlobalVars));
		for (i = 0; i < NUM_WORLDVARS; ++i)
		{
			ACS_WorldArrays[i].Clear ();
		}
		for (i = 0; i < NUM_GLOBALVARS; ++i)
		{
			ACS_GlobalArrays[i].Clear ();
		}
		level.time = 0;

		if (!multiplayer || !deathmatch)
		{
			InitPlayerClasses ();
		}

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_ENTER;	// [BC]
	}

	// [RH] Send weapon assignments now
	Net_WriteByte (DEM_SLOTSCHANGE);
	LocalWeapons.StreamOutSlots ();

	usergame = true;				// will be set false if a demo
	paused = 0;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	BorderNeedRefresh = screen->GetPageCount ();

	strncpy (level.mapname, mapname, 8);
	level.mapname[8] = 0;
	G_DoLoadLevel (0, false);
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

	wminfo.finished_ep = level.cluster - 1;
	strncpy (wminfo.lname0, level.info->pname, 8);
	strncpy (wminfo.current, level.mapname, 8);

	if (deathmatch &&
		(dmflags & DF_SAME_LEVEL) &&
		!(level.flags & LEVEL_CHANGEMAPCHEAT))
	{
		strncpy (wminfo.next, level.mapname, 8);
		strncpy (wminfo.lname1, level.info->pname, 8);
	}
	else
	{
		wminfo.next[0] = 0;
		if (secretexit)
		{
			if (Wads.CheckNumForName (level.secretmap) != -1)
			{
				strncpy (wminfo.next, level.secretmap, 8);
				strncpy (wminfo.lname1, FindLevelInfo (level.secretmap)->pname, 8);
			}
			else
			{
				secretexit = false;
			}
		}
		if (!wminfo.next[0])
		{
			strncpy (wminfo.next, level.nextmap, 8);
			strncpy (wminfo.lname1, FindLevelInfo (level.nextmap)->pname, 8);
		}
	}

	CheckWarpTransMap (wminfo.next, true);

	wminfo.next_ep = FindLevelInfo (wminfo.next)->cluster - 1;
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
		EFinishLevelType mode;

		if (thiscluster != nextcluster ||
			deathmatch ||
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

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{ // take away appropriate inventory
				G_PlayerFinishLevel (i, mode);
			}
		}

		if (mode == FINISH_SameHub)
		{ // Remember the level's state for re-entry.
			G_SnapshotLevel ();
		}
		else
		{ // Forget the states of all existing levels.
			G_ClearSnapshots ();

			if (mode == FINISH_NextHub)
			{ // Reset world variables for the new hub.
				memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
				for (i = 0; i < NUM_WORLDVARS; ++i)
				{
					ACS_WorldArrays[i].Clear ();
				}
			}
			else if (mode == FINISH_NoHub)
			{ // Reset time to zero if not entering/staying in a hub.
				level.time = 0;
			}
		}

		if (!deathmatch &&
			((level.flags & LEVEL_NOINTERMISSION) ||
			((nextcluster == thiscluster) && (thiscluster->flags & CLUSTER_HUB))))
		{
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

class DAutosaver : public DThinker
{
	DECLARE_CLASS (DAutosaver, DThinker)
public:
	void Tick ();
};

IMPLEMENT_CLASS (DAutosaver)

void DAutosaver::Tick ()
{
	gameaction = ga_autosave;
	Destroy ();
}

//
// G_DoLoadLevel 
//
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

	Printf (
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n"
			TEXTCOLOR_BOLD "%s\n\n",
			level.level_name);

	if (wipegamestate == GS_LEVEL) 
		wipegamestate = GS_FORCEWIPE;

	gamestate = GS_LEVEL; 

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = TexMan.GetTexture (gameinfo.SkyFlatName, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);

	// DOOM determines the sky texture to be used
	// depending on the current episode and the game version.
	// [RH] Fetch sky parameters from level_locals_t.
	sky1texture = TexMan.GetTexture (level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
	sky2texture = TexMan.GetTexture (level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

	// [RH] Set up details about sky rendering
	R_InitSkyMap ();

	for (i = 0; i < MAXPLAYERS; i++)
	{ 
		if (playeringame[i] && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_ENTER;	// [BC]
		memset (players[i].frags,0,sizeof(players[i].frags)); 
		players[i].fragcount = 0;
	}
/*
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
		{
			actor->touching_sectorlist = NULL;
		}
	}
*/
	SN_StopAllSequences ();
	P_SetupLevel (level.mapname, position);

	// [RH] Start lightning, if MAPINFO tells us to
	if (level.flags & LEVEL_STARTLIGHTNING)
	{
		P_StartLightning ();
	}

	displayplayer = consoleplayer;		// view the guy you are playing
	StatusBar->AttachToPlayer (&players[consoleplayer]);
	gameaction = ga_nothing; 

	// clear cmd building stuff
	ResetButtonStates ();

	SendWeaponSlot = SendWeaponChoice = 255;
	SendItemSelect = 0;
	SendItemUse = arti_none;
	mousex = mousey = 0; 
	sendpause = sendsave = sendcenterview = sendturn180 = SendLand = false;
	LocalViewAngle = 0;
	LocalViewPitch = 0;
	paused = 0;

	//Added by MC: Initialize bots.
	bglobal.Init ();

	if (timingdemo)
	{
		static BOOL firstTime = true;

		if (firstTime)
		{
			starttime = I_GetTime (false);
			firstTime = false;
		}
	}

	level.starttime = gametic;
	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	P_DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.
	
	if (demoplayback || oldgs == GS_STARTUP)
		C_HideConsole ();

	C_FlushDisplay ();

	// [RH] Always save the game when entering a new level.
	if (autosave && !savegamerestore && disableautosave < 1)
	{
		DAutosaver GCCNOWARN *dummy = new DAutosaver;
	}
}


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
	cluster_info_t *nextcluster;
	cluster_info_t *thiscluster;
	char *nextmap;

	gameaction = ga_worlddone; 

	if (level.flags & LEVEL_CHANGEMAPCHEAT)
		return;

	thiscluster = FindClusterInfo (level.cluster);
	nextmap = !secretexit || !level.secretmap[0] ? level.nextmap : level.secretmap;

	if (strncmp (nextmap, "enDSeQ", 6) == 0)
	{
		F_StartFinale (thiscluster->messagemusic, thiscluster->musicorder,
			thiscluster->cdtrack, thiscluster->cdid,
			thiscluster->finaleflat, thiscluster->exittext,
			thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
			thiscluster->flags & CLUSTER_FINALEPIC);
	}
	else
	{
		nextcluster = FindClusterInfo (FindLevelInfo (nextmap)->cluster);

		if (nextcluster->cluster != level.cluster && !deathmatch)
		{
			// Only start the finale if the next level's cluster is different
			// than the current one and we're not in deathmatch.
			if (nextcluster->entertext)
			{
				F_StartFinale (nextcluster->messagemusic, nextcluster->musicorder,
					nextcluster->cdtrack, nextcluster->cdid,
					nextcluster->finaleflat, nextcluster->entertext,
					nextcluster->flags & CLUSTER_ENTERTEXTINLUMP,
					nextcluster->flags & CLUSTER_FINALEPIC);
			}
			else if (thiscluster->exittext)
			{
				F_StartFinale (thiscluster->messagemusic, thiscluster->musicorder,
					thiscluster->cdtrack, nextcluster->cdid,
					thiscluster->finaleflat, thiscluster->exittext,
					thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
					thiscluster->flags & CLUSTER_FINALEPIC);
			}
		}
	}
} 
 
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
		strncpy (level.mapname, wminfo.next, 8);
	}
	G_DoLoadLevel (startpos, true);
	startpos = 0;
	gameaction = ga_nothing;
	viewactive = true; 
} 
 
void G_InitLevelLocals ()
{
	level_info_t *info;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any
	NormalLight.Maps = realcolormaps;
	NormalLight.Color = PalEntry (255, 255, 255);

	level.gravity = sv_gravity * 35/TICRATE;
	level.aircontrol = (fixed_t)(sv_aircontrol * 65536.f);
	level.flags = 0;

	info = FindLevelInfo (level.mapname);

	level.info = info;
	level.skyspeed1 = info->skyspeed1;
	level.skyspeed2 = info->skyspeed2;
	info = (level_info_t *)info;
	strncpy (level.skypic2, info->skypic2, 8);
	level.fadeto = info->fadeto;
	level.cdtrack = info->cdtrack;
	level.cdid = info->cdid;
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
	level.outsidefog = info->outsidefog;
	level.flags |= LEVEL_DEFINEDINMAPINFO;
	level.WallVertLight = info->WallVertLight;
	level.WallHorizLight = info->WallHorizLight;
	if (info->gravity != 0.f)
	{
		level.gravity = info->gravity * 35/TICRATE;
	}
	if (info->aircontrol != 0.f)
	{
		level.aircontrol = (fixed_t)(info->aircontrol * 65536.f);
	}

	G_AirControlChanged ();

	if (info->level_name)
	{
		cluster_info_t *clus = FindClusterInfo (info->cluster);

		level.partime = info->partime;
		level.cluster = info->cluster;
		level.clusterflags = clus ? clus->flags : 0;
		level.flags |= info->flags;
		level.levelnum = info->levelnum;
		level.music = info->music;
		level.musicorder = info->musicorder;

		strncpy (level.level_name, info->level_name, 63);
		strncpy (level.nextmap, info->nextmap, 8);
		level.nextmap[8] = 0;
		strncpy (level.secretmap, info->secretmap, 8);
		level.secretmap[8] = 0;
		strncpy (level.skypic1, info->skypic1, 8);
		level.skypic1[8] = 0;
		if (!level.skypic2[0])
			strncpy (level.skypic2, level.skypic1, 8);
		level.skypic2[8] = 0;
	}
	else
	{
		level.partime = level.cluster = 0;
		strcpy (level.level_name, "Unnamed");
		level.nextmap[0] =
			level.secretmap[0] = 0;
		level.music = NULL;
		strcpy (level.skypic1, "SKY1");
		strcpy (level.skypic2, "SKY1");
		level.flags = 0;
		level.levelnum = 1;
	}

	int clear = 0, set = 0;

	if (level.flags & LEVEL_JUMP_YES)
		clear = DF_NO_JUMP;
	if (level.flags & LEVEL_JUMP_NO)
		set = DF_NO_JUMP;
	if (level.flags & LEVEL_FREELOOK_YES)
		clear |= DF_NO_FREELOOK;
	if (level.flags & LEVEL_FREELOOK_NO)
		set |= DF_NO_FREELOOK;

	dmflags = (dmflags & ~clear) | set;

	NormalLight.ChangeFade (level.fadeto);

	if (level.Scrolls != NULL)
	{
		delete[] level.Scrolls;
		level.Scrolls = NULL;
	}
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

level_info_t *FindLevelInfo (char *mapname)
{
	int i;

	if ((i = FindWadLevelInfo (mapname)) > -1)
		return (level_info_t *)(wadlevelinfos + i);
	else
		return &TheDefaultLevelInfo;
}

level_info_t *FindLevelByNum (int num)
{
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
		if (wadlevelinfos[i].levelnum == num)
			return (level_info_t *)(wadlevelinfos + i);

	return NULL;
}

static void SetLevelNum (level_info_t *info, int num)
{
	// Avoid duplicate levelnums. The level being set always has precedence.
	for (int i = 0; i < numwadlevelinfos; ++i)
	{
		if (wadlevelinfos[i].levelnum == num)
			wadlevelinfos[i].levelnum = 0;
	}
	info->levelnum = num;
}

cluster_info_t *FindClusterInfo (int cluster)
{
	int i;

	if ((i = FindWadClusterInfo (cluster)) > -1)
		return wadclusterinfos + i;
	else
		return &TheDefaultClusterInfo;
}

void G_SetLevelStrings (void)
{
	int i;

	if (level.info)
		strncpy (level.level_name, level.info->level_name, 63);

	// Set the default episodes
	if (EpiDef.numitems == 0)
	{
		static const char eps[5][8] =
		{
			"E1M1", "E2M1", "E3M1", "E4M1", "E5M1"
		};
		static const char depinames[4][7] =
		{
			"M_EPI1", "M_EPI2", "M_EPI3", "M_EPI4"
		};
		static const char depikeys[4] = { 'k', 't', 'i', 't' };

		static const char *hepinames[5] =
		{
			"CITY OF THE DAMNED",
			"HELL'S MAW",
			"THE DOME OF D'SPARIL",
			"THE OSSUARY",
			"THE STAGNANT DEMESNE",
		};
		static const char hepikeys[5] = { 'c', 'h', 'd', 'o', 's' };

		if (gameinfo.flags & GI_MAPxx)
		{
			if (gameinfo.gametype == GAME_Hexen)
			{
				// "&wt@01" is a magic name that will become whatever map has
				// warptrans 1.
				strcpy (EpisodeMaps[0], "&wt@01");
				EpisodeMenu[0].name = copystring ("Hexen");
			}
			else
			{
				strcpy (EpisodeMaps[0], "MAP01");
				EpisodeMenu[0].name = copystring ("Hell on Earth");
			}
			EpisodeMenu[0].alphaKey = 'h';
			EpisodeMenu[0].fulltext = true;
			EpiDef.numitems = 1;
		}
		else if (gameinfo.gametype == GAME_Doom)
		{
			memcpy (EpisodeMaps, eps, 4*8);
			for (i = 0; i < 4; ++i)
			{
				EpisodeMenu[i].name = copystring (depinames[i]);
				EpisodeMenu[i].fulltext = false;
				EpisodeMenu[i].alphaKey = depikeys[i];
			}
			if (gameinfo.flags & GI_MENUHACK_RETAIL)
			{
				EpiDef.numitems = 4;
			}
			else
			{
				EpiDef.numitems = 3;
			}
		}
		else
		{
			memcpy (EpisodeMaps, eps, 5*8);
			for (i = 0; i < 5; ++i)
			{
				EpisodeMenu[i].name = copystring (hepinames[i]);
				EpisodeMenu[i].fulltext = true;
				EpisodeMenu[i].alphaKey = hepikeys[i];
			}
			if (gameinfo.flags & GI_MENUHACK_EXTENDED)
			{
				EpiDef.numitems = 5;
			}
			else
			{
				EpiDef.numitems = 3;
			}
		}
	}
}

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

void G_SerializeLevel (FArchive &arc, bool hubLoad)
{
	int i;

	arc << level.flags
		<< level.fadeto
		<< level.found_secrets
		<< level.found_items
		<< level.killed_monsters
		<< level.gravity
		<< level.aircontrol;

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
	P_SerializeThinkers (arc, hubLoad);
	P_SerializeWorld (arc);
	P_SerializePolyobjs (arc);
	P_SerializeSounds (arc);
	StatusBar->Serialize (arc);
	SerializeInterpolations (arc);

	arc << level.total_monsters << level.total_items << level.total_secrets;

	// Does this level have custom translations?
	if (arc.IsStoring ())
	{
		for (i = 0; i < MAX_ACS_TRANSLATIONS; ++i)
		{
			BYTE *trans = &translationtables[TRANSLATION_LevelScripted][i*256];
			int j;
			for (j = 0; j < 256; ++j)
			{
				if (trans[j] != j)
				{
					break;
				}
			}
			if (j < 256)
			{
				t = i;
				arc << t;
				arc.Write (trans, 256);
			}
		}
		t = 255;
		arc << t;
	}
	else
	{
		arc << t;
		while (t != 255)
		{
			if (t >= MAX_ACS_TRANSLATIONS)
			{ // hack hack to avoid crashing
				t = 0;
			}
			arc.Read (&translationtables[TRANSLATION_LevelScripted][t*256], 256);
			arc << t;
		}
	}
	if (!hubLoad)
	{
		P_SerializePlayers (arc);
	}
}

// Archives the current level
void G_SnapshotLevel ()
{
	if (level.info->snapshot)
		delete level.info->snapshot;

	if (level.info->mapname[0] != 0)
	{
		level.info->snapshotVer = SAVEVER;
		level.info->snapshot = new FCompressedMemFile;
		level.info->snapshot->Open ();

		FArchive arc (*level.info->snapshot);

		SaveVersion = SAVEVER;
		G_SerializeLevel (arc, false);
	}
}

// Unarchives the current level based on its snapshot
// The level should have already been loaded and setup.
void G_UnSnapshotLevel (bool hubLoad)
{
	if (level.info->snapshot == NULL)
		return;

	if (level.info->mapname[0] != 0)
	{
		SaveVersion = level.info->snapshotVer;
		level.info->snapshot->Reopen ();
		FArchive arc (*level.info->snapshot);
		if (hubLoad)
			arc.SetHubTravel ();
		G_SerializeLevel (arc, hubLoad);
		arc.Close ();
	}
	// No reason to keep the snapshot around once the level's been entered.
	delete level.info->snapshot;
	level.info->snapshot = NULL;
}

void G_ClearSnapshots (void)
{
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
	{
		if (wadlevelinfos[i].snapshot)
		{
			delete wadlevelinfos[i].snapshot;
			wadlevelinfos[i].snapshot = NULL;
		}
	}
}

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

static void writeSnapShot (FArchive &arc, level_info_t *i)
{
	arc << i->snapshotVer;
	writeMapName (arc, i->mapname);
	i->snapshot->Serialize (arc);
}

void G_WriteSnapshots (FILE *file)
{
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
	{
		if (wadlevelinfos[i].snapshot)
		{
			FPNGChunkArchive arc (file, SNAP_ID);
			writeSnapShot (arc, (level_info_t *)&wadlevelinfos[i]);
		}
	}

	FPNGChunkArchive *arc = NULL;
	
	// Write out which levels have been visited
	for (i = 0; i < numwadlevelinfos; ++i)
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


static void writeDefereds (FArchive &arc, level_info_t *i)
{
	writeMapName (arc, i->mapname);
	arc << i->defered;
}

void P_WriteACSDefereds (FILE *file)
{
	FPNGChunkArchive *arc = NULL;
	int i;

	for (i = 0; i < numwadlevelinfos; i++)
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


void level_locals_s::Tick ()
{
	// Reset carry sectors
	if (Scrolls != NULL)
	{
		memset (Scrolls, 0, sizeof(*Scrolls)*numsectors);
	}
}

void level_locals_s::AddScroller (DScroller *scroller, int secnum)
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

// Initializes player classes in case they are random.
// This gets called at the start of a new game, and the classes
// chosen here are used for the remainder of a single-player
// or coop game. These are ignored for deathmatch.

static void InitPlayerClasses ()
{
	if (!savegamerestore)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			SinglePlayerClass[i] = players[i].userinfo.PlayerClass;
			if (SinglePlayerClass[i] < 0 || !playeringame[i])
			{
				SinglePlayerClass[i] = (pr_classchoice() >> 6) % 3;
			}
			players[i].cls = NULL;
			players[i].CurrentPlayerClass = SinglePlayerClass[i];
		}
	}
}
