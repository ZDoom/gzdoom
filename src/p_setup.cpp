//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Do all the WAD I/O, get map description,
//		set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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


#include <math.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "templates.h"
#include "d_player.h"
#include "m_argv.h"
#include "g_game.h"
#include "w_wad.h"
#include "p_local.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "nodebuild.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "p_acs.h"
#include "announcer.h"
#include "wi_stuff.h"
#include "doomerrors.h"
#include "gi.h"
#include "p_conversation.h"
#include "a_keys.h"
#include "s_sndseq.h"
#include "sbar.h"
#include "p_setup.h"
#include "r_data/r_interpolate.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "md5.h"
#include "compatibility.h"
#include "po_man.h"
#include "r_renderer.h"
#include "p_blockmap.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "c_dispatch.h"
#include "a_dynlight.h"
#ifndef NO_EDATA
#include "edata.h"
#endif
#include "events.h"
#include "p_destructible.h"
#include "types.h"
#include "i_time.h"
#include "scripting/vm/vm.h"
#include "hwrenderer/data/flatvertices.h"
#include "maploader/maploader.h"
#include "maploader/mapdata.h"

#include "fragglescript/t_fs.h"

#define MISSING_TEXTURE_WARN_LIMIT		20

void BloodCrypt (void *data, int key, int len);
void P_ClearUDMFKeys();
void InitRenderInfo();


extern void P_TranslateTeleportThings (void);

extern int numinterpolations;
extern unsigned int R_OldBlend;

EXTERN_CVAR(Bool, am_textured)

CVAR (Bool, gennodes, false, CVAR_SERVERINFO|CVAR_GLOBALCONFIG);
CVAR (Bool, showloadtimes, false, 0);

static void P_Shutdown ();

inline bool P_IsBuildMap(MapData *map)
{
	return false;
}

inline bool P_LoadBuildMap(uint8_t *mapdata, size_t len, FMapThing **things, int *numthings)
{
	return false;
}

void P_SpawnThings (TArray<FMapThing> &MapThingsConverted, TArray<FUDMFKey> &MapThingsUserData, TMap<unsigned,unsigned>  &MapThingsUserDataIndex, int position);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
TArray<vertexdata_t> vertexdatas;

bool			hasglnodes;

static void P_AllocateSideDefs (MapData *map, int count);

//===========================================================================
//
//
//
//===========================================================================

void P_LoadBehavior(MapData * map)
{
	if (map->Size(ML_BEHAVIOR) > 0)
	{
		FBehavior::StaticLoadModule(-1, &map->Reader(ML_BEHAVIOR), map->Size(ML_BEHAVIOR));
	}
	if (!FBehavior::StaticCheckAllGood())
	{
		Printf("ACS scripts unloaded.\n");
		FBehavior::StaticUnloadModules();
	}
}

//===========================================================================
//
// P_PrecacheLevel
//
// Preloads all relevant graphics for the level.
//
//===========================================================================
void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist);

static void P_PrecacheLevel()
{
	int i;
	uint8_t *hitlist;
	TMap<PClassActor *, bool> actorhitlist;
	int cnt = TexMan.NumTextures();

	if (demoplayback)
		return;

	hitlist = new uint8_t[cnt];
	memset(hitlist, 0, cnt);

	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ((actor = iterator.Next()))
	{
		actorhitlist[actor->GetClass()] = true;
	}

	for (auto n : gameinfo.PrecachedClasses)
	{
		PClassActor *cls = PClass::FindActor(n);
		if (cls != NULL) actorhitlist[cls] = true;
	}
	for (unsigned i = 0; i < level.info->PrecacheClasses.Size(); i++)
	{
		// level.info can only store names, no class pointers.
		PClassActor *cls = PClass::FindActor(level.info->PrecacheClasses[i]);
		if (cls != NULL) actorhitlist[cls] = true;
	}

	for (i = level.sectors.Size() - 1; i >= 0; i--)
	{
		hitlist[level.sectors[i].GetTexture(sector_t::floor).GetIndex()] |= FTextureManager::HIT_Flat;
		hitlist[level.sectors[i].GetTexture(sector_t::ceiling).GetIndex()] |= FTextureManager::HIT_Flat;
	}

	for (i = level.sides.Size() - 1; i >= 0; i--)
	{
		hitlist[level.sides[i].GetTexture(side_t::top).GetIndex()] |= FTextureManager::HIT_Wall;
		hitlist[level.sides[i].GetTexture(side_t::mid).GetIndex()] |= FTextureManager::HIT_Wall;
		hitlist[level.sides[i].GetTexture(side_t::bottom).GetIndex()] |= FTextureManager::HIT_Wall;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.

	if (sky1texture.isValid())
	{
		hitlist[sky1texture.GetIndex()] |= FTextureManager::HIT_Sky;
	}
	if (sky2texture.isValid())
	{
		hitlist[sky2texture.GetIndex()] |= FTextureManager::HIT_Sky;
	}

	for (auto n : gameinfo.PrecachedTextures)
	{
		FTextureID tex = TexMan.CheckForTexture(n, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) hitlist[tex.GetIndex()] |= FTextureManager::HIT_Wall;
	}
	for (unsigned i = 0; i < level.info->PrecacheTextures.Size(); i++)
	{
		FTextureID tex = TexMan.CheckForTexture(level.info->PrecacheTextures[i], ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) hitlist[tex.GetIndex()] |= FTextureManager::HIT_Wall;
	}

	// This is just a temporary solution, until the hardware renderer's texture manager is in a better state.
	if (!V_IsHardwareRenderer())
		SWRenderer->Precache(hitlist, actorhitlist);
	else
		hw_PrecacheTexture(hitlist, actorhitlist);

	delete[] hitlist;
}

extern polyblock_t **PolyBlockMap;


//==========================================================================
//
//
//
//==========================================================================

void P_FreeLevelData ()
{
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	auto mo = it.Next();
	while (mo)
	{
		auto next = it.Next();
		mo->Destroy();
		mo = next;
	}

	// [ZZ] delete per-map event handlers
	E_Shutdown(true);
	FCanvasTextureInfo::EmptyList();
	R_FreePastViewers();
	P_ClearUDMFKeys();

	// [RH] Clear all ThingID hash chains.
	AActor::ClearTIDHashes();

	interpolator.ClearInterpolations();	// [RH] Nothing to interpolate on a fresh level.
	FPolyObj::ClearAllSubsectorLinks(); // can't be done as part of the polyobj deletion process.
	SN_StopAllSequences ();
	DThinker::DestroyAllThinkers ();
	P_ClearPortals();
	tagManager.Clear();
	level.total_monsters = level.total_items = level.total_secrets =
		level.killed_monsters = level.found_items = level.found_secrets =
		wminfo.maxfrags = 0;
		
	// delete allocated data in the level arrays.
	if (level.sectors.Size() > 0)
	{
		delete[] level.sectors[0].e;
		if (level.sectors[0].subsectors)
		{
			delete[] level.sectors[0].subsectors;
			level.sectors[0].subsectors = nullptr;
		}
	}
	for (auto &sub : level.subsectors)
	{
		if (sub.BSP != nullptr) delete sub.BSP;
	}
	if (level.sides.Size() > 0 && level.sides[0].segs)
	{
		delete[] level.sides[0].segs;
		level.sides[0].segs = nullptr;
	}


	FBehavior::StaticUnloadModules ();
	level.sections.Clear();
	level.segs.Clear();
	level.sectors.Clear();
	level.lines.Clear();
	level.sides.Clear();
	level.loadsectors.Clear();
	level.loadlines.Clear();
	level.loadsides.Clear();
	level.vertexes.Clear();
	level.nodes.Clear();
	level.gamenodes.Reset();
	level.subsectors.Clear();
	level.gamesubsectors.Reset();
	level.rejectmatrix.Clear();
	level.Zones.Clear();
	level.blockmap.Clear();

	if (PolyBlockMap != NULL)
	{
		for (int i = level.blockmap.bmapwidth*level.blockmap.bmapheight-1; i >= 0; --i)
		{
			polyblock_t *link = PolyBlockMap[i];
			while (link != NULL)
			{
				polyblock_t *next = link->next;
				delete link;
				link = next;
			}
		}
		delete[] PolyBlockMap;
		PolyBlockMap = NULL;
	}
	if (polyobjs != NULL)
	{
		delete[] polyobjs;
		polyobjs = NULL;
	}
	po_NumPolyobjs = 0;

	level.deathmatchstarts.Clear();
	level.AllPlayerStarts.Clear();
	memset(level.playerstarts, 0, sizeof(level.playerstarts));

	P_FreeStrifeConversations ();
	level.Scrolls.Clear();
	P_ClearUDMFKeys();
}

//===========================================================================
//
//
//
//===========================================================================

extern FMemArena secnodearena;
extern msecnode_t *headsecnode;

void P_FreeExtraLevelData()
{
	// Free all blocknodes and msecnodes.
	// *NEVER* call this function without calling
	// P_FreeLevelData() first, or they might not all be freed.
	{
		FBlockNode *node = FBlockNode::FreeBlocks;
		while (node != NULL)
		{
			FBlockNode *next = node->NextBlock;
			delete node;
			node = next;
		}
		FBlockNode::FreeBlocks = NULL;
	}
	secnodearena.FreeAllBlocks();
	headsecnode = nullptr;
}


//===========================================================================
//
// P_SetupLevel
//
// [RH] position indicates the start spot to spawn at
//
//===========================================================================

void P_SetupLevel (const char *lumpname, int position, bool newGame)
{
	cycle_t times[20];
	int i;

	level.ShaderStartTime = I_msTimeFS(); // indicate to the shader system that the level just started

	// This is motivated as follows:

	bool RequireGLNodes = true;	// Even the software renderer needs GL nodes now.

	for (i = 0; i < (int)countof(times); ++i)
	{
		times[i].Reset();
	}

	level.maptype = MAPTYPE_UNKNOWN;
	wminfo.partime = 180;

	if (!savegamerestore)
	{
		level.SetMusicVolume(level.MusicVolume);
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].killcount = players[i].secretcount 
				= players[i].itemcount = 0;
		}
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		players[i].mo = NULL;
	}
	// [RH] Clear any scripted translation colors the previous level may have set.
	for (i = 0; i < int(translationtables[TRANSLATION_LevelScripted].Size()); ++i)
	{
		FRemapTable *table = translationtables[TRANSLATION_LevelScripted][i];
		if (table != NULL)
		{
			delete table;
			translationtables[TRANSLATION_LevelScripted][i] = NULL;
		}
	}
	translationtables[TRANSLATION_LevelScripted].Clear();

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer].viewz = NO_VALUE; 

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start ();

	// [RH] clear out the mid-screen message
	C_MidPrint (NULL, NULL);

	// Free all level data from the previous map
	P_FreeLevelData ();
	vertexdatas.Clear();
	vertexdatas.ShrinkToFit();

	MapData *map = P_OpenMapData(lumpname, true);
	if (map == NULL)
	{
		I_Error("Unable to open map '%s'\n", lumpname);
	}

	// [ZZ] init per-map static handlers. we need to call this before everything is set up because otherwise scripts don't receive PlayerEntered event
	//      (which happens at god-knows-what stage in this function, but definitely not the last part, because otherwise it'd work to put E_InitStaticHandlers before the player spawning)
	E_InitStaticHandlers(true);

	// generate a checksum for the level, to be included and checked with savegames.
	map->GetChecksum(level.md5);
	// find map num
	level.lumpnum = map->lumpnum;
	hasglnodes = true;

	if (newGame)
	{
		E_NewGame(EventHandlerType::PerMap);
	}
	FBehavior::StaticUnloadModules ();

	if (!map->HasBehavior || map->isText)
	{
		// Doom format and UDMF text maps get strict monster activation unless the mapinfo
		// specifies differently.
		if (!(level.flags2 & LEVEL2_LAXACTIVATIONMAPINFO))
		{
			level.flags2 &= ~LEVEL2_LAXMONSTERACTIVATION;
		}
	}

	if (!map->HasBehavior && !map->isText)
	{
		// set compatibility flags
		if (gameinfo.gametype == GAME_Strife)
		{
			level.flags2 |= LEVEL2_RAILINGHACK;
		}
		level.flags2 |= LEVEL2_DUMMYSWITCHES;
	}
	


	// note: most of this ordering is important 

	// [RH] Load in the BEHAVIOR lump
	if (map->HasBehavior)
	{
		P_LoadBehavior (map);
		level.maptype = MAPTYPE_HEXEN;
	}
	else
	{
		// We need translators only for Doom format maps.
		const char *translator;

		if (!level.info->Translator.IsEmpty())
		{
			// The map defines its own translator.
			translator = level.info->Translator.GetChars();
		}
		else
		{
			// Has the user overridden the game's default translator with a commandline parameter?
			translator = Args->CheckValue("-xlat");
			if (translator == NULL) 
			{
				// Use the game's default.
				translator = gameinfo.translator.GetChars();
			}
		}
		P_LoadTranslator(translator);
		level.maptype = MAPTYPE_DOOM;
	}
	if (map->isText)
	{
		level.maptype = MAPTYPE_UDMF;
	}
	FName checksum = CheckCompatibility(map);
    
	T_LoadScripts(map);


	FBehavior::StaticLoadDefaultModules ();
#ifndef NO_EDATA
	LoadMapinfoACSLump();
#endif


	P_LoadStrifeConversations (map, lumpname);
    
    // ----------------------------------------->

    MapLoader maploader(level, &tagManager);
    maploader.maptype = level.maptype;
    maploader.ForceNodeBuild = gennodes;
	maploader.level = &level;
    
    if (ib_compatflags & BCOMPATF_REBUILDNODES)
    {
        maploader.ForceNodeBuild = true;
    }


	FMissingTextureTracker missingtex;

	times[0].Clock();
	if (!map->isText)
	{
		maploader.LoadVertexes (map);
		maploader.LoadSectors (map, missingtex);

		if (!map->HasBehavior)
			maploader.LoadLineDefs (map);
		else
			maploader.LoadLineDefs2 (map);	// [RH] Load Hexen-style linedefs

		maploader.LoadSideDefs2 (map, missingtex);
		maploader.FinishLoadingLineDefs ();

		if (!map->HasBehavior)
			maploader.LoadThings (map);
		else
			maploader.LoadThings2 (map);	// [RH] Load Hexen-style things
	}
	else
	{
		maploader.ParseTextMap(map, missingtex);
	}
	times[0].Unclock();

	maploader.SetCompatibilityParams(checksum);

	maploader.LoopSidedefs (true);

    maploader.SummarizeMissingTextures(missingtex);

    bool reloop = maploader.LoadBsp(map);

	maploader.LoadBlockMap (map);

	maploader.LoadReject (map);

	maploader.GroupLines (false);
	maploader.SetSlopes();

	maploader.FloodZones ();

	maploader.SetRenderSector();
	maploader.FixMinisegReferences();
	maploader.FixHoles();


	// [RH] Spawn slope creating things first.
	maploader.SpawnSlopeMakers();
	maploader.CopySlopes();
	
	//////////////////////// <-------------------------------------

    // set the head node for gameplay purposes. If the separate gamenodes array is not empty, use that, otherwise use the render nodes.
    level.headgamenode = level.gamenodes.Size() > 0 ? &level.gamenodes[level.gamenodes.Size() - 1] : level.nodes.Size()? &level.nodes[level.nodes.Size() - 1] : nullptr;

	// phares 8/10/98: Clear body queue so the corpses from previous games are
	// not assumed to be from this one.
	bodyqueslot = 0;
	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	CreateSections(level.sections);
	
	// Spawn 3d floors - must be done before spawning things so it can't be done in P_SpawnSpecials
	P_Spawn3DFloors();

	times[14].Clock();
	P_SpawnThings(maploader.MapThingsConverted, maploader.MapThingsUserData, maploader.MapThingsUserDataIndex, position);

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo != NULL)
			players[i].health = players[i].mo->health;
	}
	times[14].Unclock();

	times[15].Clock();
	if (!map->HasBehavior && !map->isText)
		P_TranslateTeleportThings ();	// [RH] Assign teleport destination TIDs
	times[15].Unclock();

	delete map;

	// set up world state
	P_SpawnSpecials ();

	// disable reflective planes on sloped sectors.
	for (auto &sec : level.sectors)
	{
		if (sec.floorplane.isSlope()) sec.reflect[sector_t::floor] = 0;
		if (sec.ceilingplane.isSlope()) sec.reflect[sector_t::ceiling] = 0;
	}
	for (auto &node : level.nodes)
	{
		double fdx = FIXED2DBL(node.dx);
		double fdy = FIXED2DBL(node.dy);
		node.len = (float)g_sqrt(fdx * fdx + fdy * fdy);
	}

	// This must be done BEFORE the PolyObj Spawn!!!
	InitRenderInfo();			// create hardware independent renderer resources for the level.
	screen->mVertexData->CreateVBO();
	SWRenderer->SetColormap();	//The SW renderer needs to do some special setup for the level's default colormap.
	InitPortalGroups();
	P_InitHealthGroups();

	times[16].Clock();
	if (reloop) maploader.LoopSidedefs (false);
	PO_Init (maploader.sidetemp.Data());				// Initialize the polyobjs
	if (!level.IsReentering())
		P_FinalizePortals();	// finalize line portals after polyobjects have been initialized. This info is needed for properly flagging them.
	times[16].Unclock();


	// if deathmatch, randomly spawn the active players
	if (deathmatch)
	{
		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (playeringame[i])
			{
				players[i].mo = NULL;
				G_DeathMatchSpawnPlayer (i);
			}
		}
	}
	// the same, but for random single/coop player starts
	else if (level.flags2 & LEVEL2_RANDOMPLAYERSTARTS)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				players[i].mo = NULL;
				FPlayerStart *mthing = G_PickPlayerStart(i);
				P_SpawnPlayer(mthing, i, (level.flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
			}
		}
	}

	// [SP] move unfriendly players around
	// horribly hacky - yes, this needs rewritten.
	if (level.deathmatchstarts.Size () > 0)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != NULL)
			{
				if (!(players[i].mo->flags & MF_FRIENDLY))
				{
					AActor * oldSpawn = players[i].mo;
					G_DeathMatchSpawnPlayer (i);
					oldSpawn->Destroy();
				}
			}
		}
	}


	// Don't count monsters in end-of-level sectors if option is on
	if (dmflags2 & DF2_NOCOUNTENDMONST)
	{
		TThinkerIterator<AActor> it;
		AActor * mo;

		while ((mo=it.Next()))
		{
			if (mo->flags & MF_COUNTKILL)
			{
				if (mo->Sector->damageamount > 0 && (mo->Sector->Flags & (SECF_ENDGODMODE|SECF_ENDLEVEL)) == (SECF_ENDGODMODE|SECF_ENDLEVEL))
				{
					mo->ClearCounters();
				}
			}
		}
	}

	T_PreprocessScripts();        // preprocess FraggleScript scripts

	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	R_OldBlend = 0xffffffff;

	// [RH] Remove all particles
	P_ClearParticles ();

	times[17].Clock();
	// preload graphics and sounds
	if (precache)
	{
		P_PrecacheLevel ();
		S_PrecacheLevel ();
	}
	times[17].Unclock();

	if (deathmatch)
	{
		AnnounceGameStart ();
	}

	// This check was previously done at run time each time the heightsec was checked.
	// However, since 3D floors are static data, we can easily precalculate this and store it in the sector's flags for quick access.
	for (auto &s : level.sectors)
	{
		if (s.heightsec != nullptr)
		{
			// If any of these 3D floors render their planes, ignore heightsec.
			for (auto &ff : s.e->XFloor.ffloors)
			{
				if ((ff->flags & (FF_EXISTS | FF_RENDERPLANES)) == (FF_EXISTS | FF_RENDERPLANES))
				{
					s.MoreFlags |= SECMF_IGNOREHEIGHTSEC;	// mark the heightsec inactive.
				}
			}
		}
	}

	P_ResetSightCounters (true);
	//Printf ("free memory: 0x%x\n", Z_FreeMemory());

    /*
	if (showloadtimes)
	{
		Printf ("---Total load times---\n");
		for (i = 0; i < 18; ++i)
		{
			static const char *timenames[] =
			{
				"load vertexes",
				"load sectors",
				"load sides",
				"load lines",
				"load sides 2",
				"load lines 2",
				"loop sides",
				"load subsectors",
				"load nodes",
				"load segs",
				"load blockmap",
				"load reject",
				"group lines",
				"flood zones",
				"load things",
				"translate teleports",
				"init polys",
				"precache"
			};
			Printf ("Time%3d:%9.4f ms (%s)\n", i, times[i].TimeMS(), timenames[i]);
		}
	}
     */
    
	// Create a backup of the map data so the savegame code can toss out all fields that haven't changed in order to reduce processing time and file size.
	// Note that we want binary identity here, so assignment is not sufficient because it won't initialize any padding bytes.
	// Note that none of these structures may contain non POD fields anyway.
	level.loadsectors.Resize(level.sectors.Size());
	memcpy(&level.loadsectors[0], &level.sectors[0], level.sectors.Size() * sizeof(level.sectors[0]));
	level.loadlines.Resize(level.lines.Size());
	memcpy(&level.loadlines[0], &level.lines[0], level.lines.Size() * sizeof(level.lines[0]));
	level.loadsides.Resize(level.sides.Size());
	memcpy(&level.loadsides[0], &level.sides[0], level.sides.Size() * sizeof(level.sides[0]));
}



//
// P_Init
//
void P_Init ()
{
	atterm (P_Shutdown);

	P_InitEffects ();		// [RH]
	P_InitTerrainTypes ();
	P_InitKeyMessages ();
	R_InitSprites ();
}

static void P_Shutdown ()
{	
	DThinker::DestroyThinkersInList(STAT_STATIC);	
	P_DeinitKeyMessages ();
	P_FreeLevelData ();
	P_FreeExtraLevelData ();
	// [ZZ] delete global event handlers
	E_Shutdown(false);
	ST_Clear();
	FS_Close();
	for (auto &p : players)
	{
		if (p.psprites != nullptr) p.psprites->Destroy();
	}
}

#if 0
#include "c_dispatch.h"
CCMD (lineloc)
{
	if (argv.argc() != 2)
	{
		return;
	}
	int linenum = atoi (argv[1]);
	if (linenum < 0 || linenum >= numlines)
	{
		Printf ("No such line\n");
	}
	Printf ("(%f,%f) -> (%f,%f)\n", lines[linenum].v1->fX(),
		lines[linenum].v1->fY(),
		lines[linenum].v2->fX(),
		lines[linenum].v2->fY());
}
#endif

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	for (auto &sector : level.sectors)
	{
		Printf(PRINT_LOG, "Sector %d\n", sector.sectornum);
		for (int j = 0; j<sector.subsectorcount; j++)
		{
			subsector_t * sub = sector.subsectors[j];

			Printf(PRINT_LOG, "    Subsector %d - real sector = %d - %s\n", int(sub->Index()), sub->sector->sectornum, sub->hacked & 1 ? "hacked" : "");
			for (uint32_t k = 0; k<sub->numlines; k++)
			{
				seg_t * seg = sub->firstline + k;
				if (seg->linedef)
				{
					Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d",
						seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(),
						seg->Index(), seg->linedef->Index(), seg->sidedef != seg->linedef->sidedef[0]);
				}
				else
				{
					Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg",
						seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(), seg->Index());
				}
				if (seg->PartnerSeg)
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;
					Printf(PRINT_LOG, ", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
				}
				else if (seg->backsector)
				{
					Printf(PRINT_LOG, ", back sector = %d (no partnerseg)", seg->backsector->sectornum);
				}

				Printf(PRINT_LOG, "\n");
			}
		}
	}
}
