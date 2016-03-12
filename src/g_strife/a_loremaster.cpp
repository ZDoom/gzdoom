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
	
	if (victim != NULL && target != NULL && !(victim->flags7 & MF7_DONTTHRUST))
	{
		fixedvec3 fixthrust = victim->Vec3To(target);
		DVector3 thrust(fixthrust.x, fixthrust.y, fixthrust.z);

		thrust.MakeUnit();
		thrust *= double((255*50*FRACUNIT) / (victim->Mass ? victim->Mass : 1));
	
		victim->vel.x += fixed_t(thrust.X);
		victim->vel.y += fixed_t(thrust.Y);
		victim->vel.z += fixed_t(thrust.Z);
	}
	return damage;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoremasterChain)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn("LoreShot2", self->Pos(), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-(self->vel.x >> 1), -(self->vel.y >> 1), -(self->vel.z >> 1)), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-self->vel.x, -self->vel.y, -self->vel.z), ALLOW_REPLACE);
	return 0;
}
