/*
** g_level.h
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

#ifndef __G_LEVEL_H__
#define __G_LEVEL_H__

#include "doomtype.h"
#include "doomdef.h"
#include "m_fixed.h"
#include "tarray.h"

#define NUM_WORLDVARS			256
#define NUM_GLOBALVARS			64

#define LEVEL_NOINTERMISSION	0x00000001
#define	LEVEL_DOUBLESKY			0x00000004

#define LEVEL_MAP07SPECIAL		0x00000010
#define LEVEL_BRUISERSPECIAL	0x00000020
#define LEVEL_CYBORGSPECIAL		0x00000040
#define LEVEL_SPIDERSPECIAL		0x00000080

#define LEVEL_SPECLOWERFLOOR	0x00000100
#define LEVEL_SPECOPENDOOR		0x00000200
#define LEVEL_SPECACTIONSMASK	0x00000300

#define LEVEL_MONSTERSTELEFRAG	0x00000400
#define LEVEL_ACTOWNSPECIAL		0x00000800
#define LEVEL_SNDSEQTOTALCTRL	0x00001000
#define LEVEL_FORCENOSKYSTRETCH	0x00002000

#define LEVEL_JUMP_NO			0x00004000
#define LEVEL_JUMP_YES			0x00008000
#define LEVEL_FREELOOK_NO		0x00010000
#define LEVEL_FREELOOK_YES		0x00020000

// The absence of both of the following bits means that this level does not
// use falling damage (though damage can be forced with dmflags).
#define LEVEL_FALLDMG_ZD		0x00040000		// Level uses ZDoom's falling damage
#define LEVEL_FALLDMG_HX		0x00080000		// Level uses Hexen's falling damage

#define LEVEL_HEADSPECIAL		0x00100000		// Heretic episode 1/4
#define LEVEL_MINOTAURSPECIAL	0x00200000		// Heretic episode 2/5
#define LEVEL_SORCERER2SPECIAL	0x00400000		// Heretic episode 3
#define LEVEL_SPECKILLMONSTERS	0x00800000

#define LEVEL_STARTLIGHTNING	0x01000000		// Automatically start lightning
#define LEVEL_FILTERSTARTS		0x02000000		// Apply mapthing filtering to player starts

#define LEVEL_SWAPSKIES			0x10000000		// Used by lightning
#define LEVEL_DEFINEDINMAPINFO	0x20000000		// Level was defined in a MAPINFO lump
#define LEVEL_CHANGEMAPCHEAT	0x40000000		// Don't display cluster messages
#define LEVEL_VISITED			0x80000000		// Used for intermission map

struct acsdefered_s;
class FBehavior;

struct level_info_s
{
	char		mapname[8];
	int			levelnum;
	char		pname[8];
	char		nextmap[8];
	char		secretmap[8];
	char		skypic1[8];
	int			cluster;
	int			partime;
	DWORD		flags;
	char		*music;
	char		*level_name;
	int			musicorder;
	FCompressedMemFile	*snapshot;
	DWORD		snapshotVer;
	struct acsdefered_s *defered;
};
typedef struct level_info_s level_info_t;

// The start of level_pwad_info_s must be the same as level_info_s.
// I previously just subclassed it from level_info_s, but that makes
// GCC 3.2 treat it as a non-POD class and spew lots of warnings when
// using offsetof on it for the MAPINFO parser in g_level.cpp.

struct level_pwad_info_s
{
	char		mapname[8];
	int			levelnum;
	char		pname[8];
	char		nextmap[8];
	char		secretmap[8];
	char		skypic1[8];
	int			cluster;
	int			partime;
	DWORD		flags;
	char		*music;
	char		*level_name;
	int			musicorder;
	FCompressedMemFile	*snapshot;
	DWORD		snapshotVer;
	struct acsdefered_s *defered;
//------------------------------------//
	char		skypic2[8];
	fixed_t		skyspeed1;
	fixed_t		skyspeed2;
	DWORD		fadeto;
	char		fadetable[8];
	DWORD		outsidefog;
	int			cdtrack;
	unsigned int cdid;
	SBYTE		WallVertLight, WallHorizLight;
	float		gravity;
	float		aircontrol;
	int			WarpTrans;
};
typedef struct level_pwad_info_s level_pwad_info_t;

// [RH] These get zeroed every tic and are updated by thinkers.
struct FSectorScrollValues
{
	fixed_t ScrollX, ScrollY;
};

struct level_locals_s
{
	void Tick ();
	void AddScroller (DScroller *, int secnum);

	int			time;
	int			starttime;
	int			partime;

	level_info_t *info;
	int			cluster;
	int			clusterflags;
	int			levelnum;
	int			lumpnum;
	char		level_name[64];			// the descriptive name (Outer Base, etc)
	char		mapname[8];				// the server name (base1, etc)
	char		nextmap[8];				// go here when fraglimit is hit
	char		secretmap[8];			// map to go to when used secret exit

	DWORD		flags;

	DWORD		fadeto;					// The color the palette fades to (usually black)
	DWORD		outsidefog;				// The fog for sectors with sky ceilings

	char		*music;
	int			musicorder;
	int			cdtrack;
	unsigned int cdid;
	char		skypic1[8];
	char		skypic2[8];

	fixed_t		skyspeed1;				// Scrolling speed of first sky texture
	fixed_t		skyspeed2;

	int			total_secrets;
	int			found_secrets;

	int			total_items;
	int			found_items;

	int			total_monsters;
	int			killed_monsters;

	float		gravity;
	fixed_t		aircontrol;
	fixed_t		airfriction;

	FBehavior	*behavior;

	FSectorScrollValues	*Scrolls;		// NULL if no DScrollers in this level

	SBYTE		WallVertLight;			// Light diffs for vert/horiz walls
	SBYTE		WallHorizLight;
};
typedef struct level_locals_s level_locals_t;

enum EndTypes
{
	END_Pic,
	END_Pic1,
	END_Pic2,
	END_Pic3,
	END_Bunny,
	END_Cast,
	END_Demon,
	END_Underwater,
	END_Chess
};

struct EndSequence
{
	byte EndType;
	char PicName[8];
};

extern TArray<EndSequence> EndSequences;

struct cluster_info_s
{
	int			cluster;
	char		finaleflat[8];
	char		*exittext;
	char		*entertext;
	char		*messagemusic;
	int			musicorder;
	int			flags;
	int			cdtrack;
	unsigned int cdid;
};
typedef struct cluster_info_s cluster_info_t;

// Cluster flags
#define CLUSTER_HUB				0x00000001
#define CLUSTER_EXITTEXTINLUMP	0x00000002
#define CLUSTER_ENTERTEXTINLUMP	0x00000004
#define CLUSTER_FINALEPIC		0x00000008

extern level_locals_t level;
extern level_info_t LevelInfos[];
extern cluster_info_t ClusterInfos[];

extern SDWORD ACS_WorldVars[NUM_WORLDVARS];
extern SDWORD ACS_GlobalVars[NUM_GLOBALVARS];
extern TAutoGrowArray<SDWORD> ACS_WorldArrays[NUM_WORLDVARS];
extern TAutoGrowArray<SDWORD> ACS_GlobalArrays[NUM_GLOBALVARS];

extern BOOL savegamerestore;
extern BOOL HexenHack;		// Semi-Hexen-compatibility mode

bool CheckWarpTransMap (char mapname[9]);	// mapname will be changed if it is a valid warptrans

void G_InitNew (char *mapname);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (char *mapname);

void G_ExitLevel (int position);
void G_SecretExitLevel (int position);
void G_SetForEndGame (char *nextmap);

void G_DoLoadLevel (int position, bool autosave);

void G_InitLevelLocals (void);

void G_AirControlChanged ();

void G_SetLevelStrings (void);

cluster_info_t *FindClusterInfo (int cluster);
level_info_t *FindLevelInfo (char *mapname);
level_info_t *FindLevelByNum (int num);

char *CalcMapName (int episode, int level);

void G_ParseMapInfo (void);

void G_ClearSnapshots (void);
void G_SnapshotLevel (void);
void G_UnSnapshotLevel (bool keepPlayers);
void G_SerializeSnapshots (FILE *file, bool storing);

#endif //__G_LEVEL_H__
