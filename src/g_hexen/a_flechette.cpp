#include "actor.h"
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "w_wad.h"

static FRandom pr_poisonbag ("PoisonBag");
static FRandom pr_poisoncloud ("PoisonCloud");
static FRandom pr_poisoncloudd ("PoisonCloudDamage");

void A_PoisonBagInit (AActor *);
void A_PoisonBagDamage (AActor *);
void A_PoisonBagCheck (AActor *);
void A_CheckThrowBomb (AActor *);
void A_CheckThrowBomb2 (AActor *);

// Fire Bomb (Flechette used by Mage) ---------------------------------------

class AFireBomb : public AActor
{
	DECLARE_ACTOR (AFireBomb, AActor)
public:
	void PreExplode ()
	{
		z += 32*FRACUNIT;
		RenderStyle = STYLE_Normal;
	}
};

FState AFireBomb::States[] =
{
#define S_FIREBOMB1 0
	S_NORMAL (PSBG, 'A',   20, NULL					    , &States[S_FIREBOMB1+1]),
	S_NORMAL (PSBG, 'A',   10, NULL					    , &States[S_FIREBOMB1+2]),
	S_NORMAL (PSBG, 'A',   10, NULL					    , &States[S_FIREBOMB1+3]),
	S_NORMAL (PSBG, 'B',	4, NULL					    , &States[S_FIREBOMB1+4]),
	S_NORMAL (PSBG, 'C',	4, A_Scream				    , &States[S_FIREBOMB1+5]),
	S_BRIGHT (XPL1, 'A',	4, A_Explode			    , &States[S_FIREBOMB1+6]),
	S_BRIGHT (XPL1, 'B',	4, NULL					    , &States[S_FIREBOMB1+7]),
	S_BRIGHT (XPL1, 'C',	4, NULL					    , &States[S_FIREBOMB1+8]),
	S_BRIGHT (XPL1, 'D',	4, NULL					    , &States[S_FIREBOMB1+9]),
	S_BRIGHT (XPL1, 'E',	4, NULL					    , &States[S_FIREBOMB1+10]),
	S_BRIGHT (XPL1, 'F',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFireBomb, Hexen, -1, 0)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOGRAVITY)
	PROP_Flags3 (MF3_FOILINVUL)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (S_FIREBOMB1)

	PROP_DeathSound ("FlechetteExplode")
END_DEFAULTS

// Poison Bag Artifact (Flechette) ------------------------------------------

class AArtiPoisonBag : public AInventory
{
	DECLARE_ACTOR (AArtiPoisonBag, AInventory)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void BeginPlay ();
};

FState AArtiPoisonBag::States[] =
{
	S_NORMAL (PSBG, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AArtiPoisonBag, Hexen, 8000, 72)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_PickupFlash (1)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIPSBG")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIPOISONBAG")
END_DEFAULTS

// Poison Bag 1 (The Cleric's) ----------------------------------------------

class AArtiPoisonBag1 : public AArtiPoisonBag
{
	DECLARE_STATELESS_ACTOR (AArtiPoisonBag1, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_STATELESS_ACTOR (AArtiPoisonBag1, Hexen, -1, 0)
	PROP_Inventory_Icon ("ARTIPSB1")
END_DEFAULTS

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
	DECLARE_STATELESS_ACTOR (AArtiPoisonBag2, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_STATELESS_ACTOR (AArtiPoisonBag2, Hexen, -1, 0)
	PROP_Inventory_Icon ("ARTIPSB2")
END_DEFAULTS

bool AArtiPoisonBag2::Use (bool pickup)
{
	angle_t angle = Owner->angle >> ANGLETOFINESHIFT;
	AActor *mo;

	mo = Spawn<AFireBomb> (
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
	DECLARE_STATELESS_ACTOR (AArtiPoisonBag3, AArtiPoisonBag)
public:
	bool Use (bool pickup);
};

IMPLEMENT_STATELESS_ACTOR (AArtiPoisonBag3, Hexen, -1, 0)
	PROP_Inventory_Icon ("ARTIPSB3")
END_DEFAULTS

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

FState APoisonCloud::States[] =
{
#define S_POISONCLOUD1 0
	S_NORMAL (PSBG, 'D',	1, NULL					    , &States[S_POISONCLOUD1+1]),
	S_NORMAL (PSBG, 'D',	1, A_Scream				    , &States[S_POISONCLOUD1+2]),
	S_NORMAL (PSBG, 'D',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+3]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+4]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+5]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+6]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+7]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+8]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+9]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+10]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+11]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+12]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+13]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+14]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+15]),
	S_NORMAL (PSBG, 'I',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+16]),
	S_NORMAL (PSBG, 'I',	1, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+17]),
	S_NORMAL (PSBG, 'I',	1, A_PoisonBagCheck		    , &States[S_POISONCLOUD1+3]),

#define S_POISONCLOUD_X1 (S_POISONCLOUD1+18)
	S_NORMAL (PSBG, 'H',	7, NULL					    , &States[S_POISONCLOUD_X1+1]),
	S_NORMAL (PSBG, 'G',	7, NULL					    , &States[S_POISONCLOUD_X1+2]),
	S_NORMAL (PSBG, 'F',	6, NULL					    , &States[S_POISONCLOUD_X1+3]),
	S_NORMAL (PSBG, 'D',	6, NULL					    , NULL)
};

IMPLEMENT_ACTOR (APoisonCloud, Hexen, -1, 0)
	PROP_Radius (20)
	PROP_Height (30)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_Flags2 (MF2_NODMGTHRUST)
	PROP_Flags3 (MF3_DONTSPLASH|MF3_FOILINVUL|MF3_CANBLAST|MF3_BLOODLESSIMPACT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (S_POISONCLOUD1)

	PROP_DeathSound ("PoisonShroomDeath")
END_DEFAULTS

void APoisonCloud::BeginPlay ()
{
	momx = 1; // missile objects must move to impact other objects
	special1 = 24+(pr_poisoncloud()&7);
	special2 = 0;
}

void APoisonCloud::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 4;
	distance = 40;
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
				P_PoisonPlayer (victim->player, this, this->target, 50);

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

void A_PoisonBagInit (AActor *actor)
{
	AActor *mo;
	
	mo = Spawn<APoisonCloud> (actor->x, actor->y, actor->z+28*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = actor->target;
	}
}

//===========================================================================
//
// A_PoisonBagCheck
//
//===========================================================================

void A_PoisonBagCheck (AActor *actor)
{
	if (--actor->special1 <= 0)
	{
		actor->SetState (&APoisonCloud::States[S_POISONCLOUD_X1]);
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

void A_PoisonBagDamage (AActor *actor)
{
	int bobIndex;
	
	A_Explode (actor);	
	bobIndex = actor->special2;
	actor->z += FloatBobOffsets[bobIndex]>>4;
	actor->special2 = (bobIndex+1)&63;
}

//===========================================================================
//
// A_CheckThrowBomb
//
//===========================================================================

void A_CheckThrowBomb (AActor *actor)
{
	if (--actor->health <= 0)
	{
		actor->SetState (actor->FindState(NAME_Death));
	}
}

//===========================================================================
//
// A_CheckThrowBomb2
//
//===========================================================================

void A_CheckThrowBomb2 (AActor *actor)
{
	// [RH] Check using actual velocity, although the momz < 2 check still stands
	//if (abs(actor->momx) < FRACUNIT*3/2 && abs(actor->momy) < FRACUNIT*3/2
	//	&& actor->momz < 2*FRACUNIT)
	if (actor->momz < 2*FRACUNIT &&
		TMulScale32 (actor->momx, actor->momx, actor->momy, actor->momy, actor->momz, actor->momz)
		< (3*3)/(2*2))
	{
		actor->SetState (actor->SpawnState + 6);
		actor->z = actor->floorz;
		actor->momz = 0;
		actor->flags2 &= ~MF2_BOUNCETYPE;
		actor->flags &= ~MF_MISSILE;
	}
	A_CheckThrowBomb (actor);
}
