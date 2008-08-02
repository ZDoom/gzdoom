#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

#define FIREDEMON_ATTACK_RANGE	64*8*FRACUNIT

static FRandom pr_firedemonrock ("FireDemonRock");
static FRandom pr_smbounce ("SMBounce");
static FRandom pr_firedemonchase ("FiredChase");
static FRandom pr_firedemonsplotch ("FiredSplotch");

//============================================================================
// Fire Demon AI
//
// special1			index into floatbob
// special2			whether strafing or not
//============================================================================

void A_FiredRocks (AActor *);
void A_FiredSpawnRock (AActor *);
void A_SmBounce (AActor *);
void A_FiredChase (AActor *);
void A_FiredAttack (AActor *);
void A_FiredSplotch (AActor *);

//============================================================================
//
// A_FiredRocks
//
//============================================================================

void A_FiredRocks (AActor *actor)
{
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
}

//============================================================================
//
// A_FiredSpawnRock
//
//============================================================================

void A_FiredSpawnRock (AActor *actor)
{
	AActor *mo;
	int x,y,z;
	const PClass *rtype;

	switch (pr_firedemonrock() % 5)
	{
		case 0:
			rtype = PClass::FindClass ("FireDemonRock1");
			break;
		case 1:
			rtype = PClass::FindClass ("FireDemonRock2");
			break;
		case 2:
			rtype = PClass::FindClass ("FireDemonRock3");
			break;
		case 3:
			rtype = PClass::FindClass ("FireDemonRock4");
			break;
		case 4:
		default:
			rtype = PClass::FindClass ("FireDemonRock5");
			break;
	}

	x = actor->x + ((pr_firedemonrock() - 128) << 12);
	y = actor->y + ((pr_firedemonrock() - 128) << 12);
	z = actor->z + ( pr_firedemonrock() << 11);
	mo = Spawn (rtype, x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = actor;
		mo->momx = (pr_firedemonrock() - 128) <<10;
		mo->momy = (pr_firedemonrock() - 128) <<10;
		mo->momz = (pr_firedemonrock() << 10);
		mo->special1 = 2;		// Number bounces
	}

	// Initialize fire demon
	actor->special2 = 0;
	actor->flags &= ~MF_JUSTATTACKED;
}

//============================================================================
//
// A_SmBounce
//
//============================================================================

void A_SmBounce (AActor *actor)
{
	// give some more momentum (x,y,&z)
	actor->z = actor->floorz + FRACUNIT;
	actor->momz = (2*FRACUNIT) + (pr_smbounce() << 10);
	actor->momx = pr_smbounce()%3<<FRACBITS;
	actor->momy = pr_smbounce()%3<<FRACBITS;
}

//============================================================================
//
// A_FiredAttack
//
//============================================================================

void A_FiredAttack (AActor *actor)
{
	if (actor->target == NULL)
		return;
	AActor *mo = P_SpawnMissile (actor, actor->target, PClass::FindClass ("FireDemonMissile"));
	if (mo) S_Sound (actor, CHAN_BODY, "FireDemonAttack", 1, ATTN_NORM);
}

//============================================================================
//
// A_FiredChase
//
//============================================================================

void A_FiredChase (AActor *actor)
{
	int weaveindex = actor->special1;
	AActor *target = actor->target;
	angle_t ang;
	fixed_t dist;

	if (actor->reactiontime) actor->reactiontime--;
	if (actor->threshold) actor->threshold--;

	// Float up and down
	actor->z += FloatBobOffsets[weaveindex];
	actor->special1 = (weaveindex+2)&63;

	// Ensure it stays above certain height
	if (actor->z < actor->floorz + (64*FRACUNIT))
	{
		actor->z += 2*FRACUNIT;
	}

	if(!actor->target || !(actor->target->flags&MF_SHOOTABLE))
	{	// Invalid target
		P_LookForPlayers (actor,true);
		return;
	}

	// Strafe
	if (actor->special2 > 0)
	{
		actor->special2--;
	}
	else
	{
		actor->special2 = 0;
		actor->momx = actor->momy = 0;
		dist = P_AproxDistance (actor->x - target->x, actor->y - target->y);
		if (dist < FIREDEMON_ATTACK_RANGE)
		{
			if (pr_firedemonchase() < 30)
			{
				ang = R_PointToAngle2 (actor->x, actor->y, target->x, target->y);
				if (pr_firedemonchase() < 128)
					ang += ANGLE_90;
				else
					ang -= ANGLE_90;
				ang >>= ANGLETOFINESHIFT;
				actor->momx = finecosine[ang] << 3; //FixedMul (8*FRACUNIT, finecosine[ang]);
				actor->momy = finesine[ang] << 3; //FixedMul (8*FRACUNIT, finesine[ang]);
				actor->special2 = 3;		// strafe time
			}
		}
	}

	FaceMovementDirection (actor);

	// Normal movement
	if (!actor->special2)
	{
		if (--actor->movecount<0 || !P_Move (actor))
		{
			P_NewChaseDir (actor);
		}
	}

	// Do missile attack
	if (!(actor->flags & MF_JUSTATTACKED))
	{
		if (P_CheckMissileRange (actor) && (pr_firedemonchase() < 20))
		{
			actor->SetState (actor->MissileState);
			actor->flags |= MF_JUSTATTACKED;
			return;
		}
	}
	else
	{
		actor->flags &= ~MF_JUSTATTACKED;
	}

	// make active sound
	if (pr_firedemonchase() < 3)
	{
		actor->PlayActiveSound ();
	}
}

//============================================================================
//
// A_FiredSplotch
//
//============================================================================

void A_FiredSplotch (AActor *actor)
{
	AActor *mo;

	mo = Spawn ("FireDemonSplotch1", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->momx = (pr_firedemonsplotch() - 128) << 11;
		mo->momy = (pr_firedemonsplotch() - 128) << 11;
		mo->momz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
	mo = Spawn ("FireDemonSplotch2", actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->momx = (pr_firedemonsplotch() - 128) << 11;
		mo->momy = (pr_firedemonsplotch() - 128) << 11;
		mo->momz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
}
