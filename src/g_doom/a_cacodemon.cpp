#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "s_sound.h"

static FRandom pr_headattack ("HeadAttack");

void A_HeadAttack (AActor *);

class ACacodemon : public AActor
{
	DECLARE_ACTOR (ACacodemon, AActor)
public:
	const char *GetObituary () { return GStrings("OB_CACO"); }
	const char *GetHitObituary () { return GStrings("OB_CACOHIT"); }
};

FState ACacodemon::States[] =
{
#define S_HEAD_STND 0
	S_NORMAL (HEAD, 'A',   10, A_Look						, &States[S_HEAD_STND]),

#define S_HEAD_RUN (S_HEAD_STND+1)
	S_NORMAL (HEAD, 'A',	3, A_Chase						, &States[S_HEAD_RUN+0]),

#define S_HEAD_ATK (S_HEAD_RUN+1)
	S_NORMAL (HEAD, 'B',	5, A_FaceTarget 				, &States[S_HEAD_ATK+1]),
	S_NORMAL (HEAD, 'C',	5, A_FaceTarget 				, &States[S_HEAD_ATK+2]),
	S_BRIGHT (HEAD, 'D',	5, A_HeadAttack 				, &States[S_HEAD_RUN+0]),

#define S_HEAD_PAIN (S_HEAD_ATK+3)
	S_NORMAL (HEAD, 'E',	3, NULL 						, &States[S_HEAD_PAIN+1]),
	S_NORMAL (HEAD, 'E',	3, A_Pain						, &States[S_HEAD_PAIN+2]),
	S_NORMAL (HEAD, 'F',	6, NULL 						, &States[S_HEAD_RUN+0]),

#define S_HEAD_DIE (S_HEAD_PAIN+3)
	S_NORMAL (HEAD, 'G',	8, NULL							, &States[S_HEAD_DIE+1]),
	S_NORMAL (HEAD, 'H',	8, A_Scream 					, &States[S_HEAD_DIE+2]),
	S_NORMAL (HEAD, 'I',	8, NULL 						, &States[S_HEAD_DIE+3]),
	S_NORMAL (HEAD, 'J',	8, NULL 						, &States[S_HEAD_DIE+4]),
	S_NORMAL (HEAD, 'K',	8, A_NoBlocking					, &States[S_HEAD_DIE+5]),
	S_NORMAL (HEAD, 'L',   -1, A_SetFloorClip				, NULL),

#define S_HEAD_RAISE (S_HEAD_DIE+6)
	S_NORMAL (HEAD, 'L',	8, A_UnSetFloorClip				, &States[S_HEAD_RAISE+1]),
	S_NORMAL (HEAD, 'K',	8, NULL 						, &States[S_HEAD_RAISE+2]),
	S_NORMAL (HEAD, 'J',	8, NULL 						, &States[S_HEAD_RAISE+3]),
	S_NORMAL (HEAD, 'I',	8, NULL 						, &States[S_HEAD_RAISE+4]),
	S_NORMAL (HEAD, 'H',	8, NULL 						, &States[S_HEAD_RAISE+5]),
	S_NORMAL (HEAD, 'G',	8, NULL							, &States[S_HEAD_RUN+0])
};

IMPLEMENT_ACTOR (ACacodemon, Doom, 3005, 19)
	PROP_SpawnHealth (400)
	PROP_RadiusFixed (31)
	PROP_HeightFixed (56)
	PROP_Mass (400)
	PROP_SpeedFixed (8)
	PROP_PainChance (128)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_HEAD_STND)
	PROP_SeeState (S_HEAD_RUN)
	PROP_PainState (S_HEAD_PAIN)
	PROP_MissileState (S_HEAD_ATK)
	PROP_DeathState (S_HEAD_DIE)
	PROP_RaiseState (S_HEAD_RAISE)

	PROP_SeeSound ("caco/sight")
	PROP_PainSound ("caco/pain")
	PROP_DeathSound ("caco/death")
	PROP_ActiveSound ("caco/active")
	PROP_AttackSound ("caco/melee")
END_DEFAULTS

class AStealthCacodemon : public ACacodemon
{
	DECLARE_STATELESS_ACTOR (AStealthCacodemon, ACacodemon)
public:
	const char *GetObituary () { return GStrings("OB_STEALTHCACO"); }
	const char *GetHitObituary () { return GStrings("OB_STEALTHCACO"); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthCacodemon, Doom, 9053, 119)
	PROP_FlagsSet (MF_STEALTH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (0)
END_DEFAULTS

class ACacodemonBall : public AActor
{
	DECLARE_ACTOR (ACacodemonBall, AActor)
};

FState ACacodemonBall::States[] =
{
#define S_RBALL 0
	S_BRIGHT (BAL2, 'A',	4, NULL 						, &States[S_RBALL+1]),
	S_BRIGHT (BAL2, 'B',	4, NULL 						, &States[S_RBALL+0]),

#define S_RBALLX (S_RBALL+2)
	S_BRIGHT (BAL2, 'C',	6, NULL 						, &States[S_RBALLX+1]),
	S_BRIGHT (BAL2, 'D',	6, NULL 						, &States[S_RBALLX+2]),
	S_BRIGHT (BAL2, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACacodemonBall, Doom, -1, 126)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_RBALL)
	PROP_DeathState (S_RBALLX)

	PROP_SeeSound ("caco/attack")
	PROP_DeathSound ("caco/shotx")
END_DEFAULTS

AT_SPEED_SET (CacodemonBall, speed)
{
	SimpleSpeedSetter (ACacodemonBall, 10*FRACUNIT, 20*FRACUNIT, speed);
}

void A_HeadAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = (pr_headattack()%6+1)*10;
		S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(ACacodemonBall));
}

// Dead cacodemon ----------------------------------------------------------

class ADeadCacodemon : public ACacodemon
{
	DECLARE_STATELESS_ACTOR (ADeadCacodemon, ACacodemon)
};

IMPLEMENT_STATELESS_ACTOR (ADeadCacodemon, Doom, 22, 0)
	PROP_SpawnState (S_HEAD_DIE+5)

	// Undo all the changes to default Actor properties that ACacodemon made
	PROP_SpawnHealth (1000)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Mass (100)
	PROP_SpeedFixed (0)
	PROP_PainChance (0)
	PROP_Flags (0)
	PROP_Flags2 (0)
	PROP_Flags3 (0)
	PROP_SeeState (255)
	PROP_PainState (255)
	PROP_MissileState (255)
	PROP_DeathState (255)
	PROP_RaiseState (255)
	PROP_SeeSound ("")
	PROP_PainSound ("")
	PROP_DeathSound ("")
	PROP_ActiveSound ("")
	PROP_AttackSound ("")
END_DEFAULTS
