#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"

static FRandom pr_batspawn ("BatSpawn");
static FRandom pr_batmove ("BatMove");

void A_BatSpawnInit (AActor *);
void A_BatSpawn (AActor *);
void A_BatMove (AActor *);

// Bat Spawner --------------------------------------------------------------

class ABatSpawner : public AActor
{
	DECLARE_ACTOR (ABatSpawner, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

FState ABatSpawner::States[] =
{
#define S_SPAWNBATS1 0
	S_NORMAL (TNT1, 'A',	2, NULL					    , &States[S_SPAWNBATS1+1]),
	S_NORMAL (TNT1, 'A',	2, A_BatSpawnInit		    , &States[S_SPAWNBATS1+2]),
	S_NORMAL (TNT1, 'A',	2, A_BatSpawn			    , &States[S_SPAWNBATS1+2]),

#define S_SPAWNBATS_OFF (S_SPAWNBATS1+3)
	S_NORMAL (TNT1, 'A',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (ABatSpawner, Hexen, 10225, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_None)

	PROP_SpawnState (S_SPAWNBATS1)
END_DEFAULTS

void ABatSpawner::Activate (AActor *activator)
{
	SetState (&States[S_SPAWNBATS1]);
}

void ABatSpawner::Deactivate (AActor *activator)
{
	SetState (&States[S_SPAWNBATS_OFF]);
}

// Bat ----------------------------------------------------------------------

class ABat : public AActor
{
	DECLARE_ACTOR (ABat, AActor)
};

FState ABat::States[] =
{
#define S_BAT1 0
	S_NORMAL (ABAT, 'A',	2, A_BatMove			    , &States[S_BAT1+1]),
	S_NORMAL (ABAT, 'B',	2, A_BatMove			    , &States[S_BAT1+2]),
	S_NORMAL (ABAT, 'C',	2, A_BatMove			    , &States[S_BAT1]),

#define S_BAT_DEATH (S_BAT1+3)
	S_NORMAL (ABAT, 'A',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ABat, Hexen, -1, 0)
	PROP_SpeedFixed (5)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PASSMOBJ)

	PROP_SpawnState (S_BAT1)
	PROP_DeathState (S_BAT_DEATH)
END_DEFAULTS

//===========================================================================
// Bat Spawner Variables
//	special1	frequency counter
//	special2	
//	args[0]		frequency of spawn (1=fastest, 10=slowest)
//	args[1]		spread angle (0..255)
//	args[2]		
//	args[3]		duration of bats (in octics)
//	args[4]		turn amount per move (in degrees)
//
// Bat Variables
//	special2	lifetime counter
//	args[4]		turn amount per move (in degrees)
//===========================================================================

void A_BatSpawnInit (AActor *actor)
{
	actor->special1 = 0;	// Frequency count
}

void A_BatSpawn (AActor *actor)
{
	AActor *mo;
	int delta;
	angle_t angle;

	// Countdown until next spawn
	if (actor->special1-- > 0) return;
	actor->special1 = actor->args[0];		// Reset frequency count

	delta = actor->args[1];
	if (delta==0) delta=1;
	angle = actor->angle + (((pr_batspawn()%delta)-(delta>>1))<<24);
	mo = P_SpawnMissileAngle (actor, RUNTIME_CLASS(ABat), angle, 0);
	if (mo)
	{
		mo->args[0] = pr_batspawn()&63;			// floatbob index
		mo->args[4] = actor->args[4];			// turn degrees
		mo->special2 = actor->args[3]<<3;		// Set lifetime
		mo->target = actor;
	}
}


void A_BatMove (AActor *actor)
{
	angle_t newangle;

	if (actor->special2 < 0)
	{
		actor->SetState (actor->FindState(NAME_Death));
	}
	actor->special2 -= 2;		// Called every 2 tics

	if (pr_batmove()<128)
	{
		newangle = actor->angle + ANGLE_1*actor->args[4];
	}
	else
	{
		newangle = actor->angle - ANGLE_1*actor->args[4];
	}

	// Adjust momentum vector to new direction
	newangle >>= ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[newangle]);
	actor->momy = FixedMul (actor->Speed, finesine[newangle]);

	if (pr_batmove()<15)
	{
		S_Sound (actor, CHAN_VOICE, "BatScream", 1, ATTN_IDLE);
	}

	// Handle Z movement
	actor->z = actor->target->z + 2*FloatBobOffsets[actor->args[0]];
	actor->args[0] = (actor->args[0]+3)&63;	
}
