/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "a_pickups.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "p_enemy.h"
#include "d_event.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
*/

void P_UpdateBeak (AActor *actor);

static FRandom pr_chickenplayerthink ("ChickenPlayerThink");
static FRandom pr_chicattack ("ChicAttack");
static FRandom pr_feathers ("Feathers");
static FRandom pr_beakatkpl1 ("BeakAtkPL1");
static FRandom pr_beakatkpl2 ("BeakAtkPL2");

class AChickenPlayer : public APlayerPawn
{
	DECLARE_CLASS (AChickenPlayer, APlayerPawn)
public:
	void MorphPlayerThink ();
};

IMPLEMENT_CLASS(AChickenPlayer)

void AChickenPlayer::MorphPlayerThink ()
{
	if (health > 0)
	{ // Handle beak movement
		P_UpdateBeak (this);
	}
	if (player->morphTics & 15)
	{
		return;
	}
	if (Vel.X == 0 && Vel.Y == 0 && pr_chickenplayerthink () < 160)
	{ // Twitch view angle
		Angles.Yaw += pr_chickenplayerthink.Random2() * (360. / 256. / 32.);
	}
	if ((Z() <= floorz) && (pr_chickenplayerthink() < 32))
	{ // Jump and noise
		Vel.Z += JumpZ;

		FState * painstate = FindState(NAME_Pain);
		if (painstate != NULL) SetState (painstate);
	}
	if (pr_chickenplayerthink () < 48)
	{ // Just noise
		S_Sound (this, CHAN_VOICE, "chicken/active", 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ChicAttack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ChicAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
	{
		return 0;
	}
	if (self->CheckMeleeRange())
	{
		int damage = 1 + (pr_chicattack() & 1);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_Feathers
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Feathers)
{
	PARAM_ACTION_PROLOGUE;

	int i;
	int count;
	AActor *mo;

	if (self->health > 0)
	{ // Pain
		count = pr_feathers() < 32 ? 2 : 1;
	}
	else
	{ // Death
		count = 5 + (pr_feathers()&3);
	}
	for (i = 0; i < count; i++)
	{
		mo = Spawn("Feather", self->PosPlusZ(20.), NO_REPLACE);
		mo->target = self;
		mo->Vel.X = pr_feathers.Random2() / 256.;
		mo->Vel.Y = pr_feathers.Random2() / 256.;
		mo->Vel.Z = 1. + pr_feathers() / 128.;
		mo->SetState (mo->SpawnState + (pr_feathers()&7));
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC P_UpdateBeak
//
//---------------------------------------------------------------------------

void P_UpdateBeak (AActor *self)
{
	DPSprite *pspr;
	if (self->player != nullptr && (pspr = self->player->FindPSprite(PSP_WEAPON)) != nullptr)
	{
		pspr->y = WEAPONTOP + self->player->chickenPeck / 2;
	}
}

//---------------------------------------------------------------------------
//
// PROC A_BeakRaise
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BeakRaise)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (nullptr == (player = self->player))
	{
		return 0;
	}
	player->GetPSprite(PSP_WEAPON)->y = WEAPONTOP;
	P_SetPsprite(player, PSP_WEAPON, player->ReadyWeapon->GetReadyState());
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC P_PlayPeck
//
//----------------------------------------------------------------------------

void P_PlayPeck (AActor *chicken)
{
	S_Sound (chicken, CHAN_VOICE, "chicken/peck", 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_BeakAttackPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BeakAttackPL1)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	int damage;
	DAngle slope;
	player_t *player;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	damage = 1 + (pr_beakatkpl1()&3);
	angle = player->mo->Angles.Yaw;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true, &t);
	if (t.linetarget)
	{
		player->mo->Angles.Yaw = t.angleFromSource;
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->GetPSprite(PSP_WEAPON)->Tics -= pr_beakatkpl1() & 7;
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_BeakAttackPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BeakAttackPL2)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	int damage;
	DAngle slope;
	player_t *player;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	damage = pr_beakatkpl2.HitDice (4);
	angle = player->mo->Angles.Yaw;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true, &t);
	if (t.linetarget)
	{
		player->mo->Angles.Yaw = t.angleFromSource;
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->GetPSprite(PSP_WEAPON)->Tics -= pr_beakatkpl2()&3;
	return 0;
}
