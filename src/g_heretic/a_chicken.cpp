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
#include "gstrings.h"

static FRandom pr_chickenplayerthink ("ChickenPlayerThink");
static FRandom pr_chicattack ("ChicAttack");
static FRandom pr_feathers ("Feathers");
static FRandom pr_beakatkpl1 ("BeakAtkPL1");
static FRandom pr_beakatkpl2 ("BeakAtkPL2");

void A_BeakRaise (AActor *);
void A_BeakAttackPL1 (AActor *);
void A_BeakAttackPL2 (AActor *);

void A_Feathers (AActor *);
void A_ChicAttack (AActor *);

void P_UpdateBeak (AActor *);

// Beak puff ----------------------------------------------------------------

class ABeakPuff : public AStaffPuff
{
	DECLARE_STATELESS_ACTOR (ABeakPuff, AStaffPuff)
public:
	void BeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ABeakPuff, Heretic, -1, 0)
	PROP_Mass (5)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
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
};

class ABeakPowered : public ABeak
{
	DECLARE_STATELESS_ACTOR (ABeakPowered, ABeak)
};

FState ABeak::States[] =
{
#define S_BEAKREADY 0
	S_NORMAL (BEAK, 'A',	1, A_WeaponReady			, &States[S_BEAKREADY]),

#define S_BEAKDOWN (S_BEAKREADY+1)
	S_NORMAL (BEAK, 'A',	1, A_Lower					, &States[S_BEAKDOWN]),

#define S_BEAKUP (S_BEAKDOWN+1)
	S_NORMAL (BEAK, 'A',	1, A_BeakRaise				, &States[S_BEAKUP]),

#define S_BEAKATK1 (S_BEAKUP+1)
	S_NORMAL (BEAK, 'A',   18, A_BeakAttackPL1			, &States[S_BEAKREADY]),

#define S_BEAKATK2 (S_BEAKATK1+1)
	S_NORMAL (BEAK, 'A',   12, A_BeakAttackPL2			, &States[S_BEAKREADY])
};

IMPLEMENT_ACTOR (ABeak, Heretic, -1, 0)
	PROP_Weapon_SelectionOrder (10000)
	PROP_Weapon_Flags (WIF_DONTBOB|WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_BEAKUP)
	PROP_Weapon_DownState (S_BEAKDOWN)
	PROP_Weapon_ReadyState (S_BEAKREADY)
	PROP_Weapon_AtkState (S_BEAKATK1)
	PROP_Weapon_HoldAtkState (S_BEAKATK1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_SisterType ("BeakPowered")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABeakPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_DONTBOB|WIF_BOT_MELEE|WIF_POWERED_UP)
	PROP_Weapon_AtkState (S_BEAKATK2)
	PROP_Weapon_HoldAtkState (S_BEAKATK2)
	PROP_Weapon_SisterType ("Beak")
END_DEFAULTS

// Chicken player -----------------------------------------------------------

class AChickenPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AChickenPlayer, APlayerPawn)
public:
	void MorphPlayerThink ();
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
	PROP_SpeedFixed (1)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_SLIDE|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP|MF2_TELESTOMP)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_NOSKIN)

	PROP_SpawnState (S_CHICPLAY)
	PROP_SeeState (S_CHICPLAY_RUN)
	PROP_PainState (S_CHICPLAY_PAIN)
	PROP_MissileState (S_CHICPLAY_ATK)
	PROP_MeleeState (S_CHICPLAY_ATK)
	PROP_DeathState (S_CHICPLAY_DIE)

	// [GRB]
	PROP_PlayerPawn_JumpZ (FRACUNIT)
	PROP_PlayerPawn_ViewHeight (21*FRACUNIT)
	PROP_PlayerPawn_ForwardMove1 (FRACUNIT * 2500 / 2048)
	PROP_PlayerPawn_ForwardMove2 (FRACUNIT * 2500 / 2048)
	PROP_PlayerPawn_SideMove1 (FRACUNIT * 2500 / 2048)
	PROP_PlayerPawn_SideMove2 (FRACUNIT * 2500 / 2048)
	PROP_PlayerPawn_MorphWeapon ("Beak")

	PROP_PainSound ("chicken/pain")
	PROP_DeathSound ("chicken/death")
END_DEFAULTS

AT_GAME_SET(ChickenPlayer)
{
	RUNTIME_CLASS(AChickenPlayer)->Meta.SetMetaString(APMETA_SoundClass, "chicken");
}

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

// Chicken (non-player) -----------------------------------------------------

class AChicken : public AMorphedMonster
{
	DECLARE_ACTOR (AChicken, AMorphedMonster)
};

FState AChicken::States[] =
{
#define S_CHICKEN_LOOK 0
	S_NORMAL (CHKN, 'A',   10, A_Look					, &States[S_CHICKEN_LOOK+1]),
	S_NORMAL (CHKN, 'B',   10, A_Look					, &States[S_CHICKEN_LOOK+0]),

#define S_CHICKEN_WALK (S_CHICKEN_LOOK+2)
	S_NORMAL (CHKN, 'A',	3, A_Chase					, &States[S_CHICKEN_WALK+1]),
	S_NORMAL (CHKN, 'B',	3, A_Chase					, &States[S_CHICKEN_WALK+0]),

#define S_CHICKEN_PAIN (S_CHICKEN_WALK+2)
	S_NORMAL (CHKN, 'D',	5, A_Feathers				, &States[S_CHICKEN_PAIN+1]),
	S_NORMAL (CHKN, 'C',	5, A_Pain					, &States[S_CHICKEN_WALK+0]),

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

IMPLEMENT_ACTOR (AChicken, Heretic, -1, 122)
	PROP_SpawnHealth (10)
	PROP_RadiusFixed (9)
	PROP_HeightFixed (22)
	PROP_Mass (40)
	PROP_SpeedFixed (4)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_MCROSS|MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH|MF3_ISMONSTER)

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
	PROP_Obituary("$OB_CHICKEN")
END_DEFAULTS

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

IMPLEMENT_ACTOR (AFeather, Heretic, -1, 121)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (4)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH|MF2_WINDTHRUST)
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
		mo = Spawn<AFeather> (actor->x, actor->y, actor->z+20*FRACUNIT, NO_REPLACE);
		mo->target = actor;
		mo->momx = pr_feathers.Random2() << 8;
		mo->momy = pr_feathers.Random2() << 8;
		mo->momz = FRACUNIT + (pr_feathers() << 9);
		mo->SetState (&AFeather::States[S_FEATHER+(pr_feathers()&7)]);
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
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(ABeakPuff), true);
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
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(ABeakPuff), true);
	if (linetarget)
	{
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
	P_PlayPeck (player->mo);
	player->chickenPeck = 12;
	player->psprites[ps_weapon].tics -= pr_beakatkpl2()&3;
}
