#include "templates.h"
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "a_doomglobal.h"

static FRandom pr_tracer ("Tracer");
static FRandom pr_skelfist ("SkelFist");

void A_SkelMissile (AActor *);
void A_Tracer (AActor *);
void A_SkelWhoosh (AActor *);
void A_SkelFist (AActor *);

class ARevenant : public AActor
{
	DECLARE_ACTOR (ARevenant, AActor)
public:
	const char *GetObituary () { return GStrings("OB_UNDEAD"); }
	const char *GetHitObituary () { return GStrings("OB_UNDEADHIT"); }
};

FState ARevenant::States[] =
{
#define S_SKEL_STND 0
	S_NORMAL (SKEL, 'A',   10, A_Look						, &States[S_SKEL_STND+1]),
	S_NORMAL (SKEL, 'B',   10, A_Look						, &States[S_SKEL_STND]),

#define S_SKEL_RUN (S_SKEL_STND+2)
	S_NORMAL (SKEL, 'A',	2, A_Chase						, &States[S_SKEL_RUN+1]),
	S_NORMAL (SKEL, 'A',	2, A_Chase						, &States[S_SKEL_RUN+2]),
	S_NORMAL (SKEL, 'B',	2, A_Chase						, &States[S_SKEL_RUN+3]),
	S_NORMAL (SKEL, 'B',	2, A_Chase						, &States[S_SKEL_RUN+4]),
	S_NORMAL (SKEL, 'C',	2, A_Chase						, &States[S_SKEL_RUN+5]),
	S_NORMAL (SKEL, 'C',	2, A_Chase						, &States[S_SKEL_RUN+6]),
	S_NORMAL (SKEL, 'D',	2, A_Chase						, &States[S_SKEL_RUN+7]),
	S_NORMAL (SKEL, 'D',	2, A_Chase						, &States[S_SKEL_RUN+8]),
	S_NORMAL (SKEL, 'E',	2, A_Chase						, &States[S_SKEL_RUN+9]),
	S_NORMAL (SKEL, 'E',	2, A_Chase						, &States[S_SKEL_RUN+10]),
	S_NORMAL (SKEL, 'F',	2, A_Chase						, &States[S_SKEL_RUN+11]),
	S_NORMAL (SKEL, 'F',	2, A_Chase						, &States[S_SKEL_RUN+0]),

#define S_SKEL_FIST (S_SKEL_RUN+12)
	S_NORMAL (SKEL, 'G',	0, A_FaceTarget 				, &States[S_SKEL_FIST+1]),
	S_NORMAL (SKEL, 'G',	6, A_SkelWhoosh 				, &States[S_SKEL_FIST+2]),
	S_NORMAL (SKEL, 'H',	6, A_FaceTarget 				, &States[S_SKEL_FIST+3]),
	S_NORMAL (SKEL, 'I',	6, A_SkelFist					, &States[S_SKEL_RUN+0]),

#define S_SKEL_MISS (S_SKEL_FIST+4)
	S_BRIGHT (SKEL, 'J',	0, A_FaceTarget 				, &States[S_SKEL_MISS+1]),
	S_BRIGHT (SKEL, 'J',   10, A_FaceTarget 				, &States[S_SKEL_MISS+2]),
	S_NORMAL (SKEL, 'K',   10, A_SkelMissile				, &States[S_SKEL_MISS+3]),
	S_NORMAL (SKEL, 'K',   10, A_FaceTarget 				, &States[S_SKEL_RUN+0]),

#define S_SKEL_PAIN (S_SKEL_MISS+4)
	S_NORMAL (SKEL, 'L',	5, NULL 						, &States[S_SKEL_PAIN+1]),
	S_NORMAL (SKEL, 'L',	5, A_Pain						, &States[S_SKEL_RUN+0]),

#define S_SKEL_DIE (S_SKEL_PAIN+2)
	S_NORMAL (SKEL, 'L',	7, NULL 						, &States[S_SKEL_DIE+1]),
	S_NORMAL (SKEL, 'M',	7, NULL 						, &States[S_SKEL_DIE+2]),
	S_NORMAL (SKEL, 'N',	7, A_Scream 					, &States[S_SKEL_DIE+3]),
	S_NORMAL (SKEL, 'O',	7, A_NoBlocking					, &States[S_SKEL_DIE+4]),
	S_NORMAL (SKEL, 'P',	7, NULL 						, &States[S_SKEL_DIE+5]),
	S_NORMAL (SKEL, 'Q',   -1, NULL 						, NULL),

#define S_SKEL_RAISE (S_SKEL_DIE+6)
	S_NORMAL (SKEL, 'Q',	5, NULL 						, &States[S_SKEL_RAISE+1]),
	S_NORMAL (SKEL, 'P',	5, NULL 						, &States[S_SKEL_RAISE+2]),
	S_NORMAL (SKEL, 'O',	5, NULL 						, &States[S_SKEL_RAISE+3]),
	S_NORMAL (SKEL, 'N',	5, NULL 						, &States[S_SKEL_RAISE+4]),
	S_NORMAL (SKEL, 'M',	5, NULL 						, &States[S_SKEL_RAISE+5]),
	S_NORMAL (SKEL, 'L',	5, NULL 						, &States[S_SKEL_RUN+0])
};

IMPLEMENT_ACTOR (ARevenant, Doom, 66, 20)
	PROP_SpawnHealth (300)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (500)
	PROP_SpeedFixed (10)
	PROP_PainChance (100)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Flags4 (MF4_LONGMELEERANGE|MF4_MISSILEMORE)

	PROP_SpawnState (S_SKEL_STND)
	PROP_SeeState (S_SKEL_RUN)
	PROP_PainState (S_SKEL_PAIN)
	PROP_MeleeState (S_SKEL_FIST)
	PROP_MissileState (S_SKEL_MISS)
	PROP_DeathState (S_SKEL_DIE)
	PROP_RaiseState (S_SKEL_RAISE)

	PROP_SeeSound ("skeleton/sight")
	PROP_PainSound ("skeleton/pain")
	PROP_DeathSound ("skeleton/death")
	PROP_ActiveSound ("skeleton/active")
END_DEFAULTS


class ARevenantTracer : public AActor
{
	DECLARE_ACTOR (ARevenantTracer, AActor)
};

FState ARevenantTracer::States[] =
{
#define S_TRACER 0
	S_BRIGHT (FATB, 'A',	2, A_Tracer 					, &States[S_TRACER+1]),
	S_BRIGHT (FATB, 'B',	2, A_Tracer 					, &States[S_TRACER]),

#define S_TRACEEXP (S_TRACER+2)
	S_BRIGHT (FBXP, 'A',	8, NULL 						, &States[S_TRACEEXP+1]),
	S_BRIGHT (FBXP, 'B',	6, NULL 						, &States[S_TRACEEXP+2]),
	S_BRIGHT (FBXP, 'C',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ARevenantTracer, Doom, -1, 53)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT|MF2_SEEKERMISSILE)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_TRACER)
	PROP_DeathState (S_TRACEEXP)

	PROP_SeeSound ("skeleton/attack")
	PROP_DeathSound ("skeleton/tracex")
END_DEFAULTS

class ARevenantTracerSmoke : public AActor
{
	DECLARE_ACTOR (ARevenantTracerSmoke, AActor)
};

FState ARevenantTracerSmoke::States[] =
{
	S_NORMAL (PUFF, 'B',	4, NULL 						, &States[1]),
	S_NORMAL (PUFF, 'C',	4, NULL 						, &States[2]),
	S_NORMAL (PUFF, 'B',	4, NULL 						, &States[3]),
	S_NORMAL (PUFF, 'C',	4, NULL 						, &States[4]),
	S_NORMAL (PUFF, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ARevenantTracerSmoke, Doom, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC50)

	PROP_SpawnState (0)
END_DEFAULTS

//
// A_SkelMissile
//
void A_SkelMissile (AActor *self)
{		
	AActor *missile;
		
	if (!self->target)
		return;
				
	A_FaceTarget (self);
	missile = P_SpawnMissileZ (self, self->z + 48*FRACUNIT,
		self->target, RUNTIME_CLASS(ARevenantTracer));

	if (missile != NULL)
	{
		missile->x += missile->momx;
		missile->y += missile->momy;
		missile->tracer = self->target;
	}
}

#define TRACEANGLE (0xc000000)

void A_Tracer (AActor *self)
{
	angle_t exact;
	fixed_t dist;
	fixed_t slope;
	AActor *dest;
	AActor *smoke;
				
	// killough 1/18/98: this is why some missiles do not have smoke
	// and some do. Also, internal demos start at random gametics, thus
	// the bug in which revenants cause internal demos to go out of sync.
	//
	// killough 3/6/98: fix revenant internal demo bug by subtracting
	// levelstarttic from gametic:
	//
	// [RH] level.time is always 0-based, so nothing special to do here.

	if (level.time & 3)
		return;
	
	// spawn a puff of smoke behind the rocket
	P_SpawnPuff (RUNTIME_CLASS(ABulletPuff), self->x, self->y, self->z, 0, 3);
		
	smoke = Spawn<ARevenantTracerSmoke> (self->x - self->momx,
		self->y - self->momy, self->z);
	
	smoke->momz = FRACUNIT;
	smoke->tics -= pr_tracer()&3;
	if (smoke->tics < 1)
		smoke->tics = 1;
	
	// adjust direction
	dest = self->tracer;
		
	if (!dest || dest->health <= 0)
		return;
	
	// change angle 	
	exact = R_PointToAngle2 (self->x, self->y, dest->x,  dest->y);

	if (exact != self->angle)
	{
		if (exact - self->angle > 0x80000000)
		{
			self->angle -= TRACEANGLE;
			if (exact - self->angle < 0x80000000)
				self->angle = exact;
		}
		else
		{
			self->angle += TRACEANGLE;
			if (exact - self->angle > 0x80000000)
				self->angle = exact;
		}
	}
		
	exact = self->angle>>ANGLETOFINESHIFT;
	self->momx = FixedMul (self->Speed, finecosine[exact]);
	self->momy = FixedMul (self->Speed, finesine[exact]);
	
	// change slope
	dist = P_AproxDistance (dest->x - self->x,
							dest->y - self->y);
	
	dist = dist / self->Speed;

	if (dist < 1)
		dist = 1;
	slope = (dest->z+40*FRACUNIT - self->z) / dist;

	if (slope < self->momz)
		self->momz -= FRACUNIT/8;
	else
		self->momz += FRACUNIT/8;
}


void A_SkelWhoosh (AActor *self)
{
	if (!self->target)
		return;
	A_FaceTarget (self);
	S_Sound (self, CHAN_WEAPON, "skeleton/swing", 1, ATTN_NORM);
}

void A_SkelFist (AActor *self)
{
	if (!self->target)
		return;
				
	A_FaceTarget (self);
		
	if (self->CheckMeleeRange ())
	{
		int damage = ((pr_skelfist()%10)+1)*6;
		S_Sound (self, CHAN_WEAPON, "skeleton/melee", 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
	}
}
