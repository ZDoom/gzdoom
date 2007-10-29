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

#define BONUSADD 6

FState AWeapon::States[] =
{
	S_NORMAL (SHTG, 'E',	0, A_Light0 			, NULL)
};

IMPLEMENT_POINTY_CLASS (AWeapon)
 DECLARE_POINTER (Ammo1)
 DECLARE_POINTER (Ammo2)
 DECLARE_POINTER (SisterWeapon)
END_POINTERS

BEGIN_DEFAULTS (AWeapon, Any, -1, 0)
 PROP_Inventory_PickupSound ("misc/w_pkup")
END_DEFAULTS

//===========================================================================
//
// AWeapon :: Serialize
//
//===========================================================================

void AWeapon::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << WeaponFlags
		<< AmmoType1 << AmmoType2
		<< AmmoGive1 << AmmoGive2
		<< MinAmmo1 << MinAmmo2
		<< AmmoUse1 << AmmoUse2
		<< Kickback
		<< YAdjust
		<< AR_SOUNDW(UpSound) << AR_SOUNDW(ReadySound)
		<< SisterWeaponType
		<< ProjectileType << AltProjectileType
		<< SelectionOrder
		<< MoveCombatDist
		<< Ammo1 << Ammo2 << SisterWeapon
		<< bAltFire;
}

//===========================================================================
//
// AWeapon :: TryPickup
//
// If you can't see the weapon when it's active, then you can't pick it up.
//
//===========================================================================

bool AWeapon::TryPickup (AActor *toucher)
{
	FState * ReadyState = FindState(NAME_Ready);
	if (ReadyState != NULL &&
		ReadyState->GetFrame() < sprites[ReadyState->sprite.index].numframes)
	{
		return Super::TryPickup (toucher);
	}
	return false;
}

//===========================================================================
//
// AWeapon :: Use
//
// Make the player switch to this weapon.
//
//===========================================================================

bool AWeapon::Use (bool pickup)
{
	AWeapon *useweap = this;

	// Powered up weapons cannot be used directly.
	if (WeaponFlags & WIF_POWERED_UP) return false;

	// If the player is powered-up, use the alternate version of the
	// weapon, if one exists.
	if (SisterWeapon != NULL &&
		SisterWeapon->WeaponFlags & WIF_POWERED_UP &&
		Owner->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2)))
	{
		useweap = SisterWeapon;
	}
	if (Owner->player != NULL && Owner->player->ReadyWeapon != useweap)
	{
		Owner->player->PendingWeapon = useweap;
	}
	// Return false so that the weapon is not removed from the inventory.
	return false;
}

//===========================================================================
//
// AWeapon :: HandlePickup
//
// Try to leach ammo from the weapon if you have it already.
//
//===========================================================================

bool AWeapon::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		if (static_cast<AWeapon *>(item)->PickupForAmmo (this))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AWeapon :: PickupForAmmo
//
// The player already has this weapon, so try to pick it up for ammo.
//
//===========================================================================

bool AWeapon::PickupForAmmo (AWeapon *ownedWeapon)
{
	bool gotstuff = false;

	// Don't take ammo if the weapon sticks around.
	if (!ShouldStay ())
	{
		if (AmmoGive1 > 0) gotstuff = AddExistingAmmo (ownedWeapon->Ammo1, AmmoGive1);
		if (AmmoGive2 > 0) gotstuff |= AddExistingAmmo (ownedWeapon->Ammo2, AmmoGive2);
	}
	return gotstuff;
}

//===========================================================================
//
// AWeapon :: CreateCopy
//
//===========================================================================

AInventory *AWeapon::CreateCopy (AActor *other)
{
	AWeapon *copy = static_cast<AWeapon*>(Super::CreateCopy (other));
	if (copy != this)
	{
		copy->AmmoGive1 = AmmoGive1;
		copy->AmmoGive2 = AmmoGive2;
	}
	return copy;
}

//===========================================================================
//
// AWeapon :: CreateTossable
//
// A weapon that's tossed out should contain no ammo, so you can't cheat
// by dropping it and then picking it back up.
//
//===========================================================================

AInventory *AWeapon::CreateTossable ()
{
	// Only drop the weapon that is meant to be placed in a level. That is,
	// only drop the weapon that normally gives you ammo.
	if (SisterWeapon != NULL && 
		((AWeapon*)GetDefault())->AmmoGive1 == 0 &&
		((AWeapon*)GetDefault())->AmmoGive2 == 0 &&
		(((AWeapon*)SisterWeapon->GetDefault())->AmmoGive1 > 0 ||
		 ((AWeapon*)SisterWeapon->GetDefault())->AmmoGive2 > 0))
	{
		return SisterWeapon->CreateTossable ();
	}
	AWeapon *copy = static_cast<AWeapon *> (Super::CreateTossable ());

	if (copy != NULL)
	{
		// If this weapon has a sister, remove it from the inventory too.
		if (SisterWeapon != NULL)
		{
			SisterWeapon->Destroy ();
		}
		// To avoid exploits, the tossed weapon must not have any ammo.
		copy->AmmoGive1 = 0;
		copy->AmmoGive2 = 0;
	}
	return copy;
}

//===========================================================================
//
// AWeapon :: AttachToOwner
//
//===========================================================================

void AWeapon::AttachToOwner (AActor *other)
{
	Super::AttachToOwner (other);

	Ammo1 = AddAmmo (Owner, AmmoType1, AmmoGive1);
	Ammo2 = AddAmmo (Owner, AmmoType2, AmmoGive2);
	SisterWeapon = AddWeapon (SisterWeaponType);
	if (Owner->player != NULL)
	{
		if (!Owner->player->userinfo.neverswitch && !(WeaponFlags & WIF_NO_AUTO_SWITCH))
		{
			Owner->player->PendingWeapon = this;
		}
		if (Owner->player->mo == players[consoleplayer].camera)
		{
			StatusBar->ReceivedWeapon (this);
		}
	}
}

//===========================================================================
//
// AWeapon :: AddAmmo
//
// Give some ammo to the owner, even if it's just 0.
//
//===========================================================================

AAmmo *AWeapon::AddAmmo (AActor *other, const PClass *ammotype, int amount)
{
	AAmmo *ammo;

	if (ammotype == NULL)
	{
		return NULL;
	}

	// [BC] This behavior is from the original Doom. Give 5/2 times as much ammo when
	// we pick up a weapon in deathmatch.
	if (( deathmatch ) && ( gameinfo.gametype == GAME_Doom ))
		amount = amount * 5 / 2;

	// extra ammo in baby mode and nightmare mode
	if (!(this->ItemFlags&IF_IGNORESKILL))
	{
		amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
	}
	ammo = static_cast<AAmmo *>(other->FindInventory (ammotype));
	if (ammo == NULL)
	{
		ammo = static_cast<AAmmo *>(Spawn (ammotype, 0, 0, 0, NO_REPLACE));
		ammo->Amount = MIN (amount, ammo->MaxAmount);
		ammo->AttachToOwner (other);
	}
	else if (ammo->Amount < ammo->MaxAmount)
	{
		ammo->Amount += amount;
		if (ammo->Amount > ammo->MaxAmount)
		{
			ammo->Amount = ammo->MaxAmount;
		}
	}
	return ammo;
}

//===========================================================================
//
// AWeapon :: AddExistingAmmo
//
// Give the owner some more ammo he already has.
//
//===========================================================================

bool AWeapon::AddExistingAmmo (AAmmo *ammo, int amount)
{
	if (ammo != NULL && ammo->Amount < ammo->MaxAmount)
	{
		// extra ammo in baby mode and nightmare mode
		if (!(ItemFlags&IF_IGNORESKILL))
		{
			amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
		}
		ammo->Amount += amount;
		if (ammo->Amount > ammo->MaxAmount)
		{
			ammo->Amount = ammo->MaxAmount;
		}
		return true;
	}
	return false;
}

//===========================================================================
//
// AWeapon :: AddWeapon
//
// Give the owner a weapon if they don't have it already.
//
//===========================================================================

AWeapon *AWeapon::AddWeapon (const PClass *weapontype)
{
	AWeapon *weap;

	if (weapontype == NULL)
	{
		return NULL;
	}
	weap = static_cast<AWeapon *>(Owner->FindInventory (weapontype));
	if (weap == NULL)
	{
		weap = static_cast<AWeapon *>(Spawn (weapontype, 0, 0, 0, NO_REPLACE));
		weap->AttachToOwner (Owner);
	}
	return weap;
}

//===========================================================================
//
// AWeapon :: ShouldStay
//
//===========================================================================

bool AWeapon::ShouldStay ()
{
	if (((multiplayer &&
		(!deathmatch && !alwaysapplydmflags)) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags & MF_DROPPED))
	{
		return true;
	}
	return false;
}

//===========================================================================
//
// AWeapon :: CheckAmmo
//
// Returns true if there is enough ammo to shoot.  If not, selects the
// next weapon to use.
//
//===========================================================================

bool AWeapon::CheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo)
{
	int altFire;
	int count1, count2;
	int enough, enoughmask;

	if (dmflags & DF_INFINITE_AMMO)
	{
		return true;
	}
	if (fireMode == EitherFire)
	{
		bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
		if (!gotSome && autoSwitch)
		{
			static_cast<APlayerPawn *> (Owner)->PickNewWeapon (NULL);
		}
		return gotSome;
	}
	altFire = (fireMode == AltFire);
	if (!requireAmmo && (WeaponFlags & (WIF_AMMO_OPTIONAL << altFire)))
	{
		return true;
	}
	count1 = (Ammo1 != NULL) ? Ammo1->Amount : 0;
	count2 = (Ammo2 != NULL) ? Ammo2->Amount : 0;

	enough = (count1 >= AmmoUse1) | ((count2 >= AmmoUse2) << 1);
	if (WeaponFlags & (WIF_PRIMARY_USES_BOTH << altFire))
	{
		enoughmask = 3;
	}
	else
	{
		enoughmask = 1 << altFire;
	}
	if (altFire && FindState(NAME_AltFire) == NULL)
	{ // If this weapon has no alternate fire, then there is never enough ammo for it
		enough &= 1;
	}
	if ((enough & enoughmask) == enoughmask)
	{
		return true;
	}
	// out of ammo, pick a weapon to change to
	if (autoSwitch)
	{
		static_cast<APlayerPawn *> (Owner)->PickNewWeapon (NULL);
	}
	return false;
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

bool AWeapon::DepleteAmmo (bool altFire, bool checkEnough)
{
	if (!(dmflags & DF_INFINITE_AMMO))
	{
		if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false))
		{
			return false;
		}
		if (!altFire)
		{
			if (Ammo1 != NULL)
			{
				Ammo1->Amount -= AmmoUse1;
			}
			if ((WeaponFlags & WIF_PRIMARY_USES_BOTH) && Ammo2 != NULL)
			{
				Ammo2->Amount -= AmmoUse2;
			}
		}
		else
		{
			if (Ammo2 != NULL)
			{
				Ammo2->Amount -= AmmoUse2;
			}
			if ((WeaponFlags & WIF_ALT_USES_BOTH) && Ammo1 != NULL)
			{
				Ammo1->Amount -= AmmoUse1;
			}
		}
		if (Ammo1 != NULL && Ammo1->Amount < 0)
			Ammo1->Amount = 0;
		if (Ammo2 != NULL && Ammo2->Amount < 0)
			Ammo2->Amount = 0;
	}
	return true;
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
	Owner->player->PendingWeapon = WP_NOCHANGE;
	Owner->player->ReadyWeapon = this;
	Owner->player->psprites[ps_weapon].sy = WEAPONBOTTOM;
	P_SetPsprite (Owner->player, ps_weapon, GetUpState());
}

//===========================================================================
//
// AWeapon :: EndPowerUp
//
// The Tome of Power just expired.
//
//===========================================================================

void AWeapon::EndPowerup ()
{
	if (SisterWeapon != NULL && WeaponFlags&WIF_POWERED_UP)
	{
		if (GetReadyState() != SisterWeapon->GetReadyState())
		{
			if (Owner->player->PendingWeapon == NULL ||
				Owner->player->PendingWeapon == WP_NOCHANGE)
				Owner->player->PendingWeapon = SisterWeapon;
		}
		else
		{
			Owner->player->ReadyWeapon = SisterWeapon;
		}
	}
}

//===========================================================================
//
// AWeapon :: GetUpState
//
//===========================================================================

FState *AWeapon::GetUpState ()
{
	return FindState(NAME_Select);
}

//===========================================================================
//
// AWeapon :: GetDownState
//
//===========================================================================

FState *AWeapon::GetDownState ()
{
	return FindState(NAME_Deselect);
}

//===========================================================================
//
// AWeapon :: GetReadyState
//
//===========================================================================

FState *AWeapon::GetReadyState ()
{
	return FindState(NAME_Ready);
}

//===========================================================================
//
// AWeapon :: GetAtkState
//
//===========================================================================

FState *AWeapon::GetAtkState (bool hold)
{
	FState * state=NULL;
	
	if (hold) state = FindState(NAME_Hold);
	if (state == NULL) state = FindState(NAME_Fire);
	return state;
}

//===========================================================================
//
// AWeapon :: GetAtkState
//
//===========================================================================

FState *AWeapon::GetAltAtkState (bool hold)
{
	FState * state=NULL;
	
	if (hold) state = FindState(NAME_AltHold);
	if (state == NULL) state = FindState(NAME_AltFire);
	return state;
}

/* Weapon slots ***********************************************************/

FWeaponSlots LocalWeapons;

FWeaponSlot::FWeaponSlot ()
{
	Clear ();
}

void FWeaponSlot::Clear ()
{
	for (int i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		Weapons[i] = NULL;
	}
}

bool FWeaponSlot::AddWeapon (const char *type)
{
	return AddWeapon (PClass::FindClass (type));
}

bool FWeaponSlot::AddWeapon (const PClass *type)
{
	int i;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		if (Weapons[i] == type)
			return true;	// Already present
		if (Weapons[i] == NULL)
			break;
	}
	if (i == MAX_WEAPONS_PER_SLOT)
	{ // This slot is full
		return false;
	}
	Weapons[i] = type;
	return true;
}

AWeapon *FWeaponSlot::PickWeapon (player_t *player)
{
	int i, j;

	if (player->ReadyWeapon != NULL)
	{
		for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
		{
			if (Weapons[i] == player->ReadyWeapon->GetClass() ||
				(player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP &&
				 player->ReadyWeapon->SisterWeapon != NULL &&
				 player->ReadyWeapon->SisterWeapon->GetClass() == Weapons[i]))
			{
				for (j = (unsigned)(i - 1) % MAX_WEAPONS_PER_SLOT;
					j != i;
					j = (unsigned)(j - 1) % MAX_WEAPONS_PER_SLOT)
				{
					AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory (Weapons[j]));

					if (weap != NULL && weap->CheckAmmo (AWeapon::EitherFire, false))
					{
						return weap;
					}
				}
			}
		}
	}
	for (i = MAX_WEAPONS_PER_SLOT - 1; i >= 0; i--)
	{
		AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory (Weapons[i]));

		if (weap != NULL && weap->CheckAmmo (AWeapon::EitherFire, false))
		{
			return weap;
		}
	}
	return player->ReadyWeapon;
}

void FWeaponSlots::Clear ()
{
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].Clear ();
	}
}

// If the weapon already exists in a slot, don't add it. If it doesn't,
// then add it to the specified slot. False is returned if the weapon was
// not in a slot and could not be added. True is returned otherwise.

ESlotDef FWeaponSlots::AddDefaultWeapon (int slot, const PClass *type)
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

bool FWeaponSlots::LocateWeapon (const PClass *type, int *const slot, int *const index)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		for (j = 0; j < MAX_WEAPONS_PER_SLOT; j++)
		{
			if (Slots[i].Weapons[j] == type)
			{
				*slot = i;
				*index = j;
				return true;
			}
			else if (Slots[i].Weapons[j] == NULL)
			{ // No more weapons in this slot, so try the next
				break;
			}
		}
	}
	return false;
}

static bool FindMostRecentWeapon (player_s *player, int *slot, int *index)
{
	if (player->PendingWeapon != WP_NOCHANGE)
	{
		if (player->psprites[ps_weapon].state != NULL &&
			player->psprites[ps_weapon].state->GetAction() == A_Raise)
		{
			if (LocalWeapons.LocateWeapon (player->PendingWeapon->GetClass(), slot, index))
			{
				return true;
			}
			return false;
		}
		else
		{
			return LocalWeapons.LocateWeapon (player->PendingWeapon->GetClass(), slot, index);
		}
	}
	else if (player->ReadyWeapon != NULL)
	{
		AWeapon *weap = player->ReadyWeapon;
		if (!LocalWeapons.LocateWeapon (weap->GetClass(), slot, index))
		{
			if (weap->WeaponFlags & WIF_POWERED_UP && weap->SisterWeaponType != NULL)
			{
				return LocalWeapons.LocateWeapon (weap->SisterWeaponType, slot, index);
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

AWeapon *PickNextWeapon (player_s *player)
{
	int startslot, startindex;

	if (player->ReadyWeapon == NULL || FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int start;
		int i;

		if (player->ReadyWeapon == NULL)
		{
			startslot = NUM_WEAPON_SLOTS - 1;
			startindex = MAX_WEAPONS_PER_SLOT - 1;
		}
		start = startslot * MAX_WEAPONS_PER_SLOT + startindex;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = (unsigned)((start + i) / MAX_WEAPONS_PER_SLOT) % NUM_WEAPON_SLOTS;
			int index = (unsigned)(start + i) % MAX_WEAPONS_PER_SLOT;
			const PClass *type = LocalWeapons.Slots[slot].Weapons[index];
			AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory (type));

			if (weap != NULL && weap->CheckAmmo (AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
	}
	return player->ReadyWeapon;
}

AWeapon *PickPrevWeapon (player_s *player)
{
	int startslot, startindex;

	if (player->ReadyWeapon == NULL || FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int start;
		int i;

		if (player->ReadyWeapon == NULL)
		{
			startslot = 0;
			startindex = 0;
		}
		start = startslot * MAX_WEAPONS_PER_SLOT + startindex;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = start - i;
			if (slot < 0)
				slot += NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT;
			int index = slot % MAX_WEAPONS_PER_SLOT;
			slot /= MAX_WEAPONS_PER_SLOT;
			const PClass *type = LocalWeapons.Slots[slot].Weapons[index];
			AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory (type));

			if (weap != NULL && weap->CheckAmmo (AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
	}
	return player->ReadyWeapon;
}

CCMD (setslot)
{
	int slot, i;

	if (ParsingKeyConf && WeaponSection.IsEmpty())
	{
		Printf ("You need to use weaponsection before using setslot\n");
		return;
	}

	if (argv.argc() < 2 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: setslot [slot] [weapons]\nCurrent slot assignments:\n");
		for (slot = 0; slot < NUM_WEAPON_SLOTS; ++slot)
		{
			Printf (" Slot %d:", slot);
			for (i = 0;
				i < MAX_WEAPONS_PER_SLOT && LocalWeapons.Slots[slot].GetWeapon(i) != NULL;
				++i)
			{
				Printf (" %s", LocalWeapons.Slots[slot].GetWeapon(i)->TypeName.GetChars());
			}
			Printf ("\n");
		}
		return;
	}

	LocalWeapons.Slots[slot].Clear();
	if (argv.argc() == 2)
	{
		Printf ("Slot %d cleared\n", slot);
	}
	else
	{
		for (i = 2; i < argv.argc(); ++i)
		{
			if (!LocalWeapons.Slots[slot].AddWeapon (argv[i]))
			{
				Printf ("Could not add %s to slot %d\n", argv[i], slot);
			}
		}
	}
}

CCMD (addslot)
{
	size_t slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslot <slot> <weapon>\n");
		return;
	}

	if (!LocalWeapons.Slots[slot].AddWeapon (argv[2]))
	{
		Printf ("Could not add %s to slot %d\n", argv[2], slot);
	}
}

CCMD (weaponsection)
{
	if (argv.argc() != 2)
	{
		Printf ("Usage: weaponsection <ini name>\n");
	}
	else
	{
		// Limit the section name to 32 chars
		if (strlen(argv[1]) > 32)
		{
			argv[1][32] = 0;
		}
		WeaponSection = argv[1];

		// If the ini already has definitions for this section, load them
		char fullSection[32*3];
		char *tackOn;

		if (gameinfo.gametype == GAME_Hexen)
		{
			strcpy (fullSection, "Hexen");
			tackOn = fullSection + 5;
		}
		else if (gameinfo.gametype == GAME_Heretic)
		{
			strcpy (fullSection, "Heretic");
			tackOn = fullSection + 7;
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			strcpy (fullSection, "Strife");
			tackOn = fullSection + 6;
		}
		else
		{
			strcpy (fullSection, "Doom");
			tackOn = fullSection + 4;
		}

		sprintf (tackOn, ".%s.WeaponSlots", WeaponSection.GetChars());
		if (GameConfig->SetSection (fullSection))
		{
			LocalWeapons.RestoreSlots (*GameConfig);
		}
	}
}

CCMD (addslotdefault)
{
	const PClass *type;
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslotdefault <slot> <weapon>\n");
		return;
	}

	if (ParsingKeyConf && WeaponSection.IsEmpty())
	{
		Printf ("You need to use weaponsection before using addslotdefault\n");
		return;
	}

	type = PClass::FindClass (argv[2]);
	if (type == NULL || !type->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{
		Printf ("%s is not a weapon\n", argv[2]);
	}

	switch (LocalWeapons.AddDefaultWeapon (slot, type))
	{
	case SLOTDEF_Full:
		Printf ("Could not add %s to slot %d\n", argv[2], slot);
		break;

	case SLOTDEF_Added:
		break;

	case SLOTDEF_Exists:
		break;
	}
}

int FWeaponSlots::RestoreSlots (FConfigFile &config)
{
	char buff[MAX_WEAPONS_PER_SLOT*64];
	const char *key, *value;
	int slot;
	int slotsread = 0;

	buff[sizeof(buff)-1] = 0;

	for (slot = 0; slot < NUM_WEAPON_SLOTS; ++slot)
	{
		Slots[slot].Clear ();
	}

	while (config.NextInSection (key, value))
	{
		if (strnicmp (key, "Slot[", 5) != 0 ||
			key[5] < '0' ||
			key[5] > '0'+NUM_WEAPON_SLOTS ||
			key[6] != ']' ||
			key[7] != 0)
		{
			continue;
		}
		slot = key[5] - '0';
		strncpy (buff, value, sizeof(buff)-1);
		char *tok;

		Slots[slot].Clear ();
		tok = strtok (buff, " ");
		while (tok != NULL)
		{
			Slots[slot].AddWeapon (tok);
			tok = strtok (NULL, " ");
		}
		slotsread++;
	}
	return slotsread;
}

void FWeaponSlots::SaveSlots (FConfigFile &config)
{
	char buff[MAX_WEAPONS_PER_SLOT*64];
	char keyname[16];

	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		int index = 0;

		for (int j = 0; j < MAX_WEAPONS_PER_SLOT; ++j)
		{
			if (Slots[i].Weapons[j] == NULL)
			{
				break;
			}
			if (index > 0)
			{
				buff[index++] = ' ';
			}
			const char *name = Slots[i].Weapons[j]->TypeName.GetChars();
			strcpy (buff+index, name);
			index += (int)strlen (name);
		}
		if (index > 0)
		{
			sprintf (keyname, "Slot[%d]", i);
			config.SetValueForKey (keyname, buff);
		}
	}
}

int FWeaponSlot::CountWeapons ()
{
	int i;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; ++i)
	{
		if (Weapons[i] == NULL)
		{
			break;
		}
	}
	return i;
}
