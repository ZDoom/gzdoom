#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"

static FRandom pr_fogspawn ("FogSpawn");

void A_FogSpawn (AActor *);
void A_FogMove (AActor *);

// Fog Spawner --------------------------------------------------------------

class AFogSpawner : public AActor
{
	DECLARE_ACTOR (AFogSpawner, AActor)
};

FState AFogSpawner::States[] =
{
#define S_SPAWNFOG1 0
	S_NORMAL (TNT1, 'A',   20, A_FogSpawn			    , &States[S_SPAWNFOG1]),
};

IMPLEMENT_ACTOR (AFogSpawner, Hexen, 10000, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_RenderFlags (RF_INVISIBLE)

	PROP_SpawnState (S_SPAWNFOG1)
END_DEFAULTS

// Small Fog Patch ----------------------------------------------------------

class AFogPatchSmall : public AActor
{
	DECLARE_ACTOR (AFogPatchSmall, AActor)
};

FState AFogPatchSmall::States[] =
{
#define S_FOGPATCHS1 0
	S_NORMAL (FOGS, 'A',	7, A_FogMove			    , &States[S_FOGPATCHS1+1]),
	S_NORMAL (FOGS, 'B',	7, A_FogMove			    , &States[S_FOGPATCHS1+2]),
	S_NORMAL (FOGS, 'C',	7, A_FogMove			    , &States[S_FOGPATCHS1+3]),
	S_NORMAL (FOGS, 'D',	7, A_FogMove			    , &States[S_FOGPATCHS1+4]),
	S_NORMAL (FOGS, 'E',	7, A_FogMove			    , &States[S_FOGPATCHS1]),

#define S_FOGPATCHS0 (S_FOGPATCHS1+5)
	S_NORMAL (FOGS, 'E',	5, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFogPatchSmall, Hexen, 10001, 0)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP|MF_FLOAT)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (S_FOGPATCHS1)
	PROP_DeathState (S_FOGPATCHS0)
END_DEFAULTS

// Medium Fog Patch ---------------------------------------------------------

class AFogPatchMedium : public AActor
{
	DECLARE_ACTOR (AFogPatchMedium, AActor)
};

FState AFogPatchMedium::States[] =
{
#define S_FOGPATCHM1 0
	S_NORMAL (FOGM, 'A',	7, A_FogMove			    , &States[S_FOGPATCHM1+1]),
	S_NORMAL (FOGM, 'B',	7, A_FogMove			    , &States[S_FOGPATCHM1+2]),
	S_NORMAL (FOGM, 'C',	7, A_FogMove			    , &States[S_FOGPATCHM1+3]),
	S_NORMAL (FOGM, 'D',	7, A_FogMove			    , &States[S_FOGPATCHM1+4]),
	S_NORMAL (FOGM, 'E',	7, A_FogMove			    , &States[S_FOGPATCHM1]),

#define S_FOGPATCHM0 (S_FOGPATCHM1+5)
	S_NORMAL (FOGS, 'A',	5, NULL					    , &States[S_FOGPATCHM0+1]),
	S_NORMAL (FOGS, 'B',	5, NULL					    , &States[S_FOGPATCHM0+2]),
	S_NORMAL (FOGS, 'C',	5, NULL					    , &States[S_FOGPATCHM0+3]),
	S_NORMAL (FOGS, 'D',	5, NULL					    , &States[S_FOGPATCHM0+4]),
	S_NORMAL (FOGS, 'E',	5, NULL					    , &AFogPatchSmall::States[S_FOGPATCHS0]),
};

IMPLEMENT_ACTOR (AFogPatchMedium, Hexen, 10002, 0)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP|MF_FLOAT)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (S_FOGPATCHM1)
	PROP_DeathState (S_FOGPATCHM0)
END_DEFAULTS

// Large Fog Patch ----------------------------------------------------------

class AFogPatchLarge : public AActor
{
	DECLARE_ACTOR (AFogPatchLarge, AActor)
};

FState AFogPatchLarge::States[] =
{
#define S_FOGPATCHL1 0
	S_NORMAL (FOGL, 'A',	7, A_FogMove			    , &States[S_FOGPATCHL1+1]),
	S_NORMAL (FOGL, 'B',	7, A_FogMove			    , &States[S_FOGPATCHL1+2]),
	S_NORMAL (FOGL, 'C',	7, A_FogMove			    , &States[S_FOGPATCHL1+3]),
	S_NORMAL (FOGL, 'D',	7, A_FogMove			    , &States[S_FOGPATCHL1+4]),
	S_NORMAL (FOGL, 'E',	7, A_FogMove			    , &States[S_FOGPATCHL1]),

#define S_FOGPATCHL0 (S_FOGPATCHL1+5)
	S_NORMAL (FOGM, 'A',	4, NULL					    , &States[S_FOGPATCHL0+1]),
	S_NORMAL (FOGM, 'B',	4, NULL					    , &States[S_FOGPATCHL0+2]),
	S_NORMAL (FOGM, 'C',	4, NULL					    , &States[S_FOGPATCHL0+3]),
	S_NORMAL (FOGM, 'D',	4, NULL					    , &States[S_FOGPATCHL0+4]),
	S_NORMAL (FOGM, 'E',	4, NULL					    , &AFogPatchMedium::States[S_FOGPATCHM0]),
};

IMPLEMENT_ACTOR (AFogPatchLarge, Hexen, 10003, 0)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIP|MF_FLOAT)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (S_FOGPATCHL1)
	PROP_DeathState (S_FOGPATCHL0)
END_DEFAULTS

//==========================================================================
// Fog Variables:
//
//		args[0]		Speed (0..10) of fog
//		args[1]		Angle of spread (0..128)
//		args[2]		Frequency of spawn (1..10)
//		args[3]		Lifetime countdown
//		args[4]		Boolean: fog moving?
//		special1	Internal:  Counter for spawn frequency
//		special2	Internal:  Index into floatbob table
//
//==========================================================================

//==========================================================================
//
// A_FogSpawn
//
//==========================================================================

void A_FogSpawn (AActor *actor)
{
	static const PClass *fogs[3] =
	{
		RUNTIME_CLASS(AFogPatchSmall),
		RUNTIME_CLASS(AFogPatchMedium),
		RUNTIME_CLASS(AFogPatchLarge)
	};

	AActor *mo=NULL;
	angle_t delta;

	if (actor->special1-- > 0) return;

	actor->special1 = actor->args[2];		// Reset frequency count

	mo = Spawn (fogs[pr_fogspawn()%3], actor->x, actor->y, actor->z, ALLOW_REPLACE);

	if (mo)
	{
		delta = actor->args[1];
		if (delta==0) delta=1;
		mo->angle = actor->angle + (((pr_fogspawn()%delta)-(delta>>1))<<24);
		mo->target = actor;
		if (actor->args[0] < 1) actor->args[0] = 1;
		mo->args[0] = (pr_fogspawn() % (actor->args[0]))+1;	// Random speed
		mo->args[3] = actor->args[3];						// Set lifetime
		mo->args[4] = 1;									// Set to moving
		mo->special2 = pr_fogspawn()&63;
	}
}

//==========================================================================
//
// A_FogMove
//
//==========================================================================

void A_FogMove (AActor *actor)
{
	int speed = actor->args[0]<<FRACBITS;
	angle_t angle;
	int weaveindex;

	if (!(actor->args[4])) return;

	if (actor->args[3]-- <= 0)
	{
		actor->SetStateNF (actor->FindState(NAME_Death));
		return;
	}

	if ((actor->args[3] % 4) == 0)
	{
		weaveindex = actor->special2;
		actor->z += FloatBobOffsets[weaveindex]>>1;
		actor->special2 = (weaveindex+1)&63;
	}

	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul(speed, finecosine[angle]);
	actor->momy = FixedMul(speed, finesine[angle]);
}

