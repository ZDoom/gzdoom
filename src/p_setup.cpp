//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
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
//
//-----------------------------------------------------------------------------


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
#include "po_man.h"
#include "r_renderer.h"
#include "p_blockmap.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "c_dispatch.h"
#include "a_dynlight.h"
#include "events.h"
#include "p_destructible.h"
#include "types.h"
#include "i_time.h"
#include "scripting/vm/vm.h"
#include "a_specialspot.h"
#include "maploader/maploader.h"
#include "p_acs.h"
#include "fragglescript/t_script.h"

extern unsigned int R_OldBlend;

static void P_Shutdown ();

//===========================================================================
//
// P_PrecacheLevel
//
// Preloads all relevant graphics for the map.
//
//===========================================================================
void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist);

static void AddToList(uint8_t *hitlist, FTextureID texid, int bitmask)
{
	if (hitlist[texid.GetIndex()] & bitmask) return;	// already done, no need to process everything again.
	hitlist[texid.GetIndex()] |= (uint8_t)bitmask;

	for (auto anim : TexMan.mAnimations)
	{
		if (texid == anim->BasePic || (!anim->bDiscrete && anim->BasePic < texid && texid < anim->BasePic + anim->NumFrames))
		{
			for (int i = anim->BasePic.GetIndex(); i < anim->BasePic.GetIndex() + anim->NumFrames; i++)
			{
				hitlist[i] |= (uint8_t)bitmask;
			}
		}
	}

	auto switchdef = TexMan.FindSwitch(texid);
	if (switchdef)
	{
		for (int i = 0; i < switchdef->NumFrames; i++)
		{
			hitlist[switchdef->frames[i].Texture.GetIndex()] |= (uint8_t)bitmask;
		}
		for (int i = 0; i < switchdef->PairDef->NumFrames; i++)
		{
			hitlist[switchdef->PairDef->frames[i].Texture.GetIndex()] |= (uint8_t)bitmask;
		}
	}

	auto adoor = TexMan.FindAnimatedDoor(texid);
	if (adoor)
	{
		for (int i = 0; i < adoor->NumTextureFrames; i++)
		{
			hitlist[adoor->TextureFrames[i].GetIndex()] |= (uint8_t)bitmask;
		}
	}
}

static void PrecacheLevel(FLevelLocals *Level)
{
	if (demoplayback)
		return;

	int i;
	TMap<PClassActor *, bool> actorhitlist;
	int cnt = TexMan.NumTextures();
	TArray<uint8_t> hitlist(cnt, true);

	memset(hitlist.Data(), 0, cnt);

	AActor *actor;
	TThinkerIterator<AActor> iterator(Level);

	while ((actor = iterator.Next()))
	{
		actorhitlist[actor->GetClass()] = true;
	}

	for (auto n : gameinfo.PrecachedClasses)
	{
		PClassActor *cls = PClass::FindActor(n);
		if (cls != nullptr) actorhitlist[cls] = true;
	}
	for (unsigned i = 0; i < Level->info->PrecacheClasses.Size(); i++)
	{
		// Level->info can only store names, no class pointers.
		PClassActor *cls = PClass::FindActor(Level->info->PrecacheClasses[i]);
		if (cls != nullptr) actorhitlist[cls] = true;
	}

	for (i = Level->sectors.Size() - 1; i >= 0; i--)
	{
		AddToList(hitlist.Data(), Level->sectors[i].GetTexture(sector_t::floor), FTextureManager::HIT_Flat);
		AddToList(hitlist.Data(), Level->sectors[i].GetTexture(sector_t::ceiling), FTextureManager::HIT_Flat);
	}

	for (i = Level->sides.Size() - 1; i >= 0; i--)
	{
		AddToList(hitlist.Data(), Level->sides[i].GetTexture(side_t::top), FTextureManager::HIT_Wall);
		AddToList(hitlist.Data(), Level->sides[i].GetTexture(side_t::mid), FTextureManager::HIT_Wall);
		AddToList(hitlist.Data(), Level->sides[i].GetTexture(side_t::bottom), FTextureManager::HIT_Wall);
	}


	if (Level->skytexture1.isValid())
	{
		AddToList(hitlist.Data(), Level->skytexture1, FTextureManager::HIT_Sky);
	}
	if (Level->skytexture2.isValid())
	{
		AddToList(hitlist.Data(), Level->skytexture2, FTextureManager::HIT_Sky);
	}

	for (auto n : gameinfo.PrecachedTextures)
	{
		FTextureID tex = TexMan.CheckForTexture(n, ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) AddToList(hitlist.Data(), tex, FTextureManager::HIT_Wall);
	}
	for (unsigned i = 0; i < Level->info->PrecacheTextures.Size(); i++)
	{
		FTextureID tex = TexMan.CheckForTexture(Level->info->PrecacheTextures[i], ETextureType::Wall, FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
		if (tex.Exists()) AddToList(hitlist.Data(), tex, FTextureManager::HIT_Wall);
	}

	// This is just a temporary solution, until the hardware renderer's texture manager is in a better state.
	if (!V_IsHardwareRenderer())
		SWRenderer->Precache(hitlist.Data(), actorhitlist);
	else
		hw_PrecacheTexture(hitlist.Data(), actorhitlist);

}

//==========================================================================
//
//
//
//==========================================================================

void P_FreeLevelData ()
{

	// [ZZ] delete per-map event handlers
	E_Shutdown(true);
	R_FreePastViewers();
}

//===========================================================================
//
// P_SetupLevel
//
// [RH] position indicates the start spot to spawn at
//
//===========================================================================

void P_SetupLevel(FLevelLocals *Level, const char *lumpname, int position, bool newGame)
{
	int i;

	Level->ShaderStartTime = I_msTimeFS(); // indicate to the shader system that the level just started

	// This is motivated as follows:

	Level->maptype = MAPTYPE_UNKNOWN;
	wminfo.partime = 180;

	if (!savegamerestore)
	{
		currentSession->SetMusicVolume(currentSession->MusicVolume);
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].killcount = players[i].secretcount
				= players[i].itemcount = 0;
		}
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		players[i].mo = nullptr;
	}
	// [RH] Clear any scripted translation colors the previous level may have set.
	for (i = 0; i < int(translationtables[TRANSLATION_LevelScripted].Size()); ++i)
	{
		FRemapTable *table = translationtables[TRANSLATION_LevelScripted][i];
		if (table != nullptr)
		{
			delete table;
			translationtables[TRANSLATION_LevelScripted][i] = nullptr;
		}
	}
	translationtables[TRANSLATION_LevelScripted].Clear();

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer].viewz = NO_VALUE;

	// Make sure all sounds are stopped before Z_FreeTags.
	S_Start(currentSession->Levelinfo[0]);

	// [RH] clear out the mid-screen message
	C_MidPrint(nullptr, nullptr);

	// Free all level data from the previous map
	P_FreeLevelData();

	MapData *map = P_OpenMapData(lumpname, true);
	if (map == nullptr)
	{
		I_Error("Unable to open map '%s'\n", lumpname);
	}

	// [ZZ] init per-map static handlers. we need to call this before everything is set up because otherwise scripts don't receive PlayerEntered event
	//      (which happens at god-knows-what stage in this function, but definitely not the last part, because otherwise it'd work to put E_InitStaticHandlers before the player spawning)
	E_InitStaticHandlers(true);

	// generate a checksum for the level, to be included and checked with savegames.
	map->GetChecksum(Level->md5);
	// find map num
	Level->lumpnum = map->lumpnum;

	if (newGame)
	{
		E_NewGame(EventHandlerType::PerMap);
	}

	MapLoader loader(Level);
	loader.LoadLevel(map, lumpname, position);
	delete map;

	// if deathmatch, randomly spawn the active players
	if (deathmatch)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				players[i].mo = nullptr;
				G_DeathMatchSpawnPlayer(Level, i);
			}
		}
	}
	// the same, but for random single/coop player starts
	else if (Level->flags2 & LEVEL2_RANDOMPLAYERSTARTS)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				players[i].mo = nullptr;
				FPlayerStart *mthing = G_PickPlayerStart(Level, i);
				P_SpawnPlayer(Level, mthing, i, (Level->flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
			}
		}
	}

	// [SP] move unfriendly players around
	// horribly hacky - yes, this needs rewritten.
	if (Level->deathmatchstarts.Size() > 0)
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != nullptr)
			{
				if (!(players[i].mo->flags & MF_FRIENDLY))
				{
					AActor * oldSpawn = players[i].mo;
					G_DeathMatchSpawnPlayer(Level, i);
					oldSpawn->Destroy();
				}
			}
		}
	}


	// Don't count monsters in end-of-level sectors if option is on
	if (dmflags2 & DF2_NOCOUNTENDMONST)
	{
		TThinkerIterator<AActor> it(Level);
		AActor * mo;

		while ((mo = it.Next()))
		{
			if (mo->flags & MF_COUNTKILL)
			{
				if (mo->Sector->damageamount > 0 && (mo->Sector->Flags & (SECF_ENDGODMODE | SECF_ENDLEVEL)) == (SECF_ENDGODMODE | SECF_ENDLEVEL))
				{
					mo->ClearCounters();
				}
			}
		}
	}

	T_PreprocessScripts(Level);        // preprocess FraggleScript scripts

	// build subsector connect matrix
	//	UNUSED P_ConnectSubsectors ();

	R_OldBlend = 0xffffffff;

	// [RH] Remove all particles
	P_ClearParticles(Level);

	// preload graphics and sounds
	if (precache)
	{
		PrecacheLevel(Level);
		S_PrecacheLevel(Level->info->PrecacheSounds);
	}

	if (deathmatch)
	{
		AnnounceGameStart();
	}

	// This check was previously done at run time each time the heightsec was checked.
	// However, since 3D floors are static data, we can easily precalculate this and store it in the sector's flags for quick access.
	for (auto &s : Level->sectors)
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

	P_ResetSightCounters(true);

	Level->shadowMap = new IShadowMap(Level);

	// Create a backup of the map data so the savegame code can toss out all fields that haven't changed in order to reduce processing time and file size.
	// Note that we want binary identity here, so assignment is not sufficient because it won't initialize any padding bytes.
	// Note that none of these structures may contain non POD fields anyway.
	Level->loadsectors.Resize(Level->sectors.Size());
	memcpy(&Level->loadsectors[0], &Level->sectors[0], Level->sectors.Size() * sizeof(Level->sectors[0]));
	Level->loadlines.Resize(Level->lines.Size());
	memcpy(&Level->loadlines[0], &Level->lines[0], Level->lines.Size() * sizeof(Level->lines[0]));
	Level->loadsides.Resize(Level->sides.Size());
	memcpy(&Level->loadsides[0], &Level->sides[0], Level->sides.Size() * sizeof(Level->sides[0]));
}

//
// P_Init
//
void P_Init ()
{
	atterm (P_Shutdown);

	currentSession = new FGameSession;
	P_InitEffects ();		// [RH]
	P_InitTerrainTypes ();
	P_InitKeyMessages ();
	R_InitSprites ();
}

static void P_Shutdown ()
{
	delete currentSession;
	currentSession = nullptr;
	StaticThinkers.DestroyAllThinkers();
	P_FreeLevelData ();
	// [ZZ] delete global event handlers
	E_Shutdown(false);
	ST_Clear();
	for (auto &p : players)
	{
		if (p.psprites != nullptr) p.psprites->Destroy();
	}
}

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	ForAllLevels([](FLevelLocals *Level)
	{
		Printf("%s - %s\n", Level->MapName.GetChars(), Level->LevelName.GetChars());
		for (auto &sector : Level->sectors)
		{
			Printf(PRINT_LOG, "Sector %d\n", sector.sectornum);
			for (int j = 0; j < sector.subsectorcount; j++)
			{
				subsector_t * sub = sector.subsectors[j];

				Printf(PRINT_LOG, "    Subsector %d - real sector = %d - %s\n", int(sub->Index()), sub->sector->sectornum, sub->hacked & 1 ? "hacked" : "");
				for (uint32_t k = 0; k < sub->numlines; k++)
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
	});
}

//==========================================================================
//
//
//
//==========================================================================

CCMD(listmapsections)
{
	ForAllLevels([](FLevelLocals *Level)
	{
		Printf("%s - %s\n", Level->MapName.GetChars(), Level->LevelName.GetChars());
		for (int i = 0; i < 1000; i++)
		{
			for (auto &sub : Level->subsectors)
			{
				if (sub.mapsection == i)
				{
					Printf("Mapsection %d, sector %d, line %d\n", i, sub.render_sector->Index(), sub.firstline->linedef->Index());
					break;
				}
			}
		}
	});
}

//==========================================================================
//
//
//
//==========================================================================

CUSTOM_CVAR(Bool, forcewater, false, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL)
	{
		ForAllLevels([&](FLevelLocals *Level)
		{
			for (auto &sec : Level->sectors)
			{
				sector_t *hsec = sec.GetHeightSec();
				if (hsec && !(hsec->MoreFlags & SECMF_UNDERWATER))
				{
					if (self)
					{
						hsec->MoreFlags |= SECMF_FORCEDUNDERWATER;
					}
					else
					{
						hsec->MoreFlags &= ~SECMF_FORCEDUNDERWATER;
					}
				}
			}
		});
	}
}
