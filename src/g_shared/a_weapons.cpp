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
#include "thingdef/thingdef.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_net.h"
#include "farchive.h"

#define BONUSADD 6

IMPLEMENT_POINTY_CLASS (AWeapon)
 DECLARE_POINTER (Ammo1)
 DECLARE_POINTER (Ammo2)
 DECLARE_POINTER (SisterWeapon)
END_POINTERS

FString WeaponSection;
TArray<FString> KeyConfWeapons;
FWeaponSlots *PlayingKeyConf;

TArray<const PClass *> Weapons_ntoh;
TMap<const PClass *, int> Weapons_hton;

static int STACK_ARGS ntoh_cmp(const void *a, const void *b);

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
		<< UpSound << ReadySound
		<< SisterWeaponType
		<< ProjectileType << AltProjectileType
		<< SelectionOrder
		<< MoveCombatDist
		<< Ammo1 << Ammo2 << SisterWeapon << GivenAsMorphWeapon
		<< bAltFire
		<< ReloadCounter;		
		if (SaveVersion >= 3615) {
			arc << BobStyle << BobSpeed << BobRangeX << BobRangeY;
		}
	arc << FOVScale
		<< Crosshair;
	if (SaveVersion >= 4203)
	{
		arc << MinSelAmmo1 << MinSelAmmo2;
	}
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
// AWeapon :: TryPickup
//
// If you can't see the weapon when it's active, then you can't pick it up.
//
//===========================================================================

bool AWeapon::TryPickupRestricted (AActor *&toucher)
{
	// Wrong class, but try to pick up for ammo
	if (ShouldStay())
	{ // Can't pick up weapons for other classes in coop netplay
		return false;
	}

	bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
	gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
	if (gaveSome)
	{
		GoAwayAndDie ();
	}
	return gaveSome;
}


//===========================================================================
//
// AWeapon :: TryPickup
//
//===========================================================================

bool AWeapon::TryPickup (AActor *&toucher)
{
	FState * ReadyState = FindState(NAME_Ready);
	if (ReadyState != NULL &&
		ReadyState->GetFrame() < sprites[ReadyState->sprite].numframes)
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
		Owner->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true))
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
// AWeapon :: Destroy
//
//===========================================================================

void AWeapon::Destroy()
{
	AWeapon *sister = SisterWeapon;

	if (sister != NULL)
	{
		// avoid recursion
		sister->SisterWeapon = NULL;
		if (sister != this)
		{ // In case we are our own sister, don't crash.
			sister->Destroy();
		}
	}
	Super::Destroy();
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
		int oldamount1 = 0;
		int oldamount2 = 0;
		if (ownedWeapon->Ammo1 != NULL) oldamount1 = ownedWeapon->Ammo1->Amount;
		if (ownedWeapon->Ammo2 != NULL) oldamount2 = ownedWeapon->Ammo2->Amount;

		if (AmmoGive1 > 0) gotstuff = AddExistingAmmo (ownedWeapon->Ammo1, AmmoGive1);
		if (AmmoGive2 > 0) gotstuff |= AddExistingAmmo (ownedWeapon->Ammo2, AmmoGive2);

		AActor *Owner = ownedWeapon->Owner;
		if (gotstuff && Owner != NULL && Owner->player != NULL)
		{
			if (ownedWeapon->Ammo1 != NULL && oldamount1 == 0)
			{
				static_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(ownedWeapon->Ammo1->GetClass());
			}
			else if (ownedWeapon->Ammo2 != NULL && oldamount2 == 0)
			{
				static_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(ownedWeapon->Ammo2->GetClass());
			}
		}
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
			SisterWeapon->SisterWeapon = NULL;
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
		if (!Owner->player->userinfo.GetNeverSwitch() && !(WeaponFlags & WIF_NO_AUTO_SWITCH))
		{
			Owner->player->PendingWeapon = this;
		}
		if (Owner->player->mo == players[consoleplayer].camera)
		{
			StatusBar->ReceivedWeapon (this);
		}
	}
	GivenAsMorphWeapon = false; // will be set explicitly by morphing code
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
	if (( deathmatch ) && ( gameinfo.gametype & GAME_DoomChex ))
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
EXTERN_CVAR(Bool, sv_unlimited_pickup)

bool AWeapon::AddExistingAmmo (AAmmo *ammo, int amount)
{
	if (ammo != NULL && (ammo->Amount < ammo->MaxAmount || sv_unlimited_pickup))
	{
		// extra ammo in baby mode and nightmare mode
		if (!(ItemFlags&IF_IGNORESKILL))
		{
			amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
		}
		ammo->Amount += amount;
		if (ammo->Amount > ammo->MaxAmount && !sv_unlimited_pickup)
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

	if (weapontype == NULL || !weapontype->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
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

bool AWeapon::CheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo, int ammocount)
{
	int altFire;
	int count1, count2;
	int enough, enoughmask;
	int lAmmoUse1;

	if ((dmflags & DF_INFINITE_AMMO) || (Owner->player->cheats & CF_INFINITEAMMO))
	{
		return true;
	}
	if (fireMode == EitherFire)
	{
		bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
		if (!gotSome && autoSwitch)
		{
			barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (NULL);
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

	if ((WeaponFlags & WIF_DEHAMMO) && (Ammo1 == NULL))
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
	if (altFire && FindState(NAME_AltFire) == NULL)
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
		barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (NULL);
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

bool AWeapon::DepleteAmmo (bool altFire, bool checkEnough, int ammouse)
{
	if (!((dmflags & DF_INFINITE_AMMO) || (Owner->player->cheats & CF_INFINITEAMMO)))
	{
		if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false, false, ammouse))
		{
			return false;
		}
		if (!altFire)
		{
			if (Ammo1 != NULL)
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
	if (Owner == NULL)
	{
		return;
	}
	Owner->player->PendingWeapon = WP_NOCHANGE;
	Owner->player->ReadyWeapon = this;
	Owner->player->psprites[ps_weapon].sy = WEAPONBOTTOM;
	Owner->player->refire = 0;
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

//===========================================================================
//
// AWeapon :: GetStateForButtonName
//
//===========================================================================

FState *AWeapon::GetStateForButtonName (FName button)
{
	return FindState(button);
}


/* Weapon giver ***********************************************************/

IMPLEMENT_CLASS(AWeaponGiver)

void AWeaponGiver::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	if (SaveVersion >= 4246)
	{
		arc << DropAmmoFactor;
	}
}

bool AWeaponGiver::TryPickup(AActor *&toucher)
{
	FDropItem *di = GetDropItems();
	AWeapon *weap;

	if (di != NULL)
	{
		const PClass *ti = PClass::FindClass(di->Name);
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			if (master == NULL)
			{
				master = weap = static_cast<AWeapon*>(Spawn(di->Name, 0, 0, 0, NO_REPLACE));
				if (weap != NULL)
				{
					weap->ItemFlags &= ~IF_ALWAYSPICKUP;	// use the flag of this item only.
					weap->flags = (weap->flags & ~MF_DROPPED) | (this->flags & MF_DROPPED);

					// If our ammo gives are non-negative, transfer them to the real weapon.
					if (AmmoGive1 >= 0) weap->AmmoGive1 = AmmoGive1;
					if (AmmoGive2 >= 0) weap->AmmoGive2 = AmmoGive2;

					// If DropAmmoFactor is non-negative, modify the given ammo amounts.
					if (DropAmmoFactor > 0)
					{
						weap->AmmoGive1 = FixedMul(weap->AmmoGive1, DropAmmoFactor);
						weap->AmmoGive2 = FixedMul(weap->AmmoGive2, DropAmmoFactor);
					}
					weap->BecomeItem();
				}
				else return false;
			}

			weap = barrier_cast<AWeapon*>(master);
			bool res = weap->CallTryPickup(toucher);
			if (res)
			{
				GoAwayAndDie();
				master = NULL;
			}
			return res;
		}
	}
	return false;
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
	return AddWeapon (PClass::FindClass (type));
}

bool FWeaponSlot::AddWeapon(const PClass *type)
{
	unsigned int i;
	
	if (type == NULL)
	{
		return false;
	}
	
	if (!type->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
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
	while (tok != NULL)
	{
		AddWeapon(tok);
		tok = strtok(NULL, " ");
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

int FWeaponSlot::LocateWeapon(const PClass *type)
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

	if (player->mo == NULL)
	{
		return NULL;
	}
	// Does this slot even have any weapons?
	if (Weapons.Size() == 0)
	{
		return player->ReadyWeapon;
	}
	if (player->ReadyWeapon != NULL)
	{
		for (i = 0; (unsigned)i < Weapons.Size(); i++)
		{
			if (Weapons[i].Type == player->ReadyWeapon->GetClass() ||
				(player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP &&
				 player->ReadyWeapon->SisterWeapon != NULL &&
				 player->ReadyWeapon->SisterWeapon->GetClass() == Weapons[i].Type))
			{
				for (j = (i == 0 ? Weapons.Size() - 1 : i - 1);
					j != i;
					j = (j == 0 ? Weapons.Size() - 1 : j - 1))
				{
					AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory(Weapons[j].Type));

					if (weap != NULL && weap->IsKindOf(RUNTIME_CLASS(AWeapon)))
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

		if (weap != NULL && weap->IsKindOf(RUNTIME_CLASS(AWeapon)))
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
		fixed_t pos = Weapons[i].Position;
		const PClass *type = Weapons[i].Type;
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

//===========================================================================
//
// FWeaponSlots :: LocateWeapon
//
// Returns true if the weapon is in a slot, false otherwise. If the weapon
// is found, it can also optionally return the slot and index for it.
//
//===========================================================================

bool FWeaponSlots::LocateWeapon (const PClass *type, int *const slot, int *const index)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		j = Slots[i].LocateWeapon(type);
		if (j >= 0)
		{
			if (slot != NULL) *slot = i;
			if (index != NULL) *index = j;
			return true;
		}
	}
	return false;
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
	else if (player->ReadyWeapon != NULL)
	{
		AWeapon *weap = player->ReadyWeapon;
		if (!player->weapons.LocateWeapon(weap->GetClass(), slot, index))
		{
			// If the current weapon wasn't found and is powered up,
			// look for its non-powered up version.
			if (weap->WeaponFlags & WIF_POWERED_UP && weap->SisterWeaponType != NULL)
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

	if (player->mo == NULL)
	{
		return NULL;
	}
	if (player->ReadyWeapon == NULL || FindMostRecentWeapon(player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == NULL)
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
			const PClass *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != NULL && weap->CheckAmmo(AWeapon::EitherFire, false))
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

	if (player->mo == NULL)
	{
		return NULL;
	}
	if (player->ReadyWeapon == NULL || FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == NULL)
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
			const PClass *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != NULL && weap->CheckAmmo(AWeapon::EitherFire, false))
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
	for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
	{
		PClass *cls = PClass::m_Types[i];

		if (cls->ActorInfo != NULL &&
			(cls->ActorInfo->GameFilter == GAME_Any || (cls->ActorInfo->GameFilter & gameinfo.gametype)) &&
			cls->ActorInfo->Replacement == NULL &&	// Replaced weapons don't get slotted.
			cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)) &&
			!(static_cast<AWeapon*>(GetDefaultByType(cls))->WeaponFlags & WIF_POWERED_UP) &&
			!LocateWeapon(cls, NULL, NULL)			// Don't duplicate it if it's already present.
			)
		{
			int slot = cls->Meta.GetMetaInt(AWMETA_SlotNumber, -1);
			if ((unsigned)slot < NUM_WEAPON_SLOTS)
			{
				fixed_t position = cls->Meta.GetMetaFixed(AWMETA_SlotPriority, INT_MAX);
				FWeaponSlot::WeaponInfo info = { cls, position };
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
		for (unsigned j = 0; j < gameinfo.DefaultWeaponSlots[i].Size(); i++)
		{
			const PClass *cls = PClass::FindClass(gameinfo.DefaultWeaponSlots[i][j]);
			if (cls == NULL)
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

void FWeaponSlots::StandardSetup(const PClass *type)
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

void FWeaponSlots::LocalSetup(const PClass *type)
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

void FWeaponSlots::SetFromPlayer(const PClass *type)
{
	Clear();
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		const char *str = type->Meta.GetMetaString(APMETA_Slot0 + i);
		if (str != NULL)
		{
			Slots[i].AddWeaponList(str, false);
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
		if (players[consoleplayer].mo != NULL)
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
	else if (PlayingKeyConf != NULL)
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
			Net_WriteWeapon(PClass::FindClass(argv[i]));
		}
	}
}

//===========================================================================
//
// CCMD addslot
//
//===========================================================================

void FWeaponSlots::AddSlot(int slot, const PClass *type, bool feedback)
{
	if (type != NULL && !Slots[slot].AddWeapon(type) && feedback)
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

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != NULL)
	{
		PlayingKeyConf->AddSlot(int(slot), PClass::FindClass(argv[2]), false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOT);
		Net_WriteByte(slot);
		Net_WriteWeapon(PClass::FindClass(argv[2]));
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
void FWeaponSlots::AddSlotDefault(int slot, const PClass *type, bool feedback)
{
	if (type != NULL && type->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
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
	const PClass *type;
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslotdefault <slot> <weapon>\n");
		return;
	}

	type = PClass::FindClass (argv[2]);
	if (type == NULL || !type->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{
		Printf ("%s is not a weapon\n", argv[2]);
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != NULL)
	{
		PlayingKeyConf->AddSlotDefault(int(slot), PClass::FindClass(argv[2]), false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOTDEFAULT);
		Net_WriteByte(slot);
		Net_WriteWeapon(PClass::FindClass(argv[2]));
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
	PlayingKeyConf = NULL;
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
	const PClass *cls;

	Weapons_ntoh.Clear();
	Weapons_hton.Clear();

	cls = NULL;
	Weapons_ntoh.Push(cls);		// Index 0 is always NULL.
	for (i = 0; i < PClass::m_Types.Size(); ++i)
	{
		PClass *cls = PClass::m_Types[i];

		if (cls->ActorInfo != NULL && cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
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

static int STACK_ARGS ntoh_cmp(const void *a, const void *b)
{
	const PClass *c1 = *(const PClass **)a;
	const PClass *c2 = *(const PClass **)b;
	int g1 = c1->ActorInfo->GameFilter == GAME_Any ? 1 : (c1->ActorInfo->GameFilter & gameinfo.gametype) ? 0 : 2;
	int g2 = c2->ActorInfo->GameFilter == GAME_Any ? 1 : (c2->ActorInfo->GameFilter & gameinfo.gametype) ? 0 : 2;
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

void P_WriteDemoWeaponsChunk(BYTE **demo)
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

void P_ReadDemoWeaponsChunk(BYTE **demo)
{
	int count, i;
	const PClass *type;
	const char *s;

	count = ReadWord(demo);
	Weapons_ntoh.Resize(count);
	Weapons_hton.Clear(count);

	Weapons_ntoh[0] = type = NULL;
	Weapons_hton[type] = 0;

	for (i = 1; i < count; ++i)
	{
		s = ReadStringConst(demo);
		type = PClass::FindClass(s);
		// If a demo was recorded with a weapon that is no longer present,
		// should we report it?
		Weapons_ntoh[i] = type;
		if (type != NULL)
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

void Net_WriteWeapon(const PClass *type)
{
	int index, *index_p;

	index_p = Weapons_hton.CheckKey(type);
	if (index_p == NULL)
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

const PClass *Net_ReadWeapon(BYTE **stream)
{
	int index;

	index = ReadByte(stream);
	if (index & 0x80)
	{
		index = (index & 0x7F) | (ReadByte(stream) << 7);
	}
	if ((unsigned)index >= Weapons_ntoh.Size())
	{
		return NULL;
	}
	return Weapons_ntoh[index];
}

//===========================================================================
//
// A_ZoomFactor
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AWeapon, A_ZoomFactor)
{
	ACTION_PARAM_START(2);
	ACTION_PARAM_FLOAT(zoom, 0);
	ACTION_PARAM_INT(flags, 1);

	if (self->player != NULL && self->player->ReadyWeapon != NULL)
	{
		zoom = 1 / clamp(zoom, 0.1f, 50.f);
		if (flags & 1)
		{ // Make the zoom instant.
			self->player->FOV = self->player->DesiredFOV * zoom;
		}
		if (flags & 2)
		{ // Disable pitch/yaw scaling.
			zoom = -zoom;
		}
		self->player->ReadyWeapon->FOVScale = zoom;
	}
}

//===========================================================================
//
// A_SetCrosshair
//
//===========================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AWeapon, A_SetCrosshair)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(xhair, 0);

	if (self->player != NULL && self->player->ReadyWeapon != NULL)
	{
		self->player->ReadyWeapon->Crosshair = xhair;
	}
}
