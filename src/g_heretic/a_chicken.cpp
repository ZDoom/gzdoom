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
	if (!(velx | vely) && pr_chickenplayerthink () < 160)
	{ // Twitch view angle
		angle += pr_chickenplayerthink.Random2 () << 19;
	}
	if ((Z() <= floorz) && (pr_chickenplayerthink() < 32))
	{ // Jump and noise
		velz += JumpZ;

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
	if (!self->target)
	{
		return;
	}
	if (self->CheckMeleeRange())
	{
		int damage = 1 + (pr_chicattack() & 1);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Feathers
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Feathers)
{
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
		mo = Spawn("Feather", self->PosPlusZ(20*FRACUNIT), NO_REPLACE);
		mo->target = self;
		mo->velx = pr_feathers.Random2() << 8;
		mo->vely = pr_feathers.Random2() << 8;
		mo->velz = FRACUNIT + (pr_feathers() << 9);
		mo->SetState (mo->SpawnState + (pr_feathers()&7));
	}
}

//---------------------------------------------------------------------------
//
// PROC P_UpdateBeak
//
//---------------------------------------------------------------------------

void P_UpdateBeak (AActor *self)
{
	if (self->player != NULL)
	{
		self->player->psprites[ps_weapon].sy = WEAPONTOP +
			(self->player->chickenPeck << (FRACBITS-1));
	}
}

//---------------------------------------------------------------------------
//
// PROC A_BeakRaise
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BeakRaise)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	player->psprites[ps_weapon].sy = WEAPONTOP;
	P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
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
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	damage = 1 + (pr_beakatkpl1()&3);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE, &linetarget);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true, &linetarget);
	if (linetarget)
	{
		player->mo->angle = player->mo->AngleTo(linetarget);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->psprites[ps_weapon].tics -= pr_beakatkpl1() & 7;
}

//----------------------------------------------------------------------------
//
// PROC A_BeakAttackPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BeakAttackPL2)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	damage = pr_beakatkpl2.HitDice (4);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE, &linetarget);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true, &linetarget);
	if (linetarget)
	{
		player->mo->angle = player->mo->AngleTo(linetarget);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->psprites[ps_weapon].tics -= pr_beakatkpl2()&3;
}
