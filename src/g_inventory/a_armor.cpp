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
IMPLEMENT_CLASS(AHexenArmor, false, false)

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