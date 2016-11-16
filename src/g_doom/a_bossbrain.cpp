/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "statnums.h"
#include "a_specialspot.h"
#include "vm.h"
#include "doomstat.h"
#include "g_level.h"
*/

static FRandom pr_spawnfly ("SpawnFly");

DEFINE_ACTION_FUNCTION(AActor, A_BrainSpit)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS_DEF(spawntype, AActor);

	DSpotState *state = DSpotState::GetSpotState();
	AActor *targ;
	AActor *spit;
	bool isdefault = false;

	// shoot a cube at current target
	targ = state->GetNextInList(PClass::FindActor("BossTarget"), G_SkillProperty(SKILLP_EasyBossBrain));

	if (targ != NULL)
	{
		if (spawntype == NULL) 
		{
			spawntype = PClass::FindActor("SpawnShot");
			isdefault = true;
		}

		// spawn brain missile
		spit = P_SpawnMissile (self, targ, spawntype);

		if (spit != NULL)
		{
			// Boss cubes should move freely to their destination so it's
			// probably best to disable all collision detection for them.
			if (spit->flags & MF_NOCLIP) spit->flags5 |= MF5_NOINTERACTION;
	
			spit->target = targ;
			spit->master = self;
			// [RH] Do this correctly for any trajectory. Doom would divide by 0
			// if the target had the same y coordinate as the spitter.
			if (spit->Vel.X == 0 && spit->Vel.Y == 0)
			{
				spit->special2 = 0;
			}
			else if (fabs(spit->Vel.Y) > fabs(spit->Vel.X))
			{
				spit->special2 = int((targ->Y() - self->Y()) / spit->Vel.Y);
			}
			else
			{
				spit->special2 = int((targ->X() - self->X()) / spit->Vel.X);
			}
			// [GZ] Calculates when the projectile will have reached destination
			spit->special2 += level.maptime;
			spit->flags6 |= MF6_BOSSCUBE;
		}

		if (!isdefault)
		{
			S_Sound(self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NONE);
		}
		else
		{
			// compatibility fallback
			S_Sound (self, CHAN_WEAPON, "brain/spit", 1, ATTN_NONE);
		}
	}
	return 0;
}

