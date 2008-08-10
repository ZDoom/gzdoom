#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_doomglobal.h"
#include "statnums.h"
#include "a_specialspot.h"
#include "thingdef/thingdef.h"

static FRandom pr_brainscream ("BrainScream");
static FRandom pr_brainexplode ("BrainExplode");
static FRandom pr_spawnfly ("SpawnFly");

DEFINE_ACTION_FUNCTION(AActor, A_BrainAwake)
{
	// killough 3/26/98: only generates sound now
	S_Sound (self, CHAN_VOICE, "brain/sight", 1, ATTN_NONE);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainPain)
{
	S_Sound (self, CHAN_VOICE, "brain/pain", 1, ATTN_NONE);
}

static void BrainishExplosion (fixed_t x, fixed_t y, fixed_t z)
{
	AActor *boom = Spawn("Rocket", x, y, z, NO_REPLACE);
	if (boom != NULL)
	{
		boom->DeathSound = "misc/brainexplode";
		boom->momz = pr_brainscream() << 9;

		const PClass *cls = PClass::FindClass("BossBrain");
		if (cls != NULL)
		{
			FState *state = cls->ActorInfo->FindState(NAME_Brainexplode);
			if (state != NULL)
				boom->SetState (state);

		}
		boom->effects = 0;
		boom->Damage = 0;	// disables collision detection which is not wanted here
		boom->tics -= pr_brainscream() & 7;
		if (boom->tics < 1)
			boom->tics = 1;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainScream)
{
	fixed_t x;
		
	for (x = self->x - 196*FRACUNIT; x < self->x + 320*FRACUNIT; x += 8*FRACUNIT)
	{
		BrainishExplosion (x, self->y - 320*FRACUNIT,
			128 + (pr_brainscream() << (FRACBITS + 1)));
	}
	S_Sound (self, CHAN_VOICE, "brain/death", 1, ATTN_NONE);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainExplode)
{
	fixed_t x = self->x + pr_brainexplode.Random2()*2048;
	fixed_t z = 128 + pr_brainexplode()*2*FRACUNIT;
	BrainishExplosion (x, self->y, z);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainDie)
{
	// [RH] If noexit, then don't end the level.
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0, false);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainSpit)
{
	DSpotState *state = DSpotState::GetSpotState();
	AActor *targ;
	AActor *spit;

	// shoot a cube at current target
	targ = state->GetNextInList(PClass::FindClass("BossTarget"), G_SkillProperty(SKILLP_EasyBossBrain));

	if (targ != NULL)
	{
		const PClass *spawntype = NULL;
		int index = CheckIndex (1, NULL);
		if (index >= 0) spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
		if (spawntype == NULL) spawntype = PClass::FindClass("SpawnShot");

		// spawn brain missile
		spit = P_SpawnMissile (self, targ, spawntype);

		if (spit != NULL)
		{
			spit->target = targ;
			spit->master = self;
			// [RH] Do this correctly for any trajectory. Doom would divide by 0
			// if the target had the same y coordinate as the spitter.
			if ((spit->momx | spit->momy) == 0)
			{
				spit->reactiontime = 0;
			}
			else if (abs(spit->momy) > abs(spit->momx))
			{
				spit->reactiontime = (targ->y - self->y) / spit->momy;
			}
			else
			{
				spit->reactiontime = (targ->x - self->x) / spit->momx;
			}
			// [GZ] Calculates when the projectile will have reached destination
			spit->reactiontime += level.maptime;
		}

		if (index >= 0)
		{
			S_Sound(self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NONE);
		}
		else
		{
			// compatibility fallback
			S_Sound (self, CHAN_WEAPON, "brain/spit", 1, ATTN_NONE);
		}
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_SpawnFly)
{
	AActor *newmobj;
	AActor *fog;
	AActor *targ;
	int r;
		
	// [GZ] Should be more fiable than a countdown...
	if (self->reactiontime > level.maptime)
		return; // still flying
		
	targ = self->target;


	const PClass *spawntype = NULL;
	int index = CheckIndex (1, NULL);
		// First spawn teleport fire.
	if (index >= 0) 
	{
		spawntype = PClass::FindClass ((ENamedName)StateParameters[index]);
		if (spawntype != NULL) 
		{
			fog = Spawn (spawntype, targ->x, targ->y, targ->z, ALLOW_REPLACE);
			if (fog != NULL) S_Sound (fog, CHAN_BODY, fog->SeeSound, 1, ATTN_NORM);
		}
	}
	else
	{
		fog = Spawn("SpawnFire", targ->x, targ->y, targ->z, ALLOW_REPLACE);
		if (fog != NULL) S_Sound (fog, CHAN_BODY, "brain/spawn", 1, ATTN_NORM);
	}

	FName SpawnName;

	if (self->master != NULL)
	{
		FDropItem *di;   // di will be our drop item list iterator
		FDropItem *drop; // while drop stays as the reference point.
		int n=0;

		drop = di = GetDropItems(self->master->GetClass());
		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					if (di->amount < 0) di->amount = 1; // default value is -1, we need a positive value.
					n += di->amount; // this is how we can weight the list.
					di = di->Next;
				}
			}
			di = drop;
			n = pr_spawnfly(n);
			while (n > 0)
			{
				if (di->Name != NAME_None)
				{
					n -= di->amount; // logically, none of the -1 values have survived by now.
					if (n > -1) di = di->Next; // If we get into the negatives, we've reached the end of the list.
				}
			}

			SpawnName = di->Name;
		}
	}
	if (SpawnName == NAME_None)
	{
		const char *type;
		// Randomly select monster to spawn.
		r = pr_spawnfly ();

		// Probability distribution (kind of :),
		// decreasing likelihood.
			 if (r < 50)  type = "DoomImp";
		else if (r < 90)  type = "Demon";
		else if (r < 120) type = "Spectre";
		else if (r < 130) type = "PainElemental";
		else if (r < 160) type = "Cacodemon";
		else if (r < 162) type = "Archvile";
		else if (r < 172) type = "Revenant";
		else if (r < 192) type = "Arachnotron";
		else if (r < 222) type = "Fatso";
		else if (r < 246) type = "HellKnight";
		else			  type = "BaronOfHell";

		SpawnName = type;
	}
	spawntype = PClass::FindClass(SpawnName);
	if (spawntype != NULL)
	{
		newmobj = Spawn (spawntype, targ->x, targ->y, targ->z, ALLOW_REPLACE);
		if (newmobj != NULL)
		{
			// Make the new monster hate what the boss eye hates
			AActor *eye = self->target;

			if (eye != NULL)
			{
				newmobj->CopyFriendliness (eye, false);
			}
			if (newmobj->SeeState != NULL && P_LookForPlayers (newmobj, true))
				newmobj->SetState (newmobj->SeeState);

			if (!(newmobj->ObjectFlags & OF_EuthanizeMe))
			{
				// telefrag anything in this spot
				P_TeleportMove (newmobj, newmobj->x, newmobj->y, newmobj->z, true);
			}
		}
	}

	// remove self (i.e., cube).
	self->Destroy ();
}

// travelling cube sound
DEFINE_ACTION_FUNCTION(AActor, A_SpawnSound)
{
	S_Sound (self, CHAN_BODY, "brain/cube", 1, ATTN_IDLE);
	CALL_ACTION(A_SpawnFly, self);
}
