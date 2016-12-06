/*
** a_ammo.cpp
** Implements ammo and backpack items.
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

#include "c_dispatch.h"
#include "d_player.h"
#include "serializer.h"

IMPLEMENT_CLASS(PClassAmmo, false, false)

PClassAmmo::PClassAmmo()
{
	DropAmount = 0;
}

void PClassAmmo::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassAmmo)));
	Super::DeriveData(newclass);
	PClassAmmo *newc = static_cast<PClassAmmo *>(newclass);

	newc->DropAmount = DropAmount;
}

IMPLEMENT_CLASS(AAmmo, false, false)

DEFINE_FIELD(AAmmo, BackpackAmount)
DEFINE_FIELD(AAmmo, BackpackMaxAmount)

//===========================================================================
//
// AAmmo :: Serialize
//
//===========================================================================

void AAmmo::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AAmmo*)GetDefault();
	arc("backpackamount", BackpackAmount, def->BackpackAmount)
		("backpackmaxamount", BackpackMaxAmount, def->BackpackMaxAmount);
}

//===========================================================================
//
// AAmmo :: GetParentAmmo
//
// Returns the least-derived ammo type that this ammo is a descendant of.
// That is, if this ammo is an immediate subclass of Ammo, then this ammo's
// type is returned. If this ammo's superclass is not Ammo, then this
// function travels up the inheritance chain until it finds a type that is
// an immediate subclass of Ammo and returns that.
//
// The intent of this is that all unique ammo types will be immediate
// subclasses of Ammo. To make different pickups with different ammo amounts,
// you subclass the type of ammo you want a different amount for and edit
// that.
//
//===========================================================================

PClassActor *AAmmo::GetParentAmmo () const
{
	PClass *type = GetClass();

	while (type->ParentClass != RUNTIME_CLASS(AAmmo) && type->ParentClass != NULL)
	{
		type = type->ParentClass;
	}
	return static_cast<PClassActor *>(type);
}

//===========================================================================
//
// AAmmo :: HandlePickup
//
//===========================================================================
EXTERN_CVAR(Bool, sv_unlimited_pickup)

bool AAmmo::HandlePickup (AInventory *item)
{
	if (GetClass() == item->GetClass() ||
		(item->IsKindOf (RUNTIME_CLASS(AAmmo)) && static_cast<AAmmo*>(item)->GetParentAmmo() == GetClass()))
	{
		if (Amount < MaxAmount || sv_unlimited_pickup)
		{
			int receiving = item->Amount;

			if (!(item->ItemFlags & IF_IGNORESKILL))
			{ // extra ammo in baby mode and nightmare mode
				receiving = int(receiving * G_SkillProperty(SKILLP_AmmoFactor));
			}
			int oldamount = Amount;

			if (Amount > 0 && Amount + receiving < 0)
			{
				Amount = 0x7fffffff;
			}
			else
			{
				Amount += receiving;
			}
			if (Amount > MaxAmount && !sv_unlimited_pickup)
			{
				Amount = MaxAmount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;

			// If the player previously had this ammo but ran out, possibly switch
			// to a weapon that uses it, but only if the player doesn't already
			// have a weapon pending.

			assert (Owner != NULL);

			if (oldamount == 0 && Owner != NULL && Owner->player != NULL)
			{
				barrier_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(GetClass());
			}
		}
		return true;
	}
	return false;
}

//===========================================================================
//
// AAmmo :: CreateCopy
//
//===========================================================================

AInventory *AAmmo::CreateCopy (AActor *other)
{
	AInventory *copy;
	int amount = Amount;

	// extra ammo in baby mode and nightmare mode
	if (!(ItemFlags&IF_IGNORESKILL))
	{
		amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
	}

	if (GetClass()->ParentClass != RUNTIME_CLASS(AAmmo) && GetClass() != RUNTIME_CLASS(AAmmo))
	{
		PClassActor *type = GetParentAmmo();
		if (!GoAway ())
		{
			Destroy ();
		}

		copy = static_cast<AInventory *>(Spawn (type));
		copy->Amount = amount;
		copy->BecomeItem ();
	}
	else
	{
		copy = Super::CreateCopy (other);
		copy->Amount = amount;
	}
	if (copy->Amount > copy->MaxAmount)
	{ // Don't pick up more ammo than you're supposed to be able to carry.
		copy->Amount = copy->MaxAmount;
	}
	return copy;
}

//===========================================================================
//
// AAmmo :: CreateTossable
//
//===========================================================================

AInventory *AAmmo::CreateTossable()
{
	AInventory *copy = Super::CreateTossable();
	if (copy != NULL)
	{ // Do not increase ammo by dropping it and picking it back up at
	  // certain skill levels.
		copy->ItemFlags |= IF_IGNORESKILL;
	}
	return copy;
}


// Backpack -----------------------------------------------------------------

IMPLEMENT_CLASS(ABackpackItem, false, false)

DEFINE_FIELD(ABackpackItem, bDepleted)

//===========================================================================
//
// ABackpackItem :: Serialize
//
//===========================================================================

void ABackpackItem::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (ABackpackItem*)GetDefault();
	arc("bdepleted", bDepleted, def->bDepleted);
}

//===========================================================================
//
// ABackpackItem :: CreateCopy
//
// A backpack is being added to a player who doesn't yet have one. Give them
// every kind of ammo, and increase their max amounts.
//
//===========================================================================

AInventory *ABackpackItem::CreateCopy (AActor *other)
{
	// Find every unique type of ammo. Give it to the player if
	// he doesn't have it already, and double its maximum capacity.
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClass *type = PClassActor::AllActorClasses[i];

		if (type->ParentClass == RUNTIME_CLASS(AAmmo))
		{
			PClassActor *atype = static_cast<PClassActor *>(type);
			AAmmo *ammo = static_cast<AAmmo *>(other->FindInventory(atype));
			int amount = static_cast<AAmmo *>(GetDefaultByType(type))->BackpackAmount;
			// extra ammo in baby mode and nightmare mode
			if (!(ItemFlags&IF_IGNORESKILL))
			{
				amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
			}
			if (amount < 0) amount = 0;
			if (ammo == NULL)
			{ // The player did not have the ammo. Add it.
				ammo = static_cast<AAmmo *>(Spawn(atype));
				ammo->Amount = bDepleted ? 0 : amount;
				if (ammo->BackpackMaxAmount > ammo->MaxAmount)
				{
					ammo->MaxAmount = ammo->BackpackMaxAmount;
				}
				if (ammo->Amount > ammo->MaxAmount)
				{
					ammo->Amount = ammo->MaxAmount;
				}
				ammo->AttachToOwner (other);
			}
			else
			{ // The player had the ammo. Give some more.
				if (ammo->MaxAmount < ammo->BackpackMaxAmount)
				{
					ammo->MaxAmount = ammo->BackpackMaxAmount;
				}
				if (!bDepleted && ammo->Amount < ammo->MaxAmount)
				{
					ammo->Amount += amount;
					if (ammo->Amount > ammo->MaxAmount)
					{
						ammo->Amount = ammo->MaxAmount;
					}
				}
			}
		}
	}
	return Super::CreateCopy (other);
}

//===========================================================================
//
// ABackpackItem :: HandlePickup
//
// When the player picks up another backpack, just give them more ammo.
//
//===========================================================================

bool ABackpackItem::HandlePickup (AInventory *item)
{
	// Since you already have a backpack, that means you already have every
	// kind of ammo in your inventory, so we don't need to look at the
	// entire PClass list to discover what kinds of ammo exist, and we don't
	// have to alter the MaxAmount either.
	if (item->IsKindOf (RUNTIME_CLASS(ABackpackItem)))
	{
		for (AInventory *probe = Owner->Inventory; probe != NULL; probe = probe->Inventory)
		{
			if (probe->GetClass()->ParentClass == RUNTIME_CLASS(AAmmo))
			{
				if (probe->Amount < probe->MaxAmount || sv_unlimited_pickup)
				{
					int amount = static_cast<AAmmo*>(probe->GetDefault())->BackpackAmount;
					// extra ammo in baby mode and nightmare mode
					if (!(item->ItemFlags&IF_IGNORESKILL))
					{
						amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
					}
					probe->Amount += amount;
					if (probe->Amount > probe->MaxAmount && !sv_unlimited_pickup)
					{
						probe->Amount = probe->MaxAmount;
					}
				}
			}
		}
		// The pickup always succeeds, even if you didn't get anything
		item->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return false;
}

//===========================================================================
//
// ABackpackItem :: CreateTossable
//
// The tossed backpack must not give out any more ammo, otherwise a player
// could cheat by dropping their backpack and picking it up for more ammo.
//
//===========================================================================

AInventory *ABackpackItem::CreateTossable ()
{
	ABackpackItem *pack = static_cast<ABackpackItem *>(Super::CreateTossable());
	if (pack != NULL)
	{
		pack->bDepleted = true;
	}
	return pack;
}

//===========================================================================
//
// ABackpackItem :: DetachFromOwner
//
//===========================================================================

void ABackpackItem::DetachFromOwner ()
{
	// When removing a backpack, drop the player's ammo maximums to normal
	AInventory *item;

	for (item = Owner->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->GetClass()->ParentClass == RUNTIME_CLASS(AAmmo) &&
			item->MaxAmount == static_cast<AAmmo*>(item)->BackpackMaxAmount)
		{
			item->MaxAmount = static_cast<AInventory*>(item->GetDefault())->MaxAmount;
			if (item->Amount > item->MaxAmount)
			{
				item->Amount = item->MaxAmount;
			}
		}
	}
}

