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

EXTERN_CVAR(Bool, sv_unlimited_pickup)

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

	mo = Spawn ("PoisonBag", Owner->Vec3Offset(
		16*finecosine[angle],
		24*finesine[angle], 
		-Owner->floorclip+8*FRACUNIT), ALLOW_REPLACE);
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

	mo = Spawn ("FireBomb", Owner->Vec3Offset(
		16*finecosine[angle],
		24*finesine[angle], 
		-Owner->floorclip+8*FRACUNIT), ALLOW_REPLACE);
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

	mo = Spawn("ThrowingBomb", Owner->PosPlusZ(-Owner->floorclip+35*FRACUNIT + (Owner->player? Owner->player->crouchoffset : 0)), ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = Owner->angle + (((pr_poisonbag()&7) - 4) << 24);

		/* Original flight code from Hexen
		 * mo->momz = 4*FRACUNIT+((player->lookdir)<<(FRACBITS-4));
		 * mo->z += player->lookdir<<(FRACBITS-4);
		 * P_ThrustMobj(mo, mo->angle, mo->info->speed);
		 * mo->momx += player->mo->momx>>1;
		 * mo->momy += player->mo->momy>>1;
		 */

		// When looking straight ahead, it uses a z velocity of 4 while the xy velocity
		// is as set by the projectile. To accommodate this with a proper trajectory, we
		// aim the projectile ~20 degrees higher than we're looking at and increase the
		// speed we fire at accordingly.
		angle_t orgpitch = angle_t(-Owner->pitch) >> ANGLETOFINESHIFT;
		angle_t modpitch = angle_t(0xDC00000 - Owner->pitch) >> ANGLETOFINESHIFT;
		angle_t angle = mo->angle >> ANGLETOFINESHIFT;
		fixed_t speed = fixed_t(sqrt((double)mo->Speed*mo->Speed + (4.0*65536*4*65536)));
		fixed_t xyscale = FixedMul(speed, finecosine[modpitch]);

		mo->velz = FixedMul(speed, finesine[modpitch]);
		mo->velx = FixedMul(xyscale, finecosine[angle]) + (Owner->velx >> 1);
		mo->vely = FixedMul(xyscale, finesine[angle]) + (Owner->vely >> 1);
		mo->AddZ(FixedMul(mo->Speed, finesine[orgpitch]));

		mo->target = Owner;
		mo->tics -= pr_poisonbag()&3;
		P_CheckMissileSpawn(mo, Owner->radius);
		return true;
	}
	return false;
}

// Poison Bag 4 (Generic Giver) ----------------------------------------------

class AArtiPoisonBagGiver : public AArtiPoisonBag
{
	DECLARE_CLASS (AArtiPoisonBagGiver, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiPoisonBagGiver)

bool AArtiPoisonBagGiver::Use (bool pickup)
{
	const PClass *MissileType = PClass::FindClass((ENamedName) this->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	if (MissileType != NULL)
	{
		AActor *mo = Spawn (MissileType, Owner->Pos(), ALLOW_REPLACE);
		if (mo != NULL)
		{
			if (mo->IsKindOf(RUNTIME_CLASS(AInventory)))
			{
				AInventory *inv = static_cast<AInventory *>(mo);
				if (inv->CallTryPickup(Owner))
					return true;
			}
			mo->Destroy();	// Destroy if not inventory or couldn't be picked up
		}
	}
	return false;
}

// Poison Bag 5 (Generic Thrower) ----------------------------------------------

class AArtiPoisonBagShooter : public AArtiPoisonBag
{
	DECLARE_CLASS (AArtiPoisonBagShooter, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiPoisonBagShooter)

bool AArtiPoisonBagShooter::Use (bool pickup)
{
	const PClass *MissileType = PClass::FindClass((ENamedName) this->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_None));
	if (MissileType != NULL)
	{
		AActor *mo = P_SpawnPlayerMissile(Owner, MissileType);
		if (mo != NULL)
		{
			// automatic handling of seeker missiles
			if (mo->flags2 & MF2_SEEKERMISSILE)
			{
				mo->tracer = Owner->target;
			}
			return true;
		}
	}
	return false;
}

//============================================================================
//
// GetFlechetteType
//
//============================================================================

const PClass *GetFlechetteType(AActor *other)
{
	const PClass *spawntype = NULL;
	if (other->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		spawntype = static_cast<APlayerPawn*>(other)->FlechetteType;
	}
	if (spawntype == NULL)
	{
		// default fallback if nothing valid defined.
		spawntype = RUNTIME_CLASS(AArtiPoisonBag3);
	}
	return spawntype;
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

	if (GetClass() == GetFlechetteType(Owner))
	{
		if (Amount < MaxAmount || sv_unlimited_pickup)
		{
			Amount += item->Amount;
			if (Amount > MaxAmount && !sv_unlimited_pickup)
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

	const PClass *spawntype = GetFlechetteType(other);
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
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
	void BeginPlay ();
};

IMPLEMENT_CLASS (APoisonCloud)

void APoisonCloud::BeginPlay ()
{
	velx = 1; // missile objects must move to impact other objects
	special1 = 24+(pr_poisoncloud()&7);
	special2 = 0;
}

int APoisonCloud::DoSpecialDamage (AActor *victim, int damage, FName damagetype)
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
			// Handle passive damage modifiers (e.g. PowerProtection)
			if (victim->Inventory != NULL)
			{
				victim->Inventory->ModifyDamage(damage, damagetype, damage, true);
			}
			// Modify with damage factors
			damage = FixedMul(damage, victim->DamageFactor);
			if (damage > 0)
			{
				damage = DamageTypeDefinition::ApplyMobjDamageFactor(damage, damagetype, victim->GetClass()->ActorInfo->DamageFactors);
			}
			if (damage > 0)
			{
				P_PoisonDamage (victim->player, this,
					15+(pr_poisoncloudd()&15), false); // Don't play painsound

				// If successful, play the poison sound.
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
	
	mo = Spawn<APoisonCloud> (self->PosPlusZ(28*FRACUNIT), ALLOW_REPLACE);
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
	
	P_RadiusAttack (self, self->target, 4, 40, self->DamageType, RADF_HURTSOURCE);
	bobIndex = self->special2;
	self->AddZ(finesine[bobIndex << BOBTOFINESHIFT] >> 1);
	self->special2 = (bobIndex + 1) & 63;
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
	// [RH] Check using actual velocity, although the velz < 2 check still stands
	//if (abs(self->velx) < FRACUNIT*3/2 && abs(self->vely) < FRACUNIT*3/2
	//	&& self->velz < 2*FRACUNIT)
	if (self->velz < 2*FRACUNIT &&
		TMulScale32 (self->velx, self->velx, self->vely, self->vely, self->velz, self->velz)
		< (3*3)/(2*2))
	{
		self->SetState (self->SpawnState + 6);
		self->SetZ(self->floorz);
		self->velz = 0;
		self->BounceFlags = BOUNCE_None;
		self->flags &= ~MF_MISSILE;
	}
	CALL_ACTION(A_CheckThrowBomb, self);
}
