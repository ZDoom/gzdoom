/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/
static FRandom pr_prog ("Programmer");

// The Programmer level ending thing ----------------------------------------
// [RH] I took some liberties to make this "cooler" than it was in Strife.

class AProgLevelEnder : public AInventory
{
	DECLARE_CLASS (AProgLevelEnder, AInventory)
public:
	void Tick ();
	PalEntry GetBlend ();
};

IMPLEMENT_CLASS (AProgLevelEnder)

//============================================================================
//
// AProgLevelEnder :: Tick
//
// Fade to black, end the level, then unfade.
//
//============================================================================

void AProgLevelEnder::Tick ()
{
	if (special2 == 0)
	{ // fade out over .66 second
		special1 += 255 / (TICRATE*2/3);
		if (++special1 >= 255)
		{
			special1 = 255;
			special2 = 1;
			G_ExitLevel (0, false);
		}
	}
	else
	{ // fade in over two seconds
		special1 -= 255 / (TICRATE*2);
		if (special1 <= 0)
		{
			Destroy ();
		}
	}
}

//============================================================================
//
// AProgLevelEnder :: GetBlend
//
//============================================================================

PalEntry AProgLevelEnder::GetBlend ()
{
	return PalEntry ((BYTE)special1, 0, 0, 0);
}

//============================================================================
//
// A_ProgrammerMelee
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ProgrammerMelee)
{
	PARAM_ACTION_PROLOGUE;

	int damage;

	if (self->target == NULL)
		return 0;

	A_FaceTarget (self);

	if (!self->CheckMeleeRange ())
		return 0;

	S_Sound (self, CHAN_WEAPON, "programmer/clank", 1, ATTN_NORM);

	damage = ((pr_prog() % 10) + 1) * 6;
	int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
	P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	return 0;
}

//============================================================================
//
// A_SpotLightning
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpotLightning)
{
	PARAM_ACTION_PROLOGUE;

	AActor *spot;

	if (self->target == NULL)
		return 0;

	spot = Spawn("SpectralLightningSpot", self->target->PosAtZ(self->target->floorz), ALLOW_REPLACE);
	if (spot != NULL)
	{
		spot->threshold = 25;
		spot->target = self;
		spot->FriendPlayer = 0;
		spot->tracer = self->target;
	}
	return 0;
}

//============================================================================
//
// A_SpawnProgrammerBase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpawnProgrammerBase)
{
	PARAM_ACTION_PROLOGUE;

	AActor *foo = Spawn("ProgrammerBase", self->PosPlusZ(24.), ALLOW_REPLACE);
	if (foo != NULL)
	{
		foo->Angles.Yaw = self->Angles.Yaw + 180. + pr_prog.Random2() * (360. / 1024.);
		foo->VelFromAngle();
		foo->Vel.Z = pr_prog() / 128.;
	}
	return 0;
}

//============================================================================
//
// A_ProgrammerDeath
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ProgrammerDeath)
{
	PARAM_ACTION_PROLOGUE;

	if (!CheckBossDeath (self))
		return 0;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
		{
			players[i].mo->GiveInventoryType (RUNTIME_CLASS(AProgLevelEnder));
			break;
		}
	}
	// the sky change scripts are now done as special actions in MAPINFO
	A_BossDeath(self);
	return 0;
}
