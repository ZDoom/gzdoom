/*
#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_tracer ("Tracer");
static FRandom pr_skelfist ("SkelFist");

//
// A_SkelMissile
//
DEFINE_ACTION_FUNCTION(AActor, A_SkelMissile)
{
	PARAM_ACTION_PROLOGUE;

	AActor *missile;
		
	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	self->AddZ(16.);
	missile = P_SpawnMissile(self, self->target, PClass::FindActor("RevenantTracer"));
	self->AddZ(-16.);

	if (missile != NULL)
	{
		missile->SetOrigin(missile->Vec3Offset(missile->Vel.X, missile->Vel.Y, 0.), false);
		missile->tracer = self->target;
	}
	return 0;
}

#define TRACEANGLE (16.875)

DEFINE_ACTION_FUNCTION(AActor, A_Tracer)
{
	PARAM_ACTION_PROLOGUE;

	double dist;
	double slope;
	AActor *dest;
	AActor *smoke;
				
	// killough 1/18/98: this is why some missiles do not have smoke
	// and some do. Also, internal demos start at random gametics, thus
	// the bug in which revenants cause internal demos to go out of sync.
	//
	// killough 3/6/98: fix revenant internal demo bug by subtracting
	// levelstarttic from gametic:
	//
	// [RH] level.time is always 0-based, so nothing special to do here.

	if (level.time & 3)
		return 0;
	
	// spawn a puff of smoke behind the rocket
	P_SpawnPuff (self, PClass::FindActor(NAME_BulletPuff), self->Pos(), self->Angles.Yaw, self->Angles.Yaw, 3);
		
	smoke = Spawn ("RevenantTracerSmoke", self->Vec3Offset(-self->Vel.X, -self->Vel.Y, 0.), ALLOW_REPLACE);
	
	smoke->Vel.Z = 1.;
	smoke->tics -= pr_tracer()&3;
	if (smoke->tics < 1)
		smoke->tics = 1;
	
	// adjust direction
	dest = self->tracer;
		
	if (!dest || dest->health <= 0 || self->Speed == 0 || !self->CanSeek(dest))
		return 0;
	
	// change angle 	
	DAngle exact = self->AngleTo(dest);
	DAngle diff = deltaangle(self->Angles.Yaw, exact);

	if (diff < 0)
	{
		self->Angles.Yaw -= TRACEANGLE;
		if (deltaangle(self->Angles.Yaw, exact) > 0)
			self->Angles.Yaw = exact;
	}
	else if (diff > 0)
	{
		self->Angles.Yaw += TRACEANGLE;
		if (deltaangle(self->Angles.Yaw, exact) < 0.)
			self->Angles.Yaw = exact;
	}

	self->VelFromAngle();

	if (!(self->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
	{
		// change slope
		dist = self->DistanceBySpeed(dest, self->Speed);

		if (dest->Height >= 56.)
		{
			slope = (dest->Z() + 40. - self->Z()) / dist;
		}
		else
		{
			slope = (dest->Z() + self->Height*(2./3) - self->Z()) / dist;
		}

		if (slope < self->Vel.Z)
			self->Vel.Z -= 1. / 8;
		else
			self->Vel.Z += 1. / 8;
	}
	return 0;
}


DEFINE_ACTION_FUNCTION(AActor, A_SkelWhoosh)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;
	A_FaceTarget (self);
	S_Sound (self, CHAN_WEAPON, "skeleton/swing", 1, ATTN_NORM);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SkelFist)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
		
	if (self->CheckMeleeRange ())
	{
		int damage = ((pr_skelfist()%10)+1)*6;
		S_Sound (self, CHAN_WEAPON, "skeleton/melee", 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	return 0;
}
