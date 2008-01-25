#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "gi.h"
#include "a_sharedglobal.h"
#include "a_strifeglobal.h"

static FRandom pr_shootgun ("ShootGun");

void A_ShootGun (AActor *);
void A_TossGib (AActor *);
void A_Beacon (AActor *);

// Base class for the rebels ------------------------------------------------

class ARebel : public AStrifeHumanoid
{
	DECLARE_ACTOR (ARebel, AStrifeHumanoid)
};

FState ARebel::States[] =
{
#define S_REBEL_STND 0
	S_NORMAL (HMN1, 'P',	5, A_Look2,				&States[S_REBEL_STND]),
	S_NORMAL (HMN1, 'Q',	8, NULL,				&States[S_REBEL_STND]),
	S_NORMAL (HMN1, 'R',	8, NULL,				&States[S_REBEL_STND]),

#define S_REBEL_WAND (S_REBEL_STND+3)
	S_NORMAL (HMN1, 'A',	6, A_Wander,			&States[S_REBEL_WAND+1]),
	S_NORMAL (HMN1, 'B',	6, A_Wander,			&States[S_REBEL_WAND+2]),
	S_NORMAL (HMN1, 'C',	6, A_Wander,			&States[S_REBEL_WAND+3]),
	S_NORMAL (HMN1, 'D',	6, A_Wander,			&States[S_REBEL_WAND+4]),
	S_NORMAL (HMN1, 'A',	6, A_Wander,			&States[S_REBEL_WAND+5]),
	S_NORMAL (HMN1, 'B',	6, A_Wander,			&States[S_REBEL_WAND+6]),
	S_NORMAL (HMN1, 'C',	6, A_Wander,			&States[S_REBEL_WAND+7]),
	S_NORMAL (HMN1, 'D',	6, A_Wander,			&States[S_REBEL_STND]),

#define S_REBEL_CHASE (S_REBEL_WAND+8)
	S_NORMAL (HMN1, 'A',	3, A_Chase,				&States[S_REBEL_CHASE+1]),
	S_NORMAL (HMN1, 'A',	3, A_Chase,				&States[S_REBEL_CHASE+2]),
	S_NORMAL (HMN1, 'B',	3, A_Chase,				&States[S_REBEL_CHASE+3]),
	S_NORMAL (HMN1, 'B',	3, A_Chase,				&States[S_REBEL_CHASE+4]),
	S_NORMAL (HMN1, 'C',	3, A_Chase,				&States[S_REBEL_CHASE+5]),
	S_NORMAL (HMN1, 'C',	3, A_Chase,				&States[S_REBEL_CHASE+6]),
	S_NORMAL (HMN1, 'D',	3, A_Chase,				&States[S_REBEL_CHASE+7]),
	S_NORMAL (HMN1, 'D',	3, A_Chase,				&States[S_REBEL_CHASE]),

#define S_REBEL_ATK (S_REBEL_CHASE+8)
	S_NORMAL (HMN1, 'E',   10, A_FaceTarget,		&States[S_REBEL_ATK+1]),
	S_BRIGHT (HMN1, 'F',   10, A_ShootGun,			&States[S_REBEL_ATK+2]),
	S_NORMAL (HMN1, 'E',   10, A_ShootGun,			&States[S_REBEL_CHASE]),

#define S_REBEL_PAIN (S_REBEL_ATK+3)
	S_NORMAL (HMN1, 'O',	3, NULL,				&States[S_REBEL_PAIN+1]),
	S_NORMAL (HMN1, 'O',	3, A_Pain,				&States[S_REBEL_CHASE]),

#define S_REBEL_DIE (S_REBEL_PAIN+2)
	S_NORMAL (HMN1, 'G',	5, NULL,				&States[S_REBEL_DIE+1]),
	S_NORMAL (HMN1, 'H',	5, A_Scream,			&States[S_REBEL_DIE+2]),
	S_NORMAL (HMN1, 'I',	3, A_NoBlocking,		&States[S_REBEL_DIE+3]),
	S_NORMAL (HMN1, 'J',	4, NULL,				&States[S_REBEL_DIE+4]),
	S_NORMAL (HMN1, 'K',	3, NULL,				&States[S_REBEL_DIE+5]),
	S_NORMAL (HMN1, 'L',	3, NULL,				&States[S_REBEL_DIE+6]),
	S_NORMAL (HMN1, 'M',	3, NULL,				&States[S_REBEL_DIE+7]),
	S_NORMAL (HMN1, 'N',   -1, NULL,				NULL),

#define S_REBEL_XDIE (S_REBEL_DIE+8)
	S_NORMAL (RGIB, 'A',	4, A_TossGib,			&States[S_REBEL_XDIE+1]),
	S_NORMAL (RGIB, 'B',	4, A_XScream,			&States[S_REBEL_XDIE+2]),
	S_NORMAL (RGIB, 'C',	3, A_NoBlocking,		&States[S_REBEL_XDIE+3]),
	S_NORMAL (RGIB, 'D',	3, A_TossGib,			&States[S_REBEL_XDIE+4]),
	S_NORMAL (RGIB, 'E',	3, A_TossGib,			&States[S_REBEL_XDIE+5]),
	S_NORMAL (RGIB, 'F',	3, A_TossGib,			&States[S_REBEL_XDIE+6]),
	S_NORMAL (RGIB, 'G',	3, NULL,				&States[S_REBEL_XDIE+7]),
	S_NORMAL (RGIB, 'H', 1400, NULL,				NULL)
};

IMPLEMENT_ACTOR (ARebel, Strife, -1, 0)
	PROP_SpawnState (S_REBEL_STND)
	PROP_SeeState (S_REBEL_CHASE)
	PROP_PainState (S_REBEL_PAIN)
	PROP_MissileState (S_REBEL_ATK)
	PROP_DeathState (S_REBEL_DIE)
	PROP_XDeathState (S_REBEL_XDIE)

	PROP_SpawnHealth (60)
	PROP_PainChance (250)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_FRIENDLY)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_ISMONSTER)
	PROP_Flags4 (MF4_NOSPLASHALERT)
	PROP_MinMissileChance (150)
	PROP_Tag ("Rebel")

	PROP_SeeSound ("rebel/sight")
	PROP_PainSound ("rebel/pain")
	PROP_DeathSound ("rebel/death")
	PROP_ActiveSound ("rebel/active")
	PROP_Obituary ("$OB_REBEL")
END_DEFAULTS

//============================================================================
//
// A_ShootGun
//
//============================================================================

void A_ShootGun (AActor *self)
{
	int pitch;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "monsters/rifle", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);
	P_LineAttack (self, self->angle + (pr_shootgun.Random2() << 19),
		MISSILERANGE, pitch,
		3*(pr_shootgun() % 5 + 1), NAME_None, RUNTIME_CLASS(AStrifePuff));
}

// Rebel 1 ------------------------------------------------------------------

class ARebel1 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel1, ARebel)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (ARebel1, Strife, 9, 0)
	PROP_StrifeType (43)
	PROP_StrifeTeaserType (42)
	PROP_StrifeTeaserType2 (43)
END_DEFAULTS

//============================================================================
//
// ARebel1 :: NoBlockingSet
//
// Only this type of rebel drops bullet clips by default.
//
//============================================================================

void ARebel1::NoBlockingSet ()
{
	P_DropItem (this, "ClipOfBullets", -1, 256);
}

// Rebel 2 ------------------------------------------------------------------

class ARebel2 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel2, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel2, Strife, 144, 0)
	PROP_StrifeType (44)
	PROP_StrifeTeaserType (43)
	PROP_StrifeTeaserType2 (44)
END_DEFAULTS

// Rebel 3 ------------------------------------------------------------------

class ARebel3 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel3, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel3, Strife, 145, 0)
	PROP_StrifeType (45)
	PROP_StrifeTeaserType (44)
	PROP_StrifeTeaserType2 (45)
END_DEFAULTS

// Rebel 4 ------------------------------------------------------------------

class ARebel4 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel4, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel4, Strife, 149, 0)
	PROP_StrifeType (46)
	PROP_StrifeTeaserType (45)
	PROP_StrifeTeaserType2 (46)
END_DEFAULTS

// Rebel 5 ------------------------------------------------------------------

class ARebel5 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel5, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel5, Strife, 150, 0)
	PROP_StrifeType (47)
	PROP_StrifeTeaserType (46)
	PROP_StrifeTeaserType2 (47)
END_DEFAULTS

// Rebel 6 ------------------------------------------------------------------

class ARebel6 : public ARebel
{
	DECLARE_STATELESS_ACTOR (ARebel6, ARebel)
};

IMPLEMENT_STATELESS_ACTOR (ARebel6, Strife, 151, 0)
	PROP_StrifeType (48)
	PROP_StrifeTeaserType (47)
	PROP_StrifeTeaserType2 (48)
END_DEFAULTS

// Teleporter Beacon --------------------------------------------------------

class ATeleporterBeacon : public AInventory
{
	DECLARE_ACTOR (ATeleporterBeacon, AInventory)
public:
	bool Use (bool pickup);
};

FState ATeleporterBeacon::States[] =
{
	S_NORMAL (BEAC, 'A',  -1, NULL,			NULL),

	S_NORMAL (BEAC, 'A',  30, NULL,			&States[2]),
	S_NORMAL (BEAC, 'A', 160, A_Beacon,		&States[1])
};

IMPLEMENT_ACTOR (ATeleporterBeacon, Strife, 10, 0)
	PROP_StrifeType (166)
	PROP_SpawnHealth (5)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Inventory_MaxAmount (3)
	PROP_Flags (MF_SPECIAL|MF_DROPPED)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_Icon ("I_BEAC")
	PROP_Tag ("Teleporter_Beacon")
	PROP_Inventory_PickupMessage("$TXT_BEACON")
END_DEFAULTS

bool ATeleporterBeacon::Use (bool pickup)
{
	AInventory *drop;

	// Increase the amount by one so that when DropInventory decrements it,
	// the actor will have the same number of beacons that he started with.
	// When we return to UseInventory, it will take care of decrementing
	// Amount again and disposing of this item if there are no more.
	Amount++;
	drop = Owner->DropInventory (this);
	if (drop == NULL)
	{
		Amount--;
		return false;
	}
	else
	{
		drop->SetState (drop->SeeState);
		drop->target = Owner;
		return true;
	}
}

void A_Beacon (AActor *self)
{
	AActor *owner = self->target;
	ARebel *rebel;
	angle_t an;

	rebel = Spawn<ARebel1> (self->x, self->y, ONFLOORZ, ALLOW_REPLACE);
	if (!P_TryMove (rebel, rebel->x, rebel->y, true))
	{
		rebel->Destroy ();
		return;
	}
	// Once the rebels start teleporting in, you can't pick up the beacon anymore.
	self->flags &= ~MF_SPECIAL;
	static_cast<AInventory *>(self)->DropTime = 0;
	// Set up the new rebel.
	rebel->threshold = BASETHRESHOLD;
	rebel->target = NULL;
	rebel->flags4 |= MF4_INCOMBAT;
	rebel->LastHeard = owner;	// Make sure the rebels look for targets
	if (deathmatch)
	{
		rebel->health *= 2;
	}
	if (owner != NULL)
	{
		// Rebels are the same color as their owner
		rebel->Translation = owner->Translation;
		rebel->FriendPlayer = owner->player != NULL ? BYTE(owner->player - players + 1) : 0;
		// Set the rebel's target to whatever last hurt the player, so long as it's not
		// one of the player's other rebels.
		if (owner->target != NULL && !rebel->IsFriend (owner->target))
		{
			rebel->target = owner->target;
		}
	}

	rebel->SetState (rebel->SeeState);
	rebel->angle = self->angle;
	an = self->angle >> ANGLETOFINESHIFT;
	Spawn<ATeleportFog> (rebel->x + 20*finecosine[an], rebel->y + 20*finesine[an], rebel->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	if (--self->health < 0)
	{
		self->Destroy ();
	}
}
