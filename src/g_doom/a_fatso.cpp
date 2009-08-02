/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

//
// Mancubus attack,
// firing three missiles in three different directions?
// Doesn't look like it.
//
#define FATSPREAD (ANG90/8)

DEFINE_ACTION_FUNCTION(AActor, A_FatRaise)
{
	A_FaceTarget (self);
	S_Sound (self, CHAN_WEAPON, "fatso/raiseguns", 1, ATTN_NORM);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FatAttack1)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

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
		missile->velx = FixedMul (missile->Speed, finecosine[an]);
		missile->vely = FixedMul (missile->Speed, finesine[an]);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FatAttack2)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

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
		missile->velx = FixedMul (missile->Speed, finecosine[an]);
		missile->vely = FixedMul (missile->Speed, finesine[an]);
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FatAttack3)
{
	AActor *missile;
	angle_t an;

	if (!self->target)
		return;

	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	A_FaceTarget (self);
	
	missile = P_SpawnMissile (self, self->target, spawntype);
	if (missile != NULL)
	{
		missile->angle -= FATSPREAD/2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->velx = FixedMul (missile->Speed, finecosine[an]);
		missile->vely = FixedMul (missile->Speed, finesine[an]);
	}

	missile = P_SpawnMissile (self, self->target, spawntype);
	if (missile != NULL)
	{
		missile->angle += FATSPREAD/2;
		an = missile->angle >> ANGLETOFINESHIFT;
		missile->velx = FixedMul (missile->Speed, finecosine[an]);
		missile->vely = FixedMul (missile->Speed, finesine[an]);
	}
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

AActor * P_OldSpawnMissile(AActor * source, AActor * dest, const PClass *type);

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Mushroom)
{
	int i, j;

	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(spawntype, 0);
	ACTION_PARAM_INT(n, 1);
	ACTION_PARAM_INT(flags, 2);

	if (n == 0) n = self->GetMissileDamage (0, 1);
	if (spawntype == NULL) spawntype = PClass::FindClass("FatShot");

	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, true);
	if (self->z <= self->floorz + (128<<FRACBITS))
	{
		P_HitFloor (self);
	}

	// Now launch mushroom cloud
	AActor *target = Spawn("Mapspot", 0, 0, 0, NO_REPLACE);	// We need something to aim at.
	target->height = self->height;
	for (i = -n; i <= n; i += 8)
	{
		for (j = -n; j <= n; j += 8)
		{
			AActor *mo;
			target->x = self->x + (i << FRACBITS); // Aim in many directions from source
			target->y = self->y + (j << FRACBITS);
			target->z = self->z + (P_AproxDistance(i,j) << (FRACBITS+2)); // Aim up fairly high
			if (flags == 0 && (self->state->Misc1 == 0 || !(i_compatflags & COMPATF_MUSHROOM)))
			{
				mo = P_SpawnMissile (self, target, spawntype); // Launch fireball
			}
			else
			{
				mo = P_OldSpawnMissile (self, target, spawntype); // Launch fireball
			}
			if (mo != NULL)
			{
				mo->velx >>= 1;
				mo->vely >>= 1;				  // Slow it down a bit
				mo->velz >>= 1;
				mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
			}
		}
	}
	target->Destroy();
}
