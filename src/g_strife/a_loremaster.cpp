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
	
		target->momx += fixed_t(thrust.X);
		target->momy += fixed_t(thrust.Y);
		target->momz += fixed_t(thrust.Z);
	}
	return damage;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoremasterChain)
{
	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn("LoreShot2", self->x, self->y, self->z, ALLOW_REPLACE);
	Spawn("LoreShot2", self->x - (self->momx >> 1), self->y - (self->momy >> 1), self->z - (self->momz >> 1), ALLOW_REPLACE);
	Spawn("LoreShot2", self->x - self->momx, self->y - self->momy, self->z - self->momz, ALLOW_REPLACE);
}
