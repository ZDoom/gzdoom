#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_action.h"

static FRandom pr_bruisattack ("BruisAttack");

void A_BruisAttack (AActor *);

class ABaronOfHell : public AActor
{
	DECLARE_ACTOR (ABaronOfHell, AActor)
public:
	const char *GetObituary () { return GStrings(OB_BARON); }
	const char *GetHitObituary () { return GStrings(OB_BARONHIT); }
};

FState ABaronOfHell::States[] =
{
#define S_BOSS_STND 0
	S_NORMAL (BOSS, 'A',   10, A_Look						, &States[S_BOSS_STND+1]),
	S_NORMAL (BOSS, 'B',   10, A_Look						, &States[S_BOSS_STND]),

#define S_BOSS_RUN (S_BOSS_STND+2)
	S_NORMAL (BOSS, 'A',	3, A_Chase						, &States[S_BOSS_RUN+1]),
	S_NORMAL (BOSS, 'A',	3, A_Chase						, &States[S_BOSS_RUN+2]),
	S_NORMAL (BOSS, 'B',	3, A_Chase						, &States[S_BOSS_RUN+3]),
	S_NORMAL (BOSS, 'B',	3, A_Chase						, &States[S_BOSS_RUN+4]),
	S_NORMAL (BOSS, 'C',	3, A_Chase						, &States[S_BOSS_RUN+5]),
	S_NORMAL (BOSS, 'C',	3, A_Chase						, &States[S_BOSS_RUN+6]),
	S_NORMAL (BOSS, 'D',	3, A_Chase						, &States[S_BOSS_RUN+7]),
	S_NORMAL (BOSS, 'D',	3, A_Chase						, &States[S_BOSS_RUN+0]),

#define S_BOSS_ATK (S_BOSS_RUN+8)
	S_NORMAL (BOSS, 'E',	8, A_FaceTarget 				, &States[S_BOSS_ATK+1]),
	S_NORMAL (BOSS, 'F',	8, A_FaceTarget 				, &States[S_BOSS_ATK+2]),
	S_NORMAL (BOSS, 'G',	8, A_BruisAttack				, &States[S_BOSS_RUN+0]),

#define S_BOSS_PAIN (S_BOSS_ATK+3)
	S_NORMAL (BOSS, 'H',	2, NULL 						, &States[S_BOSS_PAIN+1]),
	S_NORMAL (BOSS, 'H',	2, A_Pain						, &States[S_BOSS_RUN+0]),

#define S_BOSS_DIE (S_BOSS_PAIN+2)
	S_NORMAL (BOSS, 'I',	8, NULL 						, &States[S_BOSS_DIE+1]),
	S_NORMAL (BOSS, 'J',	8, A_Scream 					, &States[S_BOSS_DIE+2]),
	S_NORMAL (BOSS, 'K',	8, NULL 						, &States[S_BOSS_DIE+3]),
	S_NORMAL (BOSS, 'L',	8, A_NoBlocking					, &States[S_BOSS_DIE+4]),
	S_NORMAL (BOSS, 'M',	8, NULL 						, &States[S_BOSS_DIE+5]),
	S_NORMAL (BOSS, 'N',	8, NULL 						, &States[S_BOSS_DIE+6]),
	S_NORMAL (BOSS, 'O',   -1, A_BossDeath					, NULL),

#define S_BOSS_RAISE (S_BOSS_DIE+7)
	S_NORMAL (BOSS, 'O',	8, NULL 						, &States[S_BOSS_RAISE+1]),
	S_NORMAL (BOSS, 'N',	8, NULL 						, &States[S_BOSS_RAISE+2]),
	S_NORMAL (BOSS, 'M',	8, NULL 						, &States[S_BOSS_RAISE+3]),
	S_NORMAL (BOSS, 'L',	8, NULL 						, &States[S_BOSS_RAISE+4]),
	S_NORMAL (BOSS, 'K',	8, NULL 						, &States[S_BOSS_RAISE+5]),
	S_NORMAL (BOSS, 'J',	8, NULL 						, &States[S_BOSS_RAISE+6]),
	S_NORMAL (BOSS, 'I',	8, NULL 						, &States[S_BOSS_RUN+0])
};

IMPLEMENT_ACTOR (ABaronOfHell, Doom, 3003, 3)
	PROP_SpawnHealth (1000)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (64)
	PROP_Mass (1000)
	PROP_SpeedFixed (8)
	PROP_PainChance (50)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_SpawnState (S_BOSS_STND)
	PROP_SeeState (S_BOSS_RUN)
	PROP_PainState (S_BOSS_PAIN)
	PROP_MeleeState (S_BOSS_ATK)
	PROP_MissileState (S_BOSS_ATK)
	PROP_DeathState (S_BOSS_DIE)
	PROP_RaiseState (S_BOSS_RAISE)

	PROP_SeeSound ("baron/sight")
	PROP_PainSound ("baron/pain")
	PROP_DeathSound ("baron/death")
	PROP_ActiveSound ("baron/active")
END_DEFAULTS

class AStealthBaron : public ABaronOfHell
{
	DECLARE_STATELESS_ACTOR (AStealthBaron, ABaronOfHell)
public:
	const char *GetObituary () { return GStrings(OB_STEALTHBARON); }
	const char *GetHitObituary () { return GStrings(OB_STEALTHBARON); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthBaron, Doom, 9052, 100)
	PROP_FlagsSet (MF_STEALTH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (0)
END_DEFAULTS

class ABaronBall : public AActor
{
	DECLARE_ACTOR (ABaronBall, AActor)
};

FState ABaronBall::States[] =
{
#define S_BRBALL 0
	S_BRIGHT (BAL7, 'A',	4, NULL 						, &States[S_BRBALL+1]),
	S_BRIGHT (BAL7, 'B',	4, NULL 						, &States[S_BRBALL+0]),

#define S_BRBALLX (S_BRBALL+2)
	S_BRIGHT (BAL7, 'C',	6, NULL 						, &States[S_BRBALLX+1]),
	S_BRIGHT (BAL7, 'D',	6, NULL 						, &States[S_BRBALLX+2]),
	S_BRIGHT (BAL7, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABaronBall, Doom, -1, 154)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (16)
	PROP_SpeedFixed (15)
	PROP_Damage (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_BRBALL)
	PROP_DeathState (S_BRBALLX)

	PROP_SeeSound ("baron/attack")
	PROP_DeathSound ("baron/shotx")
END_DEFAULTS

AT_SPEED_SET (BaronBall, speed)
{
	SimpleSpeedSetter (ABaronBall, 15*FRACUNIT, 20*FRACUNIT, speed);
}

class AHellKnight : public ABaronOfHell
{
	DECLARE_ACTOR (AHellKnight, ABaronOfHell)
public:
	const char *GetObituary () { return GStrings(OB_KNIGHT); }
	const char *GetHitObituary () { return GStrings(OB_KNIGHTHIT); }
};

FState AHellKnight::States[] =
{
#define S_BOS2_STND 0
	S_NORMAL (BOS2, 'A',   10, A_Look						, &States[S_BOS2_STND+1]),
	S_NORMAL (BOS2, 'B',   10, A_Look						, &States[S_BOS2_STND]),

#define S_BOS2_RUN (S_BOS2_STND+2)
	S_NORMAL (BOS2, 'A',	3, A_Chase						, &States[S_BOS2_RUN+1]),
	S_NORMAL (BOS2, 'A',	3, A_Chase						, &States[S_BOS2_RUN+2]),
	S_NORMAL (BOS2, 'B',	3, A_Chase						, &States[S_BOS2_RUN+3]),
	S_NORMAL (BOS2, 'B',	3, A_Chase						, &States[S_BOS2_RUN+4]),
	S_NORMAL (BOS2, 'C',	3, A_Chase						, &States[S_BOS2_RUN+5]),
	S_NORMAL (BOS2, 'C',	3, A_Chase						, &States[S_BOS2_RUN+6]),
	S_NORMAL (BOS2, 'D',	3, A_Chase						, &States[S_BOS2_RUN+7]),
	S_NORMAL (BOS2, 'D',	3, A_Chase						, &States[S_BOS2_RUN+0]),

#define S_BOS2_ATK (S_BOS2_RUN+8)
	S_NORMAL (BOS2, 'E',	8, A_FaceTarget 				, &States[S_BOS2_ATK+1]),
	S_NORMAL (BOS2, 'F',	8, A_FaceTarget 				, &States[S_BOS2_ATK+2]),
	S_NORMAL (BOS2, 'G',	8, A_BruisAttack				, &States[S_BOS2_RUN+0]),

#define S_BOS2_PAIN (S_BOS2_ATK+3)
	S_NORMAL (BOS2, 'H',	2, NULL 						, &States[S_BOS2_PAIN+1]),
	S_NORMAL (BOS2, 'H',	2, A_Pain						, &States[S_BOS2_RUN+0]),

#define S_BOS2_DIE (S_BOS2_PAIN+2)
	S_NORMAL (BOS2, 'I',	8, NULL 						, &States[S_BOS2_DIE+1]),
	S_NORMAL (BOS2, 'J',	8, A_Scream 					, &States[S_BOS2_DIE+2]),
	S_NORMAL (BOS2, 'K',	8, NULL 						, &States[S_BOS2_DIE+3]),
	S_NORMAL (BOS2, 'L',	8, A_NoBlocking						, &States[S_BOS2_DIE+4]),
	S_NORMAL (BOS2, 'M',	8, NULL 						, &States[S_BOS2_DIE+5]),
	S_NORMAL (BOS2, 'N',	8, NULL 						, &States[S_BOS2_DIE+6]),
	S_NORMAL (BOS2, 'O',   -1, NULL 						, NULL),

#define S_BOS2_RAISE (S_BOS2_DIE+7)
	S_NORMAL (BOS2, 'O',	8, NULL 						, &States[S_BOS2_RAISE+1]),
	S_NORMAL (BOS2, 'N',	8, NULL 						, &States[S_BOS2_RAISE+2]),
	S_NORMAL (BOS2, 'M',	8, NULL 						, &States[S_BOS2_RAISE+3]),
	S_NORMAL (BOS2, 'L',	8, NULL 						, &States[S_BOS2_RAISE+4]),
	S_NORMAL (BOS2, 'K',	8, NULL 						, &States[S_BOS2_RAISE+5]),
	S_NORMAL (BOS2, 'J',	8, NULL 						, &States[S_BOS2_RAISE+6]),
	S_NORMAL (BOS2, 'I',	8, NULL 						, &States[S_BOS2_RUN+0])
};

IMPLEMENT_ACTOR (AHellKnight, Doom, 69, 113)
	PROP_SpawnHealth (500)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (64)
	PROP_Mass (1000)
	PROP_SpeedFixed (8)
	PROP_PainChance (50)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_SpawnState (S_BOS2_STND)
	PROP_SeeState (S_BOS2_RUN)
	PROP_PainState (S_BOS2_PAIN)
	PROP_MeleeState (S_BOS2_ATK)
	PROP_MissileState (S_BOS2_ATK)
	PROP_DeathState (S_BOS2_DIE)
	PROP_RaiseState (S_BOS2_RAISE)

	PROP_SeeSound ("knight/sight")
	PROP_PainSound ("knight/pain")
	PROP_DeathSound ("knight/death")
	PROP_ActiveSound ("knight/active")
END_DEFAULTS

class AStealthHellKnight : public AHellKnight
{
	DECLARE_STATELESS_ACTOR (AStealthHellKnight, AHellKnight)
public:
	const char *GetObituary () { return GStrings(OB_STEALTHKNIGHT); }
	const char *GetHitObituary () { return GStrings(OB_STEALTHKNIGHT); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthHellKnight, Doom, 9056, 101)
	PROP_FlagsSet (MF_STEALTH)
	PROP_Alpha (0)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

void A_BruisAttack (AActor *self)
{
	if (!self->target)
		return;
				
	if (P_CheckMeleeRange (self))
	{
		int damage = (pr_bruisattack()%8+1)*10;
		S_Sound (self, CHAN_WEAPON, "baron/melee", 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(ABaronBall));
}
