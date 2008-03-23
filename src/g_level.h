/*
** g_level.h
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

#ifndef __G_LEVEL_H__
#define __G_LEVEL_H__

#include "doomtype.h"
#include "doomdef.h"
#include "m_fixed.h"
#include "tarray.h"
#include "name.h"

#define NUM_WORLDVARS			256
#define NUM_GLOBALVARS			64

#define LEVEL_NOINTERMISSION		UCONST64(0x00000001)
#define LEVEL_NOINVENTORYBAR		UCONST64(0x00000002)	// This effects Doom only, since it's the only one without a standard inventory bar.
#define	LEVEL_DOUBLESKY				UCONST64(0x00000004)
#define LEVEL_HASFADETABLE			UCONST64(0x00000008)	// Level uses Hexen's fadetable mapinfo to get fog

#define LEVEL_MAP07SPECIAL			UCONST64(0x00000010)
#define LEVEL_BRUISERSPECIAL		UCONST64(0x00000020)
#define LEVEL_CYBORGSPECIAL			UCONST64(0x00000040)
#define LEVEL_SPIDERSPECIAL			UCONST64(0x00000080)

#define LEVEL_SPECLOWERFLOOR		UCONST64(0x00000100)
#define LEVEL_SPECOPENDOOR			UCONST64(0x00000200)
#define LEVEL_SPECACTIONSMASK		UCONST64(0x00000300)

#define LEVEL_MONSTERSTELEFRAG		UCONST64(0x00000400)
#define LEVEL_ACTOWNSPECIAL			UCONST64(0x00000800)
#define LEVEL_SNDSEQTOTALCTRL		UCONST64(0x00001000)
#define LEVEL_FORCENOSKYSTRETCH		UCONST64(0x00002000)

#define LEVEL_CROUCH_NO				UCONST64(0x00004000)
#define LEVEL_JUMP_NO				UCONST64(0x00008000)
#define LEVEL_FREELOOK_NO			UCONST64(0x00010000)
#define LEVEL_FREELOOK_YES			UCONST64(0x00020000)

// The absence of both of the following bits means that this level does not
// use falling damage (though damage can be forced with dmflags).
#define LEVEL_FALLDMG_ZD			UCONST64(0x00040000)	// Level uses ZDoom's falling damage
#define LEVEL_FALLDMG_HX			UCONST64(0x00080000)	// Level uses Hexen's falling damage

#define LEVEL_HEADSPECIAL			UCONST64(0x00100000)	// Heretic episode 1/4
#define LEVEL_MINOTAURSPECIAL		UCONST64(0x00200000)	// Heretic episode 2/5
#define LEVEL_SORCERER2SPECIAL		UCONST64(0x00400000)	// Heretic episode 3
#define LEVEL_SPECKILLMONSTERS		UCONST64(0x00800000)

#define LEVEL_STARTLIGHTNING		UCONST64(0x01000000)	// Automatically start lightning
#define LEVEL_FILTERSTARTS			UCONST64(0x02000000)	// Apply mapthing filtering to player starts
#define LEVEL_LOOKUPLEVELNAME		UCONST64(0x04000000)	// Level name is the name of a language string
#define LEVEL_HEXENFORMAT			UCONST64(0x08000000)	// Level uses the Hexen map format

#define LEVEL_SWAPSKIES				UCONST64(0x10000000)	// Used by lightning
#define LEVEL_NOALLIES				UCONST64(0x20000000)	// i.e. Inside Strife's front base
#define LEVEL_CHANGEMAPCHEAT		UCONST64(0x40000000)	// Don't display cluster messages
#define LEVEL_VISITED				UCONST64(0x80000000)	// Used for intermission map

#define LEVEL_DEATHSLIDESHOW		UCONST64(0x100000000)	// Slideshow on death
#define LEVEL_ALLMAP				UCONST64(0x200000000)	// The player picked up a map on this level

#define LEVEL_LAXMONSTERACTIVATION	UCONST64(0x400000000)	// Monsters can open doors depending on the door speed
#define LEVEL_LAXACTIVATIONMAPINFO	UCONST64(0x800000000)	// LEVEL_LAXMONSTERACTIVATION is not a default.

#define LEVEL_MISSILESACTIVATEIMPACT UCONST64(0x1000000000)	// Missiles are the activators of SPAC_IMPACT events, not their shooters
#define LEVEL_FROZEN				UCONST64(0x2000000000)	// Game is frozen by a TimeFreezer

#define LEVEL_KEEPFULLINVENTORY		UCONST64(0x4000000000)	// doesn't reduce the amount of inventory items to 1

#define LEVEL_MUSICDEFINED			UCONST64(0x8000000000)	// a marker to disable the $map command in SNDINFO for this map
#define LEVEL_MONSTERFALLINGDAMAGE	UCONST64(0x10000000000)
#define LEVEL_CLIPMIDTEX			UCONST64(0x20000000000)
#define LEVEL_WRAPMIDTEX			UCONST64(0x40000000000)

#define LEVEL_CHECKSWITCHRANGE		UCONST64(0x80000000000)	

#define LEVEL_PAUSE_MUSIC_IN_MENUS	UCONST64(0x100000000000)
#define LEVEL_TOTALINFIGHTING		UCONST64(0x200000000000)
#define LEVEL_NOINFIGHTING			UCONST64(0x400000000000)

#define LEVEL_NOMONSTERS			UCONST64(0x800000000000)
#define LEVEL_INFINITE_FLIGHT		UCONST64(0x1000000000000)

#define LEVEL_ALLOWRESPAWN			UCONST64(0x2000000000000)

#define LEVEL_FORCETEAMPLAYON		UCONST64(0x4000000000000)
#define LEVEL_FORCETEAMPLAYOFF		UCONST64(0x8000000000000)


struct acsdefered_s;

struct FSpecialAction
{
	FName Type;					// this is initialized before the actors...
	BYTE Action;
	int Args[5];				// must allow 16 bit tags for 666 & 667!
	FSpecialAction *Next;
};

struct level_info_s
{
	char		mapname[9];
	int			levelnum;
	char		pname[9];
	char		nextmap[9];
	char		secretmap[9];
	char		skypic1[9];
	int			cluster;
	int			partime;
	int			sucktime;
	QWORD		flags;
	char		*music;
	char		*level_name;
	char		fadetable[9];
	SBYTE		WallVertLight, WallHorizLight;
	const char	*f1;
	// TheDefaultLevelInfo initializes everything above this line.
	int			musicorder;
	FCompressedMemFile	*snapshot;
	DWORD		snapshotVer;
	struct acsdefered_s *defered;
	char		skypic2[9];
	float		skyspeed1;
	float		skyspeed2;
	DWORD		fadeto;
	DWORD		outsidefog;
	int			cdtrack;
	unsigned int cdid;
	float		gravity;
	float		aircontrol;
	int			WarpTrans;
	int			airsupply;
	DWORD		compatflags;
	DWORD		compatmask;
	char		*translator;	// for converting Doom-format linedef and sector types.

	// Redirection: If any player is carrying the specified item, then
	// you go to the RedirectMap instead of this one.
	FName		RedirectType;
	char		RedirectMap[9];

	char		enterpic[9];
	char		exitpic[9];
	char 		*intermusic;
	int			intermusicorder;

	char		soundinfo[9];
	char		sndseq[9];
	char		bordertexture[9];

	int			fogdensity;
	int			outsidefogdensity;
	int			skyfog;
	FSpecialAction * specialactions;

	float		teamdamage;
};
typedef struct level_info_s level_info_t;

// [RH] These get zeroed every tic and are updated by thinkers.
struct FSectorScrollValues
{
	fixed_t ScrollX, ScrollY;
};

struct level_locals_s
{
	void Tick ();
	void AddScroller (DScroller *, int secnum);

	int			time;			// time in the hub
	int			maptime;		// time in the map
	int			totaltime;		// time in the game
	int			starttime;
	int			partime;
	int			sucktime;

	level_info_t *info;
	int			cluster;
	int			clusterflags;
	int			levelnum;
	int			lumpnum;
	char		level_name[64];			// the descriptive name (Outer Base, etc)
	char		mapname[256];			// the server name (base1, etc)
	char		nextmap[9];				// go here when fraglimit is hit
	char		secretmap[9];			// map to go to when used secret exit

	QWORD		flags;

	DWORD		fadeto;					// The color the palette fades to (usually black)
	DWORD		outsidefog;				// The fog for sectors with sky ceilings

	char		*music;
	int			musicorder;
	int			cdtrack;
	unsigned int cdid;
	char		skypic1[9];
	char		skypic2[9];

	float		skyspeed1;				// Scrolling speed of sky textures, in pixels per ms
	float		skyspeed2;

	int			total_secrets;
	int			found_secrets;

	int			total_items;
	int			found_items;

	int			total_monsters;
	int			killed_monsters;

	float		gravity;
	fixed_t		aircontrol;
	fixed_t		airfriction;
	int			airsupply;

	FSectorScrollValues	*Scrolls;		// NULL if no DScrollers in this level

	SBYTE		WallVertLight;			// Light diffs for vert/horiz walls
	SBYTE		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot

	const char	*f1;

	float		teamdamage;

	bool		IsJumpingAllowed() const;
	bool		IsCrouchingAllowed() const;
	bool		IsFreelookAllowed() const;
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
	END_Chess,
	END_Strife,
	END_BuyStrife
};

struct EndSequence
{
	BYTE EndType;
	char PicName[9];
};

extern TArray<EndSequence> EndSequences;

struct cluster_info_s
{
	int			cluster;
	char		finaleflat[9];
	char		*exittext;
	char		*entertext;
	char		*messagemusic;
	int			musicorder;
	int			flags;
	int			cdtrack;
	char		*clustername;
	unsigned int cdid;
};
typedef struct cluster_info_s cluster_info_t;

// Cluster flags
#define CLUSTER_HUB				0x00000001	// Cluster uses hub behavior
#define CLUSTER_EXITTEXTINLUMP	0x00000002	// Exit text is the name of a lump
#define CLUSTER_ENTERTEXTINLUMP	0x00000004	// Enter text is the name of a lump
#define CLUSTER_FINALEPIC		0x00000008	// Finale "flat" is actually a full-sized image
#define CLUSTER_LOOKUPEXITTEXT	0x00000010	// Exit text is the name of a language string
#define CLUSTER_LOOKUPENTERTEXT	0x00000020	// Enter text is the name of a language string

extern level_locals_t level;

extern TArray<level_info_t> wadlevelinfos;

extern SDWORD ACS_WorldVars[NUM_WORLDVARS];
extern SDWORD ACS_GlobalVars[NUM_GLOBALVARS];

struct InitIntToZero
{
	void Init(int &v)
	{
		v = 0;
	}
};
typedef TMap<SDWORD, SDWORD, THashTraits<SDWORD>, InitIntToZero> FWorldGlobalArray;

extern FWorldGlobalArray ACS_WorldArrays[NUM_WORLDVARS];
extern FWorldGlobalArray ACS_GlobalArrays[NUM_GLOBALVARS];

extern bool savegamerestore;

// mapname will be changed if it is a valid warptrans
bool CheckWarpTransMap (char mapname[9], bool substitute);

void G_InitNew (const char *mapname, bool bTitleLevel);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (const char *mapname, int skill = -1);

void G_ExitLevel (int position, bool keepFacing);
void G_SecretExitLevel (int position);
const char *G_GetExitMap();
const char *G_GetSecretExitMap();

void G_ChangeLevel(const char * levelname, int position, bool keepFacing, int nextSkill=-1, 
				   bool nointermission=false, bool resetinventory=false, bool nomonsters=false);

void G_SetForEndGame (char *nextmap);

void G_StartTravel ();
void G_FinishTravel ();

void G_DoLoadLevel (int position, bool autosave);

void G_InitLevelLocals (void);

void G_AirControlChanged ();

void G_MakeEpisodes (void);
const char *G_MaybeLookupLevelName (level_info_t *level);

cluster_info_t *FindClusterInfo (int cluster);
level_info_t *FindLevelInfo (const char *mapname);
level_info_t *FindLevelByNum (int num);
level_info_t *CheckLevelRedirect (level_info_t *info);

char *CalcMapName (int episode, int level);

void G_ParseMapInfo (void);
void G_UnloadMapInfo ();

void G_ClearSnapshots (void);
void G_SnapshotLevel (void);
void G_UnSnapshotLevel (bool keepPlayers);
struct PNGHandle;
void G_ReadSnapshots (PNGHandle *png);
void G_WriteSnapshots (FILE *file);

enum ESkillProperty
{
	SKILLP_AmmoFactor,
	SKILLP_DamageFactor,
	SKILLP_FastMonsters,
	SKILLP_Respawn,
	SKILLP_RespawnLimit,
	SKILLP_Aggressiveness,
	SKILLP_DisableCheats,
	SKILLP_AutoUseHealth,
	SKILLP_SpawnFilter,
	SKILLP_EasyBossBrain,
	SKILLP_ACSReturn
};
int G_SkillProperty(ESkillProperty prop);

typedef TMap<FName, FString> SkillMenuNames;

struct FSkillInfo
{
	FName Name;
	fixed_t AmmoFactor, DoubleAmmoFactor;
	fixed_t DamageFactor;
	bool FastMonsters;
	bool DisableCheats;
	bool AutoUseHealth;
	bool EasyBossBrain;
	int RespawnCounter;
	int RespawnLimit;
	fixed_t Aggressiveness;
	int SpawnFilter;
	int ACSReturn;
	FString MenuName;
	SkillMenuNames MenuNamesForPlayerClass;
	bool MenuNameIsLump;
	bool MustConfirm;
	FString MustConfirmText;
	char Shortcut;
	FString TextColor;

	FSkillInfo() {}
	FSkillInfo(const FSkillInfo &other)
	{
		operator=(other);
	}
	FSkillInfo &operator=(const FSkillInfo &other);
	int GetTextColor() const;
};

extern TArray<FSkillInfo> AllSkills;



#endif //__G_LEVEL_H__
