/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "gi.h"
#include "a_sharedglobal.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_shootgun ("ShootGun");

//============================================================================
//
// A_ShootGun
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ShootGun)
{
	PARAM_ACTION_PROLOGUE;

	DAngle pitch;

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON, "monsters/rifle", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE);
	P_LineAttack (self, self->Angles.Yaw + pr_shootgun.Random2() * (11.25 / 256),
		MISSILERANGE, pitch,
		3*(pr_shootgun() % 5 + 1), NAME_Hitscan, NAME_StrifePuff);
	return 0;
}

// Teleporter Beacon --------------------------------------------------------

class ATeleporterBeacon : public AInventory
{
	DECLARE_CLASS (ATeleporterBeacon, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (ATeleporterBeacon)

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
		drop->SetState(drop->FindState(NAME_Drop));
		drop->target = Owner;
		return true;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_Beacon)
{
	PARAM_ACTION_PROLOGUE;

	AActor *owner = self->target;
	AActor *rebel;

	rebel = Spawn("Rebel1", self->PosAtZ(self->floorz), ALLOW_REPLACE);
	if (!P_TryMove (rebel, rebel->Pos(), true))
	{
		rebel->Destroy ();
		return 0;
	}
	// Once the rebels start teleporting in, you can't pick up the beacon anymore.
	self->flags &= ~MF_SPECIAL;
	static_cast<AInventory *>(self)->DropTime = 0;
	// Set up the new rebel.
	rebel->threshold = rebel->DefThreshold;
	rebel->target = NULL;
	rebel->flags4 |= MF4_INCOMBAT;
	rebel->LastHeard = owner;	// Make sure the rebels look for targets
	if (deathmatch)
	{
		rebel->health *= 2;
	}
	if (owner != NULL)
	{
		// Rebels are the same color as their owner (but only in multiplayer)
		if (multiplayer)
		{
			rebel->Translation = owner->Translation;
		}
		rebel->SetFriendPlayer(owner->player);
		// Set the rebel's target to whatever last hurt the player, so long as it's not
		// one of the player's other rebels.
		if (owner->target != NULL && !rebel->IsFriend (owner->target))
		{
			rebel->target = owner->target;
		}
	}

	rebel->SetState (rebel->SeeState);
	rebel->Angles.Yaw = self->Angles.Yaw;
	P_SpawnTeleportFog(rebel, rebel->Vec3Angle(20., self->Angles.Yaw, 0), false, true);
	if (--self->health < 0)
	{
		self->SetState(self->FindState(NAME_Death));
	}
	return 0;
}
