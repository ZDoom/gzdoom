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
#include "p_spec.h"
#include "actor.h"
#include "b_bot.h"
#include "p_effect.h"
#include "d_player.h"
#include "p_destructible.h"
#include "r_data/r_sections.h"
#include "r_data/r_canvastexture.h"
#include "r_data/r_interpolate.h"
#include "doom_aabbtree.h"

//============================================================================
//
// This is used to mark processed portals for some collection functions.
//
//============================================================================

struct FPortalBits
{
	TArray<uint32_t> data;

	void setSize(int num)
	{
		data.Resize((num + 31) / 32);
		clear();
	}

	void clear()
	{
		memset(&data[0], 0, data.Size() * sizeof(uint32_t));
	}

	void setBit(int group)
	{
		data[group >> 5] |= (1 << (group & 31));
	}

	int getBit(int group)
	{
		return data[group >> 5] & (1 << (group & 31));
	}
};

class DACSThinker;
class DFraggleThinker;
class DSpotState;
class DSeqNode;
struct FStrifeDialogueNode;
class DAutomapBase;
struct wbstartstruct_t;
class DSectorMarker;
struct FTranslator;
struct EventManager;

typedef TMap<int, int> FDialogueIDMap;				// maps dialogue IDs to dialogue array index (for ACS)
typedef TMap<FName, int> FDialogueMap;				// maps actor class names to dialogue array index
typedef TMap<int, FUDMFKeys> FUDMFKeyMap;

struct FLevelLocals
{
	void *level;
	void *Level;	// bug catchers.
	FLevelLocals();
	~FLevelLocals();

	void *operator new(size_t blocksize)
	{
		// Null the allocated memory before running the constructor.
		// If we later allocate secondary levels they need to behave exactly like a global variable, i.e. start nulled.
		auto block = ::operator new(blocksize);
		memset(block, 0, blocksize);
		return block;
	}


	friend class MapLoader;

	void Tick();
	void Mark();
	void AddScroller(int secnum);
	void SetInterMusic(const char *nextmap);
	void SetMusicVolume(float v);
	void ClearLevelData();
	void ClearPortals();
	bool CheckIfExitIsGood(AActor *self, level_info_t *newmap);
	void FormatMapName(FString &mapname, const char *mapnamecolor);
	void ClearAllSubsectorLinks();
	void TranslateLineDef (line_t *ld, maplinedef_t *mld, int lineindexforid = -1);
	int TranslateSectorSpecial(int special);
	bool IsTIDUsed(int tid);
	int FindUniqueTID(int start_tid, int limit);
	int GetConversation(int conv_id);
	int GetConversation(FName classname);
	void SetConversation(int convid, PClassActor *Class, int dlgindex);
	int FindNode (const FStrifeDialogueNode *node);
    int GetInfighting();
	void SetCompatLineOnSide(bool state);
	int GetCompatibility(int mask);
	int GetCompatibility2(int mask);
	void ApplyCompatibility();
	void ApplyCompatibility2();

	void Init();

private:
	bool ShouldDoIntermission(cluster_info_t* nextcluster, cluster_info_t* thiscluster);
	line_t *FindPortalDestination(line_t *src, int tag);
	void BuildPortalBlockmap();
	void UpdatePortal(FLinePortal *port);
	void CollectLinkedPortals();
	void CreateLinkedPortals();
	bool ChangePortalLine(line_t *line, int destid);
	void AddDisplacementForPortal(FSectorPortal *portal);
	void AddDisplacementForPortal(FLinePortal *portal);
	bool ConnectPortalGroups();

	void SerializePlayers(FSerializer &arc, bool skipload);
	void CopyPlayer(player_t *dst, player_t *src, const char *name);
	void ReadOnePlayer(FSerializer &arc, bool skipload);
	void ReadMultiplePlayers(FSerializer &arc, int numPlayers, int numPlayersNow, bool skipload);
	void SerializeSounds(FSerializer &arc);
	void PlayerSpawnPickClass (int playernum);

public:
	void SnapshotLevel();
	void UnSnapshotLevel(bool hubLoad);

	void FinalizePortals();
	bool ChangePortal(line_t *ln, int thisid, int destid);
	unsigned GetSkyboxPortal(AActor *actor);
	unsigned GetPortal(int type, int plane, sector_t *orgsec, sector_t *destsec, const DVector2 &displacement);
	unsigned GetStackPortal(AActor *point, int plane);
	DVector2 GetPortalOffsetPosition(double x, double y, double dx, double dy);
	bool CollectConnectedGroups(int startgroup, const DVector3 &position, double upperz, double checkradius, FPortalGroupArray &out);

	void ActivateInStasisPlat(int tag);
	bool CreateCeiling(sector_t *sec, DCeiling::ECeiling type, line_t *line, int tag, double speed, double speed2, double height, int crush, int silent, int change, DCeiling::ECrushMode hexencrush);
	void ActivateInStasisCeiling(int tag);
	bool CreateFloor(sector_t *sec, DFloor::EFloor floortype, line_t *line, double speed, double height, int crush, int change, bool hexencrush, bool hereticlower);
	void DoDeferedScripts();
	void AdjustPusher(int tag, int magnitude, int angle, bool wind);
	int Massacre(bool baddies = false, FName cls = NAME_None);
	AActor *SpawnMapThing(FMapThing *mthing, int position);
	AActor *SpawnMapThing(int index, FMapThing *mt, int position);
	AActor *SpawnPlayer(FPlayerStart *mthing, int playernum, int flags = 0);
	void StartLightning();
	void ForceLightning(int mode);
	void ClearDynamic3DFloorData();
	void WorldDone(void);
	void AirControlChanged();
	AActor *SelectTeleDest(int tid, int tag, bool norandom);
	bool AlignFlat(int linenum, int side, int fc);
	void ReplaceTextures(const char *fromname, const char *toname, int flags);

	bool EV_Thing_Spawn(int tid, AActor *source, int type, DAngle angle, bool fog, int newtid);
	bool EV_Thing_Move(int tid, AActor *source, int mapspot, bool fog);
	bool EV_Thing_Projectile(int tid, AActor *source, int type, const char *type_name, DAngle angle,
		double speed, double vspeed, int dest, AActor *forcedest, int gravity, int newtid, bool leadTarget);
	int EV_Thing_Damage(int tid, AActor *whofor0, int amount, FName type);

	bool EV_DoPlat(int tag, line_t *line, DPlat::EPlatType type, double height, double speed, int delay, int lip, int change);
	void EV_StopPlat(int tag, bool remove);
	bool EV_DoPillar(DPillar::EPillar type, line_t *line, int tag, double speed, double height, double height2, int crush, bool hexencrush);
	bool EV_DoDoor(DDoor::EVlDoor type, line_t *line, AActor *thing, int tag, double speed, int delay, int lock, int lightTag, bool boomgen = false, int topcountdown = 0);
	bool EV_SlidingDoor(line_t *line, AActor *thing, int tag, int speed, int delay, DAnimatedDoor::EADType type);
	bool EV_DoCeiling(DCeiling::ECeiling type, line_t *line, int tag, double speed, double speed2, double height, int crush, int silent, int change, DCeiling::ECrushMode hexencrush = DCeiling::ECrushMode::crushDoom);
	bool EV_CeilingCrushStop(int tag, bool remove);
	bool EV_StopCeiling(int tag, line_t *line);
	bool EV_BuildStairs(int tag, DFloor::EStair type, line_t *line, double stairsize, double speed, int delay, int reset, int igntxt, int usespecials);
	bool EV_DoFloor(DFloor::EFloor floortype, line_t *line, int tag, double speed, double height, int crush, int change, bool hexencrush, bool hereticlower = false);
	bool EV_FloorCrushStop(int tag, line_t *line);
	bool EV_StopFloor(int tag, line_t *line);
	bool EV_DoDonut(int tag, line_t *line, double pillarspeed, double slimespeed);
	bool EV_DoElevator(line_t *line, DElevator::EElevator type, double speed, double height, int tag);
	bool EV_StartWaggle(int tag, line_t *line, int height, int speed, int offset, int timer, bool ceiling);
	bool EV_DoChange(line_t *line, EChange changetype, int tag);

	void EV_StartLightFlickering(int tag, int upper, int lower);
	void EV_StartLightStrobing(int tag, int upper, int lower, int utics, int ltics);
	void EV_StartLightStrobing(int tag, int utics, int ltics);
	void EV_TurnTagLightsOff(int tag);
	void EV_LightTurnOn(int tag, int bright);
	void EV_LightTurnOnPartway(int tag, double frac);
	void EV_LightChange(int tag, int value);
	void EV_StartLightGlowing(int tag, int upper, int lower, int tics);
	void EV_StartLightFading(int tag, int value, int tics);
	void EV_StopLightEffect(int tag);

	bool EV_Teleport(int tid, int tag, line_t *line, int side, AActor *thing, int flags);
	bool EV_SilentLineTeleport(line_t *line, int side, AActor *thing, int id, INTBOOL reverse);
	bool EV_TeleportOther(int other_tid, int dest_tid, bool fog);
	bool EV_TeleportGroup(int group_tid, AActor *victim, int source_tid, int dest_tid, bool moveSource, bool fog);
	bool EV_TeleportSector(int tag, int source_tid, int dest_tid, bool fog, int group_tid);

	void RecalculateDrawnSubsectors();
	FSerializer &SerializeSubsectors(FSerializer &arc, const char *key);
	void SpawnExtraPlayers();
	void Serialize(FSerializer &arc, bool hubload);
	DThinker *FirstThinker (int statnum);

	// g_Game
	void PlayerReborn (int player);
	bool CheckSpot (int playernum, FPlayerStart *mthing);
	void DoReborn (int playernum, bool freshbot);
	void QueueBody (AActor *body);
	double PlayersRangeFromSpot (FPlayerStart *spot);
	FPlayerStart *SelectFarthestDeathmatchSpot (size_t selections);
	FPlayerStart *SelectRandomDeathmatchSpot (int playernum, unsigned int selections);
	void DeathMatchSpawnPlayer (int playernum);
	FPlayerStart *PickPlayerStart(int playernum, int flags = 0);
	bool DoCompleted(FString nextlevel, wbstartstruct_t &wminfo);
	void StartTravel();
	int FinishTravel();
	void ChangeLevel(const char *levelname, int position, int flags, int nextSkill = -1);
	const char *GetSecretExitMap();
	void ExitLevel(int position, bool keepFacing);
	void SecretExitLevel(int position);
	void DoLoadLevel(const FString &nextmapname, int position, bool autosave, bool newGame);

	void DeleteAllAttachedLights();
	void RecreateAllAttachedLights();


private:
	// Work data for CollectConnectedGroups.
	FPortalBits processMask;
	TArray<FLinePortal*> foundPortals;
	TArray<int> groupsToCheck;

public:

	FSectorTagIterator GetSectorTagIterator(int tag)
	{
		return FSectorTagIterator(tagManager, tag);
	}
	FSectorTagIterator GetSectorTagIterator(int tag, line_t *line)
	{
		return FSectorTagIterator(tagManager, tag, line);
	}
	FLineIdIterator GetLineIdIterator(int tag)
	{
		return FLineIdIterator(tagManager, tag);
	}
	template<class T> TThinkerIterator<T> GetThinkerIterator(FName subtype = NAME_None, int statnum = MAX_STATNUM+1)
	{
		if (subtype == NAME_None) return TThinkerIterator<T>(this, statnum);
		else return TThinkerIterator<T>(this, subtype, statnum);
	}
	template<class T> TThinkerIterator<T> GetThinkerIterator(FName subtype, int statnum, AActor *prev)
	{
		return TThinkerIterator<T>(this, subtype, statnum, prev);
	}
	FActorIterator GetActorIterator(int tid)
	{
		return FActorIterator(TIDHash, tid);
	}
	FActorIterator GetActorIterator(int tid, AActor *start)
	{
		return FActorIterator(TIDHash, tid, start);
	}
	NActorIterator GetActorIterator(FName type, int tid)
	{
		return NActorIterator(TIDHash, type, tid);
	}
	AActor *SingleActorFromTID(int tid, AActor *defactor)
	{
		return tid == 0 ? defactor : GetActorIterator(tid).Next();
	}

	bool SectorHasTags(sector_t *sector)
	{
		return tagManager.SectorHasTags(sector);
	}
	bool SectorHasTag(sector_t *sector, int tag)
	{
		return tagManager.SectorHasTag(sector, tag);
	}
	bool SectorHasTag(int sector, int tag)
	{
		return tagManager.SectorHasTag(sector, tag);
	}
	int GetFirstSectorTag(const sector_t *sect) const
	{
		return tagManager.GetFirstSectorTag(sect);
	}
	int GetFirstSectorTag(int i) const
	{
		return tagManager.GetFirstSectorTag(i);
	}
	int GetFirstLineId(const line_t *sect) const
	{
		return tagManager.GetFirstLineID(sect);
	}

	bool LineHasId(int line, int tag)
	{
		return tagManager.LineHasID(line, tag);
	}
	bool LineHasId(line_t *line, int tag)
	{
		return tagManager.LineHasID(line, tag);
	}

	int FindFirstSectorFromTag(int tag)
	{
		auto it = GetSectorTagIterator(tag);
		return it.Next();
	}
	
	int FindFirstLineFromID(int tag)
	{
		auto it = GetLineIdIterator(tag);
		return it.Next();
	}

	int isFrozen()
	{
		return frozenstate;
	}

private:	// The engine should never ever access subsectors of the game nodes. This is only needed for actually implementing PointInSector.
	subsector_t *PointInSubsector(double x, double y);
public:
	sector_t *PointInSectorBuggy(double x, double y);
	subsector_t *PointInRenderSubsector (fixed_t x, fixed_t y);

	sector_t *PointInSector(const DVector2 &pos)
	{
		return PointInSubsector(pos.X, pos.Y)->sector;
	}

	sector_t *PointInSector(double x, double y)
	{
		return PointInSubsector(x, y)->sector;
	}

	subsector_t *PointInRenderSubsector (const DVector2 &pos)
	{
		return PointInRenderSubsector(FloatToFixed(pos.X), FloatToFixed(pos.Y));
	}

	FPolyObj *GetPolyobj (int polyNum)
	{
		auto index = Polyobjects.FindEx([=](const auto &poly) { return poly.tag == polyNum; });
		return index == Polyobjects.Size()? nullptr : &Polyobjects[index];
	}


	void ClearTIDHashes ()
	{
		memset(TIDHash, 0, sizeof(TIDHash));
	}


	bool CheckReject(sector_t *s1, sector_t *s2)
	{
		if (rejectmatrix.Size() > 0)
		{
			int pnum = int(s1->Index()) * sectors.Size() + int(s2->Index());
			return !(rejectmatrix[pnum >> 3] & (1 << (pnum & 7)));
		}
		return true;
	}

	DThinker *CreateThinker(PClass *cls, int statnum = STAT_DEFAULT)
	{
		DThinker *thinker = static_cast<DThinker*>(cls->CreateNew());
		assert(thinker->IsKindOf(RUNTIME_CLASS(DThinker)));
		thinker->ObjectFlags |= OF_JustSpawned;
		Thinkers.Link(thinker, statnum);
		thinker->Level = this;
		return thinker;
	}

	template<typename T, typename... Args>
	T* CreateThinker(Args&&... args)
	{
		auto thinker = static_cast<T*>(CreateThinker(RUNTIME_CLASS(T), T::DEFAULT_STAT));
		thinker->Construct(std::forward<Args>(args)...);
		return thinker;
	}
	
	void SetMusic();


	TArray<vertex_t> vertexes;
	TArray<sector_t> sectors;
	TArray<extsector_t> extsectors; // container for non-trivial sector information. sector_t must be trivially copyable for *_fakeflat to work as intended.
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
	TArray<FSectorPortalGroup *> portalGroups;
	TArray<FLinePortalSpan> linePortalSpans;
	FSectionContainer sections;
	FCanvasTextureInfo canvasTextureInfo;
	EventManager *localEventManager = nullptr;
	DoomLevelAABBTree* aabbTree = nullptr;

	// [ZZ] Destructible geometry information
	TMap<int, FHealthGroup> healthGroups;

	FBlockmap blockmap;
	TArray<polyblock_t *> PolyBlockMap;
	FUDMFKeyMap UDMFKeys[4];

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
	AActor *TIDHash[128];

	TArray<FStrifeDialogueNode *> StrifeDialogues;
	FDialogueIDMap DialogueRoots;
	FDialogueMap ClassRoots;
	FCajunMaster BotInfo;

	int ii_compatflags = 0;
	int ii_compatflags2 = 0;
	int ib_compatflags = 0;
	int i_compatflags = 0;
	int i_compatflags2 = 0;

	DSectorMarker *SectorMarker;

	uint8_t		md5[16];			// for savegame validation. If the MD5 does not match the savegame won't be loaded.
	int			time;			// time in the hub
	int			maptime;			// time in the map
	int			totaltime;		// time in the game
	int			starttime;
	int			partime;
	int			sucktime;
	uint32_t	spawnindex;

	level_info_t *info;
	int			cluster;
	int			clusterflags;
	int			levelnum;
	int			lumpnum;
	FString		LevelName;
	FString		MapName;			// the lump name (E1M1, MAP01, etc)
	FString		NextMap;			// go here when using the regular exit
	FString		NextSecretMap;		// map to go to when used secret exit
	FString		AuthorName;
	FString		F1Pic;
	FTranslator *Translator;
	EMapType	maptype;
	FTagManager tagManager;
	FInterpolator interpolator;

	uint64_t	ShaderStartTime = 0;	// tell the shader system when we started the level (forces a timer restart)

	static const int BODYQUESIZE = 32;
	TObjPtr<AActor*> bodyque[BODYQUESIZE];
	TObjPtr<DAutomapBase*> automap = nullptr;
	int bodyqueslot;
	
	// For now this merely points to the global player array, but with this in place, access to this array can be moved over to the level.
	// As things progress each level needs to be able to point to different players, even if they are just null if the second level is merely a skybox or camera target.
	// But even if it got a real player, the level will not own it - the player merely links to the level.
	// This should also be made a real object eventually.
	player_t *Players[MAXPLAYERS];
	
	// This is to allow refactoring without refactoring the data right away.
	bool PlayerInGame(int pnum)
	{
		return playeringame[pnum];
	}
	
	// This needs to be done better, but for now it should be good enough.
	bool PlayerInGame(player_t *player)
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (player == Players[i]) return PlayerInGame(i);
		}
		return false;
	}

	int PlayerNum(player_t *player)
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (player == Players[i]) return i;
		}
		return -1;
	}
	
	bool isPrimaryLevel() const
	{
		return true;
	}
	
	// Gets the console player without having the calling code be aware of the level's state.
	player_t *GetConsolePlayer() const
	{
		return isPrimaryLevel()? Players[consoleplayer] : nullptr;
	}
	
	bool isConsolePlayer(AActor *mo) const
	{
		auto p = GetConsolePlayer();
		if (!p) return false;
		return p->mo == mo;
	}

	bool isCamera(AActor *mo) const
	{
		auto p = GetConsolePlayer();
		if (!p) return false;
		return p->camera == mo;
	}

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
	int			cdtrack;
	unsigned int cdid;
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

	double      max_velocity;
	double      avg_velocity;

	double		gravity;
	double		aircontrol;
	double		airfriction;
	int			airsupply;
	int			DefaultEnvironment;		// Default sound environment.

	DSeqNode *SequenceListHead;

	// [RH] particle globals
	uint32_t			ActiveParticles;
	uint32_t			InactiveParticles;
	TArray<particle_t>	Particles;
	TArray<uint16_t>	ParticlesInSubsec;
	FThinkerCollection Thinkers;

	TArray<DVector2>	Scrolls;		// NULL if no DScrollers in this level

	int8_t		WallVertLight;			// Light diffs for vert/horiz walls
	int8_t		WallHorizLight;

	bool		FromSnapshot;			// The current map was restored from a snapshot
	bool		HasHeightSecs;			// true if some Transfer_Heights effects are present in the map. If this is false, some checks in the renderer can be shortcut.
	bool		HasDynamicLights;		// Another render optimization for maps with no lights at all.
	int		frozenstate;

	double		teamdamage;

	// former OpenGL-exclusive properties that should also be usable by the true color software renderer.
	int fogdensity;
	int outsidefogdensity;
	int skyfog;

	FName		deathsequence;
	float		pixelstretch;
	float		MusicVolume;

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

	//==========================================================================
	//
	//
	//==========================================================================

	bool IsJumpingAllowed() const
	{
		if (dmflags & DF_NO_JUMP)
			return false;
		if (dmflags & DF_YES_JUMP)
			return true;
		return !(flags & LEVEL_JUMP_NO);
	}

	//==========================================================================
	//
	//
	//==========================================================================

	bool IsCrouchingAllowed() const
	{
		if (dmflags & DF_NO_CROUCH)
			return false;
		if (dmflags & DF_YES_CROUCH)
			return true;
		return !(flags & LEVEL_CROUCH_NO);
	}

	//==========================================================================
	//
	//
	//==========================================================================

	bool IsFreelookAllowed() const
	{
		if (dmflags & DF_NO_FREELOOK)
			return false;
		if (dmflags & DF_YES_FREELOOK)
			return true;
		return !(flags & LEVEL_FREELOOK_NO);
	}

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
		return savegamerestore
			|| (info != nullptr && info->Snapshot.mBuffer != nullptr && info->isValid());
	}
};


extern FLevelLocals level;
extern FLevelLocals *primaryLevel;	// level for which to display the user interface. This will always be the one the current consoleplayer is in.
extern FLevelLocals *currentVMLevel;

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

inline FLevelLocals *line_t::GetLevel() const
{
	return frontsector->Level;
}
inline FLinePortal *line_t::getPortal() const
{
	return portalindex == UINT_MAX && portalindex >= GetLevel()->linePortals.Size() ? (FLinePortal*)nullptr : &GetLevel()->linePortals[portalindex];
}

// returns true if the portal is crossable by actors
inline bool line_t::isLinePortal() const
{
	return portalindex == UINT_MAX && portalindex >= GetLevel()->linePortals.Size() ? false : !!(GetLevel()->linePortals[portalindex].mFlags & PORTF_PASSABLE);
}

// returns true if the portal needs to be handled by the renderer
inline bool line_t::isVisualPortal() const
{
	return portalindex == UINT_MAX && portalindex >= GetLevel()->linePortals.Size() ? false : !!(GetLevel()->linePortals[portalindex].mFlags & PORTF_VISIBLE);
}

inline line_t *line_t::getPortalDestination() const
{
	return portalindex >= GetLevel()->linePortals.Size() ? (line_t*)nullptr : GetLevel()->linePortals[portalindex].mDestination;
}

inline int line_t::getPortalAlignment() const
{
	return portalindex >= GetLevel()->linePortals.Size() ? 0 : GetLevel()->linePortals[portalindex].mAlign;
}

inline bool line_t::hitSkyWall(AActor* mo) const
{
	return backsector &&
		backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
		mo->Z() >= backsector->ceilingplane.ZatPoint(mo->PosRelative(this));
}

// This must later be extended to return an array with all levels.
// It is meant for code that needs to iterate over all levels to make some global changes, e.g. configuation CCMDs.
inline TArrayView<FLevelLocals *> AllLevels()
{
	return TArrayView<FLevelLocals *>(&primaryLevel, 1);
}
