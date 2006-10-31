#include "actor.h"
#include "a_action.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"

void A_WakeOracleSpectre (AActor *);

// Oracle -------------------------------------------------------------------

FState AOracle::States[] =
{
	S_NORMAL (ORCL, 'A', -1, NULL,						NULL),

	S_NORMAL (ORCL, 'B',  5, NULL,						&States[2]),
	S_NORMAL (ORCL, 'C',  5, NULL,						&States[3]),
	S_NORMAL (ORCL, 'D',  5, NULL,						&States[4]),
	S_NORMAL (ORCL, 'E',  5, NULL,						&States[5]),
	S_NORMAL (ORCL, 'F',  5, NULL,						&States[6]),
	S_NORMAL (ORCL, 'G',  5, NULL,						&States[7]),
	S_NORMAL (ORCL, 'H',  5, NULL,						&States[8]),
	S_NORMAL (ORCL, 'I',  5, NULL,						&States[9]),
	S_NORMAL (ORCL, 'J',  5, NULL,						&States[10]),
	S_NORMAL (ORCL, 'K',  5, NULL,						&States[11]),
	S_NORMAL (ORCL, 'L',  5, A_NoBlocking,				&States[12]),
	S_NORMAL (ORCL, 'M',  5, NULL,						&States[13]),
	S_NORMAL (ORCL, 'N',  5, A_WakeOracleSpectre,		&States[14]),
	S_NORMAL (ORCL, 'O',  5, NULL,						&States[15]),
	S_NORMAL (ORCL, 'P',  5, NULL,						&States[16]),
	S_NORMAL (ORCL, 'Q', -1, NULL,						NULL)
};

IMPLEMENT_ACTOR (AOracle, Strife, 199, 0)
	PROP_StrifeType (65)
	PROP_StrifeTeaserType (62)
	PROP_StrifeTeaserType2 (63)
	PROP_SpawnHealth (1)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL|MF_NOTDMATCH)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_FIRERESIST)
	PROP_MaxDropOffHeight (32)
	PROP_MinMissileChance (150)
	PROP_Tag ("ORACLE")
END_DEFAULTS

//============================================================================
//
// AOracle :: NoBlockingSet
//
//============================================================================

void AOracle::NoBlockingSet ()
{
	P_DropItem (this, "Meat", -1, 256);
}

void A_WakeOracleSpectre (AActor *self)
{
	TThinkerIterator<AAlienSpectre3> it;
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
	if (inflictor != NULL && inflictor->IsKindOf (RUNTIME_CLASS(ASpectralLightningV1)))
		return -1;
	return Super::TakeSpecialDamage(inflictor, source, damage, damagetype);
}
