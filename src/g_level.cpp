/*
** g_level.cpp
** Parses MAPINFO and controls movement between levels
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
#include "statnums.h"

#include "gi.h"

#include "g_hub.h"

EXTERN_CVAR (Float, sv_gravity)
EXTERN_CVAR (Float, sv_aircontrol)
EXTERN_CVAR (Int, disableautosave)

// Hey, GCC, these macros better be safe!
#define lioffset(x)		((size_t)&((level_info_t*)1)->x - 1)
#define cioffset(x)		((size_t)&((cluster_info_t*)1)->x - 1)

#define SNAP_ID			MAKE_ID('s','n','A','p')
#define DSNP_ID			MAKE_ID('d','s','N','p')
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
static void ClearEpisodes ();
static void ClearLevelInfoStrings (level_info_t *linfo);
static void ClearClusterInfoStrings (cluster_info_t *cinfo);

static FRandom pr_classchoice ("RandomPlayerClassChoice");

TArray<EndSequence> EndSequences;

extern bool timingdemo;

// Start time for timing demos
int starttime;


// ACS variables with world scope
SDWORD ACS_WorldVars[NUM_WORLDVARS];
FWorldGlobalArray ACS_WorldArrays[NUM_WORLDVARS];

// ACS variables with global scope
SDWORD ACS_GlobalVars[NUM_GLOBALVARS];
FWorldGlobalArray ACS_GlobalArrays[NUM_GLOBALVARS];

extern bool netdemo;
extern FString BackupSaveName;

bool savegamerestore;

extern int mousex, mousey;
extern bool sendpause, sendsave, sendturn180, SendLand;
extern const AInventory *SendItemUse, *SendItemDrop;

void *statcopy;					// for statistics driver

level_locals_t level;			// info about current level

static TArray<cluster_info_t> wadclusterinfos;
TArray<level_info_t> wadlevelinfos;

// MAPINFO is parsed slightly differently when the map name is just a number.
static bool HexenHack;

static char unnamed[] = "Unnamed";
static level_info_t TheDefaultLevelInfo =
{
 	"",			// mapname
 	0, 			// levelnum
 	"", 		// pname,
 	"", 		// nextmap
 	"",			// secretmap
 	"SKY1",		// skypic1
 	0, 			// cluster
 	0, 			// partime
 	0, 			// sucktime
 	0, 			// flags
 	NULL, 		// music
 	unnamed, 	// level_name
 	"COLORMAP",	// fadetable
 	+8, 		// WallVertLight
 	-8,			// WallHorizLight
	"",			// [RC] F1
};

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
	"sucktime",
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
	"strifefallingdamage",	// Strife's falling damage is really unforgiving
	"nofallingdamage",
	"noallies",
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
	"missilesactivateimpactlines",
	"missileshootersactivetimpactlines",
	"noinventorybar",
	"deathslideshow",
	"redirect",
	"strictmonsteractivation",
	"laxmonsteractivation",
	"additive_scrollers",
	"interpic",
	"exitpic",
	"enterpic",
	"intermusic",
	"airsupply",
	"specialaction",
	"keepfullinventory",
	"monsterfallingdamage",
	"nomonsterfallingdamage",
	"sndseq",
	"sndinfo",
	"soundinfo",
	"clipmidtextures",
	"wrapmidtextures",
	"allowcrouch",
	"nocrouch",
	"pausemusicinmenus",
	"compat_shorttex",	
	"compat_stairs",		
	"compat_limitpain",	
	"compat_nopassover",	
	"compat_notossdrops",	
	"compat_useblocking", 
	"compat_nodoorlight",	
	"compat_ravenscroll",	
	"compat_soundtarget",	
	"compat_dehhealth",	
	"compat_trace",		
	"compat_dropoff",
	"compat_boomscroll",
	"compat_invisibility",
	"bordertexture",
	"f1", // [RC] F1 help
	"noinfighting",
	"normalinfighting",
	"totalinfighting",
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
	MITYPE_CLRBYTES,
	MITYPE_REDIRECT,
	MITYPE_SPECIALACTION,
	MITYPE_COMPATFLAG,
	MITYPE_F1, // [RC] F1 help
};

struct MapInfoHandler
{
	EMIType type;
	QWORD data1, data2;
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
	{ MITYPE_INT,		lioffset(sucktime), 0 },
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
	{ MITYPE_SETFLAG,	LEVEL_FALLDMG_ZD|LEVEL_FALLDMG_HX, 0 },
	{ MITYPE_SCFLAGS,	0, ~(LEVEL_FALLDMG_ZD|LEVEL_FALLDMG_HX) },
	{ MITYPE_SETFLAG,	LEVEL_NOALLIES, 0 },
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
	{ MITYPE_SETFLAG,	LEVEL_MISSILESACTIVATEIMPACT, 0 },
	{ MITYPE_CLRFLAG,	LEVEL_MISSILESACTIVATEIMPACT, 0 },
	{ MITYPE_SETFLAG,	LEVEL_NOINVENTORYBAR, 0 },
	{ MITYPE_SETFLAG,	LEVEL_DEATHSLIDESHOW, 0 },
	{ MITYPE_REDIRECT,	lioffset(RedirectMap), 0 },
	{ MITYPE_CLRFLAG,	LEVEL_LAXMONSTERACTIVATION, LEVEL_LAXACTIVATIONMAPINFO },
	{ MITYPE_SETFLAG,	LEVEL_LAXMONSTERACTIVATION, LEVEL_LAXACTIVATIONMAPINFO },
	{ MITYPE_COMPATFLAG, COMPATF_BOOMSCROLL},
	{ MITYPE_LUMPNAME,	lioffset(exitpic), 0 },
	{ MITYPE_LUMPNAME,	lioffset(exitpic), 0 },
	{ MITYPE_LUMPNAME,	lioffset(enterpic), 0 },
	{ MITYPE_MUSIC,		lioffset(intermusic), lioffset(intermusicorder) },
	{ MITYPE_INT,		lioffset(airsupply), 0 },
	{ MITYPE_SPECIALACTION, lioffset(specialactions), 0 },
	{ MITYPE_SETFLAG,	LEVEL_KEEPFULLINVENTORY, 0 },
	{ MITYPE_SETFLAG,	LEVEL_MONSTERFALLINGDAMAGE, 0 },
	{ MITYPE_CLRFLAG,	LEVEL_MONSTERFALLINGDAMAGE, 0 },
	{ MITYPE_LUMPNAME,	lioffset(sndseq), 0 },
	{ MITYPE_LUMPNAME,	lioffset(soundinfo), 0 },
	{ MITYPE_LUMPNAME,	lioffset(soundinfo), 0 },
	{ MITYPE_SETFLAG,	LEVEL_CLIPMIDTEX, 0 },
	{ MITYPE_SETFLAG,	LEVEL_WRAPMIDTEX, 0 },
	{ MITYPE_SCFLAGS,	LEVEL_CROUCH_YES, ~LEVEL_CROUCH_NO },
	{ MITYPE_SCFLAGS,	LEVEL_CROUCH_NO, ~LEVEL_CROUCH_YES },
	{ MITYPE_SCFLAGS,	LEVEL_PAUSE_MUSIC_IN_MENUS, 0 },
	{ MITYPE_COMPATFLAG, COMPATF_SHORTTEX},
	{ MITYPE_COMPATFLAG, COMPATF_STAIRINDEX},
	{ MITYPE_COMPATFLAG, COMPATF_LIMITPAIN},
	{ MITYPE_COMPATFLAG, COMPATF_NO_PASSMOBJ},
	{ MITYPE_COMPATFLAG, COMPATF_NOTOSSDROPS},
	{ MITYPE_COMPATFLAG, COMPATF_USEBLOCKING},
	{ MITYPE_COMPATFLAG, COMPATF_NODOORLIGHT},
	{ MITYPE_COMPATFLAG, COMPATF_RAVENSCROLL},
	{ MITYPE_COMPATFLAG, COMPATF_SOUNDTARGET},
	{ MITYPE_COMPATFLAG, COMPATF_DEHHEALTH},
	{ MITYPE_COMPATFLAG, COMPATF_TRACE},
	{ MITYPE_COMPATFLAG, COMPATF_DROPOFF},
	{ MITYPE_COMPATFLAG, COMPATF_BOOMSCROLL},
	{ MITYPE_COMPATFLAG, COMPATF_INVISIBILITY},
	{ MITYPE_LUMPNAME,	lioffset(bordertexture), 0 },
	{ MITYPE_F1,        lioffset(f1), 0, }, 
	{ MITYPE_SCFLAGS,	LEVEL_NOINFIGHTING, ~LEVEL_TOTALINFIGHTING },
	{ MITYPE_SCFLAGS,	0, ~(LEVEL_NOINFIGHTING|LEVEL_TOTALINFIGHTING)},
	{ MITYPE_SCFLAGS,	LEVEL_TOTALINFIGHTING, ~LEVEL_NOINFIGHTING },
};

static const char *MapInfoClusterLevel[] =
{
	"entertext",
	"exittext",
	"music",
	"flat",
	"pic",
	"hub",
	"cdtrack",
	"cdid",
	"entertextislump",
	"exittextislump",
	"name",
	NULL
};

MapInfoHandler ClusterHandlers[] =
{
	{ MITYPE_STRING,	cioffset(entertext), CLUSTER_LOOKUPENTERTEXT },
	{ MITYPE_STRING,	cioffset(exittext), CLUSTER_LOOKUPEXITTEXT },
	{ MITYPE_MUSIC,		cioffset(messagemusic), cioffset(musicorder) },
	{ MITYPE_LUMPNAME,	cioffset(finaleflat), 0 },
	{ MITYPE_LUMPNAME,	cioffset(finaleflat), CLUSTER_FINALEPIC },
	{ MITYPE_SETFLAG,	CLUSTER_HUB, 0 },
	{ MITYPE_INT,		cioffset(cdtrack), 0 },
	{ MITYPE_HEX,		cioffset(cdid), 0 },
	{ MITYPE_SETFLAG,	CLUSTER_ENTERTEXTINLUMP, 0 },
	{ MITYPE_SETFLAG,	CLUSTER_EXITTEXTINLUMP, 0 },
	{ MITYPE_STRING,	cioffset(clustername), 0 },
};

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_info_t *levelinfo,
							   cluster_info_t *clusterinfo,
							   QWORD levelflags);

static int FindWadLevelInfo (char *name)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
		if (!strnicmp (name, wadlevelinfos[i].mapname, 8))
			return i;
		
	return -1;
}

static int FindWadClusterInfo (int cluster)
{
	for (unsigned int i = 0; i < wadclusterinfos.Size(); i++)
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
	strcpy (levelinfo->skypic1, "-NOFLAT-");
	strcpy (levelinfo->skypic2, "-NOFLAT-");
	strcpy (levelinfo->bordertexture, gameinfo.borderFlat);
	if (gameinfo.gametype != GAME_Hexen)
	{
		// For maps without a BEHAVIOR, this will be cleared.
		levelinfo->flags |= LEVEL_LAXMONSTERACTIVATION;
	}
	else
	{
		levelinfo->flags |= LEVEL_MONSTERFALLINGDAMAGE;
	}
	levelinfo->airsupply = 10;
}

//
// G_ParseMapInfo
// Parses the MAPINFO lumps of all loaded WADs and generates
// data for wadlevelinfos and wadclusterinfos.
//
void G_ParseMapInfo ()
{
	int lump, lastlump = 0;

	atterm (G_UnloadMapInfo);

	// Parse the default MAPINFO for the current game.
	switch (gameinfo.gametype)
	{
	case GAME_Doom:
		switch (gamemission)
		{
		case doom:
			G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/doom1.txt"));
			break;
		case pack_plut:
			G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/plutonia.txt"));
			break;
		case pack_tnt:
			G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/tnt.txt"));
			break;
		default:
			G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/doom2.txt"));
			break;
		}
		break;

	case GAME_Heretic:
		G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/heretic.txt"));
		break;

	case GAME_Hexen:
		G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/hexen.txt"));
		break;

	case GAME_Strife:
		G_DoParseMapInfo (Wads.GetNumForFullName ("mapinfo/strife.txt"));
		break;

	default:
		break;
	}

	// Parse any extra MAPINFOs.
	while ((lump = Wads.FindLump ("MAPINFO", &lastlump)) != -1)
	{
		G_DoParseMapInfo (lump);
	}
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
	QWORD levelflags;

	SetLevelDefaults (&defaultinfo);
	SC_OpenLumpNum (lump, "MAPINFO");
	HexenHack = false;

	while (SC_GetString ())
	{
		switch (SC_MustMatchString (MapInfoTopLevel))
		{
		case MITL_DEFAULTMAP:
			if (defaultinfo.music != NULL) delete [] defaultinfo.music;
			if (defaultinfo.intermusic != NULL) delete [] defaultinfo.intermusic;
			SetLevelDefaults (&defaultinfo);
			ParseMapInfoLower (MapHandlers, MapInfoMapLevel, &defaultinfo, NULL, defaultinfo.flags);
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
				// no auto sound sequences, falling damage,
				// monsters activate their own specials, and missiles
				// are always the activators of impact lines.
				levelflags |= LEVEL_NOINTERMISSION
							| LEVEL_SNDSEQTOTALCTRL
							| LEVEL_FALLDMG_HX
							| LEVEL_ACTOWNSPECIAL
							| LEVEL_MISSILESACTIVATEIMPACT;
			}
			levelindex = FindWadLevelInfo (sc_String);
			if (levelindex == -1)
			{
				levelindex = wadlevelinfos.Reserve(1);
			}
			else
			{
				ClearLevelInfoStrings (&wadlevelinfos[levelindex]);
			}
			levelinfo = &wadlevelinfos[levelindex];
			memcpy (levelinfo, &defaultinfo, sizeof(*levelinfo));
			if (levelinfo->music != NULL)
			{
				levelinfo->music = copystring (levelinfo->music);
			}
			if (levelinfo->intermusic != NULL)
			{
				levelinfo->intermusic = copystring (levelinfo->intermusic);
			}
			if (HexenHack)
			{
				levelinfo->WallHorizLight = levelinfo->WallVertLight = 0;
			}
			uppercopy (levelinfo->mapname, sc_String);
			SC_MustGetString ();
			if (SC_Compare ("lookup"))
			{
				SC_MustGetString ();
				ReplaceString (&levelinfo->level_name, sc_String);
				levelflags |= LEVEL_LOOKUPLEVELNAME;
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
			// When the second sky is -NOFLAT-, make it a copy of the first sky
			if (strcmp (levelinfo->skypic2, "-NOFLAT-") == 0)
			{
				strcpy (levelinfo->skypic2, levelinfo->skypic1);
			}
			if (levelinfo->f1 != NULL)
			{
				levelinfo->f1 = copystring (levelinfo->f1);
			}
			SetLevelNum (levelinfo, levelinfo->levelnum);	// Wipe out matching levelnums from other maps.
			if (levelinfo->pname[0] != 0)
			{
				if (TexMan.AddPatch(levelinfo->pname) < 0)
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
				clusterindex = wadclusterinfos.Reserve(1);
				clusterinfo = &wadclusterinfos[clusterindex];
			}
			else
			{
				clusterinfo = &wadclusterinfos[clusterindex];
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
				if (clusterinfo->clustername != NULL)
				{
					delete[] clusterinfo->clustername;
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
			ClearEpisodes ();
			break;
		}
	}
	SC_Close ();
	if (defaultinfo.music != NULL)
	{
		delete [] defaultinfo.music;
	}
	if (defaultinfo.intermusic != NULL)
	{
		delete [] defaultinfo.intermusic;
	}
}

static void ClearLevelInfoStrings(level_info_t *linfo)
{
	if (linfo->music != NULL)
	{
		delete[] linfo->music;
		linfo->music = NULL;
	}
	if (linfo->intermusic != NULL)
	{
		delete[] linfo->intermusic;
		linfo->intermusic = NULL;
	}
	if (linfo->level_name != NULL)
	{
		delete[] linfo->level_name;
		linfo->level_name = NULL;
	}
	for (FSpecialAction *spac = linfo->specialactions; spac != NULL; )
	{
		FSpecialAction *next = spac->Next;
		delete spac;
		spac = next;
	}
}

static void ClearClusterInfoStrings(cluster_info_t *cinfo)
{
	if (cinfo->exittext != NULL)
	{
		delete[] cinfo->exittext;
		cinfo->exittext = NULL;
	}
	if (cinfo->entertext != NULL)
	{
		delete[] cinfo->entertext;
		cinfo->entertext = NULL;
	}
	if (cinfo->messagemusic != NULL)
	{
		delete[] cinfo->messagemusic;
		cinfo->messagemusic = NULL;
	}
	if (cinfo->clustername != NULL)
	{
		delete[] cinfo->clustername;
		cinfo->clustername = NULL;
	}
}

static void ClearEpisodes()
{
	for (int i = 0; i < EpiDef.numitems; ++i)
	{
		delete[] const_cast<char *>(EpisodeMenu[i].name);
		EpisodeMenu[i].name = NULL;
	}
	EpiDef.numitems = 0;
}

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_info_t *levelinfo,
							   cluster_info_t *clusterinfo,
							   QWORD flags)
{
	int entry;
	MapInfoHandler *handler;
	BYTE *info;

	info = levelinfo ? (BYTE *)levelinfo : (BYTE *)clusterinfo;

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

		case MITYPE_REDIRECT:
			SC_MustGetString ();
			levelinfo->RedirectType = sc_String;
			/*
			if (levelinfo->RedirectType == NULL ||
				!(levelinfo->RedirectType->IsDescendantOf (RUNTIME_CLASS(AInventory))))
			{
				SC_ScriptError ("%s is not an inventory item", sc_String);
			}
			*/
			// Intentional fall-through

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
				case 'S':	type = END_Strife;		break;
				default:	type = END_Pic3;		break;
				}
				newSeq.EndType = type;
				useseq = true;
			}
			else if (SC_Compare ("endpic"))
			{
				SC_MustGetString ();
				newSeq.EndType = END_Pic;
				strncpy (newSeq.PicName, sc_String, 8);
				newSeq.PicName[8] = 0;
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
			else if (SC_Compare ("endbuystrife"))
			{
				newSeq.EndType = END_BuyStrife;
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
			flags |= handler->data2;
			break;

		case MITYPE_SKY:
			SC_MustGetString ();	// get texture name;
			uppercopy ((char *)(info + handler->data1), sc_String);
			SC_MustGetFloat ();		// get scroll speed
			if (HexenHack)
			{
				sc_Float /= 256;
			}
			// Sky scroll speed is specified as pixels per tic, but we
			// want pixels per millisecond.
			*((float *)(info + handler->data2)) = sc_Float * 35 / 1000;
			break;

		case MITYPE_SETFLAG:
			flags |= handler->data1;
			flags |= handler->data2;
			break;

		case MITYPE_CLRFLAG:
			flags &= ~handler->data1;
			flags |= handler->data2;
			break;

		case MITYPE_SCFLAGS:
			flags = (flags & handler->data2) | handler->data1;
			break;

		case MITYPE_CLUSTER:
			SC_MustGetNumber ();
			*((int *)(info + handler->data1)) = sc_Number;
			// If this cluster hasn't been defined yet, add it. This is especially needed
			// for Hexen, because it doesn't have clusterdefs. If we don't do this, every
			// level on Hexen will sometimes be considered as being on the same hub,
			// depending on the check done.
			if (FindWadClusterInfo (sc_Number) == -1)
			{
				unsigned int clusterindex = wadclusterinfos.Reserve(1);
				clusterinfo = &wadclusterinfos[clusterindex];
				memset (clusterinfo, 0, sizeof(cluster_info_t));
				clusterinfo->cluster = sc_Number;
				if (gameinfo.gametype == GAME_Hexen)
				{
					clusterinfo->flags |= CLUSTER_HUB;
				}
			}
			break;

		case MITYPE_STRING:
			SC_MustGetString ();
			if (SC_Compare ("lookup"))
			{
				flags |= handler->data2;
				SC_MustGetString ();
			}
			ReplaceString ((char **)(info + handler->data1), sc_String);
			break;

		case MITYPE_F1:
			SC_MustGetString ();
			{
				char *colon = strchr (sc_String, ':');
				if (colon)
				{
					*colon = 0;
				}
				ReplaceString ((char **)(info + handler->data1), sc_String);
			}
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
				if (levelinfo != NULL)
				{
					// Flag the level so that the $MAP command doesn't override this.
					flags|=LEVEL_MUSICDEFINED;
				}
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

		case MITYPE_SPECIALACTION:
			{
				int FindLineSpecial(const char *str);

				FSpecialAction **so = (FSpecialAction**)(info + handler->data1);
				FSpecialAction *sa = new FSpecialAction;
				sa->Next = *so;
				*so = sa;
				SC_SetCMode(true);
				SC_MustGetString();
				sa->Type = FName(sc_String);
				SC_CheckString(",");
				SC_MustGetString();
				strlwr(sc_String);
				sa->Action = FindLineSpecial(sc_String);
				int j = 0;
				while (j < 5 && SC_CheckString(","))
				{
					SC_MustGetNumber();
					sa->Args[j++] = sc_Number;
				}
				SC_SetCMode(false);
			}
			break;

		case MITYPE_COMPATFLAG:
			if (!SC_CheckNumber()) sc_Number = 1;

			if (levelinfo != NULL)
			{
				if (sc_Number) levelinfo->compatflags |= (DWORD)handler->data1;
				else levelinfo->compatflags &= ~ (DWORD)handler->data1;
				levelinfo->compatmask |= (DWORD)handler->data1;
			}
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
// noskillmenu
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
	bool noskill = false;

	// Get map name
	SC_MustGetString ();
	uppercopy (map, sc_String);
	map[8] = 0;

	SC_MustGetString ();
	if (SC_Compare ("teaser"))
	{
		SC_MustGetString ();
		if (gameinfo.flags & GI_SHAREWARE)
		{
			uppercopy (map, sc_String);
		}
		SC_MustGetString ();
	}
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
		else if (SC_Compare("noskillmenu"))
		{
			noskill = true;
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
				memmove (&EpisodeNoSkill[i], &EpisodeNoSkill[i+1], 
					sizeof(EpisodeNoSkill[0])*(EpiDef.numitems - i - 1));
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
		EpisodeNoSkill[i] = noskill;
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
	unsigned int i, num;

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
	if (!strncmp(nextmap, "enDSeQ",6)) return;	// If there is already an end sequence please leave it alone!!!

	if (gameinfo.gametype == GAME_Strife)
	{
		SetEndSequence (nextmap, gameinfo.flags & GI_SHAREWARE ? END_BuyStrife : END_Strife);
	}
	else if (gameinfo.gametype == GAME_Hexen)
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

void G_UnloadMapInfo ()
{
	unsigned int i;

	G_ClearSnapshots ();

	for (i = 0; i < wadlevelinfos.Size(); ++i)
	{
		ClearLevelInfoStrings (&wadlevelinfos[i]);
	}
	wadlevelinfos.Clear();

	for (i = 0; i < wadclusterinfos.Size(); ++i)
	{
		ClearClusterInfoStrings (&wadclusterinfos[i]);
	}
	wadclusterinfos.Clear();

	ClearEpisodes();
}

level_info_t *FindLevelByWarpTrans (int num)
{
	for (unsigned i = wadlevelinfos.Size(); i-- != 0; )
		if (wadlevelinfos[i].WarpTrans == num)
			return &wadlevelinfos[i];

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
	// Remove any existing defereds
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
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
static char d_mapname[256];
static int d_skill=-1;

void G_DeferedInitNew (const char *mapname, int newskill)
{
	strncpy (d_mapname, mapname, 8);
	d_skill = newskill;
	CheckWarpTransMap (d_mapname, true);
	gameaction = ga_newgame2;
}

CCMD (map)
{
	if (netgame)
	{
		Printf ("Use "TEXTCOLOR_BOLD"changemap"TEXTCOLOR_NORMAL" instead. "TEXTCOLOR_BOLD"Map"
				TEXTCOLOR_NORMAL" is for single-player only.\n");
		return;
	}
	if (argv.argc() > 1)
	{
		MapData * map = P_OpenMapData(argv[1]);
		if (map == NULL)
			Printf ("No map %s\n", argv[1]);
		else
		{
			delete map;
			G_DeferedInitNew (argv[1]);
		}
	}
	else
	{
		Printf ("Usage: map <map name>\n");
	}
}

CCMD (open)
{
	if (netgame)
	{
		Printf ("You cannot use open in multiplayer games.\n");
		return;
	}
	if (argv.argc() > 1)
	{
		sprintf(d_mapname, "file:%s", argv[1]);
		MapData * map = P_OpenMapData(d_mapname);
		if (map == NULL)
			Printf ("No map %s\n", d_mapname);
		else
		{
			delete map;
			gameaction = ga_newgame2;
			d_skill = -1;
		}
	}
	else
	{
		Printf ("Usage: open <map file>\n");
	}
}


void G_NewInit ()
{
	int i;

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
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		player_t *p = &players[i];
		userinfo_t saved_ui = players[i].userinfo;
		int chasecam = p->cheats & CF_CHASECAM;
		p->~player_t();
		::new(p) player_t;
		players[i].cheats |= chasecam;
		players[i].playerstate = PST_DEAD;
		playeringame[i] = 0;
		players[i].userinfo = saved_ui;
	}
	BackupSaveName = "";
	consoleplayer = 0;
	NextSkill = -1;
}

void G_DoNewGame (void)
{
	G_NewInit ();
	playeringame[consoleplayer] = 1;
	if (d_skill != -1) gameskill = d_skill;
	G_InitNew (d_mapname, false);
	gameaction = ga_nothing;
}

void G_InitNew (const char *mapname, bool bTitleLevel)
{
	EGameSpeed oldSpeed;
	bool wantFast;
	int i;

	if (!savegamerestore)
	{
		G_ClearSnapshots ();
		P_RemoveDefereds ();

		// [RH] Mark all levels as not visited
		for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
			wadlevelinfos[i].flags = wadlevelinfos[i].flags & ~LEVEL_VISITED;
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

	if (StatusBar != NULL)
	{
		delete StatusBar;
	}
	if (bTitleLevel)
	{
		StatusBar = new FBaseStatusBar (0);
	}
	else if (gameinfo.gametype == GAME_Doom)
	{
		StatusBar = CreateDoomStatusBar ();
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		StatusBar = CreateHereticStatusBar ();
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		StatusBar = CreateHexenStatusBar ();
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		StatusBar = CreateStrifeStatusBar ();
	}
	else
	{
		StatusBar = new FBaseStatusBar (0);
	}
	StatusBar->AttachToPlayer (&players[consoleplayer]);
	StatusBar->NewGame ();
	setsizeneeded = true;

	// Set the initial quest log text for Strife.
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		players[i].SetLogText ("Find help");
	}

	// [RH] If this map doesn't exist, bomb out
	MapData * map = P_OpenMapData(mapname);
	if (!map)
	{
		I_Error ("Could not find map %s\n", mapname);
	}
	delete map;

	if (dmflags & DF_MONSTERS_RESPAWN)
	{
		respawnmonsters = TICRATE;
	}
	else if (gameinfo.gametype & (GAME_Doom|GAME_Strife) && gameskill == sk_nightmare)
	{
		respawnmonsters = TICRATE;
	}
	else
	{
		respawnmonsters = 0;
	}
	// Monsters wait longer before respawning in Strife.
	respawnmonsters *= gameinfo.gametype != GAME_Strife ? 12 : 16;

	oldSpeed = GameSpeed;
	wantFast = (dmflags & DF_FAST_MONSTERS) || (gameskill == sk_nightmare);
	GameSpeed = wantFast ? SPEED_Fast : SPEED_Normal;

	if (oldSpeed != GameSpeed)
	{
		FActorInfo::StaticSpeedSet ();
	}

	if (!savegamerestore)
	{
		if (!netgame)
		{ // [RH] Change the random seed for each new single player game
			rngseed = rngseed*3/2;
		}
		FRandom::StaticClearRandom ();
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
		level.maptime = 0;
		level.totaltime = 0;

		if (!multiplayer || !deathmatch)
		{
			InitPlayerClasses ();
		}

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_ENTER;	// [BC]
	}

	usergame = !bTitleLevel;		// will be set false if a demo
	paused = 0;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	BorderNeedRefresh = screen->GetPageCount ();

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
bool 			secretexit;
static int		startpos;	// [RH] Support for multiple starts per level
extern int		NoWipe;		// [RH] Don't wipe when travelling in hubs
static bool		startkeepfacing;	// [RH] Support for keeping your facing angle
static bool		resetinventory;	// Reset the inventory to the player's default for the next level

// [RH] The position parameter to these next three functions should
//		match the first parameter of the single player start spots
//		that should appear in the next map.

static void goOn (int position, bool keepFacing, bool secret, bool resetinv)
{
	static bool unloading;

	if (unloading)
	{
		Printf (TEXTCOLOR_RED "Unloading scripts cannot exit the level again.\n");
		return;
	}

	cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
	cluster_info_t *nextcluster = FindClusterInfo (CheckLevelRedirect (FindLevelInfo (level.nextmap))->cluster);

	startpos = position;
	startkeepfacing = keepFacing;
	gameaction = ga_completed;
	secretexit = secret;
	resetinventory = resetinv;

	bglobal.End();	//Added by MC:

	// [RH] Give scripts a chance to do something
	unloading = true;
	FBehavior::StaticStartTypedScripts (SCRIPT_Unloading, NULL, false, 0, true);
	unloading = false;

	if (thiscluster && (thiscluster->flags & CLUSTER_HUB))
	{
		if ((level.flags & LEVEL_NOINTERMISSION) || (nextcluster == thiscluster))
			NoWipe = 35;
		D_DrawIcon = "TELEICON";
	}

	// un-crouch all players here
	for(int i=0;i<MAXPLAYERS;i++)
	{
		players[i].Uncrouch();
	}
}

void G_ExitLevel (int position, bool keepFacing)
{
	goOn (position, keepFacing, false, false);
}

// Here's for the german edition.
void G_SecretExitLevel (int position) 
{
	// [RH] Check for secret levels is done in 
	//		G_DoCompleted()

	goOn (position, false, true, false);
}

void G_ChangeLevel(const char * levelname, int position, bool keepFacing, int nextSkill, 
				   bool nointermission, bool resetinventory, bool nomonsters)
{
	strncpy (level.nextmap, levelname, 8);
	level.nextmap[8] = 0;

	if (nextSkill != -1) NextSkill = nextSkill;

	if (!nomonsters) dmflags = dmflags & ~DF_NO_MONSTERS;
	else dmflags = dmflags | DF_NO_MONSTERS;

	if (nointermission) level.flags |= LEVEL_NOINTERMISSION;

	goOn(position, keepFacing, false, resetinventory);
}

void G_DoCompleted (void)
{
	int i; 

	gameaction = ga_nothing;

	if (gamestate == GS_TITLELEVEL)
	{
		strncpy (level.mapname, level.nextmap, 8);
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
			MapData * map = P_OpenMapData(level.secretmap);
			if (map != NULL)
			{
				delete map;
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
			if (strncmp (level.nextmap, "enDSeQ", 6) == 0)
			{
				strncpy (wminfo.next, level.nextmap, 8);
				wminfo.lname1[0] = 0;
			}
			else
			{
				level_info_t *nextinfo = CheckLevelRedirect (FindLevelInfo (level.nextmap));
				strncpy (wminfo.next, nextinfo->mapname, 8);
				strncpy (wminfo.lname1, nextinfo->pname, 8);
			}
		}
	}

	CheckWarpTransMap (wminfo.next, true);

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
	cluster_info_t *nextcluster = FindClusterInfo (CheckLevelRedirect (FindLevelInfo (level.nextmap))->cluster);
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
			G_PlayerFinishLevel (i, mode, resetinventory);
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
		// With hub statistics the time should be per hub.
		// Additionally there is a global time counter now so nothing is missed by changing it
		//else if (mode == FINISH_NoHub)
		{ // Reset time to zero if not entering/staying in a hub.
			level.time = 0;
		}
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
			"\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n"
			TEXTCOLOR_BOLD "%s - %s\n\n",
			level.mapname, level.level_name);

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
	// [RH] Fetch sky parameters from level_locals_t.
	sky1texture = TexMan.GetTexture (level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
	sky2texture = TexMan.GetTexture (level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);

	// [RH] Set up details about sky rendering
	R_InitSkyMap ();

	for (i = 0; i < MAXPLAYERS; i++)
	{ 
		if (playeringame[i] && (deathmatch || players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_ENTER;	// [BC]
		memset (players[i].frags,0,sizeof(players[i].frags)); 
		players[i].fragcount = 0;
	}

	P_SetupLevel (level.mapname, position);

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
	level.maptime = 0;
	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	G_FinishTravel ();
	if (players[consoleplayer].camera == NULL ||
		players[consoleplayer].camera->player != NULL)
	{ // If we are viewing through a player, make sure it is us.
        players[consoleplayer].camera = players[consoleplayer].mo;
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
			thiscluster->flags & CLUSTER_FINALEPIC,
			thiscluster->flags & CLUSTER_LOOKUPEXITTEXT);
	}
	else
	{
		nextcluster = FindClusterInfo (CheckLevelRedirect (FindLevelInfo (nextmap))->cluster);

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
					nextcluster->flags & CLUSTER_FINALEPIC,
					nextcluster->flags & CLUSTER_LOOKUPENTERTEXT);
			}
			else if (thiscluster->exittext)
			{
				F_StartFinale (thiscluster->messagemusic, thiscluster->musicorder,
					thiscluster->cdtrack, nextcluster->cdid,
					thiscluster->finaleflat, thiscluster->exittext,
					thiscluster->flags & CLUSTER_EXITTEXTINLUMP,
					thiscluster->flags & CLUSTER_FINALEPIC,
					thiscluster->flags & CLUSTER_LOOKUPEXITTEXT);
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

			// Only living players travel. Dead ones get a new body on the new level.
			if (players[i].health > 0)
			{
				pawn->UnlinkFromWorld ();
				P_DelSector_List ();
				pawn->RemoveFromHash ();
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
			P_SpawnPlayer (&playerstarts[pawn->player - players], true);

			pawndup = pawn->player->mo;
			if (!startkeepfacing)
			{
				pawn->angle = pawndup->angle;
				pawn->pitch = pawndup->pitch;
			}
			pawn->x = pawndup->x;
			pawn->y = pawndup->y;
			pawn->z = pawndup->z;
			pawn->momx = pawndup->momx;
			pawn->momy = pawndup->momy;
			pawn->momz = pawndup->momz;
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
			DObject::PointerSubstitution (oldpawn, pawn);
			oldpawn->Destroy();
			pawndup->Destroy ();
			pawn->LinkToWorld ();
			pawn->AddToHash ();
			pawn->SetState(pawn->SpawnState);

			for (inv = pawn->Inventory; inv != NULL; inv = inv->Inventory)
			{
				inv->ChangeStatNum (STAT_INVENTORY);
				inv->LinkToWorld ();
				inv->Travelled ();
			}
		}
	}
}
 
void G_InitLevelLocals ()
{
	level_info_t *info;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any
	NormalLight.Maps = realcolormaps;

	// [BB] Instead of just setting the color, we also have reset Desaturate and build the lights.
	NormalLight.ChangeColor (PalEntry (255, 255, 255), 0);

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
	level.airsupply = info->airsupply*TICRATE;
	level.outsidefog = info->outsidefog;
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
		level.sucktime = info->sucktime;
		level.cluster = info->cluster;
		level.clusterflags = clus ? clus->flags : 0;
		level.flags |= info->flags;
		level.levelnum = info->levelnum;
		level.music = info->music;
		level.musicorder = info->musicorder;
		level.f1 = info->f1; // [RC] And import the f1 name

		strncpy (level.level_name, info->level_name, 63);
		G_MaybeLookupLevelName (NULL);
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
		level.sucktime = 0;
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
	if (level.flags & LEVEL_CROUCH_YES)
		clear |= DF_NO_CROUCH;
	if (level.flags & LEVEL_CROUCH_NO)
		set |= DF_NO_CROUCH;
	if (level.flags & LEVEL_FREELOOK_YES)
		clear |= DF_NO_FREELOOK;
	if (level.flags & LEVEL_FREELOOK_NO)
		set |= DF_NO_FREELOOK;

	dmflags = (dmflags & ~clear) | set;

	compatflags.Callback();

	NormalLight.ChangeFade (level.fadeto);
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
		return &wadlevelinfos[i];
	else
		return &TheDefaultLevelInfo;
}

level_info_t *FindLevelByNum (int num)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
		if (wadlevelinfos[i].levelnum == num)
			return &wadlevelinfos[i];

	return NULL;
}

level_info_t *CheckLevelRedirect (level_info_t *info)
{
	if (info->RedirectType != NAME_None)
	{
		const PClass *type = PClass::FindClass(info->RedirectType);
		if (type != NULL)
		{
			for (int i = 0; i < MAXPLAYERS; ++i)
			{
				if (playeringame[i] && players[i].mo->FindInventory (type))
				{
					level_info_t *newinfo = FindLevelInfo (info->RedirectMap);
					if (newinfo != NULL)
					{
						info = newinfo;
					}
					break;
				}
			}
		}
	}
	return info;
}

static void SetLevelNum (level_info_t *info, int num)
{
	// Avoid duplicate levelnums. The level being set always has precedence.
	for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
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
		return &wadclusterinfos[i];
	else
		return &TheDefaultClusterInfo;
}

const char *G_MaybeLookupLevelName (level_info_t *ininfo)
{
	level_info_t *info;

	if (ininfo == NULL)
	{
		info = level.info;
	}
	else
	{
		info = ininfo;
	}

	if (info != NULL && info->flags & LEVEL_LOOKUPLEVELNAME)
	{
		const char *thename;
		const char *lookedup;

		lookedup = GStrings[info->level_name];
		if (lookedup == NULL)
		{
			thename = info->level_name;
		}
		else
		{
			char checkstring[32];

			// Strip out the header from the localized string
			if (info->mapname[0] == 'E' && info->mapname[2] == 'M')
			{
				sprintf (checkstring, "%s: ", info->mapname);
			}
			else if (info->mapname[0] == 'M' && info->mapname[1] == 'A' && info->mapname[2] == 'P')
			{
				sprintf (checkstring, "%d: ", atoi(info->mapname + 3));
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
		if (ininfo == NULL)
		{
			strncpy (level.level_name, thename, 63);
		}
		return thename;
	}
	return info != NULL ? info->level_name : NULL;
}

void G_MakeEpisodes ()
{
	int i;

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
			"MNU_COTD",
			"MNU_HELLSMAW",
			"MNU_DOME",
			"MNU_OSSUARY",
			"MNU_DEMESNE",
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
	int i = level.totaltime;

	arc << level.flags
		<< level.fadeto
		<< level.found_secrets
		<< level.found_items
		<< level.killed_monsters
		<< level.gravity
		<< level.aircontrol
		<< level.maptime
		<< i;

	// Hub transitions must keep the current total time
	if (!hubLoad)
		level.totaltime=i;

	if (arc.IsStoring ())
	{
		arc.WriteName (level.skypic1);
		arc.WriteName (level.skypic2);
	}
	else
	{
		strncpy (level.skypic1, arc.ReadName(), 8);
		strncpy (level.skypic2, arc.ReadName(), 8);
		sky1texture = TexMan.GetTexture (level.skypic1, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
		sky2texture = TexMan.GetTexture (level.skypic2, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
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

	// This must be saved, too, of course!
	FCanvasTextureInfo::Serialize (arc);

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

	if (level.info->mapname[0] != 0 || level.info == &TheDefaultLevelInfo)
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

	if (level.info->mapname[0] != 0 || level.info == &TheDefaultLevelInfo)
	{
		SaveVersion = level.info->snapshotVer;
		level.info->snapshot->Reopen ();
		FArchive arc (*level.info->snapshot);
		if (hubLoad)
			arc.SetHubTravel ();
		G_SerializeLevel (arc, hubLoad);
		arc.Close ();

		TThinkerIterator<APlayerPawn> it;
		APlayerPawn *pawn, *next;

		next = it.Next();
		while ((pawn = next) != 0)
		{
			next = it.Next();
			if (pawn->player == NULL || pawn->player->mo == NULL || !playeringame[pawn->player - players])
			{
				pawn->Destroy ();
			}
		}
	}
	// No reason to keep the snapshot around once the level's been entered.
	delete level.info->snapshot;
	level.info->snapshot = NULL;
}

void G_ClearSnapshots (void)
{
	for (unsigned int i = 0; i < wadlevelinfos.Size(); i++)
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


static void writeDefereds (FArchive &arc, level_info_t *i)
{
	writeMapName (arc, i->mapname);
	arc << i->defered;
}

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
				SinglePlayerClass[i] = (pr_classchoice()) % PlayerClasses.Size ();
			}
			players[i].cls = NULL;
			players[i].CurrentPlayerClass = SinglePlayerClass[i];
		}
	}
}
