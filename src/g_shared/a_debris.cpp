#include "actor.h"
#include "info.h"
#include "m_random.h"

static FRandom pr_dirt ("SpawnDirt");

// Convenient macros --------------------------------------------------------

#define _DEBCOMMON(cls,parent,ns,dstate,sid) \
	class cls : public parent { DECLARE_STATELESS_ACTOR (cls, parent) static FState States[ns]; }; \
	IMPLEMENT_ACTOR (cls, Any, -1, sid) \
		PROP_DeathState (dstate) \
		PROP_SpawnState (0)

#define _DEBSTARTSTATES(cls,ns) \
	END_DEFAULTS  FState cls::States[ns] =

#define DEBRIS(cls,spawnnum,ns,dstate) \
	_DEBCOMMON(cls,AActor,ns,dstate,spawnnum) \
		PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE) \
		PROP_Flags2 (MF2_NOTELEPORT) \
	 _DEBSTARTSTATES(cls,ns)

#define SHARD(cls,spawnnum,ns,dstate) \
	_DEBCOMMON(cls,AGlassShard,ns,dstate,spawnnum) \
		PROP_RadiusFixed (5) \
		PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE|MF_NOGRAVITY) \
		PROP_Flags2 (MF2_NOTELEPORT|MF2_HEXENBOUNCE) \
	 _DEBSTARTSTATES(cls,ns)

// Rocks --------------------------------------------------------------------

#define S_ROCK1 0
#define S_ROCK1_D (S_ROCK1+1)

DEBRIS (ARock1, 41, 2, S_ROCK1_D)
{
	S_NORMAL (ROKK, 'A',   20, NULL, &States[S_ROCK1]),
	S_NORMAL (ROKK, 'A',   10, NULL, NULL)
};

#define S_ROCK2 0
#define S_ROCK2_D (S_ROCK2+1)

DEBRIS (ARock2, 42, 2, S_ROCK2_D)
{
	S_NORMAL (ROKK, 'B',   20, NULL, &States[S_ROCK2]),
	S_NORMAL (ROKK, 'B',   10, NULL, NULL)
};

#define S_ROCK3 0
#define S_ROCK3_D (S_ROCK3+1)

DEBRIS (ARock3, 43, 2, S_ROCK3_D)
{
	S_NORMAL (ROKK, 'C',   20, NULL, &States[S_ROCK3+0]),
	S_NORMAL (ROKK, 'C',   10, NULL, NULL)
};

// Dirt ---------------------------------------------------------------------

#define S_DIRT1 0
#define S_DIRT1_D (S_DIRT1+1)

DEBRIS (ADirt1, 44, 2, S_DIRT1_D)
{
	S_NORMAL (ROKK, 'D',   20, NULL, &States[S_DIRT1]),
	S_NORMAL (ROKK, 'D',   10, NULL, NULL)
};

#define S_DIRT2 0
#define S_DIRT2_D (S_DIRT2+1)

DEBRIS (ADirt2, 45, 2, S_DIRT2_D)
{
	S_NORMAL (ROKK, 'E',   20, NULL, &States[S_DIRT2]),
	S_NORMAL (ROKK, 'E',   10, NULL, NULL)
};

#define S_DIRT3 0
#define S_DIRT3_D (S_DIRT3+1)

DEBRIS (ADirt3, 46, 2, S_DIRT3_D)
{
	S_NORMAL (ROKK, 'F',   20, NULL, &States[S_DIRT3]),
	S_NORMAL (ROKK, 'F',   10, NULL, NULL)
};

#define S_DIRT4 0
#define S_DIRT4_D (S_DIRT4+1)

DEBRIS (ADirt4, 47, 2, S_DIRT4_D)
{
	S_NORMAL (ROKK, 'G',   20, NULL, &States[S_DIRT4]),
	S_NORMAL (ROKK, 'G',   10, NULL, NULL)
};

#define S_DIRT5 0
#define S_DIRT5_D (S_DIRT5+1)

DEBRIS (ADirt5, 48, 2, S_DIRT5_D)
{
	S_NORMAL (ROKK, 'H',   20, NULL, &States[S_DIRT5]),
	S_NORMAL (ROKK, 'H',   10, NULL, NULL)
};

#define S_DIRT6 0
#define S_DIRT6_D (S_DIRT6+1)

DEBRIS (ADirt6, 49, 2, S_DIRT6_D)
{
	S_NORMAL (ROKK, 'I',   20, NULL, &States[S_DIRT6]),
	S_NORMAL (ROKK, 'I',   10, NULL, NULL)
};

// Stained glass ------------------------------------------------------------

class AGlassShard : public AActor
{
	DECLARE_STATELESS_ACTOR (AGlassShard, AActor)
public:
	bool FloorBounceMissile (secplane_t &plane)
	{
		fixed_t bouncemomz = FixedMul (momz, (fixed_t)(-0.3*FRACUNIT));

		if (abs (bouncemomz) < (FRACUNIT/2))
		{
			Destroy ();
		}
		else
		{
			if (!Super::FloorBounceMissile (plane))
			{
				momz = bouncemomz;
				return false;
			}
		}
		return true;
	}
};

IMPLEMENT_ABSTRACT_ACTOR (AGlassShard)

#define S_SGSHARD1 0
#define S_SGSHARD1_D (S_SGSHARD1+5)

SHARD (ASGShard1, 54, 6, S_SGSHARD1_D)
{
	S_NORMAL (SGSA, 'A',	4, NULL 					, &States[S_SGSHARD1+1]),
	S_NORMAL (SGSA, 'B',	4, NULL 					, &States[S_SGSHARD1+2]),
	S_NORMAL (SGSA, 'C',	4, NULL 					, &States[S_SGSHARD1+3]),
	S_NORMAL (SGSA, 'D',	4, NULL 					, &States[S_SGSHARD1+4]),
	S_NORMAL (SGSA, 'E',	4, NULL 					, &States[S_SGSHARD1+0]),

	S_NORMAL (SGSA, 'E',   30, NULL 					, NULL)
};

#define S_SGSHARD2 0
#define S_SGSHARD2_D (S_SGSHARD2+5)

SHARD (ASGShard2, 55, 6, S_SGSHARD2_D)
{
	S_NORMAL (SGSA, 'F',	4, NULL 					, &States[S_SGSHARD2+1]),
	S_NORMAL (SGSA, 'G',	4, NULL 					, &States[S_SGSHARD2+2]),
	S_NORMAL (SGSA, 'H',	4, NULL 					, &States[S_SGSHARD2+3]),
	S_NORMAL (SGSA, 'I',	4, NULL 					, &States[S_SGSHARD2+4]),
	S_NORMAL (SGSA, 'J',	4, NULL 					, &States[S_SGSHARD2+0]),

	S_NORMAL (SGSA, 'J',   30, NULL 					, NULL)
};

#define S_SGSHARD3 0
#define S_SGSHARD3_D (S_SGSHARD3+5)

SHARD (ASGShard3, 56, 6, S_SGSHARD3_D)
{
	S_NORMAL (SGSA, 'K',	4, NULL 					, &States[S_SGSHARD3+1]),
	S_NORMAL (SGSA, 'L',	4, NULL 					, &States[S_SGSHARD3+2]),
	S_NORMAL (SGSA, 'M',	4, NULL 					, &States[S_SGSHARD3+3]),
	S_NORMAL (SGSA, 'N',	4, NULL 					, &States[S_SGSHARD3+4]),
	S_NORMAL (SGSA, 'O',	4, NULL 					, &States[S_SGSHARD3+0]),

	S_NORMAL (SGSA, 'O',   30, NULL 					, NULL)
};

#define S_SGSHARD4 0
#define S_SGSHARD4_D (S_SGSHARD4+5)

SHARD (ASGShard4, 57, 6, S_SGSHARD4_D)
{
	S_NORMAL (SGSA, 'P',	4, NULL 					, &States[S_SGSHARD4+1]),
	S_NORMAL (SGSA, 'Q',	4, NULL 					, &States[S_SGSHARD4+2]),
	S_NORMAL (SGSA, 'R',	4, NULL 					, &States[S_SGSHARD4+3]),
	S_NORMAL (SGSA, 'S',	4, NULL 					, &States[S_SGSHARD4+4]),
	S_NORMAL (SGSA, 'T',	4, NULL 					, &States[S_SGSHARD4+0]),

	S_NORMAL (SGSA, 'T',   30, NULL 					, NULL)
};

#define S_SGSHARD5 0
#define S_SGSHARD5_D (S_SGSHARD5+5)

SHARD (ASGShard5, 58, 6, S_SGSHARD5_D)
{
	S_NORMAL (SGSA, 'U',	4, NULL 					, &States[S_SGSHARD5+1]),
	S_NORMAL (SGSA, 'V',	4, NULL 					, &States[S_SGSHARD5+2]),
	S_NORMAL (SGSA, 'W',	4, NULL 					, &States[S_SGSHARD5+3]),
	S_NORMAL (SGSA, 'X',	4, NULL 					, &States[S_SGSHARD5+4]),
	S_NORMAL (SGSA, 'Y',	4, NULL 					, &States[S_SGSHARD5+0]),

	S_NORMAL (SGSA, 'Y',   30, NULL 					, NULL)
};

#define S_SGSHARD6 0
#define S_SGSHARD6_D (S_SGSHARD6+1)

SHARD (ASGShard6, 59, 2, S_SGSHARD6_D)
{
	S_NORMAL (SGSB, 'A',	4, NULL 					, &States[S_SGSHARD6+0]),
	S_NORMAL (SGSB, 'A',   30, NULL 					, NULL)
};

#define S_SGSHARD7 0
#define S_SGSHARD7_D (S_SGSHARD7+1)

SHARD (ASGShard7, 60, 2, S_SGSHARD7_D)
{
	S_NORMAL (SGSB, 'B',	4, NULL 					, &States[S_SGSHARD7+0]),
	S_NORMAL (SGSB, 'B',   30, NULL 					, NULL)
};

#define S_SGSHARD8 0
#define S_SGSHARD8_D (S_SGSHARD8+1)

SHARD (ASGShard8, 61, 2, S_SGSHARD8_D)
{
	S_NORMAL (SGSB, 'C',	4, NULL 					, &States[S_SGSHARD8+0]),
	S_NORMAL (SGSB, 'C',   30, NULL 					, NULL)
};

#define S_SGSHARD9 0
#define S_SGSHARD9_D (S_SGSHARD9+1)

SHARD (ASGShard9, 62, 2, S_SGSHARD9_D)
{
	S_NORMAL (SGSB, 'D',	4, NULL 					, &States[S_SGSHARD9+0]),
	S_NORMAL (SGSB, 'D',   30, NULL 					, NULL)
};

#define S_SGSHARD0 0
#define S_SGSHARD0_D (S_SGSHARD0+1)

SHARD (ASGShard0, 63, 2, S_SGSHARD0_D)
{
	S_NORMAL (SGSB, 'E',	4, NULL 					, &States[S_SGSHARD0+0]),
	S_NORMAL (SGSB, 'E',   30, NULL 					, NULL)
};

// Dirt stuff

void P_SpawnDirt (AActor *actor, fixed_t radius)
{
	fixed_t x,y,z;
	const TypeInfo *dtype = NULL;
	AActor *mo;
	angle_t angle;

	angle = pr_dirt()<<5;		// <<24 >>19
	x = actor->x + FixedMul(radius,finecosine[angle]);
	y = actor->y + FixedMul(radius,finesine[angle]);
	z = actor->z + (pr_dirt()<<9) + FRACUNIT;
	switch (pr_dirt()%6)
	{
		case 0:
			dtype = RUNTIME_CLASS(ADirt1);
			break;
		case 1:
			dtype = RUNTIME_CLASS(ADirt2);
			break;
		case 2:
			dtype = RUNTIME_CLASS(ADirt3);
			break;
		case 3:
			dtype = RUNTIME_CLASS(ADirt4);
			break;
		case 4:
			dtype = RUNTIME_CLASS(ADirt5);
			break;
		case 5:
			dtype = RUNTIME_CLASS(ADirt6);
			break;
	}
	mo = Spawn (dtype, x, y, z);
	if (mo)
	{
		mo->momz = pr_dirt()<<10;
	}
}
