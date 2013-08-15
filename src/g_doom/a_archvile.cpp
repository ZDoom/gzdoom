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
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
	return 0;
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
DEFINE_ACTION_FUNCTION(AActor, A_StartFire)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self, 0);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FireCrackle)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self, 0);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Fire)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT(height)		{ height = 0; }
	
	A_Fire(self, height);
	return 0;
}

void A_Fire(AActor *self, int height)
{
	AActor *dest;
	angle_t an;
				
	dest = self->tracer;
	if (dest == NULL || self->target == NULL)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z + height);
}



//
// A_VileTarget
// Spawn the hellfire
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_VileTarget)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT(fire, AActor)	{ fire = PClass::FindActor("ArchvileFire"); }

	AActor *fog;
		
	if (!self->target)
		return 0;

	A_FaceTarget (self);

	fog = Spawn (fire, self->target->x, self->target->y,
		self->target->z, ALLOW_REPLACE);
	
	self->tracer = fog;
	fog->target = self;
	fog->tracer = self->target;
	A_Fire(fog, 0);
	return 0;
}




//
// A_VileAttack
//

// A_VileAttack flags
#define VAF_DMGTYPEAPPLYTODIRECT 1

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_VileAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND_OPT	(snd)		{ snd = "vile/stop"; }
	PARAM_INT_OPT	(dmg)		{ dmg = 20; }
	PARAM_INT_OPT	(blastdmg)	{ blastdmg = 70; }
	PARAM_INT_OPT	(blastrad)	{ blastrad = 70; }
	PARAM_FIXED_OPT	(thrust)	{ thrust = FRACUNIT; }
	PARAM_NAME_OPT	(dmgtype)	{ dmgtype = NAME_Fire; }
	PARAM_INT_OPT	(flags)		{ flags = 0; }

	AActor *fire, *target;
	angle_t an;
		
	if (NULL == (target = self->target))
		return 0;
	
	A_FaceTarget (self);

	if (!P_CheckSight (self, target, 0) )
		return 0;

	S_Sound (self, CHAN_WEAPON, snd, 1, ATTN_NORM);

	int newdam;

	if (flags & VAF_DMGTYPEAPPLYTODIRECT)
		newdam = P_DamageMobj (target, self, self, dmg, dmgtype);

	else
		newdam = P_DamageMobj (target, self, self, dmg, NAME_None);

	P_TraceBleed (newdam > 0 ? newdam : dmg, target);
		
	an = self->angle >> ANGLETOFINESHIFT;
	fire = self->tracer;

	if (fire != NULL)
	{
		// move the fire between the vile and the player
		fire->SetOrigin (target->x - FixedMul (24*FRACUNIT, finecosine[an]),
						 target->y - FixedMul (24*FRACUNIT, finesine[an]),
						 target->z);
		
		P_RadiusAttack (fire, self, blastdmg, blastrad, dmgtype, 0);
	}
	target->velz = Scale(thrust, 1000, target->Mass);
	return 0;
}
