#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "a_pickups.h"
#include "p_local.h"
#include "a_hereticglobal.h"
#include "a_sharedglobal.h"
#include "p_enemy.h"
#include "d_event.h"
#include "ravenshared.h"
#include "gstrings.h"

void A_BeakReady (player_t *, pspdef_t *);
void A_BeakRaise (player_t *, pspdef_t *);
void A_BeakAttackPL1 (player_t *, pspdef_t *);
void A_BeakAttackPL2 (player_t *, pspdef_t *);

void A_Feathers (AActor *);
void A_ChicLook (AActor *);
void A_ChicChase (AActor *);
void A_ChicPain (AActor *);
void A_ChicAttack (AActor *);

// Beak puff ----------------------------------------------------------------

class ABeakPuff : public AStaffPuff
{
	DECLARE_STATELESS_ACTOR (ABeakPuff, AStaffPuff)
public:
	void BeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ABeakPuff, Heretic, -1, 0)
	PROP_AttackSound ("chicken/attack")
END_DEFAULTS

void ABeakPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT;
}

// Beak ---------------------------------------------------------------------

class ABeak : public AWeapon
{
	DECLARE_ACTOR (ABeak, AWeapon)
	AT_GAME_SET_FRIEND (Beak)
private:
	static FWeaponInfo WeaponInfo1, WeaponInfo2;
};

FState ABeak::States[] =
{
#define S_BEAKREADY 0
	S_NORMAL (BEAK, 'A',	1, A_BeakReady				, &States[S_BEAKREADY]),

#define S_BEAKDOWN (S_BEAKREADY+1)
	S_NORMAL (BEAK, 'A',	1, A_Lower					, &States[S_BEAKDOWN]),

#define S_BEAKUP (S_BEAKDOWN+1)
	S_NORMAL (BEAK, 'A',	1, A_BeakRaise				, &States[S_BEAKUP]),

#define S_BEAKATK1 (S_BEAKUP+1)
	S_NORMAL (BEAK, 'A',   18, A_BeakAttackPL1			, &States[S_BEAKREADY]),

#define S_BEAKATK2 (S_BEAKATK1+1)
	S_NORMAL (BEAK, 'A',   12, A_BeakAttackPL2			, &States[S_BEAKREADY])
};

FWeaponInfo ABeak::WeaponInfo1 =
{
	0,
	am_noammo,
	0,
	0,
	&States[S_BEAKUP],
	&States[S_BEAKDOWN],
	&States[S_BEAKREADY],
	&States[S_BEAKATK1],
	&States[S_BEAKATK1],
	NULL,
	NULL,
	150,
	15*FRACUNIT,
	NULL,
	NULL
};

FWeaponInfo ABeak::WeaponInfo2 =
{
	0,
	am_noammo,
	0,
	0,
	&States[S_BEAKUP],
	&States[S_BEAKDOWN],
	&States[S_BEAKREADY],
	&States[S_BEAKATK2],
	&States[S_BEAKATK2],
	NULL,
	NULL,
	150,
	15*FRACUNIT,
	NULL,
	NULL
};

IMPLEMENT_ACTOR (ABeak, Heretic, -1, 0)
END_DEFAULTS

AT_GAME_SET (Beak)
{
	wpnlev1info[wp_beak] = &ABeak::WeaponInfo1;
	wpnlev2info[wp_beak] = &ABeak::WeaponInfo2;
}

// Chicken player -----------------------------------------------------------

class AChickenPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AChickenPlayer, APlayerPawn)
public:
	fixed_t GetJumpZ () { return FRACUNIT; }
};

FState AChickenPlayer::States[] =
{
#define S_CHICPLAY 0
	S_NORMAL (CHKN, 'A',   -1, NULL 					, NULL),

#define S_CHICPLAY_RUN (S_CHICPLAY+1)
	S_NORMAL (CHKN, 'A',	3, NULL 					, &States[S_CHICPLAY_RUN+1]),
	S_NORMAL (CHKN, 'B',	3, NULL 					, &States[S_CHICPLAY_RUN+2]),
	S_NORMAL (CHKN, 'A',	3, NULL 					, &States[S_CHICPLAY_RUN+3]),
	S_NORMAL (CHKN, 'B',	3, NULL 					, &States[S_CHICPLAY_RUN+0]),

#define S_CHICPLAY_ATK (S_CHICPLAY_RUN+4)
	S_NORMAL (CHKN, 'C',   12, NULL 					, &States[S_CHICPLAY]),

#define S_CHICPLAY_PAIN (S_CHICPLAY_ATK+1)
	S_NORMAL (CHKN, 'D',	4, A_Feathers				, &States[S_CHICPLAY_PAIN+1]),
	S_NORMAL (CHKN, 'C',	4, A_Pain					, &States[S_CHICPLAY]),

#define S_CHICPLAY_DIE (S_CHICPLAY_PAIN+2)
	S_NORMAL (CHKN, 'E',	6, A_Scream 				, &States[S_CHICPLAY_DIE+1]),
	S_NORMAL (CHKN, 'F',	6, A_Feathers				, &States[S_CHICPLAY_DIE+2]),
	S_NORMAL (CHKN, 'G',	6, NULL 					, &States[S_CHICPLAY_DIE+3]),
	S_NORMAL (CHKN, 'H',	6, A_NoBlocking 			, &States[S_CHICPLAY_DIE+4]),
	S_NORMAL (CHKN, 'I',	6, NULL 					, &States[S_CHICPLAY_DIE+5]),
	S_NORMAL (CHKN, 'J',	6, NULL 					, &States[S_CHICPLAY_DIE+6]),
	S_NORMAL (CHKN, 'K',	6, NULL 					, &States[S_CHICPLAY_DIE+7]),
	S_NORMAL (CHKN, 'L',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AChickenPlayer, Heretic, -1, 0)
	PROP_SpawnHealth (30)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (24)
	PROP_PainChance (255)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_NOTDMATCH)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_SLIDE|MF2_PASSMOBJ|MF2_FLOORCLIP|MF2_LOGRAV|MF2_TELESTOMP)

	PROP_SpawnState (S_CHICPLAY)
	PROP_SeeState (S_CHICPLAY_RUN)
	PROP_PainState (S_CHICPLAY_PAIN)
	PROP_MissileState (S_CHICPLAY_ATK)
	PROP_DeathState (S_CHICPLAY_DIE)

	PROP_PainSound ("chicken/pain")
	PROP_DeathSound ("chicken/death")
END_DEFAULTS

// Chicken (non-player) -----------------------------------------------------

class AChicken : public AActor
{
	DECLARE_ACTOR (AChicken, AActor)
public:
	void Destroy ();
	const char *GetObituary ();
};

FState AChicken::States[] =
{
#define S_CHICKEN_LOOK 0
	S_NORMAL (CHKN, 'A',   10, A_ChicLook				, &States[S_CHICKEN_LOOK+1]),
	S_NORMAL (CHKN, 'B',   10, A_ChicLook				, &States[S_CHICKEN_LOOK+0]),

#define S_CHICKEN_WALK (S_CHICKEN_LOOK+2)
	S_NORMAL (CHKN, 'A',	3, A_ChicChase				, &States[S_CHICKEN_WALK+1]),
	S_NORMAL (CHKN, 'B',	3, A_ChicChase				, &States[S_CHICKEN_WALK+0]),

#define S_CHICKEN_PAIN (S_CHICKEN_WALK+2)
	S_NORMAL (CHKN, 'D',	5, A_Feathers				, &States[S_CHICKEN_PAIN+1]),
	S_NORMAL (CHKN, 'C',	5, A_ChicPain				, &States[S_CHICKEN_WALK+0]),

#define S_CHICKEN_ATK (S_CHICKEN_PAIN+2)
	S_NORMAL (CHKN, 'A',	8, A_FaceTarget 			, &States[S_CHICKEN_ATK+1]),
	S_NORMAL (CHKN, 'C',   10, A_ChicAttack 			, &States[S_CHICKEN_WALK+0]),

#define S_CHICKEN_DIE (S_CHICKEN_ATK+2)
	S_NORMAL (CHKN, 'E',	6, A_Scream 				, &States[S_CHICKEN_DIE+1]),
	S_NORMAL (CHKN, 'F',	6, A_Feathers				, &States[S_CHICKEN_DIE+2]),
	S_NORMAL (CHKN, 'G',	6, NULL 					, &States[S_CHICKEN_DIE+3]),
	S_NORMAL (CHKN, 'H',	6, A_NoBlocking 			, &States[S_CHICKEN_DIE+4]),
	S_NORMAL (CHKN, 'I',	6, NULL 					, &States[S_CHICKEN_DIE+5]),
	S_NORMAL (CHKN, 'J',	6, NULL 					, &States[S_CHICKEN_DIE+6]),
	S_NORMAL (CHKN, 'K',	6, NULL 					, &States[S_CHICKEN_DIE+7]),
	S_NORMAL (CHKN, 'L',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AChicken, Heretic, -1, 0)
	PROP_SpawnHealth (10)
	PROP_RadiusFixed (9)
	PROP_HeightFixed (22)
	PROP_Mass (40)
	PROP_SpeedFixed (4)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_PASSMOBJ)
	PROP_Flags3 (MF3_DONTMORPH)

	PROP_SpawnState (S_CHICKEN_LOOK)
	PROP_SeeState (S_CHICKEN_WALK)
	PROP_PainState (S_CHICKEN_PAIN)
	PROP_MeleeState (S_CHICKEN_ATK)
	PROP_DeathState (S_CHICKEN_DIE)

	PROP_SeeSound ("chicken/pain")
	PROP_AttackSound ("chicken/attack")
	PROP_PainSound ("chicken/pain")
	PROP_DeathSound ("chicken/death")
	PROP_ActiveSound ("chicken/active")
END_DEFAULTS

void AChicken::Destroy ()
{
	if (tracer != NULL)
	{
		tracer->Destroy ();
	}
	Super::Destroy ();
}

const char *AChicken::GetObituary ()
{
	return GStrings(OB_CHICKEN);
}

// Feather ------------------------------------------------------------------

class AFeather : public AActor
{
	DECLARE_ACTOR (AFeather, AActor)
};

FState AFeather::States[] =
{
#define S_FEATHER 0
	S_NORMAL (CHKN, 'M',	3, NULL 					, &States[S_FEATHER+1]),
	S_NORMAL (CHKN, 'N',	3, NULL 					, &States[S_FEATHER+2]),
	S_NORMAL (CHKN, 'O',	3, NULL 					, &States[S_FEATHER+3]),
	S_NORMAL (CHKN, 'P',	3, NULL 					, &States[S_FEATHER+4]),
	S_NORMAL (CHKN, 'Q',	3, NULL 					, &States[S_FEATHER+5]),
	S_NORMAL (CHKN, 'P',	3, NULL 					, &States[S_FEATHER+6]),
	S_NORMAL (CHKN, 'O',	3, NULL 					, &States[S_FEATHER+7]),
	S_NORMAL (CHKN, 'N',	3, NULL 					, &States[S_FEATHER+0]),

#define S_FEATHERX (S_FEATHER+8)
	S_NORMAL (CHKN, 'N',	6, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AFeather, Heretic, -1, 0)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_LOGRAV|MF2_CANNOTPUSH|MF2_WINDTHRUST)
	PROP_Flags3 (MF3_DONTSPLASH)

	PROP_SpawnState (S_FEATHER)
	PROP_DeathState (S_FEATHERX)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_ChicAttack
//
//----------------------------------------------------------------------------

void A_ChicAttack (AActor *actor)
{
	if (P_UpdateMorphedMonster(actor, 18))
	{
		return;
	}
	if (!actor->target)
	{
		return;
	}
	if (P_CheckMeleeRange(actor))
	{
		int damage = 1 + (P_Random() & 1);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		P_TraceBleed (damage, actor->target, actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_ChicLook
//
//----------------------------------------------------------------------------

void A_ChicLook (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 10))
	{
		return;
	}
	A_Look (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_ChicChase
//
//----------------------------------------------------------------------------

void A_ChicChase (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 3))
	{
		return;
	}
	A_Chase (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_ChicPain
//
//----------------------------------------------------------------------------

void A_ChicPain (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 10))
	{
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->PainSound, 1, ATTN_NORM);
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
		count = P_Random() < 32 ? 2 : 1;
	}
	else
	{ // Death
		count = 5 + (P_Random()&3);
	}
	for (i = 0; i < count; i++)
	{
		mo = Spawn<AFeather> (actor->x, actor->y, actor->z+20*FRACUNIT);
		mo->target = actor;
		mo->momx = PS_Random() << 8;
		mo->momy = PS_Random() << 8;
		mo->momz = FRACUNIT + (P_Random() << 9);
		mo->SetState (&AFeather::States[S_FEATHER+P_Random()&7]);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_UpdateBeak
//
//---------------------------------------------------------------------------

void P_UpdateBeak (player_t *player, pspdef_t *psp)
{
	psp->sy = WEAPONTOP + (player->chickenPeck << (FRACBITS-1));
}

//---------------------------------------------------------------------------
//
// PROC A_BeakReady
//
//---------------------------------------------------------------------------

void A_BeakReady (player_t *player, pspdef_t *psp)
{
	if (player->cmd.ucmd.buttons & BT_ATTACK)
	{ // Chicken beak attack
		player->mo->SetState (player->mo->MissileState);
		if (player->powers[pw_weaponlevel2])
		{
			P_SetPsprite (player, ps_weapon, wpnlev2info[wp_beak]->atkstate);
		}
		else
		{
			P_SetPsprite (player, ps_weapon, wpnlev1info[wp_beak]->atkstate);
		}
		P_NoiseAlert (player->mo, player->mo);
	}
	else
	{
		if (player->mo->state == player->mo->MissileState)
		{ // Take out of attack state
			player->mo->SetState (player->mo->SpawnState);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC A_BeakRaise
//
//---------------------------------------------------------------------------

void A_BeakRaise (player_t *player, pspdef_t *psp)
{
	psp->sy = WEAPONTOP;
	P_SetPsprite (player, ps_weapon,
		wpnlev1info[player->readyweapon]->readystate);
}

//----------------------------------------------------------------------------
//
// PROC P_PlayPeck
//
//----------------------------------------------------------------------------

void P_PlayPeck (AActor *chicken)
{
	S_Sound (chicken, CHAN_VOICE, "chicken/peck", 1, ATTN_NORM);
	switch (P_Random () % 3)
	{
	case 0:
		S_Sound (chicken, CHAN_VOICE, "chicken/peck1", 1, ATTN_NORM);
		break;
	case 1:
		S_Sound (chicken, CHAN_VOICE, "chicken/peck2", 1, ATTN_NORM);
		break;
	case 2:
		S_Sound (chicken, CHAN_VOICE, "chicken/peck3", 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BeakAttackPL1
//
//----------------------------------------------------------------------------

void A_BeakAttackPL1 (player_t *player, pspdef_t *psp)
{
	angle_t angle;
	int damage;
	int slope;

	damage = 1 + (P_Random()&3);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	PuffType = RUNTIME_CLASS(ABeakPuff);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage);
	if (linetarget)
	{
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	psp->tics -= P_Random() & 7;
}

//----------------------------------------------------------------------------
//
// PROC A_BeakAttackPL2
//
//----------------------------------------------------------------------------

void A_BeakAttackPL2 (player_t *player, pspdef_t *psp)
{
	angle_t angle;
	int damage;
	int slope;

	damage = HITDICE(4);
	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	PuffType = RUNTIME_CLASS(ABeakPuff);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage);
	if (linetarget)
	{
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	psp->tics -= P_Random()&3;
}
