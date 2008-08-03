#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"

const fixed_t FLAMESPEED	= fixed_t(0.45*FRACUNIT);
const fixed_t CFLAMERANGE	= 12*64*FRACUNIT;
const fixed_t FLAMEROTSPEED	= 2*FRACUNIT;

static FRandom pr_missile ("CFlameMissile");

void A_CFlameAttack (AActor *);
void A_CFlameRotate (AActor *);
void A_CFlamePuff (AActor *);
void A_CFlameMissile (AActor *);

// Flame Missile ------------------------------------------------------------

class ACFlameMissile : public AActor
{
	DECLARE_CLASS (ACFlameMissile, AActor)
public:
	void BeginPlay ();
	void Tick ();
};

IMPLEMENT_CLASS (ACFlameMissile)

void ACFlameMissile::BeginPlay ()
{
	special1 = 2;
}

void ACFlameMissile::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	fixed_t newz;
	bool changexy;
	AActor *mo;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// Handle movement
	if (momx || momy ||	(z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac || yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				if (!P_TryMove (this, x+xfrac, y+yfrac, true))
				{ // Blocked move
					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					return;
				}
			}
			z += zfrac;
			if (z <= floorz)
			{ // Hit the floor
				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (z+height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz-height;
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy)
			{
				if (!--special1)
				{
					special1 = 4;
					newz = z-12*FRACUNIT;
					if (newz < floorz)
					{
						newz = floorz;
					}
					mo = Spawn ("CFlameFloor", x, y, newz, ALLOW_REPLACE);
					if (mo)
					{
						mo->angle = angle;
					}
				}
			}
		}
	}
	// Advance the state
	if (tics != -1)
	{
		tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState ()))
			{ // mobj was removed
				return;
			}
		}
	}
}

//============================================================================
//
// A_CFlameAttack
//
//============================================================================

void A_CFlameAttack (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ACFlameMissile));
	S_Sound (actor, CHAN_WEAPON, "ClericFlameFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CFlamePuff
//
//============================================================================

void A_CFlamePuff (AActor *actor)
{
	A_UnHideThing (actor);
	actor->momx = 0;
	actor->momy = 0;
	actor->momz = 0;
	S_Sound (actor, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
}

//============================================================================
//
// A_CFlameMissile
//
//============================================================================

void A_CFlameMissile (AActor *actor)
{
	int i;
	int an, an90;
	fixed_t dist;
	AActor *mo;
	
	A_UnHideThing (actor);
	S_Sound (actor, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
	AActor *BlockingMobj = actor->BlockingMobj;
	if (BlockingMobj && BlockingMobj->flags&MF_SHOOTABLE)
	{ // Hit something, so spawn the flame circle around the thing
		dist = BlockingMobj->radius+18*FRACUNIT;
		for (i = 0; i < 4; i++)
		{
			an = (i*ANG45)>>ANGLETOFINESHIFT;
			an90 = (i*ANG45+ANG90)>>ANGLETOFINESHIFT;
			mo = Spawn ("CircleFlame", BlockingMobj->x+FixedMul(dist, finecosine[an]),
				BlockingMobj->y+FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if (mo)
			{
				mo->angle = an<<ANGLETOFINESHIFT;
				mo->target = actor->target;
				mo->momx = mo->special1 = FixedMul(FLAMESPEED, finecosine[an]);
				mo->momy = mo->special2 = FixedMul(FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;
			}
			mo = Spawn ("CircleFlame", BlockingMobj->x-FixedMul(dist, finecosine[an]),
				BlockingMobj->y-FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if(mo)
			{
				mo->angle = ANG180+(an<<ANGLETOFINESHIFT);
				mo->target = actor->target;
				mo->momx = mo->special1 = FixedMul(-FLAMESPEED, finecosine[an]);
				mo->momy = mo->special2 = FixedMul(-FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;
			}
		}
		actor->SetState (actor->SpawnState);
	}
}

//============================================================================
//
// A_CFlameRotate
//
//============================================================================

void A_CFlameRotate (AActor *actor)
{
	int an;

	an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	actor->momx = actor->special1+FixedMul(FLAMEROTSPEED, finecosine[an]);
	actor->momy = actor->special2+FixedMul(FLAMEROTSPEED, finesine[an]);
	actor->angle += ANG90/15;
}
