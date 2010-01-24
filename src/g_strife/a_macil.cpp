/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
*/

// Macil (version 1) ---------------------------------------------------------

class AMacil1 : public AActor
{
	DECLARE_CLASS (AMacil1, AActor)
};

IMPLEMENT_CLASS (AMacil1)

// Macil (version 2) ---------------------------------------------------------

class AMacil2 : public AMacil1
{
	DECLARE_CLASS (AMacil2, AMacil1)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AMacil2)

//============================================================================
//
// AMacil2 :: TakeSpecialDamage
//
// Macil2 is invulnerable to the first stage Sigil.
//
//============================================================================

int AMacil2::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor != NULL)
	{
		FName name = inflictor->GetClass()->TypeName;
		if (name == NAME_SpectralLightningV1 || name == NAME_SpectralLightningV2)
			return -1;
	}
	return Super::TakeSpecialDamage(inflictor, source, damage, damagetype);
}
