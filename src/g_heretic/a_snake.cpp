#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

void A_SnakeAttack (AActor *);
void A_SnakeAttack2 (AActor *);

// Snake --------------------------------------------------------------------

class ASnake : public AActor
{
	DECLARE_ACTOR (ASnake, AActor)
public:
	void NoBlockingSet ();
	const char *GetObituary ();
};

FState ASnake::States[] =
{
#define S_SNAKE_LOOK 0
	S_NORMAL (SNKE, 'A',   10, A_Look					, &States[S_SNAKE_LOOK+1]),
	S_NORMAL (SNKE, 'B',   10, A_Look					, &States[S_SNAKE_LOOK+0]),

#define S_SNAKE_WALK (S_SNAKE_LOOK+2)
	S_NORMAL (SNKE, 'A',	4, A_Chase					, &States[S_SNAKE_WALK+1]),
	S_NORMAL (SNKE, 'B',	4, A_Chase					, &States[S_SNAKE_WALK+2]),
	S_NORMAL (SNKE, 'C',	4, A_Chase					, &States[S_SNAKE_WALK+3]),
	S_NORMAL (SNKE, 'D',	4, A_Chase					, &States[S_SNAKE_WALK+0]),

#define S_SNAKE_ATK (S_SNAKE_WALK+4)
	S_NORMAL (SNKE, 'F',	5, A_FaceTarget 			, &States[S_SNAKE_ATK+1]),
	S_NORMAL (SNKE, 'F',	5, A_FaceTarget 			, &States[S_SNAKE_ATK+2]),
	S_NORMAL (SNKE, 'F',	4, A_SnakeAttack			, &States[S_SNAKE_ATK+3]),
	S_NORMAL (SNKE, 'F',	4, A_SnakeAttack			, &States[S_SNAKE_ATK+4]),
	S_NORMAL (SNKE, 'F',	4, A_SnakeAttack			, &States[S_SNAKE_ATK+5]),
	S_NORMAL (SNKE, 'F',	5, A_FaceTarget 			, &States[S_SNAKE_ATK+6]),
	S_NORMAL (SNKE, 'F',	5, A_FaceTarget 			, &States[S_SNAKE_ATK+7]),
	S_NORMAL (SNKE, 'F',	5, A_FaceTarget 			, &States[S_SNAKE_ATK+8]),
	S_NORMAL (SNKE, 'F',	4, A_SnakeAttack2			, &States[S_SNAKE_WALK+0]),

#define S_SNAKE_PAIN (S_SNAKE_ATK+9)
	S_NORMAL (SNKE, 'E',	3, NULL 					, &States[S_SNAKE_PAIN+1]),
	S_NORMAL (SNKE, 'E',	3, A_Pain					, &States[S_SNAKE_WALK+0]),

#define S_SNAKE_DIE (S_SNAKE_PAIN+2)
	S_NORMAL (SNKE, 'G',	5, NULL 					, &States[S_SNAKE_DIE+1]),
	S_NORMAL (SNKE, 'H',	5, A_Scream 				, &States[S_SNAKE_DIE+2]),
	S_NORMAL (SNKE, 'I',	5, NULL 					, &States[S_SNAKE_DIE+3]),
	S_NORMAL (SNKE, 'J',	5, NULL 					, &States[S_SNAKE_DIE+4]),
	S_NORMAL (SNKE, 'K',	5, NULL 					, &States[S_SNAKE_DIE+5]),
	S_NORMAL (SNKE, 'L',	5, NULL 					, &States[S_SNAKE_DIE+6]),
	S_NORMAL (SNKE, 'M',	5, A_NoBlocking 			, &States[S_SNAKE_DIE+7]),
	S_NORMAL (SNKE, 'N',	5, NULL 					, &States[S_SNAKE_DIE+8]),
	S_NORMAL (SNKE, 'O',	5, NULL 					, &States[S_SNAKE_DIE+9]),
	S_NORMAL (SNKE, 'P',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASnake, Heretic, 92, 0)
	PROP_SpawnHealth (280)
	PROP_RadiusFixed (22)
	PROP_HeightFixed (70)
	PROP_SpeedFixed (10)
	PROP_PainChance (48)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ)

	PROP_SpawnState (S_SNAKE_LOOK)
	PROP_SeeState (S_SNAKE_WALK)
	PROP_PainState (S_SNAKE_PAIN)
	PROP_MissileState (S_SNAKE_ATK)
	PROP_DeathState (S_SNAKE_DIE)

	PROP_SeeSound ("snake/sight")
	PROP_PainSound ("snake/pain")
	PROP_DeathSound ("snake/death")
	PROP_ActiveSound ("snake/active")
END_DEFAULTS

const char *ASnake::GetObituary ()
{
	return GStrings(OB_SNAKE);
}

// Snake projectile A -------------------------------------------------------

class ASnakeProjA : public AActor
{
	DECLARE_ACTOR (ASnakeProjA, AActor)
};

FState ASnakeProjA::States[] =
{
#define S_SNAKEPRO_A 0
	S_BRIGHT (SNFX, 'A',	5, NULL 					, &States[S_SNAKEPRO_A+1]),
	S_BRIGHT (SNFX, 'B',	5, NULL 					, &States[S_SNAKEPRO_A+2]),
	S_BRIGHT (SNFX, 'C',	5, NULL 					, &States[S_SNAKEPRO_A+3]),
	S_BRIGHT (SNFX, 'D',	5, NULL 					, &States[S_SNAKEPRO_A+0]),

#define S_SNAKEPRO_AX (S_SNAKEPRO_A+4)
	S_BRIGHT (SNFX, 'E',	5, NULL 					, &States[S_SNAKEPRO_AX+1]),
	S_BRIGHT (SNFX, 'F',	5, NULL 					, &States[S_SNAKEPRO_AX+2]),
	S_BRIGHT (SNFX, 'G',	4, NULL 					, &States[S_SNAKEPRO_AX+3]),
	S_BRIGHT (SNFX, 'H',	3, NULL 					, &States[S_SNAKEPRO_AX+4]),
	S_BRIGHT (SNFX, 'I',	3, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASnakeProjA, Heretic, -1, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (14)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT)

	PROP_SpawnState (S_SNAKEPRO_A)
	PROP_DeathState (S_SNAKEPRO_AX)
END_DEFAULTS

AT_SPEED_SET (SnakeProjA, speed)
{
	SimpleSpeedSetter (ASnakeProjA, 14*FRACUNIT, 20*FRACUNIT, speed);
}

void ASnake::NoBlockingSet ()
{
	P_DropItem (this, "PhoenixRodAmmo", 5, 84);
}

// Snake projectile B -------------------------------------------------------

class ASnakeProjB : public ASnakeProjA
{
	DECLARE_ACTOR (ASnakeProjB, ASnakeProjA)
};

FState ASnakeProjB::States[] =
{
#define S_SNAKEPRO_B 0
	S_BRIGHT (SNFX, 'J',	6, NULL 					, &States[S_SNAKEPRO_B+1]),
	S_BRIGHT (SNFX, 'K',	6, NULL 					, &States[S_SNAKEPRO_B+0]),

#define S_SNAKEPRO_BX (S_SNAKEPRO_B+2)
	S_BRIGHT (SNFX, 'L',	5, NULL 					, &States[S_SNAKEPRO_BX+1]),
	S_BRIGHT (SNFX, 'M',	5, NULL 					, &States[S_SNAKEPRO_BX+2]),
	S_BRIGHT (SNFX, 'N',	4, NULL 					, &States[S_SNAKEPRO_BX+3]),
	S_BRIGHT (SNFX, 'O',	3, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASnakeProjB, Heretic, -1, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_SNAKEPRO_B)
	PROP_DeathState (S_SNAKEPRO_BX)
END_DEFAULTS

AT_SPEED_SET (SnakeProjB, speed)
{
	SimpleSpeedSetter (ASnakeProjB, 14*FRACUNIT, 20*FRACUNIT, speed);
}

//----------------------------------------------------------------------------
//
// PROC A_SnakeAttack
//
//----------------------------------------------------------------------------

void A_SnakeAttack (AActor *actor)
{
	if (!actor->target)
	{
		actor->SetState (actor->SeeState);
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (actor);
	P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ASnakeProjA));
}

//----------------------------------------------------------------------------
//
// PROC A_SnakeAttack2
//
//----------------------------------------------------------------------------

void A_SnakeAttack2 (AActor *actor)
{
	if (!actor->target)
	{
		actor->SetState (actor->SeeState);
		return;
	}
	S_SoundID (actor, CHAN_BODY, actor->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (actor);
	P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ASnakeProjB));
}
