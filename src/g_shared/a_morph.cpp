//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2005-2008 Martin Howe
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

#include "info.h"
#include "a_pickups.h"
#include "p_local.h"
#include "gi.h"
#include "a_sharedglobal.h"
#include "sbar.h"
#include "a_morph.h"
#include "doomstat.h"
#include "serializer.h"
#include "p_enemy.h"
#include "d_player.h"
#include "r_data/sprites.h"
#include "g_levellocals.h"
#include "vm.h"

static FRandom pr_morphmonst ("MorphMonster");


bool P_MorphActor(AActor *activator, AActor *victim, PClassActor *ptype, PClassActor *mtype, int duration, int style, PClassActor *enter_flash, PClassActor *exit_flash)
{
	IFVIRTUALPTR(victim, AActor, Morph)
	{
		VMValue params[] = { victim, activator, ptype, mtype, duration, style, enter_flash, exit_flash };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

bool P_UndoPlayerMorph(player_t *activator, player_t *player, int unmorphflag, bool force)
{
	if (!player->mo) return false;
	IFVIRTUALPTR(player->mo, APlayerPawn, UndoPlayerMorph)
	{
		VMValue params[] = { player->mo, activator, unmorphflag, force };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoMonsterMorph
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UndoMonsterMorph (AMorphedMonster *beast, bool force)
{
	AActor *actor;

	if (beast->UnmorphTime == 0 || 
		beast->UnmorphedMe == NULL ||
		beast->flags3 & MF3_STAYMORPHED ||
		beast->UnmorphedMe->flags3 & MF3_STAYMORPHED)
	{
		return false;
	}
	actor = beast->UnmorphedMe;
	actor->SetOrigin (beast->Pos(), false);
	actor->flags |= MF_SOLID;
	beast->flags &= ~MF_SOLID;
	ActorFlags6 beastflags6 = beast->flags6;
	beast->flags6 &= ~MF6_TOUCHY;
	if (!force && !P_TestMobjLocation (actor))
	{ // Didn't fit
		actor->flags &= ~MF_SOLID;
		beast->flags |= MF_SOLID;
		beast->flags6 = beastflags6;
		beast->UnmorphTime = level.time + 5*TICRATE; // Next try in 5 seconds
		return false;
	}
	actor->Angles.Yaw = beast->Angles.Yaw;
	actor->target = beast->target;
	actor->FriendPlayer = beast->FriendPlayer;
	actor->flags = beast->FlagsSave & ~MF_JUSTHIT;
	actor->flags  = (actor->flags & ~(MF_FRIENDLY|MF_SHADOW)) | (beast->flags & (MF_FRIENDLY|MF_SHADOW));
	actor->flags3 = (actor->flags3 & ~(MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST))
					| (beast->flags3 & (MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST));
	actor->flags4 = (actor->flags4 & ~MF4_NOHATEPLAYERS) | (beast->flags4 & MF4_NOHATEPLAYERS);
	if (!(beast->FlagsSave & MF_JUSTHIT))
		actor->renderflags &= ~RF_INVISIBLE;
	actor->health = actor->SpawnHealth();
	actor->Vel = beast->Vel;
	actor->tid = beast->tid;
	actor->special = beast->special;
	actor->Score = beast->Score;
	memcpy (actor->args, beast->args, sizeof(actor->args));
	actor->AddToHash ();
	beast->UnmorphedMe = NULL;
	DObject::StaticPointerSubstitution (beast, actor);
	PClassActor *exit_flash = beast->MorphExitFlash;
	beast->Destroy ();
	AActor *eflash = Spawn(exit_flash, beast->PosPlusZ(TELEFOGHEIGHT), ALLOW_REPLACE);
	if (eflash)
		eflash->target = actor;
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_MorphedDeath
//
// Unmorphs the actor if possible.
// Returns the unmorphed actor, the style with which they were morphed and the
// health (of the AActor, not the player_t) they last had before unmorphing.
//
//----------------------------------------------------------------------------

bool P_MorphedDeath(AActor *actor, AActor **morphed, int *morphedstyle, int *morphedhealth)
{
	// May be a morphed player
	if ((actor->player) &&
		(actor->player->morphTics) &&
		(actor->player->MorphStyle & MORPH_UNDOBYDEATH) &&
		(actor->player->mo) &&
		(actor->player->mo->alternative))
	{
		AActor *realme = actor->player->mo->alternative;
		int realstyle = actor->player->MorphStyle;
		int realhealth = actor->health;
		if (P_UndoPlayerMorph(actor->player, actor->player, 0, !!(actor->player->MorphStyle & MORPH_UNDOBYDEATHFORCED)))
		{
			*morphed = realme;
			*morphedstyle = realstyle;
			*morphedhealth = realhealth;
			return true;
		}
		return false;
	}

	// May be a morphed monster
	if (actor->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
	{
		AMorphedMonster *fakeme = static_cast<AMorphedMonster *>(actor);
		AActor *realme = fakeme->UnmorphedMe;
		if (realme != NULL)
		{
			if ((fakeme->UnmorphTime) &&
				(fakeme->MorphStyle & MORPH_UNDOBYDEATH))
			{
				int realstyle = fakeme->MorphStyle;
				int realhealth = fakeme->health;
				if (P_UndoMonsterMorph(fakeme, !!(fakeme->MorphStyle & MORPH_UNDOBYDEATHFORCED)))
				{
					*morphed = realme;
					*morphedstyle = realstyle;
					*morphedhealth = realhealth;
					return true;
				}
			}
			if (realme->flags4 & MF4_BOSSDEATH)
			{
				realme->health = 0;	// make sure that A_BossDeath considers it dead.
				A_BossDeath(realme);
			}
		}
		fakeme->flags3 |= MF3_STAYMORPHED; // moved here from AMorphedMonster::Die()
		return false;
	}

	// Not a morphed player or monster
	return false;
}


// Morphed Monster (you must subclass this to do something useful) ---------

IMPLEMENT_CLASS(AMorphedMonster, false, true)

IMPLEMENT_POINTERS_START(AMorphedMonster)
	IMPLEMENT_POINTER(UnmorphedMe)
IMPLEMENT_POINTERS_END

DEFINE_FIELD(AMorphedMonster, UnmorphedMe)
DEFINE_FIELD(AMorphedMonster, UnmorphTime)
DEFINE_FIELD(AMorphedMonster, MorphStyle)
DEFINE_FIELD(AMorphedMonster, MorphExitFlash)

void AMorphedMonster::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("unmorphedme", UnmorphedMe)
		("unmorphtime", UnmorphTime)
		("morphstyle", MorphStyle)
		("morphexitflash", MorphExitFlash)
		("flagsave", FlagsSave);
}

void AMorphedMonster::OnDestroy ()
{
	if (UnmorphedMe != NULL)
	{
		UnmorphedMe->Destroy ();
	}
	Super::OnDestroy();
}

void AMorphedMonster::Die (AActor *source, AActor *inflictor, int dmgflags, FName MeansOfDeath)
{
	// Dead things don't unmorph
//	flags3 |= MF3_STAYMORPHED;
	// [MH]
	// But they can now, so that line above has been
	// moved into P_MorphedDeath() and is now set by
	// that function if and only if it is needed.
	Super::Die (source, inflictor, dmgflags, MeansOfDeath);
	if (UnmorphedMe != NULL && (UnmorphedMe->flags & MF_UNMORPHED))
	{
		UnmorphedMe->health = health;
		UnmorphedMe->CallDie (source, inflictor, dmgflags, MeansOfDeath);
	}
}

void AMorphedMonster::Tick ()
{
	if (UnmorphTime > level.time || !P_UndoMonsterMorph(this))
	{
		Super::Tick();
	}
}

