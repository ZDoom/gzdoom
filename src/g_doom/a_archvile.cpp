#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
DECLARE_ACTION(A_Fire)



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
	CALL_ACTION(A_Fire, self);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireCrackle)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	CALL_ACTION(A_Fire, self);
}

DEFINE_ACTION_FUNCTION(AActor, A_Fire)
{
	AActor *dest;
	angle_t an;
				
	dest = self->tracer;
	if (!dest)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z);
}



//
// A_VileTarget
// Spawn the hellfire
//
DEFINE_ACTION_FUNCTION(AActor, A_VileTarget)
{
	AActor *fog;
		
	if (!self->target)
		return;

	A_FaceTarget (self);

	fog = Spawn ("ArchvileFire", self->target->x, self->target->y,
		self->target->z, ALLOW_REPLACE);
	
	self->tracer = fog;
	fog->target = self;
	fog->tracer = self->target;
	CALL_ACTION(A_Fire, fog);
}




//
// A_VileAttack
//
DEFINE_ACTION_FUNCTION(AActor, A_VileAttack)
{		
	AActor *fire;
	int an;
		
	if (!self->target)
		return;
	
	A_FaceTarget (self);

	if (!P_CheckSight (self, self->target, 0) )
		return;

	S_Sound (self, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_DamageMobj (self->target, self, self, 20, NAME_None);
	P_TraceBleed (20, self->target);
	self->target->momz = 1000 * FRACUNIT / self->target->Mass;
		
	an = self->angle >> ANGLETOFINESHIFT;

	fire = self->tracer;

	if (!fire)
		return;
				
	// move the fire between the vile and the player
	fire->SetOrigin (self->target->x - FixedMul (24*FRACUNIT, finecosine[an]),
					 self->target->y - FixedMul (24*FRACUNIT, finesine[an]),
					 self->target->z);
	
	P_RadiusAttack (fire, self, 70, 70, NAME_Fire, false);
}
