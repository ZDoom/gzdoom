/*
** a_armor.cpp
** Implements all variations of armor objects
**
**---------------------------------------------------------------------------
** Copyright 2002-2016 Randy Heit
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

#include <assert.h>

#include "info.h"
#include "gi.h"
#include "a_pickups.h"
#include "a_armor.h"
#include "templates.h"
#include "g_level.h"
#include "d_player.h"
#include "serializer.h"
#include "cmdlib.h"

IMPLEMENT_CLASS(AArmor, false, false)
IMPLEMENT_CLASS(ABasicArmor, false, false)
IMPLEMENT_CLASS(ABasicArmorPickup, false, false)
IMPLEMENT_CLASS(ABasicArmorBonus, false, false)
IMPLEMENT_CLASS(AHexenArmor, false, false)

//===========================================================================
//
//
// BasicArmor
//
//
//===========================================================================

DEFINE_FIELD(ABasicArmor, AbsorbCount)
DEFINE_FIELD(ABasicArmor, SavePercent)
DEFINE_FIELD(ABasicArmor, MaxAbsorb)
DEFINE_FIELD(ABasicArmor, MaxFullAbsorb)
DEFINE_FIELD(ABasicArmor, BonusCount)
DEFINE_FIELD(ABasicArmor, ArmorType)
DEFINE_FIELD(ABasicArmor, ActualSaveAmount)

//===========================================================================
//
// ABasicArmor :: Serialize
//
//===========================================================================

void ABasicArmor::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (ABasicArmor *)GetDefault();
	arc("savepercent", SavePercent, def->SavePercent)
		("bonuscount", BonusCount, def->BonusCount)
		("maxabsorb", MaxAbsorb, def->MaxAbsorb)
		("maxfullabsorb", MaxFullAbsorb, def->MaxFullAbsorb)
		("absorbcount", AbsorbCount, def->AbsorbCount)
		("armortype", ArmorType, def->ArmorType)
		("actualsaveamount", ActualSaveAmount, def->ActualSaveAmount);
}

//===========================================================================
//
// ABasicArmor :: Tick
//
// If BasicArmor is given to the player by means other than a
// BasicArmorPickup, then it may not have an icon set. Fix that here.
//
//===========================================================================

void ABasicArmor::Tick ()
{
	Super::Tick ();
	AbsorbCount = 0;
	if (!Icon.isValid())
	{
		FString icon = gameinfo.ArmorIcon1;

		if (SavePercent >= gameinfo.Armor2Percent && gameinfo.ArmorIcon2.Len() != 0)
			icon = gameinfo.ArmorIcon2;

		if (icon[0] != 0)
			Icon = TexMan.CheckForTexture (icon, FTexture::TEX_Any);
	}
}

//===========================================================================
//
// ABasicArmor :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmor::CreateCopy (AActor *other)
{
	// BasicArmor that is in use is stored in the inventory as BasicArmor.
	// BasicArmor that is in reserve is not.
	ABasicArmor *copy = Spawn<ABasicArmor> ();
	copy->SavePercent = SavePercent != 0 ? SavePercent : 0.33335;	// slightly more than 1/3 to avoid roundoff errors.
	copy->Amount = Amount;
	copy->MaxAmount = MaxAmount;
	copy->Icon = Icon;
	copy->BonusCount = BonusCount;
	copy->ArmorType = ArmorType;
	copy->ActualSaveAmount = ActualSaveAmount;
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// ABasicArmor :: HandlePickup
//
//===========================================================================

bool ABasicArmor::HandlePickup (AInventory *item)
{
	if (item->GetClass() == RUNTIME_CLASS(ABasicArmor))
	{
		// You shouldn't be picking up BasicArmor anyway.
		return true;
	}
	if (item->IsKindOf(RUNTIME_CLASS(ABasicArmorBonus)) && !(item->ItemFlags & IF_IGNORESKILL))
	{
		ABasicArmorBonus *armor = static_cast<ABasicArmorBonus*>(item);

		armor->SaveAmount = int(armor->SaveAmount * G_SkillProperty(SKILLP_ArmorFactor));
	}
	else if (item->IsKindOf(RUNTIME_CLASS(ABasicArmorPickup)) && !(item->ItemFlags & IF_IGNORESKILL))
	{
		ABasicArmorPickup *armor = static_cast<ABasicArmorPickup*>(item);

		armor->SaveAmount = int(armor->SaveAmount * G_SkillProperty(SKILLP_ArmorFactor));
	}
	return false;
}

//===========================================================================
//
// ABasicArmor :: AbsorbDamage
//
//===========================================================================

void ABasicArmor::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	int saved;

	if (!DamageTypeDefinition::IgnoreArmor(damageType))
	{
		int full = MAX(0, MaxFullAbsorb - AbsorbCount);
		if (damage < full)
		{
			saved = damage;
		}
		else
		{
			saved = full + int((damage - full) * SavePercent);
			if (MaxAbsorb > 0 && saved + AbsorbCount > MaxAbsorb) 
			{
				saved = MAX(0,  MaxAbsorb - AbsorbCount);
			}
		}

		if (Amount < saved)
		{
			saved = Amount;
		}
		newdamage -= saved;
		Amount -= saved;
		AbsorbCount += saved;
		if (Amount == 0)
		{
			// The armor has become useless
			SavePercent = 0;
			ArmorType = NAME_None; // Not NAME_BasicArmor.
			// Now see if the player has some more armor in their inventory
			// and use it if so. As in Strife, the best armor is used up first.
			ABasicArmorPickup *best = NULL;
			AInventory *probe = Owner->Inventory;
			while (probe != NULL)
			{
				if (probe->IsKindOf (RUNTIME_CLASS(ABasicArmorPickup)))
				{
					ABasicArmorPickup *inInv = static_cast<ABasicArmorPickup*>(probe);
					if (best == NULL || best->SavePercent < inInv->SavePercent)
					{
						best = inInv;
					}
				}
				probe = probe->Inventory;
			}
			if (best != NULL)
			{
				Owner->UseInventory (best);
			}
		}
		damage = newdamage;
	}

	// Once the armor has absorbed its part of the damage, then apply its damage factor, if any, to the player
	if ((damage > 0) && (ArmorType != NAME_None)) // BasicArmor is not going to have any damage factor, so skip it.
	{
		// This code is taken and adapted from APowerProtection::ModifyDamage().
		// The differences include not using a default value, and of course the way
		// the damage factor info is obtained.
		DmgFactors *df = PClass::FindActor(ArmorType)->DamageFactors;
		if (df != NULL)
		{
			damage = newdamage = df->Apply(damageType, damage);
		}
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
//
// BasicArmorPickup
//
//
//===========================================================================


DEFINE_FIELD(ABasicArmorPickup, SavePercent)
DEFINE_FIELD(ABasicArmorPickup, MaxAbsorb)
DEFINE_FIELD(ABasicArmorPickup, MaxFullAbsorb)
DEFINE_FIELD(ABasicArmorPickup, SaveAmount)

//===========================================================================
//
// ABasicArmorPickup :: Serialize
//
//===========================================================================

void ABasicArmorPickup::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);

	auto def = (ABasicArmorPickup *)GetDefault();
	arc("savepercent", SavePercent, def->SavePercent)
		("saveamount", SaveAmount, def->SaveAmount)
		("maxabsorb", MaxAbsorb, def->MaxAbsorb)
		("maxfullabsorb", MaxFullAbsorb, def->MaxFullAbsorb);
}

//===========================================================================
//
// ABasicArmorPickup :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorPickup::CreateCopy (AActor *other)
{
	ABasicArmorPickup *copy = static_cast<ABasicArmorPickup *> (Super::CreateCopy (other));

	if (!(ItemFlags & IF_IGNORESKILL))
	{
		SaveAmount = int(SaveAmount * G_SkillProperty(SKILLP_ArmorFactor));
	}

	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxAbsorb = MaxAbsorb;
	copy->MaxFullAbsorb = MaxFullAbsorb;

	return copy;
}

//===========================================================================
//
// ABasicArmorPickup :: Use
//
// Either gives you new armor or replaces the armor you already have (if
// the SaveAmount is greater than the amount of armor you own). When the
// item is auto-activated, it will only be activated if its max amount is 0
// or if you have no armor active already.
//
//===========================================================================

bool ABasicArmorPickup::Use (bool pickup)
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();

	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> ();
		armor->BecomeItem ();
		Owner->AddInventory (armor);
	}
	else
	{
		// If you already have more armor than this item gives you, you can't
		// use it.
		if (armor->Amount >= SaveAmount + armor->BonusCount)
		{
			return false;
		}
		// Don't use it if you're picking it up and already have some.
		if (pickup && armor->Amount > 0 && MaxAmount > 0)
		{
			return false;
		}
	}
	armor->SavePercent = SavePercent;
	armor->Amount = SaveAmount + armor->BonusCount;
	armor->MaxAmount = SaveAmount;
	armor->Icon = Icon;
	armor->MaxAbsorb = MaxAbsorb;
	armor->MaxFullAbsorb = MaxFullAbsorb;
	armor->ArmorType = this->GetClass()->TypeName;
	armor->ActualSaveAmount = SaveAmount;
	return true;
}

//===========================================================================
//
//
// BasicArmorBonus
//
//
//===========================================================================


DEFINE_FIELD(ABasicArmorBonus, SavePercent)
DEFINE_FIELD(ABasicArmorBonus, MaxSaveAmount)
DEFINE_FIELD(ABasicArmorBonus, MaxAbsorb)
DEFINE_FIELD(ABasicArmorBonus, MaxFullAbsorb)
DEFINE_FIELD(ABasicArmorBonus, SaveAmount)
DEFINE_FIELD(ABasicArmorBonus, BonusCount)
DEFINE_FIELD(ABasicArmorBonus, BonusMax)

//===========================================================================
//
// ABasicArmorBonus :: Serialize
//
//===========================================================================

void ABasicArmorBonus::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (ABasicArmorBonus *)GetDefault();
	arc("savepercent", SavePercent, def->SavePercent)
		("saveamount", SaveAmount, def->SaveAmount)
		("maxsaveamount", MaxSaveAmount, def->MaxSaveAmount)
		("bonuscount", BonusCount, def->BonusCount)
		("bonusmax", BonusMax, def->BonusMax)
		("maxabsorb", MaxAbsorb, def->MaxAbsorb)
		("maxfullabsorb", MaxFullAbsorb, def->MaxFullAbsorb);
}

//===========================================================================
//
// ABasicArmorBonus :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorBonus::CreateCopy (AActor *other)
{
	ABasicArmorBonus *copy = static_cast<ABasicArmorBonus *> (Super::CreateCopy (other));

	if (!(ItemFlags & IF_IGNORESKILL))
	{
		SaveAmount = int(SaveAmount * G_SkillProperty(SKILLP_ArmorFactor));
	}

	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxSaveAmount = MaxSaveAmount;
	copy->BonusCount = BonusCount;
	copy->BonusMax = BonusMax;
	copy->MaxAbsorb = MaxAbsorb;
	copy->MaxFullAbsorb = MaxFullAbsorb;

	return copy;
}

//===========================================================================
//
// ABasicArmorBonus :: Use
//
// Tries to add to the amount of BasicArmor a player has.
//
//===========================================================================

bool ABasicArmorBonus::Use (bool pickup)
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();
	bool result = false;

	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> ();
		armor->BecomeItem ();
		armor->Amount = 0;
		armor->MaxAmount = MaxSaveAmount;
		Owner->AddInventory (armor);
	}

	if (BonusCount > 0 && armor->BonusCount < BonusMax)
	{
		armor->BonusCount = MIN (armor->BonusCount + BonusCount, BonusMax);
		result = true;
	}

	int saveAmount = MIN (SaveAmount, MaxSaveAmount);

	if (saveAmount <= 0)
	{ // If it can't give you anything, it's as good as used.
		return BonusCount > 0 ? result : true;
	}

	// If you already have more armor than this item can give you, you can't
	// use it.
	if (armor->Amount >= MaxSaveAmount + armor->BonusCount)
	{
		return result;
	}

	if (armor->Amount <= 0)
	{ // Should never be less than 0, but might as well check anyway
		armor->Amount = 0;
		armor->Icon = Icon;
		armor->SavePercent = SavePercent;
		armor->MaxAbsorb = MaxAbsorb;
		armor->ArmorType = this->GetClass()->TypeName;
		armor->MaxFullAbsorb = MaxFullAbsorb;
		armor->ActualSaveAmount = MaxSaveAmount;
	}

	armor->Amount = MIN(armor->Amount + saveAmount, MaxSaveAmount + armor->BonusCount);
	armor->MaxAmount = MAX (armor->MaxAmount, MaxSaveAmount);
	return true;
}

//===========================================================================
//
//
// HexenArmor
//
//
//===========================================================================

DEFINE_FIELD(AHexenArmor, Slots)
DEFINE_FIELD(AHexenArmor, SlotsIncrement)

//===========================================================================
//
// AHexenArmor :: Serialize
//
//===========================================================================

void AHexenArmor::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AHexenArmor *)GetDefault();
	arc.Array("slots", Slots, def->Slots, 5, true)
		.Array("slotsincrement", SlotsIncrement, def->SlotsIncrement, 4);
}

//===========================================================================
//
// AHexenArmor :: CreateCopy
//
//===========================================================================

AInventory *AHexenArmor::CreateCopy (AActor *other)
{
	// Like BasicArmor, HexenArmor is used in the inventory but not the map.
	// health is the slot this armor occupies.
	// Amount is the quantity to give (0 = normal max).
	AHexenArmor *copy = Spawn<AHexenArmor> ();
	copy->AddArmorToSlot (other, health, Amount);
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// AHexenArmor :: CreateTossable
//
// Since this isn't really a single item, you can't drop it. Ever.
//
//===========================================================================

AInventory *AHexenArmor::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// AHexenArmor :: HandlePickup
//
//===========================================================================

bool AHexenArmor::HandlePickup (AInventory *item)
{
	if (item->IsKindOf (RUNTIME_CLASS(AHexenArmor)))
	{
		if (AddArmorToSlot (Owner, item->health, item->Amount))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AddArmorToSlot
//
//===========================================================================

bool AHexenArmor::AddArmorToSlot (AActor *actor, int slot, int amount)
{
	APlayerPawn *ppawn;
	double hits;

	if (actor->player != NULL)
	{
		ppawn = static_cast<APlayerPawn *>(actor);
	}
	else
	{
		ppawn = NULL;
	}

	if (slot < 0 || slot > 3)
	{
		return false;
	}
	if (amount <= 0)
	{
		hits = SlotsIncrement[slot];
		if (Slots[slot] < hits)
		{
			Slots[slot] = hits;
			return true;
		}
	}
	else
	{
		hits = amount * 5;
		auto total = Slots[0] + Slots[1] + Slots[2] + Slots[3] + Slots[4];
		auto max = SlotsIncrement[0] + SlotsIncrement[1] + SlotsIncrement[2] + SlotsIncrement[3] + Slots[4] + 4 * 5;
		if (total < max)
		{
			Slots[slot] += hits;
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AbsorbDamage
//
//===========================================================================

void AHexenArmor::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (!DamageTypeDefinition::IgnoreArmor(damageType))
	{
		double savedPercent = Slots[0] + Slots[1] + Slots[2] + Slots[3] + Slots[4];

		if (savedPercent)
		{ // armor absorbed some damage
			if (savedPercent > 100)
			{
				savedPercent = 100;
			}
			for (int i = 0; i < 4; i++)
			{
				if (Slots[i])
				{
					// 300 damage always wipes out the armor unless some was added
					// with the dragon skin bracers.
					if (damage < 10000)
					{
						Slots[i] -= damage * SlotsIncrement[i] / 300.;
						if (Slots[i] < 2)
						{
							Slots[i] = 0;
						}
					}
					else
					{
						Slots[i] = 0;
					}
				}
			}
			int saved = int(damage * savedPercent / 100.);
			if (saved > savedPercent*2)
			{	
				saved = int(savedPercent*2);
			}
			newdamage -= saved;
			damage = newdamage;
		}
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// AHexenArmor :: DepleteOrDestroy
//
//===========================================================================

void AHexenArmor::DepleteOrDestroy()
{
	for (int i = 0; i < 4; i++)
	{
		Slots[i] = 0;
	}
}