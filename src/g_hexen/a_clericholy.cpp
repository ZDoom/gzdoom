/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "a_hexenglobal.h"
#include "gstrings.h"
#include "a_weaponpiece.h"
#include "vm.h"
#include "g_level.h"
#include "doomstat.h"
*/

#define BLAST_FULLSTRENGTH	255

static FRandom pr_holyatk2 ("CHolyAtk2");
static FRandom pr_holyseeker ("CHolySeeker");
static FRandom pr_holyweave ("CHolyWeave");
static FRandom pr_holyseek ("CHolySeek");
static FRandom pr_checkscream ("CCheckScream");
static FRandom pr_spiritslam ("CHolySlam");
static FRandom pr_wraithvergedrop ("WraithvergeDrop");

void SpawnSpiritTail (AActor *spirit);

// Holy Spirit --------------------------------------------------------------

//============================================================================
//
// CHolyFindTarget
//
//============================================================================

static void CHolyFindTarget (AActor *actor)
{
	AActor *target;

	if ( (target = P_RoughMonsterSearch (actor, 6, true)) )
	{
		actor->tracer = target;
		actor->flags |= MF_NOCLIP|MF_SKULLFLY;
		actor->flags &= ~MF_MISSILE;
	}
}

//============================================================================
//
// CHolySeekerMissile
//
// 	 Similar to P_SeekerMissile, but seeks to a random Z on the target
//============================================================================

static void CHolySeekerMissile (AActor *actor, DAngle thresh, DAngle turnMax)
{
	int dir;
	DAngle delta;
	AActor *target;
	double newZ;
	double deltaZ;

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	if (!(target->flags&MF_SHOOTABLE)
		|| (!(target->flags3&MF3_ISMONSTER) && !target->player))
	{ // Target died/target isn't a player or creature
		actor->tracer = NULL;
		actor->flags &= ~(MF_NOCLIP | MF_SKULLFLY);
		actor->flags |= MF_MISSILE;
		CHolyFindTarget(actor);
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta /= 2;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->Angles.Yaw += delta;
	}
	else
	{ // Turn counter clockwise
		actor->Angles.Yaw -= delta;
	}
	actor->VelFromAngle();

	if (!(level.time&15) 
		|| actor->Z() > target->Top()
		|| actor->Top() < target->Z())
	{
		newZ = target->Z() + ((pr_holyseeker()*target->Height) / 256.);
		deltaZ = newZ - actor->Z();
		if (fabs(deltaZ) > 15)
		{
			if (deltaZ > 0)
			{
				deltaZ = 15;
			}
			else
			{
				deltaZ = -15;
			}
		}
		actor->Vel.Z = deltaZ / actor->DistanceBySpeed(target, actor->Speed);
	}
	return;
}

//============================================================================
//
// A_CHolySeek
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolySeek)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->health--;
	if (self->health <= 0)
	{
		self->Vel.X /= 4;
		self->Vel.Y /= 4;
		self->Vel.Z = 0;
		self->SetState (self->FindState(NAME_Death));
		self->tics -= pr_holyseek()&3;
		return 0;
	}
	if (self->tracer)
	{
		CHolySeekerMissile (self, (double)self->args[0], self->args[0]*2.);
		if (!((level.time+7)&15))
		{
			self->args[0] = 5+(pr_holyseek()/20);
		}
	}

	int xyspeed = (pr_holyweave() % 5);
	int zspeed = (pr_holyweave() % 5);
	A_Weave(self, xyspeed, zspeed, 4., 2.);
	return 0;
}

//============================================================================
//
// A_CHolyCheckScream
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyCheckScream)
{
	PARAM_SELF_PROLOGUE(AActor);

	CALL_ACTION(A_CHolySeek, self);
	if (pr_checkscream() < 20)
	{
		S_Sound (self, CHAN_VOICE, "SpiritActive", 1, ATTN_NORM);
	}
	if (!self->tracer)
	{
		CHolyFindTarget(self);
	}
	return 0;
}

