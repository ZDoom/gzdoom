/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "a_specialspot.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_s2fx1 ("S2FX1");
static FRandom pr_scrc1atk ("Srcr1Attack");
static FRandom pr_dst ("D'SparilTele");
static FRandom pr_s2d ("Srcr2Decide");
static FRandom pr_s2a ("Srcr2Attack");
static FRandom pr_bluespark ("BlueSpark");

//----------------------------------------------------------------------------
//
// PROC A_Sor1Pain
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor1Pain)
{
	PARAM_ACTION_PROLOGUE;

	self->special1 = 20; // Number of steps to walk fast
	CALL_ACTION(A_Pain, self);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Chase
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor1Chase)
{
	PARAM_ACTION_PROLOGUE;

	if (self->special1)
	{
		self->special1--;
		self->tics -= 3;
	}
	A_Chase(stack, self);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr1Attack
//
// Sorcerer demon attack.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr1Attack)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	DAngle angle;

	if (!self->target)
	{
		return 0;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM);
	if (self->CheckMeleeRange ())
	{
		int damage = pr_scrc1atk.HitDice (8);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return 0;
	}

	PClassActor *fx = PClass::FindActor("SorcererFX1");
	if (self->health > (self->SpawnHealth()/3)*2)
	{ // Spit one fireball
		P_SpawnMissileZ (self, self->Z() + 48, self->target, fx );
	}
	else
	{ // Spit three fireballs
		mo = P_SpawnMissileZ (self, self->Z() + 48, self->target, fx);
		if (mo != NULL)
		{
			angle = mo->Angles.Yaw;
			P_SpawnMissileAngleZ(self, self->Z() + 48, fx, angle - 3, mo->Vel.Z);
			P_SpawnMissileAngleZ(self, self->Z() + 48, fx, angle + 3, mo->Vel.Z);
		}
		if (self->health < self->SpawnHealth()/3)
		{ // Maybe attack again
			if (self->special1)
			{ // Just attacked, so don't attack again
				self->special1 = 0;
			}
			else
			{ // Set state to attack again
				self->special1 = 1;
				self->SetState (self->FindState("Missile2"));
			}
		}
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_SorcererRise
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SorcererRise)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	self->flags &= ~MF_SOLID;
	mo = Spawn("Sorcerer2", self->Pos(), ALLOW_REPLACE);
	mo->Translation = self->Translation;
	mo->SetState (mo->FindState("Rise"));
	mo->Angles.Yaw = self->Angles.Yaw;
	mo->CopyFriendliness (self, true);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC P_DSparilTeleport
//
//----------------------------------------------------------------------------

void P_DSparilTeleport (AActor *actor)
{
	DVector3 prev;
	AActor *mo;
	AActor *spot;

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL) return;

	spot = state->GetSpotWithMinMaxDistance(PClass::FindClass("BossSpot"), actor->X(), actor->Y(), 128, 0);
	if (spot == NULL) return;

	prev = actor->Pos();
	if (P_TeleportMove (actor, spot->Pos(), false))
	{
		mo = Spawn("Sorcerer2Telefade", prev, ALLOW_REPLACE);
		if (mo) mo->Translation = actor->Translation;
		S_Sound (mo, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
		actor->SetState (actor->FindState("Teleport"));
		S_Sound (actor, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
		actor->SetZ(actor->floorz);
		actor->Angles.Yaw = spot->Angles.Yaw;
		actor->Vel.Zero();
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Decide
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr2Decide)
{
	PARAM_ACTION_PROLOGUE;

	static const int chance[] =
	{
		192, 120, 120, 120, 64, 64, 32, 16, 0
	};

	unsigned int chanceindex = self->health / ((self->SpawnHealth()/8 == 0) ? 1 : self->SpawnHealth()/8);
	if (chanceindex >= countof(chance))
	{
		chanceindex = countof(chance) - 1;
	}

	if (pr_s2d() < chance[chanceindex])
	{
		P_DSparilTeleport (self);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Attack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr2Attack)
{
	PARAM_ACTION_PROLOGUE;

	int chance;

	if (!self->target)
	{
		return 0;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NONE);
	if (self->CheckMeleeRange())
	{
		int damage = pr_s2a.HitDice (20);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return 0;
	}
	chance = self->health < self->SpawnHealth()/2 ? 96 : 48;
	if (pr_s2a() < chance)
	{ // Wizard spawners

		PClassActor *fx = PClass::FindActor("Sorcerer2FX2");
		if (fx)
		{
			P_SpawnMissileAngle(self, fx, self->Angles.Yaw - 45, 0.5);
			P_SpawnMissileAngle(self, fx, self->Angles.Yaw + 45, 0.5);
		}
	}
	else
	{ // Blue bolt
		P_SpawnMissile (self, self->target, PClass::FindActor("Sorcerer2FX1"));
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_BlueSpark
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BlueSpark)
{
	PARAM_ACTION_PROLOGUE;

	int i;
	AActor *mo;

	for (i = 0; i < 2; i++)
	{
		mo = Spawn("Sorcerer2FXSpark", self->Pos(), ALLOW_REPLACE);
		mo->Vel.X = pr_bluespark.Random2() / 128.;
		mo->Vel.Y = pr_bluespark.Random2() / 128.;
		mo->Vel.Z = 1. + pr_bluespark() / 256.;
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_GenWizard)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	mo = Spawn("Wizard", self->Pos(), ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->AddZ(-mo->GetDefault()->Height / 2, false);
		if (!P_TestMobjLocation (mo))
		{ // Didn't fit
			mo->ClearCounters();
			mo->Destroy ();
		}
		else
		{ // [RH] Make the new wizards inherit D'Sparil's target
			mo->CopyFriendliness (self->target, true);

			self->Vel.Zero();
			self->SetState (self->FindState(NAME_Death));
			self->flags &= ~MF_MISSILE;
			mo->master = self->target;
			P_SpawnTeleportFog(self, self->Pos(), false, true);
		}
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthInit
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor2DthInit)
{
	PARAM_ACTION_PROLOGUE;

	self->special1 = 7; // Animation loop counter
	P_Massacre (); // Kill monsters early
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthLoop
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor2DthLoop)
{
	PARAM_ACTION_PROLOGUE;

	if (--self->special1)
	{ // Need to loop
		self->SetState (self->FindState("DeathLoop"));
	}
	return 0;
}

