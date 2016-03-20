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
	self->AddZ(16*FRACUNIT);
	missile = P_SpawnMissile (self, self->target, PClass::FindActor("RevenantTracer"));
	self->AddZ(-16*FRACUNIT);

	if (missile != NULL)
	{
		missile->SetOrigin(missile->Vec3Offset(missile->vel.x, missile->vel.y, 0), false);
		missile->tracer = self->target;
	}
	return 0;
}

#define TRACEANGLE (0xc000000)

DEFINE_ACTION_FUNCTION(AActor, A_Tracer)
{
	PARAM_ACTION_PROLOGUE;

	angle_t exact;
	fixed_t dist;
	fixed_t slope;
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
	P_SpawnPuff (self, PClass::FindActor(NAME_BulletPuff), self->Pos(), self->angle, self->angle, 3);
		
	smoke = Spawn ("RevenantTracerSmoke", self->Vec3Offset(-self->vel.x, -self->vel.y, 0), ALLOW_REPLACE);
	
	smoke->vel.z = FRACUNIT;
	smoke->tics -= pr_tracer()&3;
	if (smoke->tics < 1)
		smoke->tics = 1;
	
	// adjust direction
	dest = self->tracer;
		
	if (!dest || dest->health <= 0 || self->Speed == 0 || !self->CanSeek(dest))
		return 0;
	
	// change angle 	
	exact = self->AngleTo(dest);

	if (exact != self->angle)
	{
		if (exact - self->angle > 0x80000000)
		{
			self->angle -= TRACEANGLE;
			if (exact - self->angle < 0x80000000)
				self->angle = exact;
		}
		else
		{
			self->angle += TRACEANGLE;
			if (exact - self->angle > 0x80000000)
				self->angle = exact;
		}
	}
		
	exact = self->angle>>ANGLETOFINESHIFT;
	self->vel.x = FixedMul (self->Speed, finecosine[exact]);
	self->vel.y = FixedMul (self->Speed, finesine[exact]);

	if (!(self->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
	{
		// change slope
		dist = self->AproxDistance (dest) / self->Speed;

		if (dist < 1)
			dist = 1;

		if (dest->height >= 56*FRACUNIT)
		{
			slope = (dest->Z()+40*FRACUNIT - self->Z()) / dist;
		}
		else
		{
			slope = (dest->Z() + self->height*2/3 - self->Z()) / dist;
		}

		if (slope < self->vel.z)
			self->vel.z -= FRACUNIT/8;
		else
			self->vel.z += FRACUNIT/8;
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
