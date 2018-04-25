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

void EndAllPowerupEffects(AInventory *item);
void InitAllPowerupEffects(AInventory *item);

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a chicken/pig.
//
// TODO: Allow morphed players to receive weapon sets (not just one weapon),
// since they have their own weapon slots now.
//
//---------------------------------------------------------------------------

bool P_MorphPlayer (player_t *activator, player_t *p, PClassActor *spawntype, int duration, int style, PClassActor *enter_flash, PClassActor *exit_flash)
{
	AInventory *item;
	APlayerPawn *morphed;
	APlayerPawn *actor;

	actor = p->mo;
	if (actor == nullptr)
	{
		return false;
	}
	if (actor->flags3 & MF3_DONTMORPH)
	{
		return false;
	}
	if ((p->mo->flags2 & MF2_INVULNERABLE) && ((p != activator) || (!(style & MORPH_WHENINVULNERABLE))))
	{ // Immune when invulnerable unless this is a power we activated
		return false;
	}
	if (p->morphTics)
	{ // Player is already a beast
		if ((p->mo->GetClass() == spawntype)
			&& (p->mo->PlayerFlags & PPF_CANSUPERMORPH)
			&& (p->morphTics < (((duration) ? duration : MORPHTICS) - TICRATE))
			&& (p->mo->FindInventory (PClass::FindActor(NAME_PowerWeaponLevel2), true) == nullptr))
		{ // Make a super chicken
			p->mo->GiveInventoryType (PClass::FindActor(NAME_PowerWeaponLevel2));
		}
		return false;
	}
	if (p->health <= 0)
	{ // Dead players cannot morph
		return false;
	}
	if (spawntype == nullptr)
	{
		return false;
	}
	if (!spawntype->IsDescendantOf (RUNTIME_CLASS(APlayerPawn)))
	{
		return false;
	}
	if (spawntype == p->mo->GetClass())
	{
		return false;
	}

	morphed = static_cast<APlayerPawn *>(Spawn (spawntype, actor->Pos(), NO_REPLACE));
	EndAllPowerupEffects(actor->Inventory);
	DObject::StaticPointerSubstitution (actor, morphed);
	if ((style & MORPH_TRANSFERTRANSLATION) && !(morphed->flags2 & MF2_DONTTRANSLATE))
	{
		morphed->Translation = actor->Translation;
	}
	if ((actor->tid != 0) && (style & MORPH_NEWTIDBEHAVIOUR))
	{
		morphed->tid = actor->tid;
		morphed->AddToHash ();
		actor->RemoveFromHash ();
		actor->tid = 0;
	}
	morphed->Angles.Yaw = actor->Angles.Yaw;
	morphed->target = actor->target;
	morphed->tracer = actor->tracer;
	morphed->alternative = actor;
	morphed->FriendPlayer = actor->FriendPlayer;
	morphed->DesignatedTeam = actor->DesignatedTeam;
	morphed->Score = actor->Score;
	p->PremorphWeapon = p->ReadyWeapon;
	morphed->special2 = actor->flags & ~MF_JUSTHIT;
	morphed->player = p;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->special2 |= MF_JUSTHIT;
	}
	if (morphed->ViewHeight > p->viewheight && p->deltaviewheight == 0)
	{ // If the new view height is higher than the old one, start moving toward it.
		p->deltaviewheight = p->GetDeltaViewHeight();
	}
	morphed->flags  |= actor->flags & (MF_SHADOW|MF_NOGRAVITY);
	morphed->flags2 |= actor->flags2 & MF2_FLY;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	AActor *eflash = Spawn(((enter_flash) ? enter_flash : PClass::FindActor("TeleportFog")), actor->PosPlusZ(TELEFOGHEIGHT), ALLOW_REPLACE);
	actor->player = nullptr;
	actor->alternative = morphed;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	p->morphTics = (duration) ? duration : MORPHTICS;

	// [MH] Used by SBARINFO to speed up face drawing
	p->MorphedPlayerClass = spawntype;

	p->MorphStyle = style;
	p->MorphExitFlash = (exit_flash) ? exit_flash : PClass::FindActor("TeleportFog");
	p->health = morphed->health;
	p->mo = morphed;
	p->Vel.X = p->Vel.Y = 0;
	morphed->ObtainInventory (actor);
	// Remove all armor
	for (item = morphed->Inventory; item != nullptr; )
	{
		AInventory *next = item->Inventory;
		if (item->IsKindOf (PClass::FindActor(NAME_Armor)))
		{
			item->DepleteOrDestroy();
		}
		item = next;
	}
	InitAllPowerupEffects(morphed->Inventory);
	morphed->ActivateMorphWeapon ();
	if (p->camera == actor)
	{
		p->camera = morphed;
	}
	morphed->ScoreIcon = actor->ScoreIcon;	// [GRB]
	if (eflash)	
		eflash->target = p->mo;
	return true;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, MorphPlayer)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_POINTER(activator, player_t);
	PARAM_CLASS(spawntype, APlayerPawn);
	PARAM_INT(duration);
	PARAM_INT(style);
	PARAM_CLASS_DEF(enter_flash, AActor);
	PARAM_CLASS_DEF(exit_flash, AActor);
	ACTION_RETURN_BOOL(P_MorphPlayer(activator, self, spawntype, duration, style, enter_flash, exit_flash));
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoPlayerMorph
//
//----------------------------------------------------------------------------

bool P_UndoPlayerMorph (player_t *activator, player_t *player, int unmorphflag, bool force)
{
	AWeapon *beastweap;
	APlayerPawn *mo;
	APlayerPawn *pmo;

	pmo = player->mo;
	// [MH]
	// Checks pmo as well; the PowerMorph destroyer will
	// try to unmorph the player; if the destroyer runs
	// because the level or game is ended while morphed,
	// by the time it gets executed the morphed player
	// pawn instance may have already been destroyed.
	if (pmo == nullptr || pmo->alternative == nullptr)
	{
		return false;
	}

	bool DeliberateUnmorphIsOkay = !!(MORPH_STANDARDUNDOING & unmorphflag);

    if ((pmo->flags2 & MF2_INVULNERABLE) // If the player is invulnerable
        && ((player != activator)       // and either did not decide to unmorph,
        || (!((player->MorphStyle & MORPH_WHENINVULNERABLE)  // or the morph style does not allow it
        || (DeliberateUnmorphIsOkay))))) // (but standard morph styles always allow it),
	{ // Then the player is immune to the unmorph.
		return false;
	}

	mo = barrier_cast<APlayerPawn *>(pmo->alternative);
	mo->SetOrigin (pmo->Pos(), false);
	mo->flags |= MF_SOLID;
	pmo->flags &= ~MF_SOLID;
	if (!force && !P_TestMobjLocation (mo))
	{ // Didn't fit
		mo->flags &= ~MF_SOLID;
		pmo->flags |= MF_SOLID;
		player->morphTics = 2*TICRATE;
		return false;
	}
	// No longer using tracer as morph storage. That is what 'alternative' is for. 
	// If the tracer has changed on the morph, change the original too.
	mo->target = pmo->target;
	mo->tracer = pmo->tracer;
	pmo->player = nullptr;
	mo->alternative = pmo->alternative = nullptr;

	// Remove the morph power if the morph is being undone prematurely.
	auto pmtype = PClass::FindActor("PowerMorph");
	for (AInventory *item = pmo->Inventory, *next = nullptr; item != nullptr; item = next)
	{
		next = item->Inventory;
		if (item->IsKindOf(pmtype))
		{
			item->Destroy();
		}
	}
	EndAllPowerupEffects(pmo->Inventory);
	mo->ObtainInventory (pmo);
	DObject::StaticPointerSubstitution (pmo, mo);
	if ((pmo->tid != 0) && (player->MorphStyle & MORPH_NEWTIDBEHAVIOUR))
	{
		mo->tid = pmo->tid;
		mo->AddToHash ();
	}
	mo->Angles.Yaw = pmo->Angles.Yaw;
	mo->player = player;
	mo->reactiontime = 18;
	mo->flags = ActorFlags::FromInt (pmo->special2) & ~MF_JUSTHIT;
	mo->Vel.X = mo->Vel.Y = 0;
	player->Vel.Zero();
	mo->Vel.Z = pmo->Vel.Z;
	mo->floorz = pmo->floorz;
	if (!(pmo->special2 & MF_JUSTHIT))
	{
		mo->renderflags &= ~RF_INVISIBLE;
	}
	mo->flags  = (mo->flags & ~(MF_SHADOW|MF_NOGRAVITY)) | (pmo->flags & (MF_SHADOW|MF_NOGRAVITY));
	mo->flags2 = (mo->flags2 & ~MF2_FLY) | (pmo->flags2 & MF2_FLY);
	mo->flags3 = (mo->flags3 & ~MF3_GHOST) | (pmo->flags3 & MF3_GHOST);
	mo->Score = pmo->Score;
	InitAllPowerupEffects(mo->Inventory);

	PClassActor *exit_flash = player->MorphExitFlash;
	bool correctweapon = !!(player->MorphStyle & MORPH_LOSEACTUALWEAPON);
	bool undobydeathsaves = !!(player->MorphStyle & MORPH_UNDOBYDEATHSAVES);

	player->morphTics = 0;
	player->MorphedPlayerClass = 0;
	player->MorphStyle = 0;
	player->MorphExitFlash = nullptr;
	player->viewheight = mo->ViewHeight;
	AInventory *level2 = mo->FindInventory (PClass::FindActor(NAME_PowerWeaponLevel2), true);
	if (level2 != nullptr)
	{
		level2->Destroy ();
	}

	if ((player->health > 0) || undobydeathsaves)
	{
		player->health = mo->health = mo->SpawnHealth();
	}
	else // killed when morphed so stay dead
	{
		mo->health = player->health;
	}

	player->mo = mo;
	if (player->camera == pmo)
	{
		player->camera = mo;
	}

	// [MH]
	// If the player that was morphed is the one
	// taking events, reset up the face, if any;
	// this is only needed for old-skool skins
	// and for the original DOOM status bar.
	if (player == &players[consoleplayer])
	{
		FName face = pmo->Face;
		if (face != NAME_None)
		{
			// Assume root-level base skin to begin with
			size_t skinindex = 0;
			// If a custom skin was in use, then reload it
			// or else the base skin for the player class.
			if ((unsigned int)player->userinfo.GetSkin() >= PlayerClasses.Size () &&
				(unsigned)player->userinfo.GetSkin() < Skins.Size())
			{

				skinindex = player->userinfo.GetSkin();
			}
			else if (PlayerClasses.Size () > 1)
			{
				const PClass *whatami = player->mo->GetClass();
				for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
				{
					if (PlayerClasses[i].Type == whatami)
					{
						skinindex = i;
						break;
					}
				}
			}
		}
	}

	AActor *eflash = nullptr;
	if (exit_flash != nullptr)
	{
		eflash = Spawn(exit_flash, pmo->Vec3Angle(20., mo->Angles.Yaw, TELEFOGHEIGHT), ALLOW_REPLACE);
		if (eflash)	eflash->target = mo;
	}
	mo->SetupWeaponSlots();		// Use original class's weapon slots.
	beastweap = player->ReadyWeapon;
	if (player->PremorphWeapon != nullptr)
	{
		player->PremorphWeapon->PostMorphWeapon ();
	}
	else
	{
		player->ReadyWeapon = player->PendingWeapon = nullptr;
	}
	if (correctweapon)
	{ // Better "lose morphed weapon" semantics
		PClassActor *morphweapon = PClass::FindActor(pmo->MorphWeapon);
		if (morphweapon != nullptr && morphweapon->IsDescendantOf(NAME_Weapon))
		{
			AWeapon *OriginalMorphWeapon = static_cast<AWeapon *>(mo->FindInventory (morphweapon));
			if ((OriginalMorphWeapon != nullptr) && (OriginalMorphWeapon->GivenAsMorphWeapon))
			{ // You don't get to keep your morphed weapon.
				if (OriginalMorphWeapon->SisterWeapon != nullptr)
				{
					OriginalMorphWeapon->SisterWeapon->Destroy ();
				}
				OriginalMorphWeapon->Destroy ();
			}
		}
 	}
	else // old behaviour (not really useful now)
	{ // Assumptions made here are no longer valid
		if (beastweap != nullptr)
		{ // You don't get to keep your morphed weapon.
			if (beastweap->SisterWeapon != nullptr)
			{
				beastweap->SisterWeapon->Destroy ();
			}
			beastweap->Destroy ();
		}
	}
	pmo->Destroy ();
	// Restore playerclass armor to its normal amount.
	auto hxarmor = mo->FindInventory(NAME_HexenArmor);
	if (hxarmor != nullptr)
	{
		double *Slots = (double*)hxarmor->ScriptVar(NAME_Slots, nullptr);
		Slots[4] = mo->HexenArmor[0];
	}
	return true;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, UndoPlayerMorph)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_POINTER_NOT_NULL(player, player_t);
	PARAM_INT_DEF(unmorphflag);
	PARAM_BOOL_DEF(force);
	ACTION_RETURN_BOOL(P_UndoPlayerMorph(self, player, unmorphflag, force));
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphMonster
//
// Returns true if the monster gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphMonster (AActor *actor, PClassActor *spawntype, int duration, int style, PClassActor *enter_flash, PClassActor *exit_flash)
{
	AMorphedMonster *morphed;

	if (actor == NULL || actor->player || spawntype == NULL ||
		actor->flags3 & MF3_DONTMORPH ||
		!(actor->flags3 & MF3_ISMONSTER) ||
		!spawntype->IsDescendantOf (PClass::FindActor(NAME_MorphedMonster)))
	{
		return false;
	}

	morphed = static_cast<AMorphedMonster *>(Spawn (spawntype, actor->Pos(), NO_REPLACE));
	DObject::StaticPointerSubstitution (actor, morphed);
	if ((style & MORPH_TRANSFERTRANSLATION) && !(morphed->flags2 & MF2_DONTTRANSLATE))
	{
		morphed->Translation = actor->Translation;
	}
	morphed->tid = actor->tid;
	morphed->Angles.Yaw = actor->Angles.Yaw;
	morphed->UnmorphedMe = actor;
	morphed->Alpha = actor->Alpha;
	morphed->RenderStyle = actor->RenderStyle;
	morphed->Score = actor->Score;

	morphed->UnmorphTime = level.time + ((duration) ? duration : MORPHTICS) + pr_morphmonst();
	morphed->MorphStyle = style;
	morphed->MorphExitFlash = (exit_flash) ? exit_flash : PClass::FindActor("TeleportFog");
	morphed->FlagsSave = actor->flags & ~MF_JUSTHIT;
	morphed->special = actor->special;
	memcpy (morphed->args, actor->args, sizeof(actor->args));
	morphed->CopyFriendliness (actor, true);
	morphed->flags |= actor->flags & MF_SHADOW;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->FlagsSave |= MF_JUSTHIT;
	}
	morphed->AddToHash ();
	actor->RemoveFromHash ();
	actor->special = 0;
	actor->tid = 0;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	AActor *eflash = Spawn(((enter_flash) ? enter_flash : PClass::FindActor("TeleportFog")), actor->PosPlusZ(TELEFOGHEIGHT), ALLOW_REPLACE);
	if (eflash)
		eflash->target = morphed;
	return true;
}

DEFINE_ACTION_FUNCTION(AActor, MorphMonster)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(spawntype, AActor);
	PARAM_INT(duration);
	PARAM_INT(style);
	PARAM_CLASS_DEF(enter_flash, AActor);
	PARAM_CLASS_DEF(exit_flash, AActor);
	ACTION_RETURN_BOOL(P_MorphMonster(self, spawntype, duration, style, enter_flash, exit_flash));
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
// FUNC P_UpdateMorphedMonster
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UpdateMorphedMonster (AMorphedMonster *beast)
{
	if (beast->UnmorphTime > level.time)
	{
		return false;
	}
	return P_UndoMonsterMorph (beast);
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

//===========================================================================
//
// EndAllPowerupEffects
//
// Calls EndEffect() on every Powerup in the inventory list.
//
//===========================================================================

void EndAllPowerupEffects(AInventory *item)
{
	auto ptype = PClass::FindActor(NAME_Powerup);
	while (item != NULL)
	{
		if (item->IsKindOf(ptype))
		{
			IFVIRTUALPTRNAME(item, NAME_Powerup, EndEffect)
			{
				VMValue params[1] = { item };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		item = item->Inventory;
	}
}

//===========================================================================
//
// InitAllPowerupEffects
//
// Calls InitEffect() on every Powerup in the inventory list.
//
//===========================================================================

void InitAllPowerupEffects(AInventory *item)
{
	auto ptype = PClass::FindActor(NAME_Powerup);
	while (item != NULL)
	{
		if (item->IsKindOf(ptype))
		{
			IFVIRTUALPTRNAME(item, NAME_Powerup, InitEffect)
			{
				VMValue params[1] = { item };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		item = item->Inventory;
	}
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

void AMorphedMonster::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	// Dead things don't unmorph
//	flags3 |= MF3_STAYMORPHED;
	// [MH]
	// But they can now, so that line above has been
	// moved into P_MorphedDeath() and is now set by
	// that function if and only if it is needed.
	Super::Die (source, inflictor, dmgflags);
	if (UnmorphedMe != NULL && (UnmorphedMe->flags & MF_UNMORPHED))
	{
		UnmorphedMe->health = health;
		UnmorphedMe->CallDie (source, inflictor, dmgflags);
	}
}

void AMorphedMonster::Tick ()
{
	if (!P_UpdateMorphedMonster (this))
	{
		Super::Tick ();
	}
}


DEFINE_ACTION_FUNCTION(AActor, A_Morph)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(type, AActor);
	PARAM_INT_DEF(duration);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(enter_flash, AActor);
	PARAM_CLASS_DEF(exit_flash, AActor);
	bool res = false;
	if (self->player)
	{
		if (type->IsDescendantOf(RUNTIME_CLASS(APlayerPawn)))
		{
			res = P_MorphPlayer(self->player, self->player, type, duration, flags, enter_flash, exit_flash);
		}
	}
	else
	{
		if (type->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
		{
			res = P_MorphMonster(self, type, duration, flags, enter_flash, exit_flash);
		}
	}
	ACTION_RETURN_BOOL(res);
}
