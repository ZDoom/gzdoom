#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_doomglobal.h"
#include "statnums.h"

void A_Fire (AActor *);		// from m_archvile.cpp

void A_BrainAwake (AActor *);
void A_BrainPain (AActor *);
void A_BrainScream (AActor *);
void A_BrainExplode (AActor *);
void A_BrainDie (AActor *);
void A_BrainSpit (AActor *);
void A_SpawnFly (AActor *);
void A_SpawnSound (AActor *);

static FRandom pr_brainscream ("BrainScream");
static FRandom pr_brainexplode ("BrainExplode");
static FRandom pr_spawnfly ("SpawnFly");

class ABossTarget : public AActor
{
	DECLARE_STATELESS_ACTOR (ABossTarget, AActor)
public:
	void BeginPlay ();
};

class DBrainState : public DThinker
{
	DECLARE_CLASS (DBrainState, DThinker)
public:
	DBrainState ()
		: DThinker (STAT_BOSSTARGET),
		  Targets (STAT_BOSSTARGET),
		  SerialTarget (NULL),
		  Easy (false)
	{}
	void Serialize (FArchive &arc);
	ABossTarget *GetTarget ();
protected:
	TThinkerIterator<ABossTarget> Targets;
	ABossTarget *SerialTarget;
	bool Easy;
};

FState ABossBrain::States[] =
{
#define S_BRAINEXPLODE 0
	S_BRIGHT (MISL, 'B',   10, NULL 			, &States[S_BRAINEXPLODE+1]),
	S_BRIGHT (MISL, 'C',   10, NULL 			, &States[S_BRAINEXPLODE+2]),
	S_BRIGHT (MISL, 'D',   10, A_BrainExplode	, NULL),

#define S_BRAIN (S_BRAINEXPLODE+3)
	S_NORMAL (BBRN, 'A',   -1, NULL 			, NULL),

#define S_BRAIN_PAIN (S_BRAIN+1)
	S_NORMAL (BBRN, 'B',   36, A_BrainPain		, &States[S_BRAIN]),

#define S_BRAIN_DIE (S_BRAIN_PAIN+1)
	S_NORMAL (BBRN, 'A',  100, A_BrainScream	, &States[S_BRAIN_DIE+1]),
	S_NORMAL (BBRN, 'A',   10, NULL 			, &States[S_BRAIN_DIE+2]),
	S_NORMAL (BBRN, 'A',   10, NULL 			, &States[S_BRAIN_DIE+3]),
	S_NORMAL (BBRN, 'A',   -1, A_BrainDie		, NULL)
};

IMPLEMENT_ACTOR (ABossBrain, Doom, 88, 0)
	PROP_SpawnHealth (250)
	//PROP_HeightFixed (86)		// don't do this; it messes up some non-id levels
	PROP_MassLong (10000000)
	PROP_PainChance (255)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags4 (MF4_NOICEDEATH)
	PROP_Flags5 (MF5_OLDRADIUSDMG)

	PROP_SpawnState (S_BRAIN)
	PROP_PainState (S_BRAIN_PAIN)
	PROP_DeathState (S_BRAIN_DIE)

	PROP_PainSound ("brain/pain")
	PROP_DeathSound ("brain/death")
END_DEFAULTS

class ABossEye : public AActor
{
	DECLARE_ACTOR (ABossEye, AActor)
};

FState ABossEye::States[] =
{
#define S_BRAINEYE 0
	S_NORMAL (SSWV, 'A',   10, A_Look						, &States[S_BRAINEYE]),

#define S_BRAINEYESEE (S_BRAINEYE+1)
	S_NORMAL (SSWV, 'A',  181, A_BrainAwake 				, &States[S_BRAINEYESEE+1]),
	S_NORMAL (SSWV, 'A',  150, A_BrainSpit					, &States[S_BRAINEYESEE+1])
};

IMPLEMENT_ACTOR (ABossEye, Doom, 89, 0)
	PROP_HeightFixed (32)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)

	PROP_SpawnState (S_BRAINEYE)
	PROP_SeeState (S_BRAINEYESEE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABossTarget, Doom, 87, 0)
	PROP_HeightFixed (32)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
END_DEFAULTS

void ABossTarget::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_BOSSTARGET);
}

class ASpawnShot : public AActor
{
	DECLARE_ACTOR (ASpawnShot, AActor)
};

FState ASpawnShot::States[] =
{
	S_BRIGHT (BOSF, 'A',	3, A_SpawnSound 				, &States[1]),
	S_BRIGHT (BOSF, 'B',	3, A_SpawnFly					, &States[2]),
	S_BRIGHT (BOSF, 'C',	3, A_SpawnFly					, &States[3]),
	S_BRIGHT (BOSF, 'D',	3, A_SpawnFly					, &States[0])
};

IMPLEMENT_ACTOR (ASpawnShot, Doom, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (32)
	PROP_SpeedFixed (10)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY|MF_NOCLIP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)

	PROP_SpawnState (0)

	PROP_SeeSound ("brain/spit")
	PROP_DeathSound ("brain/cubeboom")
END_DEFAULTS

class ASpawnFire : public AActor
{
	DECLARE_ACTOR (ASpawnFire, AActor)
};

FState ASpawnFire::States[] =
{
	S_BRIGHT (FIRE, 'A',	4, A_Fire						, &States[1]),
	S_BRIGHT (FIRE, 'B',	4, A_Fire						, &States[2]),
	S_BRIGHT (FIRE, 'C',	4, A_Fire						, &States[3]),
	S_BRIGHT (FIRE, 'D',	4, A_Fire						, &States[4]),
	S_BRIGHT (FIRE, 'E',	4, A_Fire						, &States[5]),
	S_BRIGHT (FIRE, 'F',	4, A_Fire						, &States[6]),
	S_BRIGHT (FIRE, 'G',	4, A_Fire						, &States[7]),
	S_BRIGHT (FIRE, 'H',	4, A_Fire						, NULL)
};

IMPLEMENT_ACTOR (ASpawnFire, Doom, -1, 0)
	PROP_HeightFixed (78)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)
END_DEFAULTS

void A_BrainAwake (AActor *self)
{
	// killough 3/26/98: only generates sound now
	S_Sound (self, CHAN_VOICE, "brain/sight", 1, ATTN_SURROUND);
}

void A_BrainPain (AActor *self)
{
	S_Sound (self, CHAN_VOICE, "brain/pain", 1, ATTN_SURROUND);
}

static void BrainishExplosion (fixed_t x, fixed_t y, fixed_t z)
{
	AActor *boom = Spawn("Rocket", x, y, z, NO_REPLACE);
	if (boom != NULL)
	{
		boom->momz = pr_brainscream() << 9;
		boom->SetState (&ABossBrain::States[S_BRAINEXPLODE]);
		boom->effects = 0;
		boom->Damage = 0;	// disables collision detection which is not wanted here
		boom->tics -= pr_brainscream() & 7;
		if (boom->tics < 1)
			boom->tics = 1;
	}
}

void A_BrainScream (AActor *self)
{
	fixed_t x;
		
	for (x = self->x - 196*FRACUNIT; x < self->x + 320*FRACUNIT; x += 8*FRACUNIT)
	{
		BrainishExplosion (x, self->y - 320*FRACUNIT,
			128 + (pr_brainscream() << (FRACBITS + 1)));
	}
	S_Sound (self, CHAN_VOICE, "brain/death", 1, ATTN_SURROUND);
}

void A_BrainExplode (AActor *self)
{
	fixed_t x = self->x + pr_brainexplode.Random2()*2048;
	fixed_t z = 128 + pr_brainexplode()*2*FRACUNIT;
	BrainishExplosion (x, self->y, z);
}

void A_BrainDie (AActor *self)
{
	// [RH] If noexit, then don't end the level.
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0, false);
}

void A_BrainSpit (AActor *self)
{
	TThinkerIterator<DBrainState> iterator (STAT_BOSSTARGET);
	DBrainState *state;
	AActor *targ;
	AActor *spit;

	// shoot a cube at current target
	if (NULL == (state = iterator.Next ()))
	{
		state = new DBrainState;
	}
	targ = state->GetTarget ();

	if (targ != NULL)
	{
		// spawn brain missile
		spit = P_SpawnMissile (self, targ, RUNTIME_CLASS(ASpawnShot));

		if (spit != NULL)
		{
			spit->target = targ;
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
			spit->reactiontime /= spit->state->GetTics();
		}

		S_Sound (self, CHAN_WEAPON, "brain/spit", 1, ATTN_SURROUND);
	}
}

void A_SpawnFly (AActor *self)
{
	AActor *newmobj;
	AActor *fog;
	AActor *targ;
	int r;
	const char *type;
		
	if (--self->reactiontime)
		return; // still flying
		
	targ = self->target;

	// First spawn teleport fire.
	fog = Spawn<ASpawnFire> (targ->x, targ->y, targ->z, ALLOW_REPLACE);
	S_Sound (fog, CHAN_BODY, "brain/spawn", 1, ATTN_NORM);

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

	newmobj = Spawn (type, targ->x, targ->y, targ->z, ALLOW_REPLACE);
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

		if (!(newmobj->ObjectFlags & OF_MassDestruction))
		{
			// telefrag anything in this spot
			P_TeleportMove (newmobj, newmobj->x, newmobj->y, newmobj->z, true);
		}
	}

	// remove self (i.e., cube).
	self->Destroy ();
}

// travelling cube sound
void A_SpawnSound (AActor *self)	
{
	S_Sound (self, CHAN_BODY, "brain/cube", 1, ATTN_IDLE);
	A_SpawnFly (self);
}

// Each brain on the level shares a single global state
IMPLEMENT_CLASS (DBrainState)

ABossTarget *DBrainState::GetTarget ()
{
	Easy = !Easy;

	if (G_SkillProperty(SKILLP_EasyBossBrain) && !Easy)
		return NULL;

	ABossTarget *target;

	if (SerialTarget)
	{
		do
		{
			target = Targets.Next ();
		} while (target != NULL && target != SerialTarget);
		SerialTarget = NULL;
	}
	else
	{
		target = Targets.Next ();
	}
	return (target == NULL) ? Targets.Next () : target;
}

void DBrainState::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Easy;
	if (arc.IsStoring ())
	{
		ABossTarget *target = Targets.Next ();
		arc << target;
	}
	else
	{
		arc << SerialTarget;
	}
}
