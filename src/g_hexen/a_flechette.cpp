/*
#include "actor.h"
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "w_wad.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_poisonbag ("PoisonBag");
static FRandom pr_poisoncloud ("PoisonCloud");
static FRandom pr_poisoncloudd ("PoisonCloudDamage");

DECLARE_ACTION(A_CheckThrowBomb)

// Poison Bag Artifact (Flechette) ------------------------------------------

class AArtiPoisonBag : public AInventory
{
	DECLARE_CLASS (AArtiPoisonBag, AInventory)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void BeginPlay ();
};

IMPLEMENT_CLASS (AArtiPoisonBag)

// Poison Bag 1 (The Cleric's) ----------------------------------------------

class AArtiPoisonBag1 : public AArtiPoisonBag
{
	DECLARE_CLASS (AArtiPoisonBag1, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiPoisonBag1)

bool AArtiPoisonBag1::Use (bool pickup)
{
	angle_t angle = Owner->angle >> ANGLETOFINESHIFT;
	AActor *mo;

	mo = Spawn ("PoisonBag",
		Owner->x+16*finecosine[angle],
		Owner->y+24*finesine[angle], Owner->z-
		Owner->floorclip+8*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = Owner;
		return true;
	}
	return false;
}

// Poison Bag 2 (The Mage's) ------------------------------------------------

class AArtiPoisonBag2 : public AArtiPoisonBag
{
	DECLARE_CLASS (AArtiPoisonBag2, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiPoisonBag2)

bool AArtiPoisonBag2::Use (bool pickup)
{
	angle_t angle = Owner->angle >> ANGLETOFINESHIFT;
	AActor *mo;

	mo = Spawn ("FireBomb",
		Owner->x+16*finecosine[angle],
		Owner->y+24*finesine[angle], Owner->z-
		Owner->floorclip+8*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = Owner;
		return true;
	}
	return false;
}

// Poison Bag 3 (The Fighter's) ---------------------------------------------

class AArtiPoisonBag3 : public AArtiPoisonBag
{
	DECLARE_CLASS (AArtiPoisonBag3, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiPoisonBag3)

bool AArtiPoisonBag3::Use (bool pickup)
{
	AActor *mo;

	mo = Spawn ("ThrowingBomb", Owner->x, Owner->y, 
		Owner->z-Owner->floorclip+35*FRACUNIT + (Owner->player? Owner->player->crouchoffset : 0), ALLOW_REPLACE);
	if (mo)
	{
		angle_t pitch = (angle_t)Owner->pitch >> ANGLETOFINESHIFT;

		mo->angle = Owner->angle+(((pr_poisonbag()&7)-4)<<24);
		mo->momz = 4*FRACUNIT + 2*finesine[pitch];
		mo->z += 2*finesine[pitch];
		P_ThrustMobj (mo, mo->angle, mo->Speed);
		mo->momx += Owner->momx>>1;
		mo->momy += Owner->momy>>1;
		mo->target = Owner;
		mo->tics -= pr_poisonbag()&3;
		P_CheckMissileSpawn (mo);
		return true;
	}
	return false;
}

//============================================================================
//
// AArtiPoisonBag :: HandlePickup
//
//============================================================================

bool AArtiPoisonBag::HandlePickup (AInventory *item)
{
	// Only do special handling when picking up the base class
	if (item->GetClass() != RUNTIME_CLASS(AArtiPoisonBag))
	{
		return Super::HandlePickup (item);
	}

	bool matched;

	if (Owner->IsKindOf (PClass::FindClass(NAME_ClericPlayer)))
	{
		matched = (GetClass() == RUNTIME_CLASS(AArtiPoisonBag1));
	}
	else if (Owner->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
	{
		matched = (GetClass() == RUNTIME_CLASS(AArtiPoisonBag2));
	}
	else
	{
		matched = (GetClass() == RUNTIME_CLASS(AArtiPoisonBag3));
	}
	if (matched)
	{
		if (Amount < MaxAmount)
		{
			Amount += item->Amount;
			if (Amount > MaxAmount)
			{
				Amount = MaxAmount;
			}
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

//============================================================================
//
// AArtiPoisonBag :: CreateCopy
//
//============================================================================

AInventory *AArtiPoisonBag::CreateCopy (AActor *other)
{
	// Only the base class gets special handling
	if (GetClass() != RUNTIME_CLASS(AArtiPoisonBag))
	{
		return Super::CreateCopy (other);
	}

	AInventory *copy;
	const PClass *spawntype;

	if (other->IsKindOf (PClass::FindClass(NAME_ClericPlayer)))
	{
		spawntype = RUNTIME_CLASS(AArtiPoisonBag1);
	}
	else if (other->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
	{
		spawntype = RUNTIME_CLASS(AArtiPoisonBag2);
	}
	else
	{
		spawntype = RUNTIME_CLASS(AArtiPoisonBag3);
	}
	copy = static_cast<AInventory *>(Spawn (spawntype, 0, 0, 0, NO_REPLACE));
	copy->Amount = Amount;
	copy->MaxAmount = MaxAmount;
	GoAwayAndDie ();
	return copy;
}

//============================================================================
//
// AArtiPoisonBag :: BeginPlay
//
//============================================================================

void AArtiPoisonBag::BeginPlay ()
{
	Super::BeginPlay ();
	// If a subclass's specific icon is not defined, let it use the base class's.
	if (!Icon.isValid())
	{
		AInventory *defbag;
		// Why doesn't this work?
		//defbag = GetDefault<AArtiPoisonBag>();
		defbag = (AInventory *)GetDefaultByType (RUNTIME_CLASS(AArtiPoisonBag));
		Icon = defbag->Icon;
	}
}

// Poison Cloud -------------------------------------------------------------

class APoisonCloud : public AActor
{
	DECLARE_CLASS (APoisonCloud, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
	void BeginPlay ();
};

IMPLEMENT_CLASS (APoisonCloud)

void APoisonCloud::BeginPlay ()
{
	momx = 1; // missile objects must move to impact other objects
	special1 = 24+(pr_poisoncloud()&7);
	special2 = 0;
}

int APoisonCloud::DoSpecialDamage (AActor *victim, int damage)
{
	if (victim->player)
	{
		bool mate = (target != NULL && victim->player != target->player && victim->IsTeammate (target));
		bool dopoison;

		if (!mate)
		{
			dopoison = victim->player->poisoncount < 4;
		}
		else
		{
			dopoison = victim->player->poisoncount < (int)(4.f * level.teamdamage);
		}

		if (dopoison)
		{
			int damage = 15 + (pr_poisoncloudd()&15);
			if (mate)
			{
				damage = (int)((float)damage * level.teamdamage);
			}
			if (damage > 0)
			{
				P_PoisonDamage (victim->player, this,
					15+(pr_poisoncloudd()&15), false); // Don't play painsound

				// If successful, play the posion sound.
				if (P_PoisonPlayer (victim->player, this, this->target, 50))
					S_Sound (victim, CHAN_VOICE, "*poison", 1, ATTN_NORM);
			}
		}	
		return -1;
	}
	else if (!(victim->flags3 & MF3_ISMONSTER))
	{ // only damage monsters/players with the poison cloud
		return -1;
	}
	return damage;
}

//===========================================================================
//
// A_PoisonBagInit
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PoisonBagInit)
{
	AActor *mo;
	
	mo = Spawn<APoisonCloud> (self->x, self->y, self->z+28*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = self->target;
	}
}

//===========================================================================
//
// A_PoisonBagCheck
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PoisonBagCheck)
{
	if (--self->special1 <= 0)
	{
		self->SetState (self->FindState ("Death"));
	}
	else
	{
		return;
	}
}

//===========================================================================
//
// A_PoisonBagDamage
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PoisonBagDamage)
{
	int bobIndex;
	
	P_RadiusAttack (self, self->target, 4, 40, self->DamageType, true);
	bobIndex = self->special2;
	self->z += FloatBobOffsets[bobIndex]>>4;
	self->special2 = (bobIndex+1)&63;
}

//===========================================================================
//
// A_CheckThrowBomb
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CheckThrowBomb)
{
	if (--self->health <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
	}
}

//===========================================================================
//
// A_CheckThrowBomb2
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CheckThrowBomb2)
{
	// [RH] Check using actual velocity, although the momz < 2 check still stands
	//if (abs(self->momx) < FRACUNIT*3/2 && abs(self->momy) < FRACUNIT*3/2
	//	&& self->momz < 2*FRACUNIT)
	if (self->momz < 2*FRACUNIT &&
		TMulScale32 (self->momx, self->momx, self->momy, self->momy, self->momz, self->momz)
		< (3*3)/(2*2))
	{
		self->SetState (self->SpawnState + 6);
		self->z = self->floorz;
		self->momz = 0;
		self->bouncetype = BOUNCE_None;
		self->flags &= ~MF_MISSILE;
	}
	CALL_ACTION(A_CheckThrowBomb, self);
}
