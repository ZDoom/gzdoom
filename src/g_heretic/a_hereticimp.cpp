/*
#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_impmsatk ("ImpMsAttack");
static FRandom pr_imp ("ImpExplode");


//----------------------------------------------------------------------------
//
// PROC A_ImpMsAttack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ImpMsAttack)
{
	PARAM_ACTION_PROLOGUE;

    if (!self->target || pr_impmsatk() > 64)
    {
        self->SetState (self->SeeState);
        return 0;
    }
	A_SkullAttack(self, 12 * FRACUNIT);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpExplode
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ImpExplode)
{
	PARAM_ACTION_PROLOGUE;

	AActor *chunk;

	self->flags &= ~MF_NOGRAVITY;

	chunk = Spawn("HereticImpChunk1", self->Pos(), ALLOW_REPLACE);
	chunk->velx = pr_imp.Random2 () << 10;
	chunk->vely = pr_imp.Random2 () << 10;
	chunk->velz = 9*FRACUNIT;

	chunk = Spawn("HereticImpChunk2", self->Pos(), ALLOW_REPLACE);
	chunk->velx = pr_imp.Random2 () << 10;
	chunk->vely = pr_imp.Random2 () << 10;
	chunk->velz = 9*FRACUNIT;
	if (self->special1 == 666)
	{ // Extreme death crash
		self->SetState (self->FindState("XCrash"));
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpDeath
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ImpDeath)
{
	PARAM_ACTION_PROLOGUE;

	self->flags &= ~MF_SOLID;
	self->flags2 |= MF2_FLOORCLIP;
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ImpXDeath1)
{
	PARAM_ACTION_PROLOGUE;

	self->flags &= ~MF_SOLID;
	self->flags |= MF_NOGRAVITY;
	self->flags2 |= MF2_FLOORCLIP;
	self->special1 = 666; // Flag the crash routine
	return 0;
}

