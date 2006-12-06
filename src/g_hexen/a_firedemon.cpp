#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

#define FIREDEMON_ATTACK_RANGE	64*8*FRACUNIT

static FRandom pr_firedemonrock ("FireDemonRock");
static FRandom pr_smbounce ("SMBounce");
static FRandom pr_firedemonchase ("FiredChase");
static FRandom pr_firedemonsplotch ("FiredSplotch");

//============================================================================
// Fire Demon AI
//
// special1			index into floatbob
// special2			whether strafing or not
//============================================================================

void A_FiredRocks (AActor *);
void A_FiredSpawnRock (AActor *);
void A_SmBounce (AActor *);
void A_FiredChase (AActor *);
void A_FiredAttack (AActor *);
void A_FiredSplotch (AActor *);

// FireDemon ----------------------------------------------------------------

class AFireDemon : public AActor
{
	DECLARE_ACTOR (AFireDemon, AActor)
};

FState AFireDemon::States[] =
{
#define S_FIRED_SPAWN1 0
	S_BRIGHT (FDMN, 'X',	5, NULL					    , &States[S_FIRED_SPAWN1+1]),

#define S_FIRED_LOOK1 (S_FIRED_SPAWN1+1)
	S_BRIGHT (FDMN, 'E',   10, A_Look				    , &States[S_FIRED_LOOK1+1]),
	S_BRIGHT (FDMN, 'F',   10, A_Look				    , &States[S_FIRED_LOOK1+2]),
	S_BRIGHT (FDMN, 'G',   10, A_Look				    , &States[S_FIRED_LOOK1+0]),

	S_BRIGHT (FDMN, 'E',	8, NULL					    , &States[S_FIRED_LOOK1+4]),
	S_BRIGHT (FDMN, 'F',	6, NULL					    , &States[S_FIRED_LOOK1+5]),
	S_BRIGHT (FDMN, 'G',	5, NULL					    , &States[S_FIRED_LOOK1+6]),
	S_BRIGHT (FDMN, 'F',	8, NULL					    , &States[S_FIRED_LOOK1+7]),
	S_BRIGHT (FDMN, 'E',	6, NULL					    , &States[S_FIRED_LOOK1+8]),
	S_BRIGHT (FDMN, 'F',	7, A_FiredRocks			    , &States[S_FIRED_LOOK1+9]),
	S_BRIGHT (FDMN, 'H',	5, NULL					    , &States[S_FIRED_LOOK1+10]),
	S_BRIGHT (FDMN, 'I',	5, NULL					    , &States[S_FIRED_LOOK1+11]),
	S_BRIGHT (FDMN, 'J',	5, A_UnSetInvulnerable	    , &States[S_FIRED_LOOK1+12]),

#define S_FIRED_WALK1 (S_FIRED_LOOK1+12)
	S_BRIGHT (FDMN, 'A',	5, A_FiredChase			    , &States[S_FIRED_WALK1+1]),
	S_BRIGHT (FDMN, 'B',	5, A_FiredChase			    , &States[S_FIRED_WALK1+2]),
	S_BRIGHT (FDMN, 'C',	5, A_FiredChase			    , &States[S_FIRED_WALK1+0]),

#define S_FIRED_PAIN1 (S_FIRED_WALK1+3)
	S_BRIGHT (FDMN, 'D',	6, A_Pain				    , &States[S_FIRED_WALK1]),

#define S_FIRED_ATTACK1 (S_FIRED_PAIN1+1)
	S_BRIGHT (FDMN, 'K',	3, A_FaceTarget			    , &States[S_FIRED_ATTACK1+1]),
	S_BRIGHT (FDMN, 'K',	5, A_FiredAttack		    , &States[S_FIRED_ATTACK1+2]),
	S_BRIGHT (FDMN, 'K',	5, A_FiredAttack		    , &States[S_FIRED_ATTACK1+3]),
	S_BRIGHT (FDMN, 'K',	5, A_FiredAttack		    , &States[S_FIRED_WALK1]),

#define S_FIRED_DEATH1 (S_FIRED_ATTACK1+4)
	S_BRIGHT (FDMN, 'D',	4, A_FaceTarget			    , &States[S_FIRED_DEATH1+1]),
	S_BRIGHT (FDMN, 'L',	4, A_Scream				    , &States[S_FIRED_DEATH1+2]),
	S_BRIGHT (FDMN, 'L',	4, A_NoBlocking			    , &States[S_FIRED_DEATH1+3]),
	S_BRIGHT (FDMN, 'L',  200, NULL					    , NULL),

#define S_FIRED_XDEATH1 (S_FIRED_DEATH1+4)
	S_NORMAL (FDMN, 'M',	5, A_FaceTarget			    , &States[S_FIRED_XDEATH1+1]),
	S_NORMAL (FDMN, 'N',	5, A_NoBlocking			    , &States[S_FIRED_XDEATH1+2]),
	S_NORMAL (FDMN, 'O',	5, A_FiredSplotch		    , NULL),

#define S_FIRED_ICE1 (S_FIRED_XDEATH1+3)
	S_NORMAL (FDMN, 'R',	5, A_FreezeDeath		    , &States[S_FIRED_ICE1+1]),
	S_NORMAL (FDMN, 'R',	1, A_FreezeDeathChunks	    , &States[S_FIRED_ICE1+1])
};

IMPLEMENT_ACTOR (AFireDemon, Hexen, 10060, 5)
	PROP_SpawnHealth (80)
	PROP_ReactionTime (8)
	PROP_PainChance (1)
	PROP_SpeedFixed (13)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (68)
	PROP_Mass (75)
	PROP_Damage (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_DROPOFF|MF_NOGRAVITY|MF_FLOAT)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_INVULNERABLE|MF2_MCROSS|MF2_TELESTOMP)

	PROP_SpawnState (S_FIRED_SPAWN1)
	PROP_SeeState (S_FIRED_LOOK1+3)
	PROP_PainState (S_FIRED_PAIN1)
	PROP_MissileState (S_FIRED_ATTACK1)
	PROP_CrashState (S_FIRED_XDEATH1)
	PROP_DeathState (S_FIRED_DEATH1)
	PROP_XDeathState (S_FIRED_XDEATH1)
	PROP_IDeathState (S_FIRED_ICE1)

	PROP_SeeSound ("FireDemonSpawn")
	PROP_PainSound ("FireDemonPain")
	PROP_DeathSound ("FireDemonDeath")
	PROP_ActiveSound ("FireDemonActive")
	PROP_Obituary("$OB_FIREDEMON")
END_DEFAULTS

// AFireDemonSplotch1 -------------------------------------------------------

class AFireDemonSplotch1 : public AActor
{
	DECLARE_ACTOR (AFireDemonSplotch1, AActor)
};

FState AFireDemonSplotch1::States[] =
{
	S_NORMAL (FDMN, 'P',	3, NULL					    , &States[1]),
	S_NORMAL (FDMN, 'P',	6, A_QueueCorpse		    , &States[2]),
	S_NORMAL (FDMN, 'Y',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonSplotch1, Hexen, -1, 0)
	PROP_SpawnHealth (1000)
	PROP_ReactionTime (8)
	PROP_PainChance (0)
	PROP_SpeedFixed (0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (16)
	PROP_Mass (100)
	PROP_Damage (0)
	PROP_Flags (MF_DROPOFF|MF_CORPSE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FLOORCLIP)

	PROP_SpawnState (0)
END_DEFAULTS

// AFireDemonSplotch2 -------------------------------------------------------

class AFireDemonSplotch2 : public AFireDemonSplotch1
{
	DECLARE_ACTOR (AFireDemonSplotch2, AFireDemonSplotch1)
};

FState AFireDemonSplotch2::States[] =
{
	S_NORMAL (FDMN, 'Q',	3, NULL					    , &States[1]),
	S_NORMAL (FDMN, 'Q',	6, A_QueueCorpse		    , &States[2]),
	S_NORMAL (FDMN, 'Z',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonSplotch2, Hexen, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// AFireDemonRock1 ------------------------------------------------------------

class AFireDemonRock1 : public AActor
{
	DECLARE_ACTOR (AFireDemonRock1, AActor)
};

FState AFireDemonRock1::States[] =
{
#define S_FIRED_RDROP1 0
	S_NORMAL (FDMN, 'S',	4, NULL					    , &States[S_FIRED_RDROP1]),

#define S_FIRED_RDEAD1 (S_FIRED_RDROP1+1)
	S_NORMAL (FDMN, 'S',	5, A_SmBounce			    , &States[S_FIRED_RDEAD1+1]),
	S_NORMAL (FDMN, 'S',  200, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonRock1, Hexen, -1, 0)
	PROP_SpawnHealth (1000)
	PROP_ReactionTime (8)
	PROP_PainChance (0)
	PROP_SpeedFixed (0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (5)
	PROP_Mass (16)
	PROP_Damage (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_FIRED_RDROP1)
	PROP_DeathState (S_FIRED_RDEAD1)
	PROP_XDeathState (S_FIRED_RDEAD1+1)
END_DEFAULTS

// AFireDemonRock2 ------------------------------------------------------------

class AFireDemonRock2 : public AFireDemonRock1
{
	DECLARE_ACTOR (AFireDemonRock2, AActor)
};

FState AFireDemonRock2::States[] =
{
#define S_FIRED_RDROP2 0
	S_NORMAL (FDMN, 'T',	4, NULL					    , &States[S_FIRED_RDROP2]),

#define S_FIRED_RDEAD2 (S_FIRED_RDROP2+1)
	S_NORMAL (FDMN, 'T',	5, A_SmBounce			    , &States[S_FIRED_RDEAD2+1]),
	S_NORMAL (FDMN, 'T',  200, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonRock2, Hexen, -1, 0)
	PROP_SpawnState (S_FIRED_RDROP2)
	PROP_DeathState (S_FIRED_RDEAD2)
	PROP_XDeathState (S_FIRED_RDEAD2+1)
END_DEFAULTS

// AFireDemonRock3 ------------------------------------------------------------

class AFireDemonRock3 : public AFireDemonRock1
{
	DECLARE_ACTOR (AFireDemonRock3, AFireDemonRock1)
};

FState AFireDemonRock3::States[] =
{
#define S_FIRED_RDROP3 0
	S_NORMAL (FDMN, 'U',	4, NULL					    , &States[S_FIRED_RDROP3]),

#define S_FIRED_RDEAD3 (S_FIRED_RDROP3+1)
	S_NORMAL (FDMN, 'U',	5, A_SmBounce			    , &States[S_FIRED_RDEAD3+1]),
	S_NORMAL (FDMN, 'U',  200, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonRock3, Hexen, -1, 0)
	PROP_SpawnState (S_FIRED_RDROP3)
	PROP_DeathState (S_FIRED_RDEAD3)
	PROP_XDeathState (S_FIRED_RDEAD3+1)
END_DEFAULTS

// AFireDemonRock4 ------------------------------------------------------------

class AFireDemonRock4 : public AFireDemonRock1
{
	DECLARE_ACTOR (AFireDemonRock4, AFireDemonRock1)
};

FState AFireDemonRock4::States[] =
{
#define S_FIRED_RDROP4 0
	S_NORMAL (FDMN, 'V',	4, NULL					    , &States[S_FIRED_RDROP4]),

#define S_FIRED_RDEAD4 (S_FIRED_RDROP4+1)
	S_NORMAL (FDMN, 'V',	5, A_SmBounce			    , &States[S_FIRED_RDEAD4+1]),
	S_NORMAL (FDMN, 'V',  200, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonRock4, Hexen, -1, 0)
	PROP_SpawnState (S_FIRED_RDROP4)
	PROP_DeathState (S_FIRED_RDEAD4)
	PROP_XDeathState (S_FIRED_RDEAD4+1)
END_DEFAULTS

// AFireDemonRock5 ------------------------------------------------------------

class AFireDemonRock5 : public AFireDemonRock1
{
	DECLARE_ACTOR (AFireDemonRock5, AActor)
};

FState AFireDemonRock5::States[] =
{
#define S_FIRED_RDROP5 0
	S_NORMAL (FDMN, 'W',	4, NULL					    , &States[S_FIRED_RDROP5]),

#define S_FIRED_RDEAD5 (S_FIRED_RDROP5+1)
	S_NORMAL (FDMN, 'W',	5, A_SmBounce			    , &States[S_FIRED_RDEAD5+1]),
	S_NORMAL (FDMN, 'W',  200, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonRock5, Hexen, -1, 0)
	PROP_SpawnState (S_FIRED_RDROP5)
	PROP_DeathState (S_FIRED_RDEAD5)
	PROP_XDeathState (S_FIRED_RDEAD5+1)
END_DEFAULTS

// AFireDemonMissile -----------------------------------------------------------

class AFireDemonMissile : public AActor
{
	DECLARE_ACTOR (AFireDemonMissile, AActor)
};

FState AFireDemonMissile::States[] =
{
#define S_FIRED_FX6_1 0
	S_BRIGHT (FDMB, 'A',	5, NULL					    , &States[S_FIRED_FX6_1]),

#define S_FIRED_FX6_2 (S_FIRED_FX6_1+1)
	S_BRIGHT (FDMB, 'B',	5, NULL					    , &States[S_FIRED_FX6_2+1]),
	S_BRIGHT (FDMB, 'C',	5, NULL					    , &States[S_FIRED_FX6_2+2]),
	S_BRIGHT (FDMB, 'D',	5, NULL					    , &States[S_FIRED_FX6_2+3]),
	S_BRIGHT (FDMB, 'E',	5, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFireDemonMissile, Hexen, -1, 0)
	PROP_SpawnHealth (1000)
	PROP_ReactionTime (8)
	PROP_PainChance (0)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_Mass (15)
	PROP_Damage (1)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS|MF2_FLOORCLIP)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_FIRED_FX6_1)
	PROP_DeathState (S_FIRED_FX6_2)

	PROP_DeathSound ("FireDemonMissileHit")
END_DEFAULTS

//============================================================================
//
// A_FiredRocks
//
//============================================================================

void A_FiredRocks (AActor *actor)
{
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
	A_FiredSpawnRock (actor);
}

//============================================================================
//
// A_FiredSpawnRock
//
//============================================================================

void A_FiredSpawnRock (AActor *actor)
{
	AActor *mo;
	int x,y,z;
	const PClass *rtype;

	switch (pr_firedemonrock() % 5)
	{
		case 0:
			rtype = RUNTIME_CLASS (AFireDemonRock1);
			break;
		case 1:
			rtype = RUNTIME_CLASS (AFireDemonRock2);
			break;
		case 2:
			rtype = RUNTIME_CLASS (AFireDemonRock3);
			break;
		case 3:
			rtype = RUNTIME_CLASS (AFireDemonRock4);
			break;
		case 4:
		default:
			rtype = RUNTIME_CLASS (AFireDemonRock5);
			break;
	}

	x = actor->x + ((pr_firedemonrock() - 128) << 12);
	y = actor->y + ((pr_firedemonrock() - 128) << 12);
	z = actor->z + ( pr_firedemonrock() << 11);
	mo = Spawn (rtype, x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		mo->target = actor;
		mo->momx = (pr_firedemonrock() - 128) <<10;
		mo->momy = (pr_firedemonrock() - 128) <<10;
		mo->momz = (pr_firedemonrock() << 10);
		mo->special1 = 2;		// Number bounces
	}

	// Initialize fire demon
	actor->special2 = 0;
	actor->flags &= ~MF_JUSTATTACKED;
}

//============================================================================
//
// A_SmBounce
//
//============================================================================

void A_SmBounce (AActor *actor)
{
	// give some more momentum (x,y,&z)
	actor->z = actor->floorz + FRACUNIT;
	actor->momz = (2*FRACUNIT) + (pr_smbounce() << 10);
	actor->momx = pr_smbounce()%3<<FRACBITS;
	actor->momy = pr_smbounce()%3<<FRACBITS;
}

//============================================================================
//
// A_FiredAttack
//
//============================================================================

void A_FiredAttack (AActor *actor)
{
	if (actor->target == NULL)
		return;
	AActor *mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(AFireDemonMissile));
	if (mo) S_Sound (actor, CHAN_BODY, "FireDemonAttack", 1, ATTN_NORM);
}

//============================================================================
//
// A_FiredChase
//
//============================================================================

void A_FiredChase (AActor *actor)
{
	int weaveindex = actor->special1;
	AActor *target = actor->target;
	angle_t ang;
	fixed_t dist;

	if (actor->reactiontime) actor->reactiontime--;
	if (actor->threshold) actor->threshold--;

	// Float up and down
	actor->z += FloatBobOffsets[weaveindex];
	actor->special1 = (weaveindex+2)&63;

	// Ensure it stays above certain height
	if (actor->z < actor->floorz + (64*FRACUNIT))
	{
		actor->z += 2*FRACUNIT;
	}

	if(!actor->target || !(actor->target->flags&MF_SHOOTABLE))
	{	// Invalid target
		P_LookForPlayers (actor,true);
		return;
	}

	// Strafe
	if (actor->special2 > 0)
	{
		actor->special2--;
	}
	else
	{
		actor->special2 = 0;
		actor->momx = actor->momy = 0;
		dist = P_AproxDistance (actor->x - target->x, actor->y - target->y);
		if (dist < FIREDEMON_ATTACK_RANGE)
		{
			if (pr_firedemonchase() < 30)
			{
				ang = R_PointToAngle2 (actor->x, actor->y, target->x, target->y);
				if (pr_firedemonchase() < 128)
					ang += ANGLE_90;
				else
					ang -= ANGLE_90;
				ang >>= ANGLETOFINESHIFT;
				actor->momx = finecosine[ang] << 3; //FixedMul (8*FRACUNIT, finecosine[ang]);
				actor->momy = finesine[ang] << 3; //FixedMul (8*FRACUNIT, finesine[ang]);
				actor->special2 = 3;		// strafe time
			}
		}
	}

	FaceMovementDirection (actor);

	// Normal movement
	if (!actor->special2)
	{
		if (--actor->movecount<0 || !P_Move (actor))
		{
			P_NewChaseDir (actor);
		}
	}

	// Do missile attack
	if (!(actor->flags & MF_JUSTATTACKED))
	{
		if (P_CheckMissileRange (actor) && (pr_firedemonchase() < 20))
		{
			actor->SetState (actor->MissileState);
			actor->flags |= MF_JUSTATTACKED;
			return;
		}
	}
	else
	{
		actor->flags &= ~MF_JUSTATTACKED;
	}

	// make active sound
	if (pr_firedemonchase() < 3)
	{
		actor->PlayActiveSound ();
	}
}

//============================================================================
//
// A_FiredSplotch
//
//============================================================================

void A_FiredSplotch (AActor *actor)
{
	AActor *mo;

	mo = Spawn<AFireDemonSplotch1> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->momx = (pr_firedemonsplotch() - 128) << 11;
		mo->momy = (pr_firedemonsplotch() - 128) << 11;
		mo->momz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
	mo = Spawn<AFireDemonSplotch2> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->momx = (pr_firedemonsplotch() - 128) << 11;
		mo->momy = (pr_firedemonsplotch() - 128) << 11;
		mo->momz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
}
