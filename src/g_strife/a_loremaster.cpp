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
		TVector3<double> thrust(fixthrust.x, fixthrust.y, fixthrust.z);

		thrust.MakeUnit();
		thrust *= double((255*50*FRACUNIT) / (victim->Mass ? victim->Mass : 1));
	
		victim->velx += fixed_t(thrust.X);
		victim->vely += fixed_t(thrust.Y);
		victim->velz += fixed_t(thrust.Z);
	}
	return damage;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoremasterChain)
{
	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn("LoreShot2", self->Pos(), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-(self->velx >> 1), -(self->vely >> 1), -(self->velz >> 1)), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-self->velx, -self->vely, -self->velz), ALLOW_REPLACE);
}
