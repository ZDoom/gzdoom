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

void P_SpawnSlopeMakers (FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable);
void P_SetSlopes ();
void P_CopySlopes();
void BloodCrypt (void *data, int key, int len);
void P_ClearUDMFKeys();
void InitRenderInfo();

extern AActor *P_SpawnMapThing (FMapThing *mthing, int position);

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


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
TArray<vertexdata_t> vertexdatas;

bool			hasglnodes;

TArray<FMapThing> MapThingsConverted;
TMap<unsigned,unsigned>  MapThingsUserDataIndex;	// from mapthing idx -> user data idx
TArray<FUDMFKey> MapThingsUserData;

bool		ForceNodeBuild;

static void P_AllocateSideDefs (MapData *map, int count);

//===========================================================================
//
// SummarizeMissingTextures
//
// Lists textures that were missing more than MISSING_TEXTURE_WARN_LIMIT
// times.
//
//===========================================================================

static void SummarizeMissingTextures(const FMissingTextureTracker &missing)
{
	FMissingTextureTracker::ConstIterator it(missing);
	FMissingTextureTracker::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.Count > MISSING_TEXTURE_WARN_LIMIT)
		{
			Printf(TEXTCOLOR_RED "Missing texture '"
				TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED
				"' is used %d more times\n",
				pair->Key.GetChars(), pair->Value.Count - MISSING_TEXTURE_WARN_LIMIT);
		}
	}
}

//===========================================================================
//
// Sound enviroment handling
//
//===========================================================================

void P_FloodZone (sector_t *sec, int zonenum)
{
	if (sec->ZoneNumber == zonenum)
		return;

	sec->ZoneNumber = zonenum;

	for (auto check : sec->Lines)
	{
		sector_t *other;

		if (check->sidedef[1] == NULL || (check->flags & ML_ZONEBOUNDARY))
				continue;

		if (check->frontsector == sec)
		{
			assert(check->backsector != NULL);
			other = check->backsector;
		}
		else
		{
			assert(check->frontsector != NULL);
			other = check->frontsector;
		}

		if (other->ZoneNumber != zonenum)
			P_FloodZone (other, zonenum);
	}
}

void P_FloodZones ()
{
	int z = 0, i;
	ReverbContainer *reverb;

	for (auto &sec : level.sectors)
	{
		if (sec.ZoneNumber == 0xFFFF)
		{
			P_FloodZone (&sec, z++);
		}
	}
	level.Zones.Resize(z);
	reverb = S_FindEnvironment(level.DefaultEnvironment);
	if (reverb == NULL)
	{
		Printf("Sound environment %d, %d not found\n", level.DefaultEnvironment >> 8, level.DefaultEnvironment & 255);
		reverb = DefaultEnvironments[0];
	}
	for (i = 0; i < z; ++i)
	{
		level.Zones[i].Environment = reverb;
	}
}


//===========================================================================
//
// SpawnMapThing
//
//===========================================================================
CVAR(Bool, dumpspawnedthings, false, 0)

AActor *SpawnMapThing(int index, FMapThing *mt, int position)
{
	AActor *spawned = P_SpawnMapThing(mt, position);
	if (dumpspawnedthings)
	{
		Printf("%5d: (%5f, %5f, %5f), doomednum = %5d, flags = %04x, type = %s\n",
			index, mt->pos.X, mt->pos.Y, mt->pos.Z, mt->EdNum, mt->flags, 
			spawned? spawned->GetClass()->TypeName.GetChars() : "(none)");
	}
	T_AddSpawnedThing(spawned);
	return spawned;
}

//===========================================================================
//
// SetMapThingUserData
//
//===========================================================================

static void SetMapThingUserData(AActor *actor, unsigned udi)
{
	if (actor == NULL)
	{
		return;
	}
	while (MapThingsUserData[udi].Key != NAME_None)
	{
		FName varname = MapThingsUserData[udi].Key;
		PField *var = dyn_cast<PField>(actor->GetClass()->FindSymbol(varname, true));

		if (var == NULL || (var->Flags & (VARF_Native|VARF_Private|VARF_Protected|VARF_Static)) || !var->Type->isScalar())
		{
			DPrintf(DMSG_WARNING, "%s is not a writable user variable in class %s\n", varname.GetChars(),
				actor->GetClass()->TypeName.GetChars());
		}
		else
		{ // Set the value of the specified user variable.
			void *addr = reinterpret_cast<uint8_t *>(actor) + var->Offset;
			if (var->Type == TypeString)
			{
				var->Type->InitializeValue(addr, &MapThingsUserData[udi].StringVal);
			}
			else if (var->Type->isFloat())
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].FloatVal);
			}
			else if (var->Type->isInt() || var->Type == TypeBool)
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].IntVal);
			}
		}

		udi++;
	}
}

//===========================================================================
//
// P_LoadThings
//
//===========================================================================

uint16_t MakeSkill(int flags)
{
	uint16_t res = 0;
	if (flags & 1) res |= 1+2;
	if (flags & 2) res |= 4;
	if (flags & 4) res |= 8+16;
	return res;
}

void P_LoadThings (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthing_t);

	char *mtp;
	mapthing_t *mt;

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mt = (mapthing_t*)mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	for (int i=0 ; i < numthings; i++, mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. P_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = LittleShort(mt->options);

		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].Gravity = 1;
		mti[i].Conversation = 0;
		mti[i].SkillFilter = MakeSkill(flags);
		mti[i].ClassFilter = 0xffff;	// Doom map format doesn't have class flags so spawn for all player classes
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;

		mti[i].pos.X = LittleShort(mt->x);
		mti[i].pos.Y = LittleShort(mt->y);
		mti[i].angle = LittleShort(mt->angle);
		mti[i].EdNum = LittleShort(mt->type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);


#ifndef NO_EDATA
		if (mti[i].info != NULL && mti[i].info->Special == SMT_EDThing)
		{
			ProcessEDMapthing(&mti[i], flags);
		}
		else
#endif
		{
			flags &= ~MTF_SKILLMASK;
			mti[i].flags = (short)((flags & 0xf) | 0x7e0);
			if (gameinfo.gametype == GAME_Strife)
			{
				mti[i].flags &= ~MTF_AMBUSH;
				if (flags & STF_SHADOW)			mti[i].flags |= MTF_SHADOW;
				if (flags & STF_ALTSHADOW)		mti[i].flags |= MTF_ALTSHADOW;
				if (flags & STF_STANDSTILL)		mti[i].flags |= MTF_STANDSTILL;
				if (flags & STF_AMBUSH)			mti[i].flags |= MTF_AMBUSH;
				if (flags & STF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			else
			{
				if (flags & BTF_BADEDITORCHECK)
				{
					flags &= 0x1F;
				}
				if (flags & BTF_NOTDEATHMATCH)	mti[i].flags &= ~MTF_DEATHMATCH;
				if (flags & BTF_NOTCOOPERATIVE)	mti[i].flags &= ~MTF_COOPERATIVE;
				if (flags & BTF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			if (flags & BTF_NOTSINGLE)			mti[i].flags &= ~MTF_SINGLE;
		}
	}
	delete [] mtp;
}

//===========================================================================
//
// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style. Position also controls which single-
// player start spots are spawned by filtering out those
// whose first parameter don't match position.
//
//===========================================================================

void P_LoadThings2 (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthinghexen_t);

	char *mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mapthinghexen_t *mth = (mapthinghexen_t*)mtp;

	for(int i = 0; i< numthings; i++)
	{
		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].thingid = LittleShort(mth[i].thingid);
		mti[i].pos.X = LittleShort(mth[i].x);
		mti[i].pos.Y = LittleShort(mth[i].y);
		mti[i].pos.Z = LittleShort(mth[i].z);
		mti[i].angle = LittleShort(mth[i].angle);
		mti[i].EdNum = LittleShort(mth[i].type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);
		mti[i].flags = LittleShort(mth[i].flags);
		mti[i].special = mth[i].special;
		for(int j=0;j<5;j++) mti[i].args[j] = mth[i].args[j];
		mti[i].SkillFilter = MakeSkill(mti[i].flags);
		mti[i].ClassFilter = (mti[i].flags & MTF_CLASS_MASK) >> MTF_CLASS_SHIFT;
		mti[i].flags &= ~(MTF_SKILLMASK|MTF_CLASS_MASK);
		if (level.flags2 & LEVEL2_HEXENHACK)
		{
			mti[i].flags &= 0x7ff;	// mask out Strife flags if playing an original Hexen map.
		}

		mti[i].Gravity = 1;
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;
		mti[i].friendlyseeblocks = -1;
	}
	delete[] mtp;
}

//===========================================================================
//
//
//
//===========================================================================

void P_SpawnThings (int position)
{
	int numthings = MapThingsConverted.Size();

	for (int i=0; i < numthings; i++)
	{
		AActor *actor = SpawnMapThing (i, &MapThingsConverted[i], position);
		unsigned *udi = MapThingsUserDataIndex.CheckKey((unsigned)i);
		if (udi != NULL)
		{
			SetMapThingUserData(actor, *udi);
		}
	}
}



//===========================================================================
//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
//===========================================================================

static void P_GroupLines (bool buildmap)
{
	cycle_t times[16];
	unsigned int*		linesDoneInEachSector;
	int 				total;
	sector_t*			sector;
	FBoundingBox		bbox;
	bool				flaggedNoFronts = false;
	unsigned int		jj;

	for (unsigned i = 0; i < countof(times); ++i)
	{
		times[i].Reset();
	}

	// look up sector number for each subsector
	times[0].Clock();
	for (auto &sub : level.subsectors)
	{
		sub.sector = sub.firstline->sidedef->sector;
	}
	for (auto &sub : level.subsectors)
	{
		for (jj = 0; jj < sub.numlines; ++jj)
		{
			sub.firstline[jj].Subsector = &sub;
		}
	}
	times[0].Unclock();

	// count number of lines in each sector
	times[1].Clock();
	total = 0;
	for (unsigned i = 0; i < level.lines.Size(); i++)
	{
		auto li = &level.lines[i];
		if (li->frontsector == NULL)
		{
			if (!flaggedNoFronts)
			{
				flaggedNoFronts = true;
				Printf ("The following lines do not have a front sidedef:\n");
			}
			Printf (" %d\n", i);
		}
		else
		{
			li->frontsector->Lines.Count++;
			total++;
		}

		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->Lines.Count++;
			total++;
		}
	}
	if (flaggedNoFronts)
	{
		I_Error ("You need to fix these lines to play this map.\n");
	}
	times[1].Unclock();

	// build line tables for each sector
	times[3].Clock();
	level.linebuffer.Alloc(total);
	line_t **lineb_p = &level.linebuffer[0];
	auto numsectors = level.sectors.Size();
	linesDoneInEachSector = new unsigned int[numsectors];
	memset (linesDoneInEachSector, 0, sizeof(int)*numsectors);

	sector = &level.sectors[0];
	for (unsigned i = 0; i < numsectors; i++, sector++)
	{
		if (sector->Lines.Count == 0)
		{
			Printf ("Sector %i (tag %i) has no lines\n", i, tagManager.GetFirstSectorTag(sector));
			// 0 the sector's tag so that no specials can use it
			tagManager.RemoveSectorTags(i);
		}
		else
		{
			sector->Lines.Array = lineb_p;
			lineb_p += sector->Lines.Count;
		}
	}

	for (unsigned i = 0; i < level.lines.Size(); i++)
	{
		auto li = &level.lines[i];
		if (li->frontsector != NULL)
		{
			li->frontsector->Lines[linesDoneInEachSector[li->frontsector->Index()]++] = li;
		}
		if (li->backsector != NULL && li->backsector != li->frontsector)
		{
			li->backsector->Lines[linesDoneInEachSector[li->backsector->Index()]++] = li;
		}
	}
	
	sector = &level.sectors[0];
	for (unsigned i = 0; i < numsectors; ++i, ++sector)
	{
		if (linesDoneInEachSector[i] != sector->Lines.Size())
		{
			I_Error("P_GroupLines: miscounted");
		}
		if (sector->Lines.Size() > 3)
		{
			bbox.ClearBox();
			for (auto li : sector->Lines)
			{
				bbox.AddToBox(li->v1->fPos());
				bbox.AddToBox(li->v2->fPos());
			}

			// set the center to the middle of the bounding box
			sector->centerspot.X = (bbox.Right() + bbox.Left()) / 2;
			sector->centerspot.Y = (bbox.Top() + bbox.Bottom()) / 2;
		}
		else if (sector->Lines.Size() > 0)
		{
			// For triangular sectors the above does not calculate good points unless the longest of the triangle's lines is perfectly horizontal and vertical
			DVector2 pos = { 0,0 };
			for (auto ln : sector->Lines)
			{
				pos += ln->v1->fPos() + ln->v2->fPos();
			}
			sector->centerspot = pos / (2 * sector->Lines.Size());
		}
	}
	delete[] linesDoneInEachSector;
	times[3].Unclock();

	// [RH] Moved this here
	times[4].Clock();
	// killough 1/30/98: Create xref tables for tags
	tagManager.HashTags();
	times[4].Unclock();

	times[5].Clock();
	if (!buildmap)
	{
		P_SetSlopes ();
	}
	times[5].Unclock();

	if (showloadtimes)
	{
		Printf ("---Group Lines Times---\n");
		for (int i = 0; i < 7; ++i)
		{
			Printf (" time %d:%9.4f ms\n", i, times[i].TimeMS());
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void P_LoadReject (MapData * map, bool junk)
{
	const int neededsize = (level.sectors.Size() * level.sectors.Size() + 7) >> 3;
	int rejectsize;

	if (!map->CheckName(ML_REJECT, "REJECT"))
	{
		rejectsize = 0;
	}
	else
	{
		rejectsize = junk ? 0 : map->Size(ML_REJECT);
	}

	if (rejectsize < neededsize)
	{
		if (rejectsize > 0)
		{
			Printf ("REJECT is %d byte%s too small.\n", neededsize - rejectsize,
				neededsize-rejectsize==1?"":"s");
		}
		level.rejectmatrix.Reset();
	}
	else
	{
		// Check if the reject has some actual content. If not, free it.
		rejectsize = MIN (rejectsize, neededsize);
		level.rejectmatrix.Alloc(rejectsize);

		map->Read (ML_REJECT, &level.rejectmatrix[0], rejectsize);

		int qwords = rejectsize / 8;
		int i;

		if (qwords > 0)
		{
			const uint64_t *qreject = (const uint64_t *)&level.rejectmatrix[0];

			i = 0;
			do
			{
				if (qreject[i] != 0)
					return;
			} while (++i < qwords);
		}
		rejectsize &= 7;
		qwords *= 8;
		for (i = 0; i < rejectsize; ++i)
		{
			if (level.rejectmatrix[qwords + i] != 0)
				return;
		}

		// Reject has no data, so pretend it isn't there.
		level.rejectmatrix.Reset();
	}
}

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
	MapThingsConverted.Clear();
	MapThingsUserDataIndex.Clear();
	MapThingsUserData.Clear();
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
#if 0
	FMapThing *buildthings;
	int numbuildthings;
#endif
	int i;
	bool buildmap;

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

	// [RH] Support loading Build maps (because I felt like it. :-)
	buildmap = false;
#if 0
	// deactivated because broken.
	if (map->Size(0) > 0)
	{
		uint8_t *mapdata = new uint8_t[map->Size(0)];
		map->Read(0, mapdata);
		times[0].Clock();
		buildmap = P_LoadBuildMap (mapdata, map->Size(0), &buildthings, &numbuildthings);
		times[0].Unclock();
		delete[] mapdata;
	}
#endif

    MapLoader maploader(level, &tagManager);
	maploader.maptype = level.maptype;
	if (!buildmap)
	{
		// note: most of this ordering is important 
		ForceNodeBuild = gennodes;

		// [RH] Load in the BEHAVIOR lump
		FBehavior::StaticUnloadModules ();
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
		if (ib_compatflags & BCOMPATF_REBUILDNODES)
		{
			ForceNodeBuild = true;
		}
		T_LoadScripts(map);

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

		FBehavior::StaticLoadDefaultModules ();
#ifndef NO_EDATA
		LoadMapinfoACSLump();
#endif


		P_LoadStrifeConversations (map, lumpname);

		FMissingTextureTracker missingtex;

		if (!map->isText)
		{
			maploader.LoadVertexes (map);
			maploader.LoadSectors (map, missingtex);

			if (!map->HasBehavior)
                maploader.LoadLineDefs (map);
			else
				maploader.LoadLineDefs2 (map);	// [RH] Load Hexen-style linedefs

			times[4].Clock();
			maploader.LoadSideDefs2 (map, missingtex);
			times[4].Unclock();

			times[5].Clock();
			maploader.FinishLoadingLineDefs ();
			times[5].Unclock();

			if (!map->HasBehavior)
				P_LoadThings (map);
			else
				P_LoadThings2 (map);	// [RH] Load Hexen-style things
		}
		else
		{
			times[0].Clock();
			maploader.ParseTextMap(map, missingtex);
			times[0].Unclock();
		}

		SetCompatibilityParams(checksum);

		times[6].Clock();
		maploader.LoopSidedefs (true);
		times[6].Unclock();

		SummarizeMissingTextures(missingtex);
	}
	else
	{
		ForceNodeBuild = true;
		level.maptype = MAPTYPE_BUILD;
	}
	bool reloop = false;

    maploader.LoadBsp(map);

	// set the head node for gameplay purposes. If the separate gamenodes array is not empty, use that, otherwise use the render nodes.
	level.headgamenode = level.gamenodes.Size() > 0 ? &level.gamenodes[level.gamenodes.Size() - 1] : level.nodes.Size()? &level.nodes[level.nodes.Size() - 1] : nullptr;

	maploader.LoadBlockMap (map);

	times[11].Clock();
	P_LoadReject (map, buildmap);
	times[11].Unclock();

	times[12].Clock();
	P_GroupLines (buildmap);
	times[12].Unclock();

	times[13].Clock();
	P_FloodZones ();
	times[13].Unclock();

	P_SetRenderSector();
	FixMinisegReferences();
	FixHoles();

	bodyqueslot = 0;
// phares 8/10/98: Clear body queue so the corpses from previous games are
// not assumed to be from this one.

	for (i = 0; i < BODYQUESIZE; i++)
		bodyque[i] = NULL;

	CreateSections(level.sections);

	if (!buildmap)
	{
		// [RH] Spawn slope creating things first.
		P_SpawnSlopeMakers (&MapThingsConverted[0], &MapThingsConverted[MapThingsConverted.Size()], maploader.oldvertextable);
		P_CopySlopes();

		// Spawn 3d floors - must be done before spawning things so it can't be done in P_SpawnSpecials
		P_Spawn3DFloors();

		times[14].Clock();
		P_SpawnThings(position);

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
	}
#if 0	// There is no such thing as a build map.
	else
	{
		for (i = 0; i < numbuildthings; ++i)
		{
			SpawnMapThing (i, &buildthings[i], 0);
		}
		delete[] buildthings;
	}
#endif
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
	MapThingsConverted.Clear();
	MapThingsUserDataIndex.Clear();
	MapThingsUserData.Clear();

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
