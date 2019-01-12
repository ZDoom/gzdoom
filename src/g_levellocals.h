/*
** g_levellocals.h
** The static data for a level
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2017 Christoph Oelckers
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

#pragma once

#include "doomdata.h"
#include "g_level.h"
#include "r_defs.h"
#include "r_sky.h"
#include "portal.h"
#include "p_blockmap.h"
#include "p_local.h"
#include "po_man.h"
#include "p_acs.h"
#include "p_tags.h"
#include "p_effect.h"
#include "wi_stuff.h"
#include "p_destructible.h"
#include "p_conversation.h"
#include "r_data/r_interpolate.h"
#include "r_data/r_sections.h"
#include "r_data/r_canvastexture.h"

class DACSThinker;
class DFraggleThinker;
class DSpotState;
class DSeqNode;
struct FStrifeDialogueNode;
class DSectorMarker;

typedef TMap<int, int> FDialogueIDMap;				// maps dialogue IDs to dialogue array index (for ACS)
typedef TMap<FName, int> FDialogueMap;				// maps actor class names to dialogue array index
typedef TMap<int, FUDMFKeys> FUDMFKeyMap;

struct FLevelData
{
	TArray<vertex_t> vertexes;
	TArray<sector_t> sectors;
	TArray<line_t*> linebuffer;	// contains the line lists for the sectors.
	TArray<subsector_t*> subsectorbuffer;	// contains the subsector lists for the sectors.
	TArray<line_t> lines;
	TArray<side_t> sides;
	TArray<seg_t *> segbuffer;	// contains the seg links for the sidedefs.
	TArray<seg_t> segs;
	TArray<subsector_t> subsectors;
	TArray<node_t> nodes;
	TArray<subsector_t> gamesubsectors;
	TArray<node_t> gamenodes;
	node_t *headgamenode;
	TArray<uint8_t> rejectmatrix;
	TArray<zone_t>	Zones;
	TArray<FPolyObj> Polyobjects;

	TArray<FSectorPortal> sectorPortals;
	TArray<FLinePortal> linePortals;

	// Portal information.
	FDisplacementTable Displacements;
	FPortalBlockmap PortalBlockmap;
	TArray<FLinePortal*> linkedPortals;	// only the linked portals, this is used to speed up looking for them in P_CollectConnectedGroups.
	TDeletingArray<FSectorPortalGroup *> portalGroups;
	TArray<FLinePortalSpan> linePortalSpans;
	FSectionContainer sections;
	FCanvasTextureInfo canvasTextureInfo;

	// [ZZ] Destructible geometry information
	TMap<int, FHealthGroup> healthGroups;

	FBlockmap blockmap;
	TArray<polyblock_t *> PolyBlockMap;

	// These are copies of the loaded map data that get used by the savegame code to skip unaltered fields
	// Without such a mechanism the savegame format would become too slow and large because more than 80-90% are normally still unaltered.
	TArray<sector_t>	loadsectors;
	TArray<line_t>	loadlines;
	TArray<side_t>	loadsides;

	// Maintain single and multi player starting spots.
	TArray<FPlayerStart> deathmatchstarts;
	FPlayerStart		playerstarts[MAXPLAYERS];
	TArray<FPlayerStart> AllPlayerStarts;

	FBehaviorContainer Behaviors;
	
	TDeletingArray<FStrifeDialogueNode *> StrifeDialogues;
	FDialogueIDMap DialogueRoots;
	FDialogueMap ClassRoots;

	int ii_compatflags = 0;
	int ii_compatflags2 = 0;
	int ib_compatflags = 0;
	int i_compatflags = 0;
	int i_compatflags2 = 0;

	FUDMFKeyMap UDMFKeys[4];

};


struct FLevelLocals : public FLevelData
{
	void *operator new(size_t blocksize)
	{
		// Null the allocated memory before running the constructor.
		// This was previously static memory that relied on being nulled to avoid uninitialized parts.
		auto block = ::operator new(blocksize);
		memset(block, 0, blocksize);
		return block;
	}
	FLevelLocals() : tagManager(this) {}
	~FLevelLocals();

	void Tick();
	void Mark();
	void AddScroller(int secnum);
	void SetInterMusic(const char *nextmap);
	void SetMusic();
	void ClearLevelData();
	bool CheckIfExitIsGood(AActor *self, level_info_t *newmap);
	void FormatMapName(FString &mapname, const char *mapnamecolor);
	void TranslateTeleportThings(void);
	void ClearAllSubsectorLinks();
	void ChangeAirControl(double newval);
	void InitLevelLocals(const level_info_t *info, bool isprimary);
	bool IsTIDUsed(int tid);
	int FindUniqueTID(int start_tid, int limit);
	int GetConversation(int conv_id);
	int GetConversation(FName classname);
	void SetConversation(int convid, PClassActor *Class, int dlgindex);
	int FindNode (const FStrifeDialogueNode *node);
	int GetInfighting();

	//
	// P_ClearTidHashes
	//
	// Clears the tid hashtable.
	//

	void ClearTIDHashes ()
	{
		memset(TIDHash, 0, sizeof(TIDHash));
	}

	FTagManager tagManager;
	AActor *TIDHash[128];

	DSectorMarker *SectorMarker;

	uint8_t		md5[16];			// for savegame validation. If the MD5 does not match the savegame won't be loaded.
	int			maptime;			// time in the map
	int			starttime;
	int			partime;
	int			sucktime;
	uint32_t	spawnindex;

	const level_info_t * info;		// The info is supposed to be immutable.
	int			cluster;
	int			clusterflags;
	int			levelnum;
	int			lumpnum;
	FString		LevelName;
	FString		MapName;			// the lump name (E1M1, MAP01, etc)
	FString		NextMap;			// go here when using the regular exit
	FString		NextSecretMap;		// map to go to when used secret exit
	EMapType	maptype;



	uint64_t	ShaderStartTime = 0;	// tell the shader system when we started the level (forces a timer restart)

	static const int BODYQUESIZE = 32;
	TObjPtr<AActor*> bodyque[BODYQUESIZE];
	int bodyqueslot;

	int NumMapSections;

	uint32_t		flags;
	uint32_t		flags2;
	uint32_t		flags3;

	uint32_t		fadeto;					// The color the palette fades to (usually black)
	uint32_t		outsidefog;				// The fog for sectors with sky ceilings

	uint32_t		hazardcolor;			// what color strife hazard blends the screen color as
	uint32_t		hazardflash;			// what color strife hazard flashes the screen color as

	FString		Music;
	int			musicorder;

	FTextureID	skytexture1;
	FTextureID	skytexture2;

	float		skyspeed1;				// Scrolling speed of sky textures, in pixels per ms
	float		skyspeed2;

	double		sky1pos, sky2pos;
	float		hw_sky1pos, hw_sky2pos;
	bool		skystretch;

	int			total_secrets;
	int			found_secrets;

	int			total_items;
	int			found_items;

	int			total_monsters;
	int			killed_monsters;

	double		gravity;
	double		aircontrol;
	double		airfriction;
	int			airsupply;
	int			DefaultEnvironment;		// Default sound environment.

	uint8_t freeze;						//Game in freeze mode.
	uint8_t changefreeze;				//Game wants to change freeze mode.

	FInterpolator interpolator;

	int ActiveSequences;
	DSeqNode *SequenceListHead;

	// [RH] particle globals
	uint32_t			ActiveParticles;
	uint32_t			InactiveParticles;
	TArray<particle_t>	Particles;
	TArray<uint16_t>	ParticlesInSubsec;

	TArray<DVector2>	Scrolls;		// NULL if no DScrollers in this level

	int8_t		WallVertLight;			// Light diffs for vert/horiz walls
	int8_t		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot
	bool		HasHeightSecs;			// true if some Transfer_Heights effects are present in the map. If this is false, some checks in the renderer can be shortcut.
	bool		HasDynamicLights;		// Another render optimization for maps with no lights at all.

	double		teamdamage;

	// former OpenGL-exclusive properties that should also be usable by the true color software renderer.
	int fogdensity;
	int outsidefogdensity;
	int skyfog;

	FName		deathsequence;
	float		pixelstretch;

	// Hardware render stuff that can either be set via CVAR or MAPINFO
	ELightMode	lightMode;
	bool		brightfog;
	bool		lightadditivesurfaces;
	bool		notexturefill;
	int			ImpactDecalCount;

	FDynamicLight *lights;

	// links to global game objects
	TArray<TObjPtr<AActor *>> CorpseQueue;
	TObjPtr<DFraggleThinker *> FraggleScriptThinker = nullptr;
	TObjPtr<DACSThinker*> ACSThinker = nullptr;

	TObjPtr<DSpotState *> SpotState = nullptr;

	// scale on entry
	static const int AM_NUMMARKPOINTS = 10;

	struct mpoint_t
	{
		double x, y;
	};

	// used by MTOF to scale from map-to-frame-buffer coords
	double am_scale_mtof = 0.2;
	// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
	double am_scale_ftom = 1 / 0.2;
	mpoint_t am_markpoints[AM_NUMMARKPOINTS]; // where the points are
	int am_markpointnum = 0; // next point to be assigned

// translates between frame-buffer and map distances
	inline double FTOM(double x)
	{
		return x * am_scale_ftom;
	}

	inline double MTOF(double x)
	{
		return x * am_scale_mtof;
	}


	bool		IsJumpingAllowed() const;
	bool		IsCrouchingAllowed() const;
	bool		IsFreelookAllowed() const;

	node_t		*HeadNode() const
	{
		return nodes.Size() == 0 ? nullptr : &nodes[nodes.Size() - 1];
	}
	node_t		*HeadGamenode() const
	{
		return headgamenode;
	}

	// Returns true if level is loaded from saved game or is being revisited as a part of a hub
	bool		IsReentering() const
	{
		// This is actually very simple: A reentered map never has a map time of 0 when it starts. If the map time is 0 it must be a freshly loaded instance.
		return maptime > 0;
	}
};

//==========================================================================
//
// Player is leaving the current level
//
//==========================================================================

struct FHubInfo
{
	int			levelnum;

	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	wbplayerstruct_t	plyr[MAXPLAYERS];

	FHubInfo &operator=(const wbstartstruct_t &wbs)
	{
		levelnum = wbs.finished_ep;
		maxkills = wbs.maxkills;
		maxsecret = wbs.maxsecret;
		maxitems = wbs.maxitems;
		maxfrags = wbs.maxfrags;
		memcpy(plyr, wbs.plyr, sizeof(plyr));
		return *this;
	}
};


// This struct is used to track statistics data in game
struct OneLevel
{
	int totalkills, killcount;
	int totalitems, itemcount;
	int totalsecrets, secretcount;
	int leveltime;
	FString Levelname;
};


extern FLevelLocals emptyLevelPlaceholderForZScript;
extern FLevelLocals *levelForZScript;

class FGameSession
{
public:
	TDeletingArray<FLevelLocals *> Levelinfo;
	
	TMap<FName, FCompressedBuffer> Snapshots;
	TMap<FName, TArray<acsdefered_t>> DeferredScripts;
	TMap<FName, bool> Visited;
	TArray<FHubInfo> hubdata;
	TArray<OneLevel> Statistics;// Current game's statistics
	int SinglePlayerClass[MAXPLAYERS];

	FString F1Pic;
	float		MusicVolume = 1.0f;
	int			time = 0;			// time in the hub
	int			totaltime = 0;		// time in the game
	int			frozenstate = 0;
	int			changefreeze = 0;
	int			NextSkill = -1;

	FString	nextlevel;		// Level to go to on exit
	int		nextstartpos = 0;	// [RH] Support for multiple starts per level


	void SetMusicVolume(float vol);
	void LeavingHub(int mode, cluster_info_t * cluster, wbstartstruct_t * wbs, FLevelLocals *Level);
	void InitPlayerClasses();

	void Reset()
	{
		levelForZScript = &emptyLevelPlaceholderForZScript;
		Levelinfo.DeleteAndClear();
		ClearSnapshots();
		DeferredScripts.Clear();
		Visited.Clear();
		hubdata.Clear();
		Statistics.Clear();

		MusicVolume = 1.0f;
		time = 0;
		totaltime = 0;
		frozenstate = 0;
		changefreeze = 0;

		nextlevel = "";
		nextstartpos = 0;
	}
	int isFrozen() const
	{
		return frozenstate;
	}
	bool isValid();
	FString LookupLevelName ();
	void ClearSnapshots()
	{
		decltype(Snapshots)::Iterator it(Snapshots);
		decltype(Snapshots)::Pair *pair;
		while (it.NextPair(pair))
		{
			pair->Value.Clean();
		}

		Snapshots.Clear();
	}
	void RemoveSnapshot(FName mapname)
	{
		auto snapshot = Snapshots.CheckKey(mapname);
		if (snapshot)
		{
			snapshot->Clean();
		}
		Snapshots.Remove(mapname);
	}
	void SerializeSession(FSerializer &arc);
	void SerializeACSDefereds(FSerializer &arc);
	void SerializeVisited(FSerializer &arc);

};

extern FGameSession *currentSession;

inline FSectorPortal *line_t::GetTransferredPortal()
{
	auto Level = GetLevel();
	return portaltransferred >= Level->sectorPortals.Size() ? (FSectorPortal*)nullptr : &Level->sectorPortals[portaltransferred];
}

inline FSectorPortal *sector_t::GetPortal(int plane)
{
	return &Level->sectorPortals[Portals[plane]];
}

inline double sector_t::GetPortalPlaneZ(int plane)
{
	return Level->sectorPortals[Portals[plane]].mPlaneZ;
}

inline DVector2 sector_t::GetPortalDisplacement(int plane)
{
	return Level->sectorPortals[Portals[plane]].mDisplacement;
}

inline int sector_t::GetPortalType(int plane)
{
	return Level->sectorPortals[Portals[plane]].mType;
}

inline int sector_t::GetOppositePortalGroup(int plane)
{
	return Level->sectorPortals[Portals[plane]].mDestination->PortalGroup;
}

inline bool sector_t::PortalBlocksView(int plane)
{
	if (GetPortalType(plane) != PORTS_LINKEDPORTAL) return false;
	return !!(planes[plane].Flags & (PLANEF_NORENDER | PLANEF_DISABLED | PLANEF_OBSTRUCTED));
}

inline bool sector_t::PortalBlocksSight(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NORENDER | PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalBlocksMovement(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalBlocksSound(int plane)
{
	return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_BLOCKSOUND | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
}

inline bool sector_t::PortalIsLinked(int plane)
{
	return (GetPortalType(plane) == PORTS_LINKEDPORTAL);
}

inline FLinePortal *line_t::getPortal() const
{
	auto Level = GetLevel();
	return portalindex >= Level->linePortals.Size() ? (FLinePortal*)NULL : &Level->linePortals[portalindex];
}

// returns true if the portal is crossable by actors
inline bool line_t::isLinePortal() const
{
	auto Level = GetLevel();
	return portalindex >= Level->linePortals.Size() ? false : !!(Level->linePortals[portalindex].mFlags & PORTF_PASSABLE);
}

// returns true if the portal needs to be handled by the renderer
inline bool line_t::isVisualPortal() const
{
	auto Level = GetLevel();
	return portalindex >= Level->linePortals.Size() ? false : !!(Level->linePortals[portalindex].mFlags & PORTF_VISIBLE);
}

inline line_t *line_t::getPortalDestination() const
{
	auto Level = GetLevel();
	return portalindex >= Level->linePortals.Size() ? (line_t*)NULL : Level->linePortals[portalindex].mDestination;
}

inline int line_t::getPortalAlignment() const
{
	auto Level = GetLevel();
	return portalindex >= Level->linePortals.Size() ? 0 : Level->linePortals[portalindex].mAlign;
}

inline bool line_t::hitSkyWall(AActor* mo) const
{
	return backsector &&
		backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
		mo->Z() >= backsector->ceilingplane.ZatPoint(mo->PosRelative(this));
}

// For handling CVARs that alter level settings.
// If we add multi-level handling later they need to be able to adjust and with a function like this the change can be done right now.
template<class T>
inline void ForAllLevels(T func)
{
	if (currentSession)
	{
		for (auto Level : currentSession->Levelinfo) func(Level);
	}
}

FSerializer &Serialize(FSerializer &arc, const char *key, wbplayerstruct_t &h, wbplayerstruct_t *def);
FSerializer &Serialize(FSerializer &arc, const char *key, FHubInfo &h, FHubInfo *def);