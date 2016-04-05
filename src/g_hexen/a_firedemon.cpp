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

#define FIREDEMON_ATTACK_RANGE	(64*8.)

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
	PClassActor *rtype;

	switch (pr_firedemonrock() % 5)
	{
		case 0:
			rtype = PClass::FindActor("FireDemonRock1");
			break;
		case 1:
			rtype = PClass::FindActor("FireDemonRock2");
			break;
		case 2:
			rtype = PClass::FindActor("FireDemonRock3");
			break;
		case 3:
			rtype = PClass::FindActor("FireDemonRock4");
			break;
		case 4:
		default:
			rtype = PClass::FindActor("FireDemonRock5");
			break;
	}

	double xo = (pr_firedemonrock() - 128) / 16.;
	double yo = (pr_firedemonrock() - 128) / 16.;
	double zo = pr_firedemonrock() / 32.;
	mo = Spawn (rtype, actor->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
	if (mo)
	{
		mo->target = actor;
		mo->Vel.X = (pr_firedemonrock() - 128) / 64.;
		mo->Vel.Y = (pr_firedemonrock() - 128) / 64.;
		mo->Vel.Z = (pr_firedemonrock() / 64.);
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
	PARAM_ACTION_PROLOGUE;

	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	return 0;
}

//============================================================================
//
// A_SmBounce
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SmBounce)
{
	PARAM_ACTION_PROLOGUE;

	// give some more velocity (x,y,&z)
	self->SetZ(self->floorz + 1);
	self->Vel.Z = 2. + pr_smbounce() / 64.;
	self->Vel.X = pr_smbounce() % 3;
	self->Vel.Y = pr_smbounce() % 3;
	return 0;
}

//============================================================================
//
// A_FiredAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;
	AActor *mo = P_SpawnMissile (self, self->target, PClass::FindActor("FireDemonMissile"));
	if (mo) S_Sound (self, CHAN_BODY, "FireDemonAttack", 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_FiredChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredChase)
{
	PARAM_ACTION_PROLOGUE;

	int weaveindex = self->special1;
	AActor *target = self->target;
	DAngle ang;
	double dist;

	if (self->reactiontime) self->reactiontime--;
	if (self->threshold) self->threshold--;

	// Float up and down
	self->AddZ(BobSin(weaveindex));
	self->special1 = (weaveindex + 2) & 63;

	// Ensure it stays above certain height
	if (self->Z() < self->floorz + 64)
	{
		self->AddZ(2);
	}

	if(!self->target || !(self->target->flags&MF_SHOOTABLE))
	{	// Invalid target
		P_LookForPlayers (self,true, NULL);
		return 0;
	}

	// Strafe
	if (self->special2 > 0)
	{
		self->special2--;
	}
	else
	{
		self->special2 = 0;
		self->Vel.X = self->Vel.Y = 0;
		dist = self->Distance2D(target);
		if (dist < FIREDEMON_ATTACK_RANGE)
		{
			if (pr_firedemonchase() < 30)
			{
				ang = self->AngleTo(target);
				if (pr_firedemonchase() < 128)
					ang += 90;
				else
					ang -= 90;
				self->Thrust(ang, 8);
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
			return 0;
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
	return 0;
}

//============================================================================
//
// A_FiredSplotch
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredSplotch)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	mo = Spawn ("FireDemonSplotch1", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->Vel.X = (pr_firedemonsplotch() - 128) / 32.;
		mo->Vel.Y = (pr_firedemonsplotch() - 128) / 32.;
		mo->Vel.Z = (pr_firedemonsplotch() / 64.) + 3;
	}
	mo = Spawn ("FireDemonSplotch2", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->Vel.X = (pr_firedemonsplotch() - 128) / 32.;
		mo->Vel.Y = (pr_firedemonsplotch() - 128) / 32.;
		mo->Vel.Z = (pr_firedemonsplotch() / 64.) + 3;
	}
	return 0;
}
