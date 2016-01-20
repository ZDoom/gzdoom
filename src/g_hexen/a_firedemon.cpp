/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

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

//============================================================================
//
// A_FiredSpawnRock
//
//============================================================================

void A_FiredSpawnRock (AActor *actor)
{
	AActor *mo;
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

	fixed_t xo = ((pr_firedemonrock() - 128) << 12);
	fixed_t yo = ((pr_firedemonrock() - 128) << 12);
	fixed_t zo = (pr_firedemonrock() << 11);
	mo = Spawn (rtype, actor->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
	if (mo)
	{
		mo->target = actor;
		mo->velx = (pr_firedemonrock() - 128) <<10;
		mo->vely = (pr_firedemonrock() - 128) <<10;
		mo->velz = (pr_firedemonrock() << 10);
		mo->special1 = 2;		// Number bounces
	}

	// Initialize fire demon
	actor->special2 = 0;
	actor->flags &= ~MF_JUSTATTACKED;
}

//============================================================================
//
// A_FiredRocks
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredRocks)
{
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
}

//============================================================================
//
// A_SmBounce
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SmBounce)
{
	// give some more velocity (x,y,&z)
	self->SetZ(self->floorz + FRACUNIT);
	self->velz = (2*FRACUNIT) + (pr_smbounce() << 10);
	self->velx = pr_smbounce()%3<<FRACBITS;
	self->vely = pr_smbounce()%3<<FRACBITS;
}

//============================================================================
//
// A_FiredAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredAttack)
{
	if (self->target == NULL)
		return;
	AActor *mo = P_SpawnMissile (self, self->target, PClass::FindClass ("FireDemonMissile"));
	if (mo) S_Sound (self, CHAN_BODY, "FireDemonAttack", 1, ATTN_NORM);
}

//============================================================================
//
// A_FiredChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredChase)
{
	int weaveindex = self->special1;
	AActor *target = self->target;
	angle_t ang;
	fixed_t dist;

	if (self->reactiontime) self->reactiontime--;
	if (self->threshold) self->threshold--;

	// Float up and down
	self->AddZ(finesine[weaveindex << BOBTOFINESHIFT] * 8);
	self->special1 = (weaveindex + 2) & 63;

	// Ensure it stays above certain height
	if (self->Z() < self->floorz + (64*FRACUNIT))
	{
		self->AddZ(2*FRACUNIT);
	}

	if(!self->target || !(self->target->flags&MF_SHOOTABLE))
	{	// Invalid target
		P_LookForPlayers (self,true, NULL);
		return;
	}

	// Strafe
	if (self->special2 > 0)
	{
		self->special2--;
	}
	else
	{
		self->special2 = 0;
		self->velx = self->vely = 0;
		dist = self->AproxDistance (target);
		if (dist < FIREDEMON_ATTACK_RANGE)
		{
			if (pr_firedemonchase() < 30)
			{
				ang = self->AngleTo(target);
				if (pr_firedemonchase() < 128)
					ang += ANGLE_90;
				else
					ang -= ANGLE_90;
				ang >>= ANGLETOFINESHIFT;
				self->velx = finecosine[ang] << 3; //FixedMul (8*FRACUNIT, finecosine[ang]);
				self->vely = finesine[ang] << 3; //FixedMul (8*FRACUNIT, finesine[ang]);
				self->special2 = 3;		// strafe time
			}
		}
	}

	FaceMovementDirection (self);

	// Normal movement
	if (!self->special2)
	{
		if (--self->movecount<0 || !P_Move (self))
		{
			P_NewChaseDir (self);
		}
	}

	// Do missile attack
	if (!(self->flags & MF_JUSTATTACKED))
	{
		if (P_CheckMissileRange (self) && (pr_firedemonchase() < 20))
		{
			self->SetState (self->MissileState);
			self->flags |= MF_JUSTATTACKED;
			return;
		}
	}
	else
	{
		self->flags &= ~MF_JUSTATTACKED;
	}

	// make active sound
	if (pr_firedemonchase() < 3)
	{
		self->PlayActiveSound ();
	}
}

//============================================================================
//
// A_FiredSplotch
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredSplotch)
{
	AActor *mo;

	mo = Spawn ("FireDemonSplotch1", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->velx = (pr_firedemonsplotch() - 128) << 11;
		mo->vely = (pr_firedemonsplotch() - 128) << 11;
		mo->velz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
	mo = Spawn ("FireDemonSplotch2", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->velx = (pr_firedemonsplotch() - 128) << 11;
		mo->vely = (pr_firedemonsplotch() - 128) << 11;
		mo->velz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
}
