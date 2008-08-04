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
	if (!(momx | momy) && pr_chickenplayerthink () < 160)
	{ // Twitch view angle
		angle += pr_chickenplayerthink.Random2 () << 19;
	}
	if ((z <= floorz) && (pr_chickenplayerthink() < 32))
	{ // Jump and noise
		momz += JumpZ;

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

void A_ChicAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (actor->CheckMeleeRange())
	{
		int damage = 1 + (pr_chicattack() & 1);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Feathers
//
//----------------------------------------------------------------------------

void A_Feathers (AActor *actor)
{
	int i;
	int count;
	AActor *mo;

	if (actor->health > 0)
	{ // Pain
		count = pr_feathers() < 32 ? 2 : 1;
	}
	else
	{ // Death
		count = 5 + (pr_feathers()&3);
	}
	for (i = 0; i < count; i++)
	{
		mo = Spawn("Feather", actor->x, actor->y, actor->z+20*FRACUNIT, NO_REPLACE);
		mo->target = actor;
		mo->momx = pr_feathers.Random2() << 8;
		mo->momy = pr_feathers.Random2() << 8;
		mo->momz = FRACUNIT + (pr_feathers() << 9);
		mo->SetState (mo->SpawnState + (pr_feathers()&7));
	}
}

//---------------------------------------------------------------------------
//
// PROC P_UpdateBeak
//
//---------------------------------------------------------------------------

void P_UpdateBeak (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->psprites[ps_weapon].sy = WEAPONTOP +
			(actor->player->chickenPeck << (FRACBITS-1));
	}
}

//---------------------------------------------------------------------------
//
// PROC A_BeakRaise
//
//---------------------------------------------------------------------------

void A_BeakRaise (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
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

void A_BeakAttackPL1 (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	damage = 1 + (pr_beakatkpl1()&3);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE, &linetarget);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true);
	if (linetarget)
	{
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
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

void A_BeakAttackPL2 (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	damage = pr_beakatkpl2.HitDice (4);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE, &linetarget);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, "BeakPuff", true);
	if (linetarget)
	{
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->psprites[ps_weapon].tics -= pr_beakatkpl2()&3;
}
