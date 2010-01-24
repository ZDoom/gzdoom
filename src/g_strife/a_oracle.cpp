/*
#include "actor.h"
#include "a_action.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "r_defs.h"
#include "thingdef/thingdef.h"
*/

class AOracle : public AActor
{
	DECLARE_CLASS (AOracle, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};


IMPLEMENT_CLASS (AOracle)


DEFINE_ACTION_FUNCTION(AActor, A_WakeOracleSpectre)
{
	TThinkerIterator<AActor> it(NAME_AlienSpectre3);
	AActor *spectre = it.Next();

	if (spectre != NULL)
	{
		spectre->Sector->SoundTarget = spectre->LastHeard = self->LastHeard;
		spectre->target = self->target;
		spectre->SetState (spectre->SeeState);
	}
}


//============================================================================
//
// AOracle :: TakeSpecialDamage
//
// The Oracle is invulnerable to the first stage Sigil.
//
//============================================================================

int AOracle::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor != NULL)
	{
		FName name = inflictor->GetClass()->TypeName;
		if (name == NAME_SpectralLightningV1 || name == NAME_SpectralLightningV2)
			return -1;
	}
	return Super::TakeSpecialDamage(inflictor, source, damage, damagetype);
}
