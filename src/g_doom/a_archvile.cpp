/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
void A_Fire(AActor *self, int height);


//
// A_VileStart
//
DEFINE_ACTION_FUNCTION(AActor, A_VileStart)
{
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
DEFINE_ACTION_FUNCTION(AActor, A_StartFire)
{
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireCrackle)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self, 0);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Fire)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_FIXED(height,0);
	
	A_Fire(self, height);
}

void A_Fire(AActor *self, int height)
{
	AActor *dest;
				
	dest = self->tracer;
	if (dest == NULL || self->target == NULL)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	fixedvec3 newpos = dest->Vec3Angle(24 * FRACUNIT, dest->angle, height);
	self->SetOrigin(newpos, true);
}



//
// A_VileTarget
// Spawn the hellfire
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_VileTarget)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(fire,0);
	AActor *fog;
		
	if (!self->target)
		return;

	A_FaceTarget (self);

	fog = Spawn (fire, self->target->Pos(), ALLOW_REPLACE);
	
	self->tracer = fog;
	fog->target = self;
	fog->tracer = self->target;
	A_Fire(fog, 0);
}




//
// A_VileAttack
//

// A_VileAttack flags
#define VAF_DMGTYPEAPPLYTODIRECT 1

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_VileAttack)
{		
	ACTION_PARAM_START(7);
	ACTION_PARAM_SOUND(snd,0);
	ACTION_PARAM_INT(dmg,1);
	ACTION_PARAM_INT(blastdmg,2);
	ACTION_PARAM_INT(blastrad,3);
	ACTION_PARAM_FIXED(thrust,4);
	ACTION_PARAM_NAME(dmgtype,5);
	ACTION_PARAM_INT(flags,6);

	AActor *fire, *target;
		
	if (NULL == (target = self->target))
		return;
	
	A_FaceTarget (self);

	if (!P_CheckSight (self, target, 0) )
		return;

	S_Sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);

	int newdam;

	if (flags & VAF_DMGTYPEAPPLYTODIRECT)
		newdam = P_DamageMobj (target, self, self, dmg, dmgtype);

	else
		newdam = P_DamageMobj (target, self, self, dmg, NAME_None);

	P_TraceBleed (newdam > 0 ? newdam : dmg, target);
		
	fire = self->tracer;

	if (fire != NULL)
	{
		// move the fire between the vile and the player
		fixedvec3 pos = target->Vec3Angle(-24 * FRACUNIT, self->angle, 0);
		fire->SetOrigin (pos, true);
		
		P_RadiusAttack (fire, self, blastdmg, blastrad, dmgtype, 0);
	}
	if (!(target->flags7 & MF7_DONTTHRUST))
		target->velz = Scale(thrust, 1000, target->Mass);
}
