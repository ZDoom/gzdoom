#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"

void A_FatRaise (AActor *);
void A_FatAttack1 (AActor *);
void A_FatAttack2 (AActor *);
void A_FatAttack3 (AActor *);

class AFatso : public AActor
{
	DECLARE_ACTOR (AFatso, AActor)
public:
	const char *GetObituary () { return GStrings(OB_FATSO); }
};

FState AFatso::States[] =
{
#define S_FATT_STND 0
	S_NORMAL (FATT, 'A',   15, A_Look						, &States[S_FATT_STND+1]),
	S_NORMAL (FATT, 'B',   15, A_Look						, &States[S_FATT_STND]),

#define S_FATT_RUN (S_FATT_STND+2)
	S_NORMAL (FATT, 'A',	4, A_Chase						, &States[S_FATT_RUN+1]),
	S_NORMAL (FATT, 'A',	4, A_Chase						, &States[S_FATT_RUN+2]),
	S_NORMAL (FATT, 'B',	4, A_Chase						, &States[S_FATT_RUN+3]),
	S_NORMAL (FATT, 'B',	4, A_Chase						, &States[S_FATT_RUN+4]),
	S_NORMAL (FATT, 'C',	4, A_Chase						, &States[S_FATT_RUN+5]),
	S_NORMAL (FATT, 'C',	4, A_Chase						, &States[S_FATT_RUN+6]),
	S_NORMAL (FATT, 'D',	4, A_Chase						, &States[S_FATT_RUN+7]),
	S_NORMAL (FATT, 'D',	4, A_Chase						, &States[S_FATT_RUN+8]),
	S_NORMAL (FATT, 'E',	4, A_Chase						, &States[S_FATT_RUN+9]),
	S_NORMAL (FATT, 'E',	4, A_Chase						, &States[S_FATT_RUN+10]),
	S_NORMAL (FATT, 'F',	4, A_Chase						, &States[S_FATT_RUN+11]),
	S_NORMAL (FATT, 'F',	4, A_Chase						, &States[S_FATT_RUN+0]),

#define S_FATT_ATK (S_FATT_RUN+12)
	S_NORMAL (FATT, 'G',   20, A_FatRaise					, &States[S_FATT_ATK+1]),
	S_BRIGHT (FATT, 'H',   10, A_FatAttack1 				, &States[S_FATT_ATK+2]),
	S_NORMAL (FATT, 'I',	5, A_FaceTarget 				, &States[S_FATT_ATK+3]),
	S_NORMAL (FATT, 'G',	5, A_FaceTarget 				, &States[S_FATT_ATK+4]),
	S_BRIGHT (FATT, 'H',   10, A_FatAttack2 				, &States[S_FATT_ATK+5]),
	S_NORMAL (FATT, 'I',	5, A_FaceTarget 				, &States[S_FATT_ATK+6]),
	S_NORMAL (FATT, 'G',	5, A_FaceTarget 				, &States[S_FATT_ATK+7]),
	S_BRIGHT (FATT, 'H',   10, A_FatAttack3 				, &States[S_FATT_ATK+8]),
	S_NORMAL (FATT, 'I',	5, A_FaceTarget 				, &States[S_FATT_ATK+9]),
	S_NORMAL (FATT, 'G',	5, A_FaceTarget 				, &States[S_FATT_RUN+0]),

#define S_FATT_PAIN (S_FATT_ATK+10)
	S_NORMAL (FATT, 'J',	3, NULL 						, &States[S_FATT_PAIN+1]),
	S_NORMAL (FATT, 'J',	3, A_Pain						, &States[S_FATT_RUN+0]),

#define S_FATT_DIE (S_FATT_PAIN+2)
	S_NORMAL (FATT, 'K',	6, NULL 						, &States[S_FATT_DIE+1]),
	S_NORMAL (FATT, 'L',	6, A_Scream 					, &States[S_FATT_DIE+2]),
	S_NORMAL (FATT, 'M',	6, A_NoBlocking					, &States[S_FATT_DIE+3]),
	S_NORMAL (FATT, 'N',	6, NULL 						, &States[S_FATT_DIE+4]),
	S_NORMAL (FATT, 'O',	6, NULL 						, &States[S_FATT_DIE+5]),
	S_NORMAL (FATT, 'P',	6, NULL 						, &States[S_FATT_DIE+6]),
	S_NORMAL (FATT, 'Q',	6, NULL 						, &States[S_FATT_DIE+7]),
	S_NORMAL (FATT, 'R',	6, NULL 						, &States[S_FATT_DIE+8]),
	S_NORMAL (FATT, 'S',	6, NULL 						, &States[S_FATT_DIE+9]),
	S_NORMAL (FATT, 'T',   -1, A_BossDeath					, NULL),

#define S_FATT_RAISE (S_FATT_DIE+10)
	S_NORMAL (FATT, 'R',	5, NULL 						, &States[S_FATT_RAISE+1]),
	S_NORMAL (FATT, 'Q',	5, NULL 						, &States[S_FATT_RAISE+2]),
	S_NORMAL (FATT, 'P',	5, NULL 						, &States[S_FATT_RAISE+3]),
	S_NORMAL (FATT, 'O',	5, NULL 						, &States[S_FATT_RAISE+4]),
	S_NORMAL (FATT, 'N',	5, NULL 						, &States[S_FATT_RAISE+5]),
	S_NORMAL (FATT, 'M',	5, NULL 						, &States[S_FATT_RAISE+6]),
	S_NORMAL (FATT, 'L',	5, NULL 						, &States[S_FATT_RAISE+7]),
	S_NORMAL (FATT, 'K',	5, NULL 						, &States[S_FATT_RUN+0])
};

IMPLEMENT_ACTOR (AFatso, Doom, 67, 112)
	PROP_SpawnHealth (600)
	PROP_RadiusFixed (48)
	PROP_HeightFixed (64)
	PROP_Mass (1000)
	PROP_SpeedFixed (8)
	PROP_PainChance (80)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_SpawnState (S_FATT_STND)
	PROP_SeeState (S_FATT_RUN)
	PROP_PainState (S_FATT_PAIN)
	PROP_MissileState (S_FATT_ATK)
	PROP_DeathState (S_FATT_DIE)
	PROP_RaiseState (S_FATT_RAISE)

	PROP_SeeSound ("fatso/sight")
	PROP_PainSound ("fatso/pain")
	PROP_DeathSound ("fatso/death")
	PROP_ActiveSound ("fatso/active")
END_DEFAULTS

class AStealthFatso : public AFatso
{
	DECLARE_STATELESS_ACTOR (AStealthFatso, AFatso)
public:
	const char *GetObituary () { return GStrings(OB_STEALTHFATSO); }
};

IMPLEMENT_STATELESS_ACTOR (AStealthFatso, Doom, 9058, 123)
	PROP_FlagsSet (MF_STEALTH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (0)
END_DEFAULTS

class AFatShot : public AActor
{
	DECLARE_ACTOR (AFatShot, AActor)
};

FState AFatShot::States[] =
{
#define S_FATSHOT 0
	S_BRIGHT (MANF, 'A',	4, NULL 						, &States[S_FATSHOT+1]),
	S_BRIGHT (MANF, 'B',	4, NULL 						, &States[S_FATSHOT+0]),

#define S_FATSHOTX (S_FATSHOT+2)
	S_BRIGHT (MISL, 'B',	8, NULL 						, &States[S_FATSHOTX+1]),
	S_BRIGHT (MISL, 'C',	6, NULL 						, &States[S_FATSHOTX+2]),
	S_BRIGHT (MISL, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AFatShot, Doom, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (32)
	PROP_SpeedFixed (20)
	PROP_Damage (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_FATSHOT)
	PROP_DeathState (S_FATSHOTX)

	PROP_SeeSound ("fatso/attack")
	PROP_DeathSound ("fatso/shotx")
END_DEFAULTS

//
// Mancubus attack,
// firing three missiles in three different directions?
// Doesn't look like it.
//
#define FATSPREAD (ANG90/8)

void A_FatRaise (AActor *self)
{
	A_FaceTarget (self);
	S_Sound (self, CHAN_WEAPON, "fatso/raiseguns", 1, ATTN_NORM);
}

void A_FatAttack1 (AActor *self)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	A_FaceTarget (self);
	// Change direction  to ...
	self->angle += FATSPREAD;
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));

	missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));
	if (missile != NULL)
	{
		missile->angle += FATSPREAD;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->momx = FixedMul (missile->Speed, finecosine[an]);
		missile->momy = FixedMul (missile->Speed, finesine[an]);
	}
}

void A_FatAttack2 (AActor *self)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	A_FaceTarget (self);
	// Now here choose opposite deviation.
	self->angle -= FATSPREAD;
	P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));

	missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));
	if (missile != NULL)
	{
		missile->angle -= FATSPREAD*2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->momx = FixedMul (missile->Speed, finecosine[an]);
		missile->momy = FixedMul (missile->Speed, finesine[an]);
	}
}

void A_FatAttack3 (AActor *self)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	A_FaceTarget (self);
	
	missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));
	if (missile != NULL)
	{
		missile->angle -= FATSPREAD/2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->momx = FixedMul (missile->Speed, finecosine[an]);
		missile->momy = FixedMul (missile->Speed, finesine[an]);
	}

	missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(AFatShot));
	if (missile != NULL)
	{
		missile->angle += FATSPREAD/2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->momx = FixedMul (missile->Speed, finecosine[an]);
		missile->momy = FixedMul (missile->Speed, finesine[an]);
	}
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

void A_Mushroom (AActor *actor)
{
	int i, j, n = actor->damage;

	A_Explode (actor);	// First make normal explosion

	// Now launch mushroom cloud
	for (i = -n; i <= n; i += 8)
	{
		for (j = -n; j <= n; j += 8)
		{
			AActor target = *actor, *mo;
			target.x += i << FRACBITS; // Aim in many directions from source
			target.y += j << FRACBITS;
			target.z += P_AproxDistance(i,j) << (FRACBITS+2); // Aim up fairly high
			mo = P_SpawnMissile (actor, &target, RUNTIME_CLASS(AFatShot)); // Launch fireball
			if (mo != NULL)
			{
				mo->momx >>= 1;
				mo->momy >>= 1;				  // Slow it down a bit
				mo->momz >>= 1;
				mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
			}
		}
	}
}
