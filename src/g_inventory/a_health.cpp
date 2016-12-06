/*
** a_health.cpp
** All health items
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

#include "d_player.h"
#include "a_morph.h"
#include "a_health.h"
#include "serializer.h"

//---------------------------------------------------------------------------
//
// FUNC P_GiveBody
//
// Returns false if the body isn't needed at all.
//
//---------------------------------------------------------------------------

bool P_GiveBody (AActor *actor, int num, int max)
{
	if (actor->health <= 0 || (actor->player != NULL && actor->player->playerstate == PST_DEAD))
	{ // Do not heal dead things.
		return false;
	}

	player_t *player = actor->player;

	num = clamp(num, -65536, 65536);	// prevent overflows for bad values
	if (player != NULL)
	{
		// Max is 0 by default, preserving default behavior for P_GiveBody()
		// calls while supporting AHealth.
		if (max <= 0)
		{
			max = static_cast<APlayerPawn*>(actor)->GetMaxHealth() + player->mo->stamina;
			// [MH] First step in predictable generic morph effects
 			if (player->morphTics)
 			{
				if (player->MorphStyle & MORPH_FULLHEALTH)
				{
					if (!(player->MorphStyle & MORPH_ADDSTAMINA))
					{
						max -= player->mo->stamina;
					}
				}
				else // old health behaviour
				{
					max = MAXMORPHHEALTH;
					if (player->MorphStyle & MORPH_ADDSTAMINA)
					{
						max += player->mo->stamina;
					}
				}
 			}
		}
		// [RH] For Strife: A negative body sets you up with a percentage
		// of your full health.
		if (num < 0)
		{
			num = max * -num / 100;
			if (player->health < num)
			{
				player->health = num;
				actor->health = num;
				return true;
			}
		}
		else if (num > 0)
		{
			if (player->health < max)
			{
				num = int(num * G_SkillProperty(SKILLP_HealthFactor));
				if (num < 1) num = 1;
				player->health += num;
				if (player->health > max)
				{
					player->health = max;
				}
				actor->health = player->health;
				return true;
			}
		}
	}
	else
	{
		// Parameter value for max is ignored on monsters, preserving original
		// behaviour on AHealth as well as on existing calls to P_GiveBody().
		max = actor->SpawnHealth();
		if (num < 0)
		{
			num = max * -num / 100;
			if (actor->health < num)
			{
				actor->health = num;
				return true;
			}
		}
		else if (actor->health < max)
		{
			actor->health += num;
			if (actor->health > max)
			{
				actor->health = max;
			}
			return true;
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, GiveBody)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(num);
	PARAM_INT_DEF(max);
	ACTION_RETURN_BOOL(P_GiveBody(self, num, max));
}

//===========================================================================
//
// Classes
//
//===========================================================================

IMPLEMENT_CLASS(PClassHealth, false, false)
IMPLEMENT_CLASS(AHealth, false, false)
DEFINE_FIELD(AHealth, PrevHealth)

//===========================================================================
//
// PClassHealth Constructor
//
//===========================================================================

PClassHealth::PClassHealth()
{
	LowHealth = 0;
}

//===========================================================================
//
// PClassHealth :: DeriveData
//
//===========================================================================

void PClassHealth::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassHealth)));
	Super::DeriveData(newclass);
	PClassHealth *newc = static_cast<PClassHealth *>(newclass);
	
	newc->LowHealth = LowHealth;
	newc->LowHealthMessage = LowHealthMessage;
}


//===========================================================================
//
// AHealth :: PickupMessage
//
//===========================================================================
FString AHealth::PickupMessage ()
{
	int threshold = GetClass()->LowHealth;

	if (PrevHealth < threshold)
	{
		FString message = GetClass()->LowHealthMessage;

		if (message.IsNotEmpty())
		{
			return message;
		}
	}
	return Super::PickupMessage();
}

//===========================================================================
//
// AHealth :: TryPickup
//
//===========================================================================

bool AHealth::TryPickup (AActor *&other)
{
	PrevHealth = other->player != NULL ? other->player->health : other->health;

	// P_GiveBody adds one new feature, applied only if it is possible to pick up negative health:
	// Negative values are treated as positive percentages, ie Amount -100 means 100% health, ignoring max amount.
	if (P_GiveBody(other, Amount, MaxAmount))
	{
		GoAwayAndDie();
		return true;
	}
	return false;
}

IMPLEMENT_CLASS(AHealthPickup, false, false)

DEFINE_FIELD(AHealthPickup, autousemode)

//===========================================================================
//
// AHealthPickup :: CreateCopy
//
//===========================================================================

AInventory *AHealthPickup::CreateCopy (AActor *other)
{
	AInventory *copy = Super::CreateCopy (other);
	copy->health = health;
	return copy;
}

//===========================================================================
//
// AHealthPickup :: CreateTossable
//
//===========================================================================

AInventory *AHealthPickup::CreateTossable ()
{
	AInventory *copy = Super::CreateTossable ();
	if (copy != NULL)
	{
		copy->health = health;
	}
	return copy;
}

//===========================================================================
//
// AHealthPickup :: HandlePickup
//
//===========================================================================

bool AHealthPickup::HandlePickup (AInventory *item)
{
	// HealthPickups that are the same type but have different health amounts
	// do not count as the same item.
	if (item->health == health)
	{
		return Super::HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AHealthPickup :: Use
//
//===========================================================================

bool AHealthPickup::Use (bool pickup)
{
	return P_GiveBody (Owner, health, 0);
}

//===========================================================================
//
// AHealthPickup :: Serialize
//
//===========================================================================

void AHealthPickup::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	auto def = (AHealthPickup*)GetDefault();
	arc("autousemode", autousemode, def->autousemode);
}

