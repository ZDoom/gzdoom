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
		DVector3 thrust = victim->Vec3To(target);
		thrust.MakeResize(255. * 50 / MAX<int>(victim->Mass, 1));
		victim->Vel += thrust;
	}
	return damage;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoremasterChain)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "loremaster/active", 1, ATTN_NORM);
	Spawn("LoreShot2", self->Pos(), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-self->Vel/2.), ALLOW_REPLACE);
	Spawn("LoreShot2", self->Vec3Offset(-self->Vel), ALLOW_REPLACE);
	return 0;
}
