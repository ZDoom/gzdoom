#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "a_action.h"

CVAR (Bool, limitpainelemental, false, CVAR_SERVERINFO)

void A_PainAttack (AActor *);
void A_PainDie (AActor *);

void A_SkullAttack (AActor *self);

FState APainElemental::States[] =
{
#define S_PAIN_STND 0
	S_NORMAL (PAIN, 'A',   10, A_Look						, &States[S_PAIN_STND]),

#define S_PAIN_RUN (S_PAIN_STND+1)
	S_NORMAL (PAIN, 'A',	3, A_Chase						, &States[S_PAIN_RUN+1]),
	S_NORMAL (PAIN, 'A',	3, A_Chase						, &States[S_PAIN_RUN+2]),
	S_NORMAL (PAIN, 'B',	3, A_Chase						, &States[S_PAIN_RUN+3]),
	S_NORMAL (PAIN, 'B',	3, A_Chase						, &States[S_PAIN_RUN+4]),
	S_NORMAL (PAIN, 'C',	3, A_Chase						, &States[S_PAIN_RUN+5]),
	S_NORMAL (PAIN, 'C',	3, A_Chase						, &States[S_PAIN_RUN+0]),

#define S_PAIN_ATK (S_PAIN_RUN+6)
	S_NORMAL (PAIN, 'D',	5, A_FaceTarget 				, &States[S_PAIN_ATK+1]),
	S_NORMAL (PAIN, 'E',	5, A_FaceTarget 				, &States[S_PAIN_ATK+2]),
	S_BRIGHT (PAIN, 'F',	5, A_FaceTarget 				, &States[S_PAIN_ATK+3]),
	S_BRIGHT (PAIN, 'F',	0, A_PainAttack 				, &States[S_PAIN_RUN+0]),

#define S_PAIN_PAIN (S_PAIN_ATK+4)
	S_NORMAL (PAIN, 'G',	6, NULL 						, &States[S_PAIN_PAIN+1]),
	S_NORMAL (PAIN, 'G',	6, A_Pain						, &States[S_PAIN_RUN+0]),

#define S_PAIN_DIE (S_PAIN_PAIN+2)
	S_BRIGHT (PAIN, 'H',	8, NULL 						, &States[S_PAIN_DIE+1]),
	S_BRIGHT (PAIN, 'I',	8, A_Scream 					, &States[S_PAIN_DIE+2]),
	S_BRIGHT (PAIN, 'J',	8, NULL 						, &States[S_PAIN_DIE+3]),
	S_BRIGHT (PAIN, 'K',	8, NULL 						, &States[S_PAIN_DIE+4]),
	S_BRIGHT (PAIN, 'L',	8, A_PainDie					, &States[S_PAIN_DIE+5]),
	S_BRIGHT (PAIN, 'M',	8, NULL 						, NULL),

#define S_PAIN_RAISE (S_PAIN_DIE+6)
	S_NORMAL (PAIN, 'M',	8, NULL 						, &States[S_PAIN_RAISE+1]),
	S_NORMAL (PAIN, 'L',	8, NULL 						, &States[S_PAIN_RAISE+2]),
	S_NORMAL (PAIN, 'K',	8, NULL 						, &States[S_PAIN_RAISE+3]),
	S_NORMAL (PAIN, 'J',	8, NULL 						, &States[S_PAIN_RAISE+4]),
	S_NORMAL (PAIN, 'I',	8, NULL 						, &States[S_PAIN_RAISE+5]),
	S_NORMAL (PAIN, 'H',	8, NULL 						, &States[S_PAIN_RUN+0])
};

IMPLEMENT_ACTOR (APainElemental, Doom, 71, 115)
	PROP_SpawnHealth (400)
	PROP_RadiusFixed (31)
	PROP_HeightFixed (56)
	PROP_Mass (400)
	PROP_SpeedFixed (8)
	PROP_PainChance (128)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_FLOAT|MF_NOGRAVITY|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)

	PROP_SpawnState (S_PAIN_STND)
	PROP_SeeState (S_PAIN_RUN)
	PROP_PainState (S_PAIN_PAIN)
	PROP_MissileState (S_PAIN_ATK)
	PROP_DeathState (S_PAIN_DIE)
	PROP_RaiseState (S_PAIN_RAISE)

	PROP_SeeSound ("pain/sight")
	PROP_PainSound ("pain/pain")
	PROP_DeathSound ("pain/death")
	PROP_ActiveSound ("pain/active")
END_DEFAULTS

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull (AActor *self, angle_t angle)
{
	fixed_t x, y, z;
	
	AActor *other;
	angle_t an;
	int prestep;

	// [RH] make the optional
	if (*limitpainelemental)
	{
		// count total number of skulls currently on the level
		// if there are already 20 skulls on the level, don't spit another one
		int count = 20;
		TThinkerIterator<ALostSoul> iterator;

		while ( (other = iterator.Next ()) )
		{
			if (--count == 0)
				return;
		}
	}

	// okay, there's room for another one
	an = angle >> ANGLETOFINESHIFT;
	
	prestep = 4*FRACUNIT +
		3*(self->radius + GetDefault<ALostSoul>()->radius)/2;
	
	x = self->x + FixedMul (prestep, finecosine[an]);
	y = self->y + FixedMul (prestep, finesine[an]);
	z = self->z + 8*FRACUNIT;
				
	// Check whether the Lost Soul is being fired through a 1-sided	// phares
	// wall or an impassible line, or a "monsters can't cross" line.//   |
	// If it is, then we don't allow the spawn.						//   V

	if (Check_Sides (self, x, y))
		return;

	other = Spawn<ALostSoul> (x, y, z);

	// Check to see if the new Lost Soul's z value is above the
	// ceiling of its new sector, or below the floor. If so, kill it.

	if ((other->z >
         (other->subsector->sector->ceilingplane.ZatPoint (other->x, other->y) - other->height)) ||
        (other->z < other->subsector->sector->floorplane.ZatPoint (other->x, other->y)))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, 10000, MOD_UNKNOWN);		//   ^
		return;														//   |
	}																// phares

	// Check for movements.

	if (!P_CheckPosition (other, other->x, other->y))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, 10000, MOD_UNKNOWN);		
		return;
	}
				
	other->target = self->target;
	A_SkullAttack (other);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void A_PainAttack (AActor *self)
{
	if (!self->target)
		return;

	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle);
}

void A_PainDie (AActor *self)
{
	A_NoBlocking (self);
	A_PainShootSkull (self, self->angle + ANG90);
	A_PainShootSkull (self, self->angle + ANG180);
	A_PainShootSkull (self, self->angle + ANG270);
}
