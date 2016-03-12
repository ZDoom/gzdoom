/*
#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_dragonseek ("DragonSeek");
static FRandom pr_dragonflight ("DragonFlight");
static FRandom pr_dragonflap ("DragonFlap");
static FRandom pr_dragonfx2 ("DragonFX2");

DECLARE_ACTION(A_DragonFlight)

//============================================================================
//
// DragonSeek
//
//============================================================================

static void DragonSeek (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	int i;
	angle_t bestAngle;
	angle_t angleToSpot, angleToTarget;
	AActor *mo;

	target = actor->tracer;
	if(target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->vel.x = FixedMul (actor->Speed, finecosine[angle]);
	actor->vel.y = FixedMul (actor->Speed, finesine[angle]);
	dist = actor->AproxDistance (target) / actor->Speed;
	if (actor->Top() < target->Z() ||
		target->Top() < actor->Z())
	{
		if (dist < 1)
		{
			dist = 1;
		}
		actor->vel.z = (target->Z() - actor->Z())/dist;
	}
	if (target->flags&MF_SHOOTABLE && pr_dragonseek() < 64)
	{ // attack the destination mobj if it's attackable
		AActor *oldTarget;

		if (absangle(actor->angle - actor->AngleTo(target)) < ANGLE_45/2)
		{
			oldTarget = actor->target;
			actor->target = target;
			if (actor->CheckMeleeRange ())
			{
				int damage = pr_dragonseek.HitDice (10);
				int newdam = P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
				P_TraceBleed (newdam > 0 ? newdam : damage, actor->target, actor);
				S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
			}
			else if (pr_dragonseek() < 128 && P_CheckMissileRange(actor))
			{
				P_SpawnMissile(actor, target, PClass::FindActor("DragonFireball"));						
				S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
			}
			actor->target = oldTarget;
		}
	}
	if (dist < 4)
	{ // Hit the target thing
		if (actor->target && pr_dragonseek() < 200)
		{
			AActor *bestActor = NULL;
			bestAngle = ANGLE_MAX;
			angleToTarget = actor->AngleTo(actor->target);
			for (i = 0; i < 5; i++)
			{
				if (!target->args[i])
				{
					continue;
				}
				FActorIterator iterator (target->args[i]);
				mo = iterator.Next ();
				if (mo == NULL)
				{
					continue;
				}
				angleToSpot = actor->AngleTo(mo);
				if (absangle(angleToSpot-angleToTarget) < bestAngle)
				{
					bestAngle = absangle(angleToSpot-angleToTarget);
					bestActor = mo;
				}
			}
			if (bestActor != NULL)
			{
				actor->tracer = bestActor;
			}
		}
		else
		{
			// [RH] Don't lock up if the dragon doesn't have any
			// targets defined
			for (i = 0; i < 5; ++i)
			{
				if (target->args[i] != 0)
				{
					break;
				}
			}
			if (i < 5)
			{
				do
				{
					i = (pr_dragonseek()>>2)%5;
				} while(!target->args[i]);
				FActorIterator iterator (target->args[i]);
				actor->tracer = iterator.Next ();
			}
		}
	}
}

//============================================================================
//
// A_DragonInitFlight
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonInitFlight)
{
	PARAM_ACTION_PROLOGUE;

	FActorIterator iterator (self->tid);

	do
	{ // find the first tid identical to the dragon's tid
		self->tracer = iterator.Next ();
		if (self->tracer == NULL)
		{
			self->SetState (self->SpawnState);
			return 0;
		}
	} while (self->tracer == self);
	self->RemoveFromHash ();
	return 0;
}

//============================================================================
//
// A_DragonFlight
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonFlight)
{
	PARAM_ACTION_PROLOGUE;

	angle_t angle;

	DragonSeek (self, 4*ANGLE_1, 8*ANGLE_1);
	if (self->target)
	{
		if(!(self->target->flags&MF_SHOOTABLE))
		{ // target died
			self->target = NULL;
			return 0;
		}
		angle = self->AngleTo(self->target);
		if (absangle(self->angle-angle) < ANGLE_45/2 && self->CheckMeleeRange())
		{
			int damage = pr_dragonflight.HitDice (8);
			int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
			S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		}
		else if (absangle(self->angle-angle) <= ANGLE_1*20)
		{
			self->SetState (self->MissileState);
			S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		}
	}
	else
	{
		P_LookForPlayers (self, true, NULL);
	}
	return 0;
}

//============================================================================
//
// A_DragonFlap
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonFlap)
{
	PARAM_ACTION_PROLOGUE;

	CALL_ACTION(A_DragonFlight, self);
	if (pr_dragonflap() < 240)
	{
		S_Sound (self, CHAN_BODY, "DragonWingflap", 1, ATTN_NORM);
	}
	else
	{
		self->PlayActiveSound ();
	}
	return 0;
}

//============================================================================
//
// A_DragonAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonAttack)
{
	PARAM_ACTION_PROLOGUE;

	P_SpawnMissile (self, self->target, PClass::FindActor("DragonFireball"));
	return 0;
}

//============================================================================
//
// A_DragonFX2
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonFX2)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int i;
	int delay;

	delay = 16+(pr_dragonfx2()>>3);
	for (i = 1+(pr_dragonfx2()&3); i; i--)
	{
		fixed_t xo = ((pr_dragonfx2() - 128) << 14);
		fixed_t yo = ((pr_dragonfx2() - 128) << 14);
		fixed_t zo = ((pr_dragonfx2() - 128) << 12);

		mo = Spawn ("DragonExplosion", self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			mo->tics = delay+(pr_dragonfx2()&3)*i*2;
			mo->target = self->target;
		}
	}
	return 0;
}

//============================================================================
//
// A_DragonPain
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonPain)
{
	PARAM_ACTION_PROLOGUE;

	CALL_ACTION(A_Pain, self);
	if (!self->tracer)
	{ // no destination spot yet
		self->SetState (self->SeeState);
	}
	return 0;
}

//============================================================================
//
// A_DragonCheckCrash
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DragonCheckCrash)
{
	PARAM_ACTION_PROLOGUE;

	if (self->Z() <= self->floorz)
	{
		self->SetState (self->FindState ("Crash"));
	}
	return 0;
}
