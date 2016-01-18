/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "statnums.h"
#include "a_specialspot.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
#include "g_level.h"
*/

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
		boom->velz = pr_brainscream() << 9;

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
		
	for (x = self->X() - 196*FRACUNIT; x < self->X() + 320*FRACUNIT; x += 8*FRACUNIT)
	{
		BrainishExplosion (x, self->Y() - 320*FRACUNIT,
			128 + (pr_brainscream() << (FRACBITS + 1)));
	}
	S_Sound (self, CHAN_VOICE, "brain/death", 1, ATTN_NONE);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainExplode)
{
	fixed_t x = self->X() + pr_brainexplode.Random2()*2048;
	fixed_t z = 128 + pr_brainexplode()*2*FRACUNIT;
	BrainishExplosion (x, self->Y(), z);
}

DEFINE_ACTION_FUNCTION(AActor, A_BrainDie)
{
	// [RH] If noexit, then don't end the level.
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
		return;

	// New dmflag: Kill all boss spawned monsters before ending the level.
	if (dmflags2 & DF2_KILLBOSSMONST)
	{
		int count;	// Repeat until we have no more boss-spawned monsters.
		do			// (e.g. Pain Elementals can spawn more to kill upon death.)
		{
			TThinkerIterator<AActor> it;
			AActor *mo;
			count = 0;
			while ((mo = it.Next()))
			{
				if (mo->health > 0 && mo->flags4 & MF4_BOSSSPAWNED)
				{
					P_DamageMobj(mo, self, self, mo->health, NAME_None, 
						DMG_NO_ARMOR|DMG_FORCED|DMG_THRUSTLESS|DMG_NO_FACTOR);
					count++;
				}
			}
		} while (count != 0);
	}

	G_ExitLevel (0, false);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BrainSpit)
{
	DSpotState *state = DSpotState::GetSpotState();
	AActor *targ;
	AActor *spit;
	bool isdefault = false;

	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	// shoot a cube at current target
	targ = state->GetNextInList(PClass::FindClass("BossTarget"), G_SkillProperty(SKILLP_EasyBossBrain));

	if (targ != NULL)
	{
		if (spawntype == NULL) 
		{
			spawntype = PClass::FindClass("SpawnShot");
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
			if ((spit->velx | spit->vely) == 0)
			{
				spit->special2 = 0;
			}
			else if (abs(spit->vely) > abs(spit->velx))
			{
				spit->special2 = (targ->Y() - self->Y()) / spit->vely;
			}
			else
			{
				spit->special2 = (targ->X() - self->X()) / spit->velx;
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
}

static void SpawnFly(AActor *self, const PClass *spawntype, FSoundID sound)
{
	AActor *newmobj;
	AActor *fog;
	AActor *eye = self->master; // The eye is the spawnshot's master, not the target!
	AActor *targ = self->target; // Unlike other projectiles, the target is the intended destination.
	int r;
		
	// [GZ] Should be more viable than a countdown...
	if (self->special2 != 0)
	{
		if (self->special2 > level.maptime)
			return;		// still flying
	}
	else
	{
		if (self->reactiontime == 0 || --self->reactiontime != 0)
			return;		// still flying
	}
	
	if (spawntype != NULL)
	{
		fog = Spawn (spawntype, targ->Pos(), ALLOW_REPLACE);
		if (fog != NULL) S_Sound (fog, CHAN_BODY, sound, 1, ATTN_NORM);
	}

	FName SpawnName;

	FDropItem *di;   // di will be our drop item list iterator
	FDropItem *drop; // while drop stays as the reference point.
	int n = 0;

	// First see if this cube has its own actor list
	drop = self->GetDropItems();

	// If not, then default back to its master's list
	if (drop == NULL && eye != NULL)
		drop = eye->GetDropItems();

	if (drop != NULL)
	{
		for (di = drop; di != NULL; di = di->Next)
		{
			if (di->Name != NAME_None)
			{
				if (di->amount < 0)
				{
					di->amount = 1; // default value is -1, we need a positive value.
				}
				n += di->amount; // this is how we can weight the list.
			}
		}
		di = drop;
		n = pr_spawnfly(n);
		while (n >= 0)
		{
			if (di->Name != NAME_None)
			{
				n -= di->amount; // logically, none of the -1 values have survived by now.
			}
			if ((di->Next != NULL) && (n >= 0))
			{
				di = di->Next;
			}
			else
			{
				n = -1;
			}
		}
		SpawnName = di->Name;
	}
	if (SpawnName == NAME_None)
	{
		// Randomly select monster to spawn.
		r = pr_spawnfly ();

		// Probability distribution (kind of :),
		// decreasing likelihood.
			 if (r < 50)  SpawnName = "DoomImp";
		else if (r < 90)  SpawnName = "Demon";
		else if (r < 120) SpawnName = "Spectre";
		else if (r < 130) SpawnName = "PainElemental";
		else if (r < 160) SpawnName = "Cacodemon";
		else if (r < 162) SpawnName = "Archvile";
		else if (r < 172) SpawnName = "Revenant";
		else if (r < 192) SpawnName = "Arachnotron";
		else if (r < 222) SpawnName = "Fatso";
		else if (r < 246) SpawnName = "HellKnight";
		else			  SpawnName = "BaronOfHell";
	}
	spawntype = PClass::FindClass(SpawnName);
	if (spawntype != NULL)
	{
		newmobj = Spawn (spawntype, targ->Pos(), ALLOW_REPLACE);
		if (newmobj != NULL)
		{
			// Make the new monster hate what the boss eye hates
			if (eye != NULL)
			{
				newmobj->CopyFriendliness (eye, false);
			}
			// Make it act as if it was around when the player first made noise
			// (if the player has made noise).
			newmobj->LastHeard = newmobj->Sector->SoundTarget;

			if (newmobj->SeeState != NULL && P_LookForPlayers (newmobj, true, NULL))
			{
				newmobj->SetState (newmobj->SeeState);
			}
			if (!(newmobj->ObjectFlags & OF_EuthanizeMe))
			{
				// telefrag anything in this spot
				P_TeleportMove (newmobj, newmobj->Pos(), true);
			}
			newmobj->flags4 |= MF4_BOSSSPAWNED;
		}
	}

	// remove self (i.e., cube).
	self->Destroy ();
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnFly)
{
	FSoundID sound;

	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	if (spawntype != NULL) 
	{
		sound = GetDefaultByType(spawntype)->SeeSound;
	}
	else
	{
		spawntype = PClass::FindClass ("SpawnFire");
		sound = "brain/spawn";
	}
	SpawnFly(self, spawntype, sound);
}

// travelling cube sound
DEFINE_ACTION_FUNCTION(AActor, A_SpawnSound)
{
	S_Sound (self, CHAN_BODY, "brain/cube", 1, ATTN_IDLE);
	SpawnFly(self, PClass::FindClass("SpawnFire"), "brain/spawn");
}
