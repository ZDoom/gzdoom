#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"

static FRandom pr_imp ("ImpExplode");

//----------------------------------------------------------------------------
//
// PROC A_ImpExplode
//
//----------------------------------------------------------------------------

void A_ImpExplode (AActor *self)
{
	AActor *chunk;

	self->flags &= ~MF_NOGRAVITY;

	chunk = Spawn("HereticImpChunk1", self->x, self->y, self->z, ALLOW_REPLACE);
	chunk->momx = pr_imp.Random2 () << 10;
	chunk->momy = pr_imp.Random2 () << 10;
	chunk->momz = 9*FRACUNIT;

	chunk = Spawn("HereticImpChunk2", self->x, self->y, self->z, ALLOW_REPLACE);
	chunk->momx = pr_imp.Random2 () << 10;
	chunk->momy = pr_imp.Random2 () << 10;
	chunk->momz = 9*FRACUNIT;
	if (self->special1 == 666)
	{ // Extreme death crash
		self->SetState (self->FindState("XCrash"));
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ImpDeath
//
//----------------------------------------------------------------------------

void A_ImpDeath (AActor *self)
{
	self->flags &= ~MF_SOLID;
	self->flags2 |= MF2_FLOORCLIP;
}

//----------------------------------------------------------------------------
//
// PROC A_ImpXDeath1
//
//----------------------------------------------------------------------------

void A_ImpXDeath1 (AActor *self)
{
	self->flags &= ~MF_SOLID;
	self->flags |= MF_NOGRAVITY;
	self->flags2 |= MF2_FLOORCLIP;
	self->special1 = 666; // Flag the crash routine
}

