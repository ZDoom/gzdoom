/*
** g_level.cpp
** Parses MAPINFO and controls movement between levels
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
#include "z_zone.h"
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

#include "gi.h"


EXTERN_CVAR (Float, sv_gravity)
EXTERN_CVAR (Float, sv_aircontrol)
EXTERN_CVAR (Int, disableautosave)

#define lioffset(x)		myoffsetof(level_pwad_info_t,x)
#define cioffset(x)		myoffsetof(cluster_info_t,x)

static level_info_t *FindDefLevelInfo (char *mapname);
static cluster_info_t *FindDefClusterInfo (int cluster);
static int FindEndSequence (int type, const char *picname);
static void SetEndSequence (char *nextmap, int type);

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
int ACS_WorldVars[NUM_WORLDVARS];

// ACS variables with global scope
int ACS_GlobalVars[NUM_GLOBALVARS];


extern BOOL netdemo;
extern char BackupSaveName[PATH_MAX];

BOOL savegamerestore;

extern int mousex, mousey, joyxmove, joyymove;
extern bool sendpause, sendsave, sendcenterview, sendturn180, SendLand;
extern BYTE SendWeaponSlot, SendWeaponChoice;
extern int SendItemSelect;
extern artitype_t SendItemUse;
extern artitype_t LocalSelectedItem;

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
	NULL
};

enum EMIType
{
	MITYPE_EATNEXT,
	MITYPE_INT,
	MITYPE_FLOAT,
	MITYPE_HEX,
	MITYPE_COLOR,
	MITYPE_MAPNAME,
	MITYPE_LUMPNAME,
	MITYPE_SKY,
	MITYPE_SETFLAG,
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
	{ MITYPE_EATNEXT,	0, 0 },
	{ MITYPE_RELLIGHT,	lioffset(WallVertLight), 0 },
	{ MITYPE_RELLIGHT,	lioffset(WallHorizLight), 0 },
	{ MITYPE_FLOAT,		lioffset(gravity), 0 },
	{ MITYPE_FLOAT,		lioffset(aircontrol), 0 },
	{ MITYPE_SETFLAG,	LEVEL_FILTERSTARTS, 0 },
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
	{ MITYPE_HEX,		cioffset(cdid), 0 }
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
				{	// MAPNAME is a number; assume a Hexen wad
					int map = atoi (sc_String);
					sprintf (sc_String, "MAP%02d", map);
					SKYFLATNAME[5] = 0;
					HexenHack = true;
					// Hexen levels are automatically nointermission,
					// no auto sound sequences, and falling damage.
					levelflags |= LEVEL_NOINTERMISSION
								| LEVEL_SNDSEQTOTALCTRL
								| LEVEL_FALLDMG_HX;
				}
				levelindex = FindWadLevelInfo (sc_String);
				if (levelindex == -1)
				{
					levelindex = numwadlevelinfos++;
					wadlevelinfos = (level_pwad_info_t *)Realloc (wadlevelinfos, sizeof(level_pwad_info_t)*numwadlevelinfos);
				}
				levelinfo = wadlevelinfos + levelindex;
				memcpy (levelinfo, &defaultinfo, sizeof(*levelinfo));
				if (HexenHack)
				{
					levelinfo->WallHorizLight = levelinfo->WallVertLight = 0;
				}
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
	EndSequences.ShrinkToFit ();
}

static void ParseMapInfoLower (MapInfoHandler *handlers,
							   const char *strings[],
							   level_pwad_info_t *levelinfo,
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
				sprintf (sc_String, "MAP%02d", map);
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
					seqnum = EndSequences.Push (newSeq);
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

static int FindEndSequence (int type, const char *picname)
{
	int i, num;

	num = EndSequences.Size ();
	for (i = 0; i < num; i++)
	{
		if (EndSequences[i].EndType == type &&
			(type != END_Pic || stricmp (EndSequences[i].PicName, picname) == 0))
		{
			return i;
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
		seqnum = EndSequences.Push (newseq);
	}
	strcpy (nextmap, "enDSeQ");
	*((WORD *)(nextmap + 6)) = (WORD)seqnum;
}

void G_SetForEndGame (char *nextmap)
{
	if (gamemode == commercial)
	{
		SetEndSequence (nextmap, END_Cast);
	}
	else
	{ // The ExMx games actually have different ends based on the episode,
	  // but I want to keep this simple.
		SetEndSequence (nextmap, END_Pic1);
	}
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
	for (i = 0; LevelInfos[i].level_name; i++)
	{
		if (LevelInfos[i].defered)
		{
			zapDefereds (LevelInfos[i].defered);
			LevelInfos[i].defered = NULL;
		}
	}
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
	gameaction = ga_newgame2;
}

CCMD (map)
{
	if (argv.argc() > 1)
	{
		if (W_CheckNumForName (argv[1]) == -1)
			Printf ("No map %s\n", argv[1]);
		else
			G_DeferedInitNew (argv[1]);
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
	if (W_CheckNumForName (mapname) == -1)
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
		M_ClearRandom ();
		memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
		memset (ACS_GlobalVars, 0, sizeof(ACS_GlobalVars));
		level.time = 0;

		// force players to be initialized upon first level load
		for (i = 0; i < MAXPLAYERS; i++)
			players[i].playerstate = PST_ENTER;	// [BC]
	}

	usergame = true;				// will be set false if a demo
	paused = 0;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	BorderNeedRefresh = screen->GetPageCount ();

	strncpy (level.mapname, mapname, 8);
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
			if (W_CheckNumForName (level.secretmap) != -1)
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
			{ // Reset world variables and deferred scripts for the new hub.
				memset (ACS_WorldVars, 0, sizeof(ACS_WorldVars));
				P_RemoveDefereds ();
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
	
	if (demoplayback || oldgs == GS_STARTUP)
		C_HideConsole ();

	// Set the sky map.
	// First thing, we have a dummy sky texture name,
	//	a flat. The data is in the WAD only because
	//	we look for an actual index, instead of simply
	//	setting one.
	skyflatnum = R_FlatNumForName (SKYFLATNAME);

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
			players[i].playerstate = PST_ENTER;	// [BC]
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
		{
			actor->touching_sectorlist = NULL;
		}
	}

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
	Z_CheckHeap ();

	// clear cmd building stuff
	ResetButtonStates ();

	SendWeaponSlot = SendWeaponChoice = 255;
	SendItemSelect = 0;
	SendItemUse = arti_none;
	joyxmove = joyymove = 0;
	mousex = mousey = 0; 
	sendpause = sendsave = sendcenterview = sendturn180 = SendLand = false;
	paused = 0;

	//Added by MC: Initialize bots.
	bglobal.Init ();

	if (timingdemo)
	{
		static BOOL firstTime = true;

		if (firstTime)
		{
			starttime = I_GetTimePolled ();
			firstTime = false;
		}
	}

	level.starttime = I_GetTime ();
	G_UnSnapshotLevel (!savegamerestore);	// [RH] Restore the state of the level.
	P_DoDeferedScripts ();	// [RH] Do script actions that were triggered on another map.

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
			thiscluster->finaleflat, thiscluster->exittext);
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
					nextcluster->finaleflat, nextcluster->entertext);
			}
			else if (thiscluster->exittext)
			{
				F_StartFinale (thiscluster->messagemusic, thiscluster->musicorder,
					thiscluster->cdtrack, nextcluster->cdid,
					thiscluster->finaleflat, thiscluster->exittext);
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
	int i;

	BaseBlendA = 0.0f;		// Remove underwater blend effect, if any
	NormalLight.Maps = realcolormaps;

	level.gravity = sv_gravity;
	level.aircontrol = (fixed_t)(sv_aircontrol * 65536.f);

	if ((i = FindWadLevelInfo (level.mapname)) > -1)
	{
		level_pwad_info_t *pinfo = wadlevelinfos + i;

		level.info = (level_info_t *)pinfo;
		level.skyspeed1 = pinfo->skyspeed1;
		level.skyspeed2 = pinfo->skyspeed2;
		info = (level_info_t *)pinfo;
		strncpy (level.skypic2, pinfo->skypic2, 8);
		level.fadeto = pinfo->fadeto;
		level.cdtrack = pinfo->cdtrack;
		level.cdid = pinfo->cdid;
		if (level.fadeto == 0)
		{
			R_SetDefaultColormap (pinfo->fadetable);
			/*
		}
		else
		{
			NormalLight.ChangeFade (level.fadeto);
			*/
		}
		level.outsidefog = pinfo->outsidefog;
		level.flags |= LEVEL_DEFINEDINMAPINFO;
		level.WallVertLight = pinfo->WallVertLight;
		level.WallHorizLight = pinfo->WallHorizLight;
		if (pinfo->gravity != 0.f)
		{
			level.gravity = pinfo->gravity;
		}
		if (pinfo->aircontrol != 0.f)
		{
			level.aircontrol = (fixed_t)(pinfo->aircontrol * 65536.f);
		}
	}
	else
	{
		info = FindDefLevelInfo (level.mapname);
		level.info = info;
		level.skyspeed1 =  level.skyspeed2 = 0;
		level.fadeto = 0;
		level.outsidefog = 0xff000000;	// 0xff000000 signals not to handle it special
		level.skypic2[0] = 0;
		level.cdtrack = 0;
		level.cdid = 0;
		level.WallVertLight = +8;
		level.WallHorizLight = -8;
		R_SetDefaultColormap ("COLORMAP");
	}

	G_AirControlChanged ();

	if (info->level_name)
	{
		level.partime = info->partime;
		level.cluster = info->cluster;
		level.flags = info->flags;
		level.levelnum = info->levelnum;
		level.music = info->music;
		level.musicorder = info->musicorder;

		strncpy (level.level_name, info->level_name, 63);
		strncpy (level.nextmap, info->nextmap, 8);
		strncpy (level.secretmap, info->secretmap, 8);
		strncpy (level.skypic1, info->skypic1, 8);
		if (!level.skypic2[0])
			strncpy (level.skypic2, level.skypic1, 8);
	}
	else
	{
		level.partime = level.cluster = 0;
		strcpy (level.level_name, "Unnamed");
		level.nextmap[0] =
			level.secretmap[0] = 0;
		level.music = NULL;
		strncpy (level.skypic1, "SKY1", 8);
		strncpy (level.skypic2, "SKY1", 8);
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

	memset (level.vars, 0, sizeof(level.vars));

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

static level_info_t *FindDefLevelInfo (char *mapname)
{
	level_info_t *i;

	i = LevelInfos;
	while (i->level_name)
	{
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
		while (i->level_name)
		{
			if (i->levelnum == num && W_CheckNumForName (i->mapname) != -1)
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
	const char *namepart;
	int i, start;

	temp[0] = '0';
	temp[1] = ':';
	temp[2] = 0;
	for (i = HUSTR_E1M1; i <= HUSTR_E4M9; ++i)
	{
		if (temp[0] < '9')
			temp[0]++;
		else
			temp[0] = '1';

		if ( (namepart = strstr (GStrings(i), temp)) )
		{
			namepart += 2;
			while (*namepart && *namepart <= ' ')
				namepart++;
		}
		else
		{
			namepart = GStrings(i);
		}

		if (gameinfo.gametype != GAME_Heretic)
		{
			ReplaceString (&LevelInfos[i-HUSTR_E1M1].level_name, namepart);
			ReplaceString (&LevelInfos[i-HUSTR_E1M1].music, Musics1[i-HUSTR_E1M1]);
		}
	}

	if (gameinfo.gametype == GAME_Heretic)
	{
		for (i = 0; i < 4*9; i++)
		{
			ReplaceString (&LevelInfos[i].level_name, GStrings(HHUSTR_E1M1+i));
			ReplaceString (&LevelInfos[i].music, Musics2[i]);
			LevelInfos[i].cluster = 11 + (i / 9);
			LevelInfos[i].pname[0] = 0;
		}
		for (i = 0; i < 9; i++)
		{
			strcpy (LevelInfos[i+3*9].skypic1, "SKY1");
		}
		LevelInfos[7].flags = LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPECLOWERFLOOR|LEVEL_HEADSPECIAL;
		LevelInfos[16].flags = LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPECLOWERFLOOR|LEVEL_MINOTAURSPECIAL;
		LevelInfos[25].flags = LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPECLOWERFLOOR|LEVEL_SORCERER2SPECIAL;
		LevelInfos[34].flags = LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPECLOWERFLOOR|LEVEL_HEADSPECIAL;

		SetEndSequence (LevelInfos[16].nextmap, END_Underwater);
		SetEndSequence (LevelInfos[25].nextmap, END_Demon);

		LevelInfos[8].nextmap[3] = LevelInfos[8].secretmap[3] = '7';
		LevelInfos[17].nextmap[3] = LevelInfos[17].secretmap[3] = '5';
		LevelInfos[26].nextmap[3] = LevelInfos[26].secretmap[3] = '5';
		LevelInfos[35].nextmap[3] = LevelInfos[35].secretmap[3] = '5';
	}
	else if (gameinfo.gametype == GAME_Doom)
	{
		SetEndSequence (LevelInfos[16].nextmap, END_Pic2);
		SetEndSequence (LevelInfos[25].nextmap, END_Bunny);
	}

	SetEndSequence (LevelInfos[7].nextmap, END_Pic1);
	SetEndSequence (LevelInfos[34].nextmap, END_Pic3);
	SetEndSequence (LevelInfos[43].nextmap, END_Pic3);
	SetEndSequence (LevelInfos[77].nextmap, END_Cast);

	for (i = 0; i < 12; i++)
	{
		if (i < 9)
			ReplaceString (&LevelInfos[i+36].level_name, GStrings(HHUSTR_E5M1+i));
		ReplaceString (&LevelInfos[i+36].music, Musics1[i+36]);
	}

	for (i = 0; i < 4; i++)
		ReplaceString (&ClusterInfos[i].exittext, GStrings(E1TEXT+i));

	if (gamemission == pack_plut)
		start = PHUSTR_1;
	else if (gamemission == pack_tnt)
		start = THUSTR_1;
	else
		start = HUSTR_1;

	for (i = 0; i < 32; i++)
	{
		sprintf (temp, "%d:", i + 1);
		if ( (namepart = strstr (GStrings(i+start), temp)) )
		{
			namepart += strlen (temp);
			while (*namepart && *namepart <= ' ')
				namepart++;
		}
		else
		{
			namepart = GStrings(i+start);
		}
		ReplaceString (&LevelInfos[48+i].level_name, namepart);
		ReplaceString (&LevelInfos[48+i].music, Musics3[i]);
	}

	if (gamemission == pack_plut)
		start = P1TEXT;		// P1TEXT
	else if (gamemission == pack_tnt)
		start = T1TEXT;		// T1TEXT
	else
		start = C1TEXT;		// C1TEXT

	for (i = 0; i < 4; i++)
		ReplaceString (&ClusterInfos[4 + i].exittext, GStrings(start+i));
	for (; i < 6; i++)
		ReplaceString (&ClusterInfos[4 + i].entertext, GStrings(start+i));
	for (i = HE1TEXT; i <= HE5TEXT; i++)
		ReplaceString (&ClusterInfos[10 + i - HE1TEXT].exittext, GStrings(i));
	for (i = 0; i < 15; i++)
		ReplaceString (&ClusterInfos[i].messagemusic, Musics4[i]);

	if (level.info)
		strncpy (level.level_name, level.info->level_name, 63);
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

	for (i = 0; i < NUM_MAPVARS; i++)
	{
		arc << level.vars[i];
	}

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

	P_SerializeThinkers (arc, hubLoad);
	P_SerializeWorld (arc);
	P_SerializePolyobjs (arc);
	P_SerializeSounds (arc);
	StatusBar->Serialize (arc);

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

	level.info->snapshot = new FCompressedMemFile;
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
		BYTE zero = 0;
		arc << zero;

		// Write out which levels have been visited
		for (i = 0; i < numwadlevelinfos; ++i)
			if (wadlevelinfos[i].flags & LEVEL_VISITED)
				arc.Write (wadlevelinfos[i].mapname, 8);

		for (i = 0; LevelInfos[i].mapname[0]; ++i)
			if (LevelInfos[i].flags & LEVEL_VISITED)
				arc.Write (LevelInfos[i].mapname, 8);

		arc << zero;
	}
	else
	{
		char mapname[8];

		G_ClearSnapshots ();

		arc << mapname[0];
		while (mapname[0])
		{
			arc.Read (&mapname[1], 7);
			level_info_t *i = FindLevelInfo (mapname);
			i->snapshot = new FCompressedMemFile;
			i->snapshot->Serialize (arc);
			arc << mapname[0];
		}

		arc << mapname[0];
		while (mapname[0])
		{
			arc.Read (&mapname[1], 7);
			level_info_t *i = FindLevelInfo (mapname);
			i->flags |= LEVEL_VISITED;
			arc << mapname[0];
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
		BYTE zero = 0;
		arc << zero;
	}
	else
	{
		char mapname[8];

		P_RemoveDefereds ();

		arc << mapname[0];
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
			arc << i->defered;
			arc << mapname[0];
		}
	}
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

// Static level info from original game.
// The level names, cluster messages, and music get filled in
// by G_SetLevelStrings().

level_info_t LevelInfos[] =
{
	// Registered/Retail Episode 1
	{
		"E1M1",
		1,
		"WILV00",
		"E1M2",
		"E1M9",
		"SKY1",
		1,
		30,
	},
	{
		"E1M2",
		2,
		"WILV01",
		"E1M3",
		"E1M9",
		"SKY1",
		1,
		75,
	},
	{
		"E1M3",
		3,
		"WILV02",
		"E1M4",
		"E1M9",
		"SKY1",
		1,
		120,
	},
	{
		"E1M4",
		4,
		"WILV03",
		"E1M5",
		"E1M9",
		"SKY1",
		1,
		90,
	},
	{
		"E1M5",
		5,
		"WILV04",
		"E1M6",
		"E1M9",
		"SKY1",
		1,
		165,
	},
	{
		"E1M6",
		6,
		"WILV05",
		"E1M7",
		"E1M9",
		"SKY1",
		1,
		180,
	},
	{
		"E1M7",
		7,
		"WILV06",
		"E1M8",
		"E1M9",
		"SKY1",
		1,
		180,
	},
	{
		"E1M8",
		8,
		"WILV07",
		"",
		"E1M9",
		"SKY1",
		1,
		30,
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_BRUISERSPECIAL|LEVEL_SPECLOWERFLOOR,
	},
	{
		"E1M9",
		9,
		"WILV08",
		"E1M4",
		"E1M4",
		"SKY1",
		1,
		165,
	},

	// Registered/Retail Episode 2
	{
		"E2M1",
		11,
		"WILV10",
		"E2M2",
		"E2M9",
		"SKY2",
		2,
		90,
	},

	{
		"E2M2",
		12,
		"WILV11",
		"E2M3",
		"E2M9",
		"SKY2",
		2,
		90,
	},
	{
		"E2M3",
		13,
		"WILV12",
		"E2M4",
		"E2M9",
		"SKY2",
		2,
		90,
	},
	{
		"E2M4",
		14,
		"WILV13",
		"E2M5",
		"E2M9",
		"SKY2",
		2,
		120,
	},
	{
		"E2M5",
		15,
		"WILV14",
		"E2M6",
		"E2M9",
		"SKY2",
		2,
		90,
	},
	{
		"E2M6",
		16,
		"WILV15",
		"E2M7",
		"E2M9",
		"SKY2",
		2,
		360,
	},
	{
		"E2M7",
		17,
		"WILV16",
		"E2M8",
		"E2M9",
		"SKY2",
		2,
		240,
	},
	{
		"E2M8",
		18,
		"WILV17",
		"",
		"E2M9",
		"SKY2",
		2,
		30,
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_CYBORGSPECIAL,
	},
	{
		"E2M9",
		19,
		"WILV18",
		"E2M6",
		"E2M6",
		"SKY2",
		2,
		170,
	},

	// Registered/Retail Episode 3

	{
		"E3M1",
		21,
		"WILV20",
		"E3M2",
		"E3M9",
		"SKY3",
		3,
		90,
	},
	{
		"E3M2",
		22,
		"WILV21",
		"E3M3",
		"E3M9",
		"SKY3",
		3,
		45,
	},
	{
		"E3M3",
		23,
		"WILV22",
		"E3M4",
		"E3M9",
		"SKY3",
		3,
		90,
	},
	{
		"E3M4",
		24,
		"WILV23",
		"E3M5",
		"E3M9",
		"SKY3",
		3,
		150,
	},
	{
		"E3M5",
		25,
		"WILV24",
		"E3M6",
		"E3M9",
		"SKY3",
		3,
		90,
	},
	{
		"E3M6",
		26,
		"WILV25",
		"E3M7",
		"E3M9",
		"SKY3",
		3,
		90,
	},
	{
		"E3M7",
		27,
		"WILV26",
		"E3M8",
		"E3M9",
		"SKY3",
		3,
		165,
	},
	{
		"E3M8",
		28,
		"WILV27",
		"",
		"E3M9",
		"SKY3",
		3,
		30,
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPIDERSPECIAL,
	},
	{
		"E3M9",
		29,
		"WILV28",
		"E3M7",
		"E3M7",
		"SKY3",
		3,
		135,
	},

	// Retail Episode 4
	{
		"E4M1",
		31,
		"WILV30",
		"E4M2",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M2",
		32,
		"WILV31",
		"E4M3",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M3",
		33,
		"WILV32",
		"E4M4",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M4",
		34,
		"WILV33",
		"E4M5",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M5",
		35,
		"WILV34",
		"E4M6",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M6",
		36,
		"WILV35",
		"E4M7",
		"E4M9",
		"SKY4",
		4,
		0,
		LEVEL_CYBORGSPECIAL|LEVEL_SPECOPENDOOR,
	},
	{
		"E4M7",
		37,
		"WILV36",
		"E4M8",
		"E4M9",
		"SKY4",
		4,
	},
	{
		"E4M8",
		38,
		"WILV37",
		"",
		"E4M9",
		"SKY4",
		4,
		0,
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPIDERSPECIAL|LEVEL_SPECLOWERFLOOR,
	},
	{
		"E4M9",
		39,
		"WILV38",
		"E4M3",
		"E4M3",
		"SKY4",
		4,
	},

	// Heretic Episode 5
	{
		"E5M1",
		41,
		{0},
		"E5M2",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M2",
		42,
		{0},
		"E5M3",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M3",
		43,
		{0},
		"E5M4",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M4",
		44,
		{0},
		"E5M5",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M5",
		45,
		{0},
		"E5M6",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M6",
		46,
		{0},
		"E5M7",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M7",
		47,
		{0},
		"E5M8",
		"E5M9",
		"SKY3",
		15,
	},
	{
		"E5M8",
		48,
		{0},
		"",
		"E5M9",
		"SKY3",
		15,
		0,
		LEVEL_NOINTERMISSION|LEVEL_NOSOUNDCLIPPING|LEVEL_SPECLOWERFLOOR|LEVEL_MINOTAURSPECIAL
	},
	{
		"E5M9",
		49,
		{0},
		"E5M4",
		"E5M4",
		"SKY3",
		15,
	},

	// Heretic Episode 6
	{
		"E6M1",
		51,
		{0},
		"E6M2",
		"E6M2",
		"SKY1",
		16,
		0,
		0,
		NULL,
		"Untitled"
	},
	{
		"E6M2",
		52,
		{0},
		"E6M3",
		"E6M3",
		"SKY1",
		16,
		0,
		0,
		NULL,
		"Untitled"
	},
	{
		"E6M3",
		53,
		{0},
		"E6M1",
		"E6M1",
		"SKY1",
		16,
		0,
		0,
		NULL,
		"Untitled"
	},

	// DOOM 2 Levels

	{
		"MAP01",
		1,
		"CWILV00",
		"MAP02",
		"MAP02",
		"SKY1",
		5,
		30,
		0,
	},
	{
		"MAP02",
		2,
		"CWILV01",
		"MAP03",
		"MAP03",
		"SKY1",
		5,
		90,
		0,
	},
	{
		"MAP03",
		3,
		"CWILV02",
		"MAP04",
		"MAP04",
		"SKY1",
		5,
		120,
		0,
	},
	{
		"MAP04",
		4,
		"CWILV03",
		"MAP05",
		"MAP05",
		"SKY1",
		5,
		120,
		0,
	},
	{
		"MAP05",
		5,
		"CWILV04",
		"MAP06",
		"MAP06",
		"SKY1",
		5,
		90,
		0,
	},
	{
		"MAP06",
		6,
		"CWILV05",
		"MAP07",
		"MAP07",
		"SKY1",
		5,
		150,
		0,
	},
	{
		"MAP07",
		7,
		"CWILV06",
		"MAP08",
		"MAP08",
		"SKY1",
		6,
		120,
		LEVEL_MAP07SPECIAL,
	},
	{
		"MAP08",
		8,
		"CWILV07",
		"MAP09",
		"MAP09",
		"SKY1",
		6,
		120,
		0,
	},
	{
		"MAP09",
		9,
		"CWILV08",
		"MAP10",
		"MAP10",
		"SKY1",
		6,
		270,
		0,
	},
	{
		"MAP10",
		10,
		"CWILV09",
		"MAP11",
		"MAP11",
		"SKY1",
		6,
		90,
		0,
	},
	{
		"MAP11",
		11,
		"CWILV10",
		"MAP12",
		"MAP12",
		"SKY1",
		6,
		210,
		0,
	},
	{
		"MAP12",
		12,
		"CWILV11",
		"MAP13",
		"MAP13",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP13",
		13,
		"CWILV12",
		"MAP14",
		"MAP14",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP14",
		14,
		"CWILV13",
		"MAP15",
		"MAP15",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP15",
		15,
		"CWILV14",
		"MAP16",
		"MAP31",
		"SKY2",
		7,
		210,
		0,
	},
	{
		"MAP16",
		16,
		"CWILV15",
		"MAP17",
		"MAP17",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP17",
		17,
		"CWILV16",
		"MAP18",
		"MAP18",
		"SKY2",
		7,
		420,
		0,
	},
	{
		"MAP18",
		18,
		"CWILV17",
		"MAP19",
		"MAP19",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP19",
		19,
		"CWILV18",
		"MAP20",
		"MAP20",
		"SKY2",
		7,
		210,
		0,
	},
	{
		"MAP20",
		20,
		"CWILV19",
		"MAP21",
		"MAP21",
		"SKY2",
		7,
		150,
		0,
	},
	{
		"MAP21",
		21,
		"CWILV20",
		"MAP22",
		"MAP22",
		"SKY3",
		8,
		240,
		0,
	},
	{
		"MAP22",
		22,
		"CWILV21",
		"MAP23",
		"MAP23",
		"SKY3",
		8,
		150,
		0,
	},
	{
		"MAP23",
		23,
		"CWILV22",
		"MAP24",
		"MAP24",
		"SKY3",
		8,
		180,
		0,
	},
	{
		"MAP24",
		24,
		"CWILV23",
		"MAP25",
		"MAP25",
		"SKY3",
		8,
		150,
		0,
	},
	{
		"MAP25",
		25,
		"CWILV24",
		"MAP26",
		"MAP26",
		"SKY3",
		8,
		150,
		0,
	},
	{
		"MAP26",
		26,
		"CWILV25",
		"MAP27",
		"MAP27",
		"SKY3",
		8,
		300,
		0,
	},
	{
		"MAP27",
		27,
		"CWILV26",
		"MAP28",
		"MAP28",
		"SKY3",
		8,
		330,
		0,
	},
	{
		"MAP28",
		28,
		"CWILV27",
		"MAP29",
		"MAP29",
		"SKY3",
		8,
		420,
		0,
	},
	{
		"MAP29",
		29,
		"CWILV28",
		"MAP30",
		"MAP30",
		"SKY3",
		8,
		300,
		0,
	},
	{
		"MAP30",
		30,
		"CWILV29",
		"",
		"",
		"SKY3",
		8,
		180,
		LEVEL_MONSTERSTELEFRAG,
	},
	{
		"MAP31",
		31,
		"CWILV30",
		"MAP16",
		"MAP32",
		"SKY3",
		9,
		120,
		0,
	},
	{
		"MAP32",
		32,
		"CWILV31",
		"MAP16",
		"MAP16",
		"SKY3",
		10,
		30,
		0,
	},

	// End-of-list marker
	{
		"",
	}
};


// Episode/Cluster information
cluster_info_t ClusterInfos[] =
{
	{		// DOOM Episode 1
		1,	{ 'F','L','O','O','R','4','_','8' },
	},
	{		// DOOM Episode 2
		2,	"SFLR6_1",
	},
	{		// DOOM Episode 3
		3,	"MFLR8_4",
	},
	{		// DOOM Episode 4
		4,	"MFLR8_3",
	},
	{		// DOOM II first cluster (up thru level 6)
		5,	"SLIME16",
	},
	{		// DOOM II second cluster (up thru level 11)
		6,	"RROCK14",
	},
	{		// DOOM II third cluster (up thru level 20)
		7,	"RROCK07",
	},
	{		// DOOM II fourth cluster (up thru level 30)
		8,	"RROCK17",
	},
	{		// DOOM II fifth cluster (level 31)
		9,	"RROCK13",
	},
	{		// DOOM II sixth cluster (level 32)
		10,	"RROCK19",
	},
	{		// Heretic episode 1
		11,	"FLOOR25",
	},
	{		// Heretic episode 2
		12,	{'F','L','A','T','H','U','H','1'},
	},
	{		// Heretic episode 3
		13,	{'F','L','T','W','A','W','A','2'},
	},
	{		// Heretic episode 4
		14,	"FLOOR28",
	},
	{		// Heretic episode 5
		15,	"FLOOR08",
	},
	{		// Heretic episode 6
		16, "FLOOR25",
	},

	{ 0 }		// End-of-clusters marker
};






