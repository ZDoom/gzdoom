/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "vm.h"
*/

//
// Mancubus attack,
// firing three missiles in three different directions?
// Doesn't look like it.
//
//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

enum
{
	MSF_Standard = 0,
	MSF_Classic = 1,
	MSF_DontHurt = 2,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Mushroom)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS_DEF	(spawntype, AActor)	
	PARAM_INT_DEF	(n)					
	PARAM_INT_DEF	(flags)				
	PARAM_FLOAT_DEF	(vrange)			
	PARAM_FLOAT_DEF	(hrange)			

	int i, j;

	if (n == 0)
	{
		n = self->GetMissileDamage(0, 1);
	}
	if (spawntype == NULL)
	{
		spawntype = PClass::FindActor("FatShot");
	}

	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, (flags & MSF_DontHurt) ? 0 : RADF_HURTSOURCE);
	P_CheckSplash(self, 128.);

	// Now launch mushroom cloud
	AActor *target = Spawn("Mapspot", self->Pos(), NO_REPLACE);	// We need something to aim at.
	AActor *master = (flags & MSF_DontHurt) ? (AActor*)(self->target) : self;
	target->Height = self->Height;
 	for (i = -n; i <= n; i += 8)
	{
 		for (j = -n; j <= n; j += 8)
		{
			AActor *mo;
			target->SetXYZ(
				self->X() + i,    // Aim in many directions from source
				self->Y() + j,
				self->Z() + (P_AproxDistance(i,j) * vrange)); // Aim up fairly high
			if ((flags & MSF_Classic) || // Flag explicitely set, or no flags and compat options
				(flags == 0 && (self->state->DefineFlags & SDF_DEHACKED) && (i_compatflags & COMPATF_MUSHROOM)))
			{	// Use old function for MBF compatibility
				mo = P_OldSpawnMissile (self, master, target, spawntype);
			}
			else // Use normal function
			{
				mo = P_SpawnMissile(self, target, spawntype, master);
			}
			if (mo != NULL)
			{	// Slow it down a bit
				mo->Vel *= hrange;
				mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
			}
		}
	}
	target->Destroy();
	return 0;
}
