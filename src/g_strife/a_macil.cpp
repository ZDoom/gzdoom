#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

// Macil (version 2) ---------------------------------------------------------

class AMacil1 : public AActor
{
	DECLARE_CLASS (AMacil1, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AMacil1)

//============================================================================
//
// AMacil2 :: TakeSpecialDamage
//
// Macil is invulnerable to the first stage Sigil.
//
//============================================================================

int AMacil1::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor != NULL && inflictor->GetClass()->TypeName == NAME_SpectralLightningV1)
		return -1;

	return Super::TakeSpecialDamage(inflictor, source, damage, damagetype);
}
