/*
#include "actor.h"
#include "a_action.h"
#include "a_strifeglobal.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

// Loremaster (aka Priest) --------------------------------------------------

class ALoreShot : public AActor
{
	DECLARE_CLASS (ALoreShot, AActor)
public:
	int DoSpecialDamage (AActor *victim, int damage, FName damagetype);
};

IMPLEMENT_CLASS (ALoreShot)

int ALoreShot::DoSpecialDamage (AActor *victim, int damage, FName damagetype)
{
	FVector3 thrust;
	
	if (victim != NULL && target != NULL && !(victim->flags7 & MF7_DONTTHRUST))
	{
		thrust.X = float(target->x - victim->x);
		thrust.Y = float(target->y - victim->y);
		thrust.Z = float(target->z - victim->z);
	
		thrust.MakeUnit();
		thrust *= float((255*50*FRACUNIT) / (victim->Mass ? target->Mass : 1));
	
		victim->velx += fixed_t(thrust.X);
		victim->vely += fixed_t(thrust.Y);
		victim->velz += fixed_t(thrust.Z);
	}
	return damage;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoremasterChain)
{
	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn("LoreShot2", self->x, self->y, self->z, ALLOW_REPLACE);
	Spawn("LoreShot2", self->x - (self->velx >> 1), self->y - (self->vely >> 1), self->z - (self->velz >> 1), ALLOW_REPLACE);
	Spawn("LoreShot2", self->x - self->velx, self->y - self->vely, self->z - self->velz, ALLOW_REPLACE);
}
