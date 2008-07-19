#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"

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

	const PClass *spawntype = NULL;
	int index = CheckIndex (1, NULL);
	if (index >= 0) spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	A_FaceTarget (self);
	// Change direction  to ...
	self->angle += FATSPREAD;
	P_SpawnMissile (self, self->target, spawntype);

	missile = P_SpawnMissile (self, self->target, spawntype);
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

	const PClass *spawntype = NULL;
	int index = CheckIndex (1, NULL);
	if (index >= 0) spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	A_FaceTarget (self);
	// Now here choose opposite deviation.
	self->angle -= FATSPREAD;
	P_SpawnMissile (self, self->target, spawntype);

	missile = P_SpawnMissile (self, self->target, spawntype);
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

	const PClass *spawntype = NULL;
	int index = CheckIndex (1, NULL);
	if (index >= 0) spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	A_FaceTarget (self);
	
	missile = P_SpawnMissile (self, self->target, spawntype);
	if (missile != NULL)
	{
		missile->angle -= FATSPREAD/2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->momx = FixedMul (missile->Speed, finecosine[an]);
		missile->momy = FixedMul (missile->Speed, finesine[an]);
	}

	missile = P_SpawnMissile (self, self->target, spawntype);
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
	int i, j, n = actor->GetMissileDamage (0, 1);

	const PClass *spawntype = NULL;
	int index = CheckIndex (2, NULL);
	if (index >= 0) 
	{
		spawntype = PClass::FindClass((ENamedName)StateParameters[index]);
		n = EvalExpressionI (StateParameters[index+1], actor);
		if (n == 0)
			n = actor->GetMissileDamage (0, 1);
	}
	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	A_Explode (actor);	// First make normal explosion

	// Now launch mushroom cloud
	AActor *target = Spawn("Mapspot", 0, 0, 0, NO_REPLACE);	// We need something to aim at.
	target->height = actor->height;
	for (i = -n; i <= n; i += 8)
	{
		for (j = -n; j <= n; j += 8)
		{
			AActor *mo;
			target->x = actor->x + (i << FRACBITS); // Aim in many directions from source
			target->y = actor->y + (j << FRACBITS);
			target->z = actor->z + (P_AproxDistance(i,j) << (FRACBITS+2)); // Aim up fairly high
			mo = P_SpawnMissile (actor, target, spawntype); // Launch fireball
			if (mo != NULL)
			{
				mo->momx >>= 1;
				mo->momy >>= 1;				  // Slow it down a bit
				mo->momz >>= 1;
				mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
			}
		}
	}
	target->Destroy();
}
