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
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (ALoreShot)

int ALoreShot::DoSpecialDamage (AActor *target, int damage)
{
	FVector3 thrust;
	
	if (this->target != NULL)
	{
		thrust.X = float(this->target->x - target->x);
		thrust.Y = float(this->target->y - target->y);
		thrust.Z = float(this->target->z - target->z);
	
		thrust.MakeUnit();
		thrust *= float((255*50*FRACUNIT) / (target->Mass ? target->Mass : 1));
	
		target->velx += fixed_t(thrust.X);
		target->vely += fixed_t(thrust.Y);
		target->velz += fixed_t(thrust.Z);
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
