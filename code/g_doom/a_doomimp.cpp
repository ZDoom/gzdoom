#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"

void A_TroopAttack (AActor *);

class ADoomImp : public AActor
{
	DECLARE_ACTOR (ADoomImp, AActor)
public:
	const char *GetObituary () { return GStrings(OB_IMP); }
	const char *GetHitObituary () { return GStrings(OB_IMPHIT); }
};

FState ADoomImp::States[] =
{
#define S_TROO_STND 0
	S_NORMAL (TROO, 'A',   10, A_Look						, &States[S_TROO_STND+1]),
	S_NORMAL (TROO, 'B',   10, A_Look						, &States[S_TROO_STND]),

#define S_TROO_RUN (S_TROO_STND+2)
	S_NORMAL (TROO, 'A',	3, A_Chase						, &States[S_TROO_RUN+1]),
	S_NORMAL (TROO, 'A',	3, A_Chase						, &States[S_TROO_RUN+2]),
	S_NORMAL (TROO, 'B',	3, A_Chase						, &States[S_TROO_RUN+3]),
	S_NORMAL (TROO, 'B',	3, A_Chase						, &States[S_TROO_RUN+4]),
	S_NORMAL (TROO, 'C',	3, A_Chase						, &States[S_TROO_RUN+5]),
	S_NORMAL (TROO, 'C',	3, A_Chase						, &States[S_TROO_RUN+6]),
	S_NORMAL (TROO, 'D',	3, A_Chase						, &States[S_TROO_RUN+7]),
	S_NORMAL (TROO, 'D',	3, A_Chase						, &States[S_TROO_RUN+0]),

#define S_TROO_ATK (S_TROO_RUN+8)
	S_NORMAL (TROO, 'E',	8, A_FaceTarget 				, &States[S_TROO_ATK+1]),
	S_NORMAL (TROO, 'F',	8, A_FaceTarget 				, &States[S_TROO_ATK+2]),
	S_NORMAL (TROO, 'G',	6, A_TroopAttack				, &States[S_TROO_RUN+0]),

#define S_TROO_PAIN (S_TROO_ATK+3)
	S_NORMAL (TROO, 'H',	2, NULL 						, &States[S_TROO_PAIN+1]),
	S_NORMAL (TROO, 'H',	2, A_Pain						, &States[S_TROO_RUN+0]),

#define S_TROO_DIE (S_TROO_PAIN+2)
	S_NORMAL (TROO, 'I',	8, NULL 						, &States[S_TROO_DIE+1]),
	S_NORMAL (TROO, 'J',	8, A_Scream 					, &States[S_TROO_DIE+2]),
	S_NORMAL (TROO, 'K',	6, NULL 						, &States[S_TROO_DIE+3]),
	S_NORMAL (TROO, 'L',	6, A_NoBlocking					, &States[S_TROO_DIE+4]),
	S_NORMAL (TROO, 'M',   -1, NULL 						, NULL),

#define S_TROO_XDIE (S_TROO_DIE+5)
	S_NORMAL (TROO, 'N',	5, NULL 						, &States[S_TROO_XDIE+1]),
	S_NORMAL (TROO, 'O',	5, A_XScream					, &States[S_TROO_XDIE+2]),
	S_NORMAL (TROO, 'P',	5, NULL 						, &States[S_TROO_XDIE+3]),
	S_NORMAL (TROO, 'Q',	5, A_NoBlocking					, &States[S_TROO_XDIE+4]),
	S_NORMAL (TROO, 'R',	5, NULL 						, &States[S_TROO_XDIE+5]),
	S_NORMAL (TROO, 'S',	5, NULL 						, &States[S_TROO_XDIE+6]),
	S_NORMAL (TROO, 'T',	5, NULL 						, &States[S_TROO_XDIE+7]),
	S_NORMAL (TROO, 'U',   -1, NULL 						, NULL),

#define S_TROO_RAISE (S_TROO_XDIE+8)
	S_NORMAL (TROO, 'M',	8, NULL 						, &States[S_TROO_RAISE+1]),
	S_NORMAL (TROO, 'L',	8, NULL 						, &States[S_TROO_RAISE+2]),
	S_NORMAL (TROO, 'K',	6, NULL 						, &States[S_TROO_RAISE+3]),
	S_NORMAL (TROO, 'J',	6, NULL 						, &States[S_TROO_RAISE+4]),
	S_NORMAL (TROO, 'I',	6, NULL 						, &States[S_TROO_RUN+0])
};

IMPLEMENT_ACTOR (ADoomImp, Doom, 3001, 5)
	PROP_SpawnHealth (60)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_TROO_STND)
	PROP_SeeState (S_TROO_RUN)
	PROP_PainState (S_TROO_PAIN)
	PROP_MeleeState (S_TROO_ATK)
	PROP_MissileState (S_TROO_ATK)
	PROP_DeathState (S_TROO_DIE)
	PROP_XDeathState (S_TROO_XDIE)
	PROP_RaiseState (S_TROO_RAISE)

	PROP_SeeSound ("imp/sight1")
	PROP_PainSound ("imp/pain")
	PROP_DeathSound ("imp/death1")
	PROP_ActiveSound ("imp/active")
END_DEFAULTS

class AStealthDoomImp : public ADoomImp
{
	DECLARE_STATELESS_ACTOR (AStealthDoomImp, ADoomImp)
public:
	const char *GetObituary () { return GStrings(OB_STEALTHIMP); }
	const char *GetHitObituary () { return GStrings(OB_STEALTHIMP); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthDoomImp, Doom, 9057, 122)
	PROP_FlagsSet (MF_STEALTH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (0)
END_DEFAULTS

class ADoomImpBall : public AActor
{
	DECLARE_ACTOR (ADoomImpBall, AActor)
};

FState ADoomImpBall::States[] =
{
#define S_TBALL 0
	S_BRIGHT (BAL1, 'A',	4, NULL 						, &States[S_TBALL+1]),
	S_BRIGHT (BAL1, 'B',	4, NULL 						, &States[S_TBALL+0]),

#define S_TBALLX (S_TBALL+2)
	S_BRIGHT (BAL1, 'C',	6, NULL 						, &States[S_TBALLX+1]),
	S_BRIGHT (BAL1, 'D',	6, NULL 						, &States[S_TBALLX+2]),
	S_BRIGHT (BAL1, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ADoomImpBall, Doom, -1, 10)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_TBALL)
	PROP_DeathState (S_TBALLX)

	PROP_SeeSound ("imp/attack")
	PROP_DeathSound ("imp/shotx")
END_DEFAULTS

AT_SPEED_SET (ADoomImpBall, speed)
{
	SimpleSpeedSetter<ADoomImpBall, 10*FRACUNIT, 20*FRACUNIT> (speed);
}

//
// A_TroopAttack
//
void A_TroopAttack (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	if (P_CheckMeleeRange (self))
	{
		int damage = (P_Random (pr_troopattack)%8+1)*3;
		S_Sound (self, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(ADoomImpBall));
}

// Dead imp ----------------------------------------------------------------

class ADeadDoomImp : public ADoomImp
{
	DECLARE_STATELESS_ACTOR (ADeadDoomImp, ADoomImp)
};

IMPLEMENT_STATELESS_ACTOR (ADeadDoomImp, Doom, 20, 0)
	PROP_SKIP_SUPER
	PROP_SpawnState (S_TROO_DIE+4)
END_DEFAULTS
