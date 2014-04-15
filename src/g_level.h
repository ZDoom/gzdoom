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
#include "sc_man.h"
#include "s_sound.h"

struct level_info_t;
struct cluster_info_t;
class FScanner;

#if defined(_MSC_VER)
#pragma data_seg(".yreg$u")
#pragma data_seg()

#define MSVC_YSEG __declspec(allocate(".yreg$u"))
#define GCC_YSEG
#else
#define MSVC_YSEG
#define GCC_YSEG __attribute__((section(SECTION_YREG)))
#endif

struct FIntermissionDescriptor;
struct FIntermissionAction;

struct FMapInfoParser
{
	enum EFormatType
	{
		FMT_Unknown,
		FMT_Old,
		FMT_New
	};

	FScanner sc;
	int format_type;
	bool HexenHack;

	FMapInfoParser(int format = FMT_Unknown)
	{
		format_type = format;
		HexenHack = false;
	}

	bool ParseLookupName(FString &dest);
	void ParseMusic(FString &name, int &order);
	void ParseLumpOrTextureName(char *name);
	void ParseLumpOrTextureName(FString &name);

	void ParseCluster();
	void ParseNextMap(char *mapname);
	level_info_t *ParseMapHeader(level_info_t &defaultinfo);
	void ParseMapDefinition(level_info_t &leveldef);
	void ParseGameInfo();
	void ParseEpisodeInfo ();
	void ParseSkill ();
	void ParseMapInfo (int lump, level_info_t &gamedefaults, level_info_t &defaultinfo);

	void ParseOpenBrace();
	bool ParseCloseBrace();
	bool CheckAssign();
	void ParseAssign();
	void MustParseAssign();
	void ParseComma();
	bool CheckNumber();
	bool CheckFloat();
	void SkipToNext();
	void CheckEndOfFile(const char *block);

	void ParseIntermissionAction(FIntermissionDescriptor *Desc);
	void ParseIntermission();
	void ParseAMColors(bool);
	FName CheckEndSequence();
	FName ParseEndGame();
};

#define DEFINE_MAP_OPTION(name, old) \
	static void MapOptHandler_##name(FMapInfoParser &parse, level_info_t *info); \
	static FMapOptInfo MapOpt_##name = \
		{ #name, MapOptHandler_##name, old }; \
	MSVC_YSEG FMapOptInfo *mapopt_##name GCC_YSEG = &MapOpt_##name; \
	static void MapOptHandler_##name(FMapInfoParser &parse, level_info_t *info)


struct FMapOptInfo
{
	const char *name;
	void (*handler) (FMapInfoParser &parse, level_info_t *levelinfo);
	bool old;
};

enum ELevelFlags
{
	LEVEL_NOINTERMISSION		= 0x00000001,
	LEVEL_NOINVENTORYBAR		= 0x00000002,	// This effects Doom only, since it's the only one without a standard inventory bar.
	LEVEL_DOUBLESKY				= 0x00000004,
	LEVEL_HASFADETABLE			= 0x00000008,	// Level uses Hexen's fadetable mapinfo to get fog

	LEVEL_MAP07SPECIAL			= 0x00000010,
	LEVEL_BRUISERSPECIAL		= 0x00000020,
	LEVEL_CYBORGSPECIAL			= 0x00000040,
	LEVEL_SPIDERSPECIAL			= 0x00000080,

	LEVEL_SPECLOWERFLOOR		= 0x00000100,
	LEVEL_SPECOPENDOOR			= 0x00000200,
	LEVEL_SPECLOWERFLOORTOHIGHEST=0x00000300,
	LEVEL_SPECACTIONSMASK		= 0x00000300,

	LEVEL_MONSTERSTELEFRAG		= 0x00000400,
	LEVEL_ACTOWNSPECIAL			= 0x00000800,
	LEVEL_SNDSEQTOTALCTRL		= 0x00001000,
	LEVEL_FORCENOSKYSTRETCH		= 0x00002000,

	LEVEL_CROUCH_NO				= 0x00004000,
	LEVEL_JUMP_NO				= 0x00008000,
	LEVEL_FREELOOK_NO			= 0x00010000,
	LEVEL_FREELOOK_YES			= 0x00020000,

	// The absence of both of the following bits means that this level does not
	// use falling damage (though damage can be forced with dmflags,.
	LEVEL_FALLDMG_ZD			= 0x00040000,	// Level uses ZDoom's falling damage
	LEVEL_FALLDMG_HX			= 0x00080000,	// Level uses Hexen's falling damage

	LEVEL_HEADSPECIAL			= 0x00100000,	// Heretic episode 1/4
	LEVEL_MINOTAURSPECIAL		= 0x00200000,	// Heretic episode 2/5
	LEVEL_SORCERER2SPECIAL		= 0x00400000,	// Heretic episode 3
	LEVEL_SPECKILLMONSTERS		= 0x00800000,

	LEVEL_STARTLIGHTNING		= 0x01000000,	// Automatically start lightning
	LEVEL_FILTERSTARTS			= 0x02000000,	// Apply mapthing filtering to player starts
	LEVEL_LOOKUPLEVELNAME		= 0x04000000,	// Level name is the name of a language string
	LEVEL_USEPLAYERSTARTZ		= 0x08000000,	// Use the Z position of player starts

	LEVEL_SWAPSKIES				= 0x10000000,	// Used by lightning
	LEVEL_NOALLIES				= 0x20000000,	// i.e. Inside Strife's front base
	LEVEL_CHANGEMAPCHEAT		= 0x40000000,	// Don't display cluster messages
	LEVEL_VISITED				= 0x80000000,	// Used for intermission map

	// The flags QWORD is now split into 2 DWORDs 
	LEVEL2_RANDOMPLAYERSTARTS	= 0x00000001,	// Select single player starts randomnly (no voodoo dolls)
	LEVEL2_ALLMAP				= 0x00000002,	// The player picked up a map on this level

	LEVEL2_LAXMONSTERACTIVATION	= 0x00000004,	// Monsters can open doors depending on the door speed
	LEVEL2_LAXACTIVATIONMAPINFO	= 0x00000008,	// LEVEL_LAXMONSTERACTIVATION is not a default.

	LEVEL2_MISSILESACTIVATEIMPACT=0x00000010,	// Missiles are the activators of SPAC_IMPACT events, not their shooters
	LEVEL2_FROZEN				= 0x00000020,	// Game is frozen by a TimeFreezer

	LEVEL2_KEEPFULLINVENTORY	= 0x00000040,	// doesn't reduce the amount of inventory items to 1

	LEVEL2_PRERAISEWEAPON		= 0x00000080,	// players should spawn with their weapons fully raised (but not when respawning it multiplayer)
	LEVEL2_MONSTERFALLINGDAMAGE	= 0x00000100,
	LEVEL2_CLIPMIDTEX			= 0x00000200,
	LEVEL2_WRAPMIDTEX			= 0x00000400,

	LEVEL2_CHECKSWITCHRANGE		= 0x00000800,	

	LEVEL2_PAUSE_MUSIC_IN_MENUS	= 0x00001000,
	LEVEL2_TOTALINFIGHTING		= 0x00002000,
	LEVEL2_NOINFIGHTING			= 0x00004000,

	LEVEL2_NOMONSTERS			= 0x00008000,
	LEVEL2_INFINITE_FLIGHT		= 0x00010000,

	LEVEL2_ALLOWRESPAWN			= 0x00020000,

	LEVEL2_FORCETEAMPLAYON		= 0x00040000,
	LEVEL2_FORCETEAMPLAYOFF		= 0x00080000,

	LEVEL2_CONV_SINGLE_UNFREEZE	= 0x00100000,
	LEVEL2_RAILINGHACK			= 0x00200000,	// but UDMF requires them to be separate to have more control
	LEVEL2_DUMMYSWITCHES		= 0x00400000,
	LEVEL2_HEXENHACK			= 0x00800000,	// Level was defined in a Hexen style MAPINFO

	LEVEL2_SMOOTHLIGHTING		= 0x01000000,	// Level uses the smooth lighting feature.
	LEVEL2_POLYGRIND			= 0x02000000,	// Polyobjects grind corpses to gibs.
	LEVEL2_RESETINVENTORY		= 0x04000000,	// Resets player inventory when starting this level (unless in a hub)
	LEVEL2_RESETHEALTH			= 0x08000000,	// Resets player health when starting this level (unless in a hub)

	LEVEL2_NOSTATISTICS			= 0x10000000,	// This level should not have statistics collected
	LEVEL2_ENDGAME				= 0x20000000,	// This is an epilogue level that cannot be quit.
	LEVEL2_NOAUTOSAVEHINT		= 0x40000000,	// tell the game that an autosave for this level does not need to be kept
	LEVEL2_FORGETSTATE			= 0x80000000,	// forget this map's state in a hub
};


struct acsdefered_t;

struct FSpecialAction
{
	FName Type;					// this is initialized before the actors...
	BYTE Action;
	int Args[5];				// must allow 16 bit tags for 666 & 667!
};

class FCompressedMemFile;
class DScroller;

class FScanner;
struct level_info_t;

struct FOptionalMapinfoData
{
	FOptionalMapinfoData *Next;
	FName identifier;
	FOptionalMapinfoData() { Next = NULL; identifier = NAME_None; }
	virtual ~FOptionalMapinfoData() {}
	virtual FOptionalMapinfoData *Clone() const = 0;
};

struct FOptionalMapinfoDataPtr
{
	FOptionalMapinfoData *Ptr;

	FOptionalMapinfoDataPtr() throw() : Ptr(NULL) {}
	~FOptionalMapinfoDataPtr() { if (Ptr!=NULL) delete Ptr; }
	FOptionalMapinfoDataPtr(const FOptionalMapinfoDataPtr &p) throw() : Ptr(p.Ptr->Clone()) {}
	FOptionalMapinfoDataPtr &operator= (FOptionalMapinfoDataPtr &p) throw() { Ptr = p.Ptr->Clone(); return *this; }
};

typedef TMap<FName, FOptionalMapinfoDataPtr> FOptData;
typedef TMap<int, FName> FMusicMap;

enum EMapType
{
	MAPTYPE_UNKNOWN = 0,
	MAPTYPE_DOOM,
	MAPTYPE_HEXEN,
	MAPTYPE_BUILD,
	MAPTYPE_UDMF	// This does not distinguish between namespaces.
};

struct level_info_t
{
	int			levelnum;
	
	char		mapname[9];
	char		pname[9];
	char		nextmap[11];
	char		secretmap[11];
	char		skypic1[9];
	char		skypic2[9];
	char		fadetable[9];
	char		f1[9];
	char		bordertexture[9];
	char		mapbg[9];

	int			cluster;
	int			partime;
	int			sucktime;
	DWORD		flags;
	DWORD		flags2;
	FString		Music;
	FString		LevelName;
	SBYTE		WallVertLight, WallHorizLight;
	int			musicorder;
	FCompressedMemFile	*snapshot;
	DWORD		snapshotVer;
	struct acsdefered_t *defered;
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
	DWORD		compatflags, compatflags2;
	DWORD		compatmask, compatmask2;
	FString		Translator;	// for converting Doom-format linedef and sector types.
	int			DefaultEnvironment;	// Default sound environment for the map.
	FName		Intermission;
	FName		deathsequence;
	FName		slideshow;

	// Redirection: If any player is carrying the specified item, then
	// you go to the RedirectMap instead of this one.
	FName		RedirectType;
	char		RedirectMap[9];

	FString		EnterPic;
	FString		ExitPic;
	FString 	InterMusic;
	int			intermusicorder;

	FString		SoundInfo;
	FString		SndSeq;

	float		teamdamage;

	FOptData	optdata;
	FMusicMap	MusicMap;

	TArray<FSpecialAction> specialactions;

	TArray<FSoundID> PrecacheSounds;

	level_info_t() 
	{ 
		Reset(); 
	}
	~level_info_t()
	{
		ClearSnapshot(); 
		ClearDefered();
	}
	void Reset();
	bool isValid();
	FString LookupLevelName ();
	void ClearSnapshot();
	void ClearDefered();
	level_info_t *CheckLevelRedirect ();

	template<class T>
	T *GetOptData(FName id, bool create = true)
	{
		FOptionalMapinfoDataPtr *pdat = optdata.CheckKey(id);
		
		if (pdat != NULL)
		{
			return static_cast<T*>(pdat->Ptr);
		}
		else if (create)
		{
			T *newobj = new T;
			optdata[id].Ptr = newobj;
			return newobj;
		}
		else return NULL;
	}
};

// [RH] These get zeroed every tic and are updated by thinkers.
struct FSectorScrollValues
{
	fixed_t ScrollX, ScrollY;
};

struct FLevelLocals
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
	FString		LevelName;
	char		mapname[256];			// the lump name (E1M1, MAP01, etc)
	char		nextmap[11];			// go here when using the regular exit
	char		secretmap[11];			// map to go to when used secret exit
	EMapType	maptype;

	DWORD		flags;
	DWORD		flags2;

	DWORD		fadeto;					// The color the palette fades to (usually black)
	DWORD		outsidefog;				// The fog for sectors with sky ceilings

	FString		Music;
	int			musicorder;
	int			cdtrack;
	unsigned int cdid;
	int			nextmusic;				// For MUSINFO purposes
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
	int			DefaultEnvironment;		// Default sound environment.

	TObjPtr<class ASkyViewpoint> DefaultSkybox;

	FSectorScrollValues	*Scrolls;		// NULL if no DScrollers in this level

	SBYTE		WallVertLight;			// Light diffs for vert/horiz walls
	SBYTE		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot

	float		teamdamage;

	bool		IsJumpingAllowed() const;
	bool		IsCrouchingAllowed() const;
	bool		IsFreelookAllowed() const;
};


struct cluster_info_t
{
	int			cluster;
	FString		FinaleFlat;
	FString		ExitText;
	FString		EnterText;
	FString		MessageMusic;
	int			musicorder;
	int			flags;
	int			cdtrack;
	FString		ClusterName;
	unsigned int cdid;

	void Reset();

};

// Cluster flags
#define CLUSTER_HUB				0x00000001	// Cluster uses hub behavior
#define CLUSTER_EXITTEXTINLUMP	0x00000002	// Exit text is the name of a lump
#define CLUSTER_ENTERTEXTINLUMP	0x00000004	// Enter text is the name of a lump
#define CLUSTER_FINALEPIC		0x00000008	// Finale "flat" is actually a full-sized image
#define CLUSTER_LOOKUPEXITTEXT	0x00000010	// Exit text is the name of a language string
#define CLUSTER_LOOKUPENTERTEXT	0x00000020	// Enter text is the name of a language string
#define CLUSTER_LOOKUPNAME		0x00000040	// Name is the name of a language string
#define CLUSTER_LOOKUPCLUSTERNAME 0x00000080	// Cluster name is the name of a language string

extern FLevelLocals level;

extern TArray<level_info_t> wadlevelinfos;
extern TArray<cluster_info_t> wadclusterinfos;

extern bool savegamerestore;

// mapname will be changed if it is a valid warptrans
bool CheckWarpTransMap (FString &mapname, bool substitute);

void G_InitNew (const char *mapname, bool bTitleLevel);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (const char *mapname, int skill = -1);
struct FGameStartup;
void G_DeferedInitNew (FGameStartup *gs);

void G_ExitLevel (int position, bool keepFacing);
void G_SecretExitLevel (int position);
const char *G_GetExitMap();
const char *G_GetSecretExitMap();

enum 
{
	CHANGELEVEL_KEEPFACING = 1,
	CHANGELEVEL_RESETINVENTORY = 2,
	CHANGELEVEL_NOMONSTERS = 4,
	CHANGELEVEL_CHANGESKILL = 8,
	CHANGELEVEL_NOINTERMISSION = 16,
	CHANGELEVEL_RESETHEALTH = 32,
	CHANGELEVEL_PRERAISEWEAPON = 64,
};

void G_ChangeLevel(const char *levelname, int position, int flags, int nextSkill=-1);

void G_StartTravel ();
void G_FinishTravel ();

void G_DoLoadLevel (int position, bool autosave);

void G_InitLevelLocals (void);

void G_AirControlChanged ();

cluster_info_t *FindClusterInfo (int cluster);
level_info_t *FindLevelInfo (const char *mapname, bool allowdefault=true);
level_info_t *FindLevelByNum (int num);
level_info_t *CheckLevelRedirect (level_info_t *info);

FString CalcMapName (int episode, int level);

void G_ParseMapInfo (const char *basemapinfo);

void G_ClearSnapshots (void);
void P_RemoveDefereds ();
void G_SnapshotLevel (void);
void G_UnSnapshotLevel (bool keepPlayers);
struct PNGHandle;
void G_ReadSnapshots (PNGHandle *png);
void G_WriteSnapshots (FILE *file);
void G_ClearHubInfo();

enum ESkillProperty
{
	SKILLP_AmmoFactor,
	SKILLP_DropAmmoFactor,
	SKILLP_DamageFactor,
	SKILLP_FastMonsters,
	SKILLP_Respawn,
	SKILLP_RespawnLimit,
	SKILLP_Aggressiveness,
	SKILLP_DisableCheats,
	SKILLP_AutoUseHealth,
	SKILLP_SpawnFilter,
	SKILLP_EasyBossBrain,
	SKILLP_ACSReturn,
	SKILLP_MonsterHealth,
	SKILLP_FriendlyHealth,
	SKILLP_NoPain,
	SKILLP_ArmorFactor,
	SKILLP_EasyKey,
	SKILLP_SlowMonsters,
};
int G_SkillProperty(ESkillProperty prop);
const char * G_SkillName();

typedef TMap<FName, FString> SkillMenuNames;

typedef TMap<FName, FName> SkillActorReplacement;

struct FSkillInfo
{
	FName Name;
	fixed_t AmmoFactor, DoubleAmmoFactor, DropAmmoFactor;
	fixed_t DamageFactor;
	bool FastMonsters;
	bool SlowMonsters;
	bool DisableCheats;
	bool AutoUseHealth;

	bool EasyBossBrain;
	bool EasyKey;
	int RespawnCounter;
	int RespawnLimit;
	fixed_t Aggressiveness;
	int SpawnFilter;
	int ACSReturn;
	FString MenuName;
	FString PicName;
	SkillMenuNames MenuNamesForPlayerClass;
	bool MustConfirm;
	FString MustConfirmText;
	char Shortcut;
	FString TextColor;
	SkillActorReplacement Replace;
	SkillActorReplacement Replaced;
	fixed_t MonsterHealth;
	fixed_t FriendlyHealth;
	bool NoPain;
	fixed_t ArmorFactor;

	FSkillInfo() {}
	FSkillInfo(const FSkillInfo &other)
	{
		operator=(other);
	}
	FSkillInfo &operator=(const FSkillInfo &other);
	int GetTextColor() const;

	void SetReplacement(FName a, FName b);
	FName GetReplacement(FName a);
	void SetReplacedBy(FName b, FName a);
	FName GetReplacedBy(FName b);
};

extern TArray<FSkillInfo> AllSkills;
extern int DefaultSkill;

struct FEpisode
{
	FString mEpisodeName;
	FString mEpisodeMap;
	FString mPicName;
	char mShortcut;
	bool mNoSkill;
};

extern TArray<FEpisode> AllEpisodes;


#endif //__G_LEVEL_H__
