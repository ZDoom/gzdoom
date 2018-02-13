/*
** a_weapons.cpp
** Implements weapon handling
**
**---------------------------------------------------------------------------
** Copyright 2000-2016 Randy Heit
** Copyright 2006-2016 Cheistoph Oelckers
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

#include <string.h>

#include "a_pickups.h"
#include "gi.h"
#include "d_player.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_state.h"
#include "p_pspr.h"
#include "c_dispatch.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "cmdlib.h"
#include "templates.h"
#include "sbar.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_net.h"
#include "serializer.h"
#include "vm.h"

IMPLEMENT_CLASS(AWeapon, false, true)

IMPLEMENT_POINTERS_START(AWeapon)
	IMPLEMENT_POINTER(Ammo1)
	IMPLEMENT_POINTER(Ammo2)
	IMPLEMENT_POINTER(SisterWeapon)
IMPLEMENT_POINTERS_END

DEFINE_FIELD(AWeapon, AmmoType1)
DEFINE_FIELD(AWeapon, AmmoType2)	
DEFINE_FIELD(AWeapon, AmmoGive1)
DEFINE_FIELD(AWeapon, AmmoGive2)	
DEFINE_FIELD(AWeapon, MinAmmo1)
DEFINE_FIELD(AWeapon, MinAmmo2)		
DEFINE_FIELD(AWeapon, AmmoUse1)
DEFINE_FIELD(AWeapon, AmmoUse2)		
DEFINE_FIELD(AWeapon, Kickback)
DEFINE_FIELD(AWeapon, YAdjust)				
DEFINE_FIELD(AWeapon, UpSound)
DEFINE_FIELD(AWeapon, ReadySound)	
DEFINE_FIELD(AWeapon, SisterWeaponType)		
DEFINE_FIELD(AWeapon, ProjectileType)	
DEFINE_FIELD(AWeapon, AltProjectileType)	
DEFINE_FIELD(AWeapon, SelectionOrder)				
DEFINE_FIELD(AWeapon, MinSelAmmo1)
DEFINE_FIELD(AWeapon, MinSelAmmo2)	
DEFINE_FIELD(AWeapon, MoveCombatDist)			
DEFINE_FIELD(AWeapon, ReloadCounter)				
DEFINE_FIELD(AWeapon, BobStyle)					
DEFINE_FIELD(AWeapon, BobSpeed)					
DEFINE_FIELD(AWeapon, BobRangeX)
DEFINE_FIELD(AWeapon, BobRangeY)		
DEFINE_FIELD(AWeapon, Ammo1)
DEFINE_FIELD(AWeapon, Ammo2)
DEFINE_FIELD(AWeapon, SisterWeapon)
DEFINE_FIELD(AWeapon, FOVScale)
DEFINE_FIELD(AWeapon, Crosshair)					
DEFINE_FIELD(AWeapon, GivenAsMorphWeapon)
DEFINE_FIELD(AWeapon, bAltFire)
DEFINE_FIELD(AWeapon, SlotNumber)
DEFINE_FIELD(AWeapon, WeaponFlags)
DEFINE_FIELD_BIT(AWeapon, WeaponFlags, bDehAmmo, WIF_DEHAMMO)

//===========================================================================
//
//
//
//===========================================================================

FString WeaponSection;
TArray<FString> KeyConfWeapons;
FWeaponSlots *PlayingKeyConf;

TArray<PClassActor *> Weapons_ntoh;
TMap<PClassActor *, int> Weapons_hton;

static int ntoh_cmp(const void *a, const void *b);

//===========================================================================
//
//
//
//===========================================================================

void AWeapon::Finalize(FStateDefinitions &statedef)
{
	Super::Finalize(statedef);
	FState *ready = FindState(NAME_Ready);
	FState *select = FindState(NAME_Select);
	FState *deselect = FindState(NAME_Deselect);
	FState *fire = FindState(NAME_Fire);
	auto TypeName = GetClass()->TypeName;

	// Consider any weapon without any valid state abstract and don't output a warning
	// This is for creating base classes for weapon groups that only set up some properties.
	if (ready || select || deselect || fire)
	{
		if (!ready)
		{
			I_Error("Weapon %s doesn't define a ready state.", TypeName.GetChars());
		}
		if (!select)
		{
			I_Error("Weapon %s doesn't define a select state.", TypeName.GetChars());
		}
		if (!deselect)
		{
			I_Error("Weapon %s doesn't define a deselect state.", TypeName.GetChars());
		}
		if (!fire)
		{
			I_Error("Weapon %s doesn't define a fire state.", TypeName.GetChars());
		}
	}
}


//===========================================================================
//
// AWeapon :: Serialize
//
//===========================================================================

void AWeapon::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AWeapon*)GetDefault();
	arc("weaponflags", WeaponFlags, def->WeaponFlags)
		("ammogive1", AmmoGive1, def->AmmoGive1)
		("ammogive2", AmmoGive2, def->AmmoGive2)
		("minammo1", MinAmmo1, def->MinAmmo1)
		("minammo2", MinAmmo2, def->MinAmmo2)
		("ammouse1", AmmoUse1, def->AmmoUse1)
		("ammouse2", AmmoUse2, def->AmmoUse2)
		("kickback", Kickback, Kickback)
		("yadjust", YAdjust, def->YAdjust)
		("upsound", UpSound, def->UpSound)
		("readysound", ReadySound, def->ReadySound)
		("selectionorder", SelectionOrder, def->SelectionOrder)
		("ammo1", Ammo1)
		("ammo2", Ammo2)
		("sisterweapon", SisterWeapon)
		("givenasmorphweapon", GivenAsMorphWeapon, def->GivenAsMorphWeapon)
		("altfire", bAltFire, def->bAltFire)
		("reloadcounter", ReloadCounter, def->ReloadCounter)
		("bobstyle", BobStyle, def->BobStyle)
		("bobspeed", BobSpeed, def->BobSpeed)
		("bobrangex", BobRangeX, def->BobRangeX)
		("bobrangey", BobRangeY, def->BobRangeY)
		("fovscale", FOVScale, def->FOVScale)
		("crosshair", Crosshair, def->Crosshair)
		("minselammo1", MinSelAmmo1, def->MinSelAmmo1)
		("minselammo2", MinSelAmmo2, def->MinSelAmmo2);
		/* these can never change
		("ammotype1", AmmoType1, def->AmmoType1)
		("ammotype2", AmmoType2, def->AmmoType2)
		("sisterweapontype", SisterWeaponType, def->SisterWeaponType)
		("projectiletype", ProjectileType, def->ProjectileType)
		("altprojectiletype", AltProjectileType, def->AltProjectileType)
		("movecombatdist", MoveCombatDist, def->MoveCombatDist)
		*/

}

//===========================================================================
//
// AWeapon :: MarkPrecacheSounds
//
//===========================================================================

void AWeapon::MarkPrecacheSounds() const
{
	Super::MarkPrecacheSounds();
	UpSound.MarkUsed();
	ReadySound.MarkUsed();
}

//===========================================================================
//
// AWeapon :: CheckAmmo
//
// Returns true if there is enough ammo to shoot.  If not, selects the
// next weapon to use.
//
//===========================================================================

bool AWeapon::CheckAmmo(int fireMode, bool autoSwitch, bool requireAmmo, int ammocount)
{
	IFVIRTUAL(AWeapon, CheckAmmo)
	{
		VMValue params[] = { (DObject*)this, fireMode, autoSwitch, requireAmmo, ammocount };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		VMCall(func, params, 5, &ret, 1);
		return !!retval;
	}
	return DoCheckAmmo(fireMode, autoSwitch, requireAmmo, ammocount);
}

bool AWeapon::DoCheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo, int ammocount)
{
	int altFire;
	int count1, count2;
	int enough, enoughmask;
	int lAmmoUse1;

	if ((dmflags & DF_INFINITE_AMMO) || (Owner->FindInventory (PClass::FindActor(NAME_PowerInfiniteAmmo), true) != nullptr))
	{
		return true;
	}
	if (fireMode == EitherFire)
	{
		bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
		if (!gotSome && autoSwitch)
		{
			barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (nullptr);
		}
		return gotSome;
	}
	altFire = (fireMode == AltFire);
	if (!requireAmmo && (WeaponFlags & (WIF_AMMO_OPTIONAL << altFire)))
	{
		return true;
	}
	count1 = (Ammo1 != nullptr) ? Ammo1->Amount : 0;
	count2 = (Ammo2 != nullptr) ? Ammo2->Amount : 0;

	if ((WeaponFlags & WIF_DEHAMMO) && (Ammo1 == nullptr))
	{
		lAmmoUse1 = 0;
	}
	else if (ammocount >= 0 && (WeaponFlags & WIF_DEHAMMO))
	{
		lAmmoUse1 = ammocount;
	}
	else
	{
		lAmmoUse1 = AmmoUse1;
	}

	enough = (count1 >= lAmmoUse1) | ((count2 >= AmmoUse2) << 1);
	if (WeaponFlags & (WIF_PRIMARY_USES_BOTH << altFire))
	{
		enoughmask = 3;
	}
	else
	{
		enoughmask = 1 << altFire;
	}
	if (altFire && FindState(NAME_AltFire) == nullptr)
	{ // If this weapon has no alternate fire, then there is never enough ammo for it
		enough &= 1;
	}
	if (((enough & enoughmask) == enoughmask) || (enough && (WeaponFlags & WIF_AMMO_CHECKBOTH)))
	{
		return true;
	}
	// out of ammo, pick a weapon to change to
	if (autoSwitch)
	{
		barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (nullptr);
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AWeapon, CheckAmmo)
{
	PARAM_SELF_PROLOGUE(AWeapon);
	PARAM_INT(mode);
	PARAM_BOOL(autoswitch);
	PARAM_BOOL_DEF(require);
	PARAM_INT_DEF(ammocnt);
	ACTION_RETURN_BOOL(self->DoCheckAmmo(mode, autoswitch, require, ammocnt));
}

//===========================================================================
//
// AWeapon :: DepleteAmmo
//
// Use up some of the weapon's ammo. Returns true if the ammo was successfully
// depleted. If checkEnough is false, then the ammo will always be depleted,
// even if it drops below zero.
//
//===========================================================================

bool AWeapon::DepleteAmmo(bool altFire, bool checkEnough, int ammouse)
{
	IFVIRTUAL(AWeapon, DepleteAmmo)
	{
		VMValue params[] = { (DObject*)this, altFire, checkEnough, ammouse };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		VMCall(func, params, 4, &ret, 1);
		return !!retval;
	}
	return DoDepleteAmmo(altFire, checkEnough, ammouse);
}

bool AWeapon::DoDepleteAmmo (bool altFire, bool checkEnough, int ammouse)
{
	if (!((dmflags & DF_INFINITE_AMMO) || (Owner->FindInventory (PClass::FindActor(NAME_PowerInfiniteAmmo), true) != nullptr)))
	{
		if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false, false, ammouse))
		{
			return false;
		}
		if (!altFire)
		{
			if (Ammo1 != nullptr)
			{
				if (ammouse >= 0 && (WeaponFlags & WIF_DEHAMMO))
				{
					Ammo1->Amount -= ammouse;
				}
				else
				{
					Ammo1->Amount -= AmmoUse1;
				}
			}
			if ((WeaponFlags & WIF_PRIMARY_USES_BOTH) && Ammo2 != nullptr)
			{
				Ammo2->Amount -= AmmoUse2;
			}
		}
		else
		{
			if (Ammo2 != nullptr)
			{
				Ammo2->Amount -= AmmoUse2;
			}
			if ((WeaponFlags & WIF_ALT_USES_BOTH) && Ammo1 != nullptr)
			{
				Ammo1->Amount -= AmmoUse1;
			}
		}
		if (Ammo1 != nullptr && Ammo1->Amount < 0)
			Ammo1->Amount = 0;
		if (Ammo2 != nullptr && Ammo2->Amount < 0)
			Ammo2->Amount = 0;
	}
	return true;
}

DEFINE_ACTION_FUNCTION(AWeapon, DepleteAmmo)
{
	PARAM_SELF_PROLOGUE(AWeapon);
	PARAM_BOOL(altfire);
	PARAM_BOOL_DEF(checkenough);
	PARAM_INT_DEF(ammouse);
	ACTION_RETURN_BOOL(self->DoDepleteAmmo(altfire, checkenough, ammouse));
}


//===========================================================================
//
// AWeapon :: PostMorphWeapon
//
// Bring this weapon up after a player unmorphs.
//
//===========================================================================

void AWeapon::PostMorphWeapon ()
{
	DPSprite *pspr;
	if (Owner == nullptr)
	{
		return;
	}
	Owner->player->PendingWeapon = WP_NOCHANGE;
	Owner->player->ReadyWeapon = this;
	Owner->player->refire = 0;

	pspr = Owner->player->GetPSprite(PSP_WEAPON);
	pspr->y = WEAPONBOTTOM;
	pspr->ResetInterpolation();
	pspr->SetState(GetUpState());
}

//===========================================================================
//
// AWeapon :: GetUpState
//
//===========================================================================

FState *AWeapon::GetUpState ()
{
	IFVIRTUAL(AWeapon, GetUpState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		VMCall(func, params, 1, &ret, 1);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetDownState
//
//===========================================================================

FState *AWeapon::GetDownState ()
{
	IFVIRTUAL(AWeapon, GetDownState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		VMCall(func, params, 1, &ret, 1);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetReadyState
//
//===========================================================================

FState *AWeapon::GetReadyState ()
{
	IFVIRTUAL(AWeapon, GetReadyState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		VMCall(func, params, 1, &ret, 1);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetStateForButtonName
//
//===========================================================================

FState *AWeapon::GetStateForButtonName (FName button)
{
	return FindState(button);
}


/* Weapon slots ***********************************************************/

//===========================================================================
//
// FWeaponSlot :: AddWeapon
//
// Adds a weapon to the end of the slot if it isn't already in it.
//
//===========================================================================

bool FWeaponSlot::AddWeapon(const char *type)
{
	return AddWeapon(PClass::FindActor(type));
}

bool FWeaponSlot::AddWeapon(PClassActor *type)
{
	unsigned int i;
	
	if (type == nullptr)
	{
		return false;
	}
	
	if (!type->IsDescendantOf(NAME_Weapon))
	{
		Printf("Can't add non-weapon %s to weapon slots\n", type->TypeName.GetChars());
		return false;
	}

	for (i = 0; i < Weapons.Size(); i++)
	{
		if (Weapons[i].Type == type)
			return true;	// Already present
	}
	WeaponInfo info = { type, -1 };
	Weapons.Push(info);
	return true;
}

//===========================================================================
//
// FWeaponSlot :: AddWeaponList
//
// Appends all the weapons from the space-delimited list to this slot.
// Set clear to true to remove any weapons already in this slot first.
//
//===========================================================================

void FWeaponSlot :: AddWeaponList(const char *list, bool clear)
{
	FString copy(list);
	char *buff = copy.LockBuffer();
	char *tok;

	if (clear)
	{
		Clear();
	}
	tok = strtok(buff, " ");
	while (tok != nullptr)
	{
		AddWeapon(tok);
		tok = strtok(nullptr, " ");
	}
}

//===========================================================================
//
// FWeaponSlot :: LocateWeapon
//
// Returns the index for the specified weapon in this slot, or -1 if it isn't
// in this slot.
//
//===========================================================================

int FWeaponSlot::LocateWeapon(PClassActor *type)
{
	unsigned int i;

	for (i = 0; i < Weapons.Size(); ++i)
	{
		if (Weapons[i].Type == type)
		{
			return (int)i;
		}
	}
	return -1;
}

//===========================================================================
//
// FWeaponSlot :: PickWeapon
//
// Picks a weapon from this slot. If no weapon is selected in this slot,
// or the first weapon in this slot is selected, returns the last weapon.
// Otherwise, returns the previous weapon in this slot. This means
// precedence is given to the last weapon in the slot, which by convention
// is probably the strongest. Does not return weapons you have no ammo
// for or which you do not possess.
//
//===========================================================================

AWeapon *FWeaponSlot::PickWeapon(player_t *player, bool checkammo)
{
	int i, j;

	if (player->mo == nullptr)
	{
		return nullptr;
	}
	// Does this slot even have any weapons?
	if (Weapons.Size() == 0)
	{
		return player->ReadyWeapon;
	}
	if (player->ReadyWeapon != nullptr)
	{
		for (i = 0; (unsigned)i < Weapons.Size(); i++)
		{
			if (Weapons[i].Type == player->ReadyWeapon->GetClass() ||
				(player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP &&
				 player->ReadyWeapon->SisterWeapon != nullptr &&
				 player->ReadyWeapon->SisterWeapon->GetClass() == Weapons[i].Type))
			{
				for (j = (i == 0 ? Weapons.Size() - 1 : i - 1);
					j != i;
					j = (j == 0 ? Weapons.Size() - 1 : j - 1))
				{
					AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory(Weapons[j].Type));

					if (weap != nullptr && weap->IsKindOf(NAME_Weapon))
					{
						if (!checkammo || weap->CheckAmmo(AWeapon::EitherFire, false))
						{
							return weap;
						}
					}
				}
			}
		}
	}
	for (i = Weapons.Size() - 1; i >= 0; i--)
	{
		AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory(Weapons[i].Type));

		if (weap != nullptr && weap->IsKindOf(NAME_Weapon))
		{
			if (!checkammo || weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlot :: SetInitialPositions
//
// Fills in the position field for every weapon currently in the slot based
// on its position in the slot. These are not scaled to [0,1] so that extra
// weapons can use those values to go to the start or end of the slot.
//
//===========================================================================

void FWeaponSlot::SetInitialPositions()
{
	unsigned int size = Weapons.Size(), i;

	if (size == 1)
	{
		Weapons[0].Position = 0x8000;
	}
	else
	{
		for (i = 0; i < size; ++i)
		{
			Weapons[i].Position = i * 0xFF00 / (size - 1) + 0x80;
		}
	}
}

//===========================================================================
//
// FWeaponSlot :: Sort
//
// Rearranges the weapons by their position field.
//
//===========================================================================

void FWeaponSlot::Sort()
{
	// This does not use qsort(), because the sort should be stable, and
	// there is no guarantee that qsort() is stable. This insertion sort
	// should be fine.
	int i, j;

	for (i = 1; i < (int)Weapons.Size(); ++i)
	{
		int pos = Weapons[i].Position;
		PClassActor *type = Weapons[i].Type;
		for (j = i - 1; j >= 0 && Weapons[j].Position > pos; --j)
		{
			Weapons[j + 1] = Weapons[j];
		}
		Weapons[j + 1].Type = type;
		Weapons[j + 1].Position = pos;
	}
}

//===========================================================================
//
// FWeaponSlots - Copy Constructor
//
//===========================================================================

FWeaponSlots::FWeaponSlots(const FWeaponSlots &other)
{
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i] = other.Slots[i];
	}
}

//===========================================================================
//
// FWeaponSlots :: Clear
//
// Removes all weapons from every slot.
//
//===========================================================================

void FWeaponSlots::Clear()
{
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].Clear();
	}
}

//===========================================================================
//
// FWeaponSlots :: AddDefaultWeapon
//
// If the weapon already exists in a slot, don't add it. If it doesn't,
// then add it to the specified slot.
//
//===========================================================================

ESlotDef FWeaponSlots::AddDefaultWeapon (int slot, PClassActor *type)
{
	int currSlot, index;

	if (!LocateWeapon (type, &currSlot, &index))
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			bool added = Slots[slot].AddWeapon (type);
			return added ? SLOTDEF_Added : SLOTDEF_Full;
		}
		return SLOTDEF_Full;
	}
	return SLOTDEF_Exists;
}

//===========================================================================
//
// FWeaponSlots :: LocateWeapon
//
// Returns true if the weapon is in a slot, false otherwise. If the weapon
// is found, it can also optionally return the slot and index for it.
//
//===========================================================================

bool FWeaponSlots::LocateWeapon (PClassActor *type, int *const slot, int *const index)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		j = Slots[i].LocateWeapon(type);
		if (j >= 0)
		{
			if (slot != nullptr) *slot = i;
			if (index != nullptr) *index = j;
			return true;
		}
	}
	return false;
}


DEFINE_ACTION_FUNCTION(FWeaponSlots, LocateWeapon)
{
	PARAM_SELF_STRUCT_PROLOGUE(FWeaponSlots);
	PARAM_CLASS(weap, AWeapon);
	int slot = 0, index = 0;
	bool retv = self->LocateWeapon(weap, &slot, &index);
	if (numret >= 1) ret[0].SetInt(retv);
	if (numret >= 2) ret[1].SetInt(slot);
	if (numret >= 3) ret[2].SetInt(index);
	return MIN(numret, 3);
}

//===========================================================================
//
// FindMostRecentWeapon
//
// Locates the slot and index for the most recently selected weapon. If the
// player is in the process of switching to a new weapon, that is the most
// recently selected weapon. Otherwise, the current weapon is the most recent
// weapon.
//
//===========================================================================

static bool FindMostRecentWeapon(player_t *player, int *slot, int *index)
{
	if (player->PendingWeapon != WP_NOCHANGE)
	{
		return player->weapons.LocateWeapon(player->PendingWeapon->GetClass(), slot, index);
	}
	else if (player->ReadyWeapon != nullptr)
	{
		AWeapon *weap = player->ReadyWeapon;
		if (!player->weapons.LocateWeapon(weap->GetClass(), slot, index))
		{
			// If the current weapon wasn't found and is powered up,
			// look for its non-powered up version.
			if (weap->WeaponFlags & WIF_POWERED_UP && weap->SisterWeaponType != nullptr)
			{
				return player->weapons.LocateWeapon(weap->SisterWeaponType, slot, index);
			}
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

//===========================================================================
//
// FWeaponSlots :: PickNextWeapon
//
// Returns the "next" weapon for this player. If the current weapon is not
// in a slot, then it just returns that weapon, since there's nothing to
// consider it relative to.
//
//===========================================================================

AWeapon *FWeaponSlots::PickNextWeapon(player_t *player)
{
	int startslot, startindex;
	int slotschecked = 0;

	if (player->mo == nullptr)
	{
		return nullptr;
	}
	if (player->ReadyWeapon == nullptr || FindMostRecentWeapon(player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == nullptr)
		{
			startslot = NUM_WEAPON_SLOTS - 1;
			startindex = Slots[startslot].Size() - 1;
		}

		slot = startslot;
		index = startindex;
		do
		{
			if (++index >= Slots[slot].Size())
			{
				index = 0;
				slotschecked++;
				if (++slot >= NUM_WEAPON_SLOTS)
				{
					slot = 0;
				}
			}
			PClassActor *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != nullptr && weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
		while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlots :: PickPrevWeapon
//
// Returns the "previous" weapon for this player. If the current weapon is
// not in a slot, then it just returns that weapon, since there's nothing to
// consider it relative to.
//
//===========================================================================

AWeapon *FWeaponSlots::PickPrevWeapon (player_t *player)
{
	int startslot, startindex;
	int slotschecked = 0;

	if (player->mo == nullptr)
	{
		return nullptr;
	}
	if (player->ReadyWeapon == nullptr || FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == nullptr)
		{
			startslot = 0;
			startindex = 0;
		}

		slot = startslot;
		index = startindex;
		do
		{
			if (--index < 0)
			{
				slotschecked++;
				if (--slot < 0)
				{
					slot = NUM_WEAPON_SLOTS - 1;
				}
				index = Slots[slot].Size() - 1;
			}
			PClassActor *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != nullptr && weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
		while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlots :: AddExtraWeapons
//
// For every weapon class for the current game, add it to its desired slot
// and position within the slot. Does not first clear the slots.
//
//===========================================================================

void FWeaponSlots::AddExtraWeapons()
{
	unsigned int i;

	// Set fractional positions for current weapons.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].SetInitialPositions();
	}

	// Append extra weapons to the slots.
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor *cls = PClassActor::AllActorClasses[i];

		if (!cls->IsDescendantOf(NAME_Weapon))
		{
			continue;
		}
		auto weapdef = ((AWeapon*)GetDefaultByType(cls));
		auto gf = cls->ActorInfo()->GameFilter;
		if ((gf == GAME_Any || (gf & gameinfo.gametype)) &&
			cls->ActorInfo()->Replacement == nullptr &&		// Replaced weapons don't get slotted.
			!(weapdef->WeaponFlags & WIF_POWERED_UP) &&
			!LocateWeapon(cls, nullptr, nullptr)		// Don't duplicate it if it's already present.
			)
		{
			int slot = weapdef->SlotNumber;
			if ((unsigned)slot < NUM_WEAPON_SLOTS)
			{
				FWeaponSlot::WeaponInfo info = { cls, weapdef->SlotPriority };
				Slots[slot].Weapons.Push(info);
			}
		}
	}

	// Now resort every slot to put the new weapons in their proper places.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].Sort();
	}
}

//===========================================================================
//
// FWeaponSlots :: SetFromGameInfo
//
// If neither the player class nor any defined weapon contain a
// slot assignment, use the game's defaults
//
//===========================================================================

void FWeaponSlots::SetFromGameInfo()
{
	unsigned int i;

	// Only if all slots are empty
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (Slots[i].Size() > 0) return;
	}

	// Append extra weapons to the slots.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		for (unsigned j = 0; j < gameinfo.DefaultWeaponSlots[i].Size(); j++)
		{
			PClassActor *cls = PClass::FindActor(gameinfo.DefaultWeaponSlots[i][j]);
			if (cls == nullptr)
			{
				Printf("Unknown weapon class '%s' found in default weapon slot assignments\n",
					gameinfo.DefaultWeaponSlots[i][j].GetChars());
			}
			else
			{
				Slots[i].AddWeapon(cls);
			}
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: StandardSetup
//
// Setup weapons in this order:
// 1. Use slots from player class.
// 2. Add extra weapons that specify their own slots.
// 3. If all slots are empty, use the settings from the gameinfo (compatibility fallback)
//
//===========================================================================

void FWeaponSlots::StandardSetup(PClassActor *type)
{
	SetFromPlayer(type);
	AddExtraWeapons();
	SetFromGameInfo();
}

//===========================================================================
//
// FWeaponSlots :: LocalSetup
//
// Setup weapons in this order:
// 1. Run KEYCONF weapon commands, affecting slots accordingly.
// 2. Read config slots, overriding current slots. If WeaponSection is set,
//    then [<WeaponSection>.<PlayerClass>.Weapons] is tried, followed by
//    [<WeaponSection>.Weapons] if that did not exist. If WeaponSection is
//    empty, then the slots are read from [<PlayerClass>.Weapons].
//
//===========================================================================

void FWeaponSlots::LocalSetup(PClassActor *type)
{
	P_PlaybackKeyConfWeapons(this);
	if (WeaponSection.IsNotEmpty())
	{
		FString sectionclass(WeaponSection);
		sectionclass << '.' << type->TypeName.GetChars();
		if (RestoreSlots(GameConfig, sectionclass) == 0)
		{
			RestoreSlots(GameConfig, WeaponSection);
		}
	}
	else
	{
		RestoreSlots(GameConfig, type->TypeName.GetChars());
	}
}

//===========================================================================
//
// FWeaponSlots :: SendDifferences
//
// Sends the weapon slots from this instance that differ from other's.
//
//===========================================================================

void FWeaponSlots::SendDifferences(int playernum, const FWeaponSlots &other)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (other.Slots[i].Size() == Slots[i].Size())
		{
			for (j = (int)Slots[i].Size(); j-- > 0; )
			{
				if (other.Slots[i].GetWeapon(j) != Slots[i].GetWeapon(j))
				{
					break;
				}
			}
			if (j < 0)
			{ // The two slots are the same.
				continue;
			}
		}
		// The slots differ. Send mine.
		if (playernum == consoleplayer)
		{
			Net_WriteByte(DEM_SETSLOT);
		}
		else
		{
			Net_WriteByte(DEM_SETSLOTPNUM);
			Net_WriteByte(playernum);
		}
		Net_WriteByte(i);
		Net_WriteByte(Slots[i].Size());
		for (j = 0; j < Slots[i].Size(); ++j)
		{
			Net_WriteWeapon(Slots[i].GetWeapon(j));
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: SetFromPlayer
//
// Sets all weapon slots according to the player class.
//
//===========================================================================

void FWeaponSlots::SetFromPlayer(PClassActor *type)
{
	Clear();
	auto Slot = ((APlayerPawn*)GetDefaultByType(type))->Slot;
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (Slot[i] != NAME_None)
		{
			Slots[i].AddWeaponList(Slot[i], false);
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: RestoreSlots
//
// Reads slots from a config section. Any slots in the section override
// existing slot settings, while slots not present in the config are
// unaffected. Returns the number of slots read.
//
//===========================================================================

int FWeaponSlots::RestoreSlots(FConfigFile *config, const char *section)
{
	FString section_name(section);
	const char *key, *value;
	int slotsread = 0;

	section_name += ".Weapons";
	if (!config->SetSection(section_name))
	{
		return 0;
	}
	while (config->NextInSection (key, value))
	{
		if (strnicmp (key, "Slot[", 5) != 0 ||
			key[5] < '0' ||
			key[5] > '0'+NUM_WEAPON_SLOTS ||
			key[6] != ']' ||
			key[7] != 0)
		{
			continue;
		}
		Slots[key[5] - '0'].AddWeaponList(value, true);
		slotsread++;
	}
	return slotsread;
}

//===========================================================================
//
// CCMD setslot
//
//===========================================================================

void FWeaponSlots::PrintSettings()
{
	for (int i = 1; i <= NUM_WEAPON_SLOTS; ++i)
	{
		int slot = i % NUM_WEAPON_SLOTS;
		if (Slots[slot].Size() > 0)
		{
			Printf("Slot[%d]=", slot);
			for (int j = 0; j < Slots[slot].Size(); ++j)
			{
				Printf("%s ", Slots[slot].GetWeapon(j)->TypeName.GetChars());
			}
			Printf("\n");
		}
	}
}

CCMD (setslot)
{
	int slot;

	if (argv.argc() < 2 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf("Usage: setslot [slot] [weapons]\nCurrent slot assignments:\n");
		if (players[consoleplayer].mo != nullptr)
		{
			FString config(GameConfig->GetConfigPath(false));
			Printf(TEXTCOLOR_BLUE "Add the following to " TEXTCOLOR_ORANGE "%s" TEXTCOLOR_BLUE
				" to retain these bindings:\n" TEXTCOLOR_NORMAL "[", config.GetChars());
			if (WeaponSection.IsNotEmpty())
			{
				Printf("%s.", WeaponSection.GetChars());
			}
			Printf("%s.Weapons]\n", players[consoleplayer].mo->GetClass()->TypeName.GetChars());
		}
		players[consoleplayer].weapons.PrintSettings();
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != nullptr)
	{
		PlayingKeyConf->Slots[slot].Clear();
		for (int i = 2; i < argv.argc(); ++i)
		{
			PlayingKeyConf->Slots[slot].AddWeapon(argv[i]);
		}
	}
	else
	{
		if (argv.argc() == 2)
		{
			Printf ("Slot %d cleared\n", slot);
		}

		Net_WriteByte(DEM_SETSLOT);
		Net_WriteByte(slot);
		Net_WriteByte(argv.argc()-2);
		for (int i = 2; i < argv.argc(); i++)
		{
			Net_WriteWeapon(PClass::FindActor(argv[i]));
		}
	}
}

//===========================================================================
//
// CCMD addslot
//
//===========================================================================

void FWeaponSlots::AddSlot(int slot, PClassActor *type, bool feedback)
{
	if (type != nullptr && !Slots[slot].AddWeapon(type) && feedback)
	{
		Printf ("Could not add %s to slot %d\n", type->TypeName.GetChars(), slot);
	}
}

CCMD (addslot)
{
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslot <slot> <weapon>\n");
		return;
	}

	PClassActor *type= PClass::FindActor(argv[2]);
	if (type == nullptr)
	{
		Printf("%s is not a weapon\n", argv[2]);
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != nullptr)
	{
		PlayingKeyConf->AddSlot(int(slot), type, false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOT);
		Net_WriteByte(slot);
		Net_WriteWeapon(type);
	}
}

//===========================================================================
//
// CCMD weaponsection
//
//===========================================================================

CCMD (weaponsection)
{
	if (argv.argc() > 1)
	{
		WeaponSection = argv[1];
	}
}

//===========================================================================
//
// CCMD addslotdefault
//
//===========================================================================
void FWeaponSlots::AddSlotDefault(int slot, PClassActor *type, bool feedback)
{
	if (type != nullptr && type->IsDescendantOf(NAME_Weapon))
	{
		switch (AddDefaultWeapon(slot, type))
		{
		case SLOTDEF_Full:
			if (feedback)
			{
				Printf ("Could not add %s to slot %d\n", type->TypeName.GetChars(), slot);
			}
			break;

		default:
		case SLOTDEF_Added:
			break;

		case SLOTDEF_Exists:
			break;
		}
	}
}

CCMD (addslotdefault)
{
	PClassActor *type;
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslotdefault <slot> <weapon>\n");
		return;
	}

	type = PClass::FindActor(argv[2]);
	if (type == nullptr)
	{
		Printf ("%s is not a weapon\n", argv[2]);
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != nullptr)
	{
		PlayingKeyConf->AddSlotDefault(int(slot), type, false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOTDEFAULT);
		Net_WriteByte(slot);
		Net_WriteWeapon(type);
	}
}

//===========================================================================
//
// P_PlaybackKeyConfWeapons
//
// Executes the weapon-related commands from a KEYCONF lump.
//
//===========================================================================

void P_PlaybackKeyConfWeapons(FWeaponSlots *slots)
{
	PlayingKeyConf = slots;
	for (unsigned int i = 0; i < KeyConfWeapons.Size(); ++i)
	{
		FString cmd(KeyConfWeapons[i]);
		AddCommandString(cmd.LockBuffer());
	}
	PlayingKeyConf = nullptr;
}

//===========================================================================
//
// P_SetupWeapons_ntohton
//
// Initializes the ntoh and hton maps for weapon types. To populate the ntoh
// array, weapons are sorted first by game, then lexicographically. Weapons
// from the current game are sorted first, followed by weapons for all other
// games, and within each block, they are sorted by name.
//
//===========================================================================

void P_SetupWeapons_ntohton()
{
	unsigned int i;
	PClassActor *cls;

	Weapons_ntoh.Clear();
	Weapons_hton.Clear();

	cls = nullptr;
	Weapons_ntoh.Push(cls);		// Index 0 is always nullptr.
	for (i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor *cls = PClassActor::AllActorClasses[i];

		if (cls->IsDescendantOf(NAME_Weapon))
		{
			Weapons_ntoh.Push(cls);
		}
	}
	qsort(&Weapons_ntoh[1], Weapons_ntoh.Size() - 1, sizeof(Weapons_ntoh[0]), ntoh_cmp);
	for (i = 0; i < Weapons_ntoh.Size(); ++i)
	{
		Weapons_hton[Weapons_ntoh[i]] = i;
	}
}

//===========================================================================
//
// ntoh_cmp
//
// Sorting comparison function used by P_SetupWeapons_ntohton().
//
// Weapons that filter for the current game appear first, weapons that filter
// for any game appear second, and weapons that filter for some other game
// appear last. The idea here is to try to keep all the weapons that are
// most likely to be used at the start of the list so that they only need
// one byte to transmit across the network.
//
//===========================================================================

static int ntoh_cmp(const void *a, const void *b)
{
	PClassActor *c1 = *(PClassActor **)a;
	PClassActor *c2 = *(PClassActor **)b;
	int g1 = c1->ActorInfo()->GameFilter == GAME_Any ? 1 : (c1->ActorInfo()->GameFilter & gameinfo.gametype) ? 0 : 2;
	int g2 = c2->ActorInfo()->GameFilter == GAME_Any ? 1 : (c2->ActorInfo()->GameFilter & gameinfo.gametype) ? 0 : 2;
	if (g1 != g2)
	{
		return g1 - g2;
	}
	return stricmp(c1->TypeName.GetChars(), c2->TypeName.GetChars());
}

//===========================================================================
//
// P_WriteDemoWeaponsChunk
//
// Store the list of weapons so that adding new ones does not automatically
// break demos.
//
//===========================================================================

void P_WriteDemoWeaponsChunk(uint8_t **demo)
{
	WriteWord(Weapons_ntoh.Size(), demo);
	for (unsigned int i = 1; i < Weapons_ntoh.Size(); ++i)
	{
		WriteString(Weapons_ntoh[i]->TypeName.GetChars(), demo);
	}
}

//===========================================================================
//
// P_ReadDemoWeaponsChunk
//
// Restore the list of weapons that was current at the time the demo was
// recorded.
//
//===========================================================================

void P_ReadDemoWeaponsChunk(uint8_t **demo)
{
	int count, i;
	PClassActor *type;
	const char *s;

	count = ReadWord(demo);
	Weapons_ntoh.Resize(count);
	Weapons_hton.Clear(count);

	Weapons_ntoh[0] = type = nullptr;
	Weapons_hton[type] = 0;

	for (i = 1; i < count; ++i)
	{
		s = ReadStringConst(demo);
		type = PClass::FindActor(s);
		// If a demo was recorded with a weapon that is no longer present,
		// should we report it?
		Weapons_ntoh[i] = type;
		if (type != nullptr)
		{
			Weapons_hton[type] = i;
		}
	}
}

//===========================================================================
//
// Net_WriteWeapon
//
//===========================================================================

void Net_WriteWeapon(PClassActor *type)
{
	int index, *index_p;

	index_p = Weapons_hton.CheckKey(type);
	if (index_p == nullptr)
	{
		index = 0;
	}
	else
	{
		index = *index_p;
	}
	// 32767 weapons better be enough for anybody.
	assert(index >= 0 && index <= 32767);
	if (index < 128)
	{
		Net_WriteByte(index);
	}
	else
	{
		Net_WriteByte(0x80 | index);
		Net_WriteByte(index >> 7);
	}
}

//===========================================================================
//
// Net_ReadWeapon
//
//===========================================================================

PClassActor *Net_ReadWeapon(uint8_t **stream)
{
	int index;

	index = ReadByte(stream);
	if (index & 0x80)
	{
		index = (index & 0x7F) | (ReadByte(stream) << 7);
	}
	if ((unsigned)index >= Weapons_ntoh.Size())
	{
		return nullptr;
	}
	return Weapons_ntoh[index];
}

