/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "a_hexenglobal.h"
#include "gstrings.h"
#include "a_weaponpiece.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/

#define BLAST_FULLSTRENGTH	255

static FRandom pr_holyatk2 ("CHolyAtk2");
static FRandom pr_holyseeker ("CHolySeeker");
static FRandom pr_holyweave ("CHolyWeave");
static FRandom pr_holyseek ("CHolySeek");
static FRandom pr_checkscream ("CCheckScream");
static FRandom pr_spiritslam ("CHolySlam");
static FRandom pr_wraithvergedrop ("WraithvergeDrop");

void SpawnSpiritTail (AActor *spirit);

//==========================================================================
// Cleric's Wraithverge (Holy Symbol?) --------------------------------------

class ACWeapWraithverge : public AClericWeapon
{
	DECLARE_CLASS (ACWeapWraithverge, AClericWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << CHolyCount;
	}
	PalEntry GetBlend ()
	{
		if (paletteflash & PF_HEXENWEAPONS)
		{
			if (CHolyCount == 3)
				return PalEntry(128, 70, 70, 70);
			else if (CHolyCount == 2)
				return PalEntry(128, 100, 100, 100);
			else if (CHolyCount == 1)
				return PalEntry(128, 130, 130, 130);
			else
				return PalEntry(0, 0, 0, 0);
		}
		else
		{
			return PalEntry (CHolyCount * 128 / 3, 131, 131, 131);
		}
	}
	BYTE CHolyCount;
};

IMPLEMENT_CLASS (ACWeapWraithverge)

// Holy Spirit --------------------------------------------------------------

IMPLEMENT_CLASS (AHolySpirit)

bool AHolySpirit::Slam(AActor *thing)
{
	if (thing->flags&MF_SHOOTABLE && thing != target)
	{
		if (multiplayer && !deathmatch && thing->player && target->player)
		{ // don't attack other co-op players
			return true;
		}
		if (thing->flags2&MF2_REFLECTIVE
			&& (thing->player || thing->flags2&MF2_BOSS))
		{
			tracer = target;
			target = thing;
			return true;
		}
		if (thing->flags3&MF3_ISMONSTER || thing->player)
		{
			tracer = thing;
		}
		if (pr_spiritslam() < 96)
		{
			int dam = 12;
			if (thing->player || thing->flags2&MF2_BOSS)
			{
				dam = 3;
				// ghost burns out faster when attacking players/bosses
				health -= 6;
			}
			P_DamageMobj(thing, this, target, dam, NAME_Melee);
			if (pr_spiritslam() < 128)
			{
				Spawn("HolyPuff", Pos(), ALLOW_REPLACE);
				S_Sound(this, CHAN_WEAPON, "SpiritAttack", 1, ATTN_NORM);
				if (thing->flags3&MF3_ISMONSTER && pr_spiritslam() < 128)
				{
					thing->Howl();
				}
			}
		}
		if (thing->health <= 0)
		{
			tracer = NULL;
		}
	}
	return true;
}

bool AHolySpirit::SpecialBlastHandling (AActor *source, double strength)
{
	if (tracer == source)
	{
		tracer = target;
		target = source;
		GC::WriteBarrier(this, source);
	}
	return true;
}

//============================================================================
//
// A_CHolyAttack2 
//
// 	Spawns the spirits
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyAttack2)
{
	PARAM_ACTION_PROLOGUE;

	int j;
	AActor *mo;

	for (j = 0; j < 4; j++)
	{
		mo = Spawn<AHolySpirit> (self->Pos(), ALLOW_REPLACE);
		if (!mo)
		{
			continue;
		}
		switch (j)
		{ // float bob index

			case 0:
				mo->WeaveIndexZ = pr_holyatk2() & 7; // upper-left
				break;
			case 1:
				mo->WeaveIndexZ = 32 + (pr_holyatk2() & 7); // upper-right
				break;
			case 2:
				mo->WeaveIndexXY = 32 + (pr_holyatk2() & 7); // lower-left
				break;
			case 3:
				mo->WeaveIndexXY = 32 + (pr_holyatk2() & 7);
				mo->WeaveIndexZ = 32 + (pr_holyatk2() & 7);
				break;
		}
		mo->SetZ(self->Z());
		mo->Angles.Yaw = self->Angles.Yaw + 67.5 - 45.*j;
		mo->Thrust();
		mo->target = self->target;
		mo->args[0] = 10; // initial turn value
		mo->args[1] = 0; // initial look angle
		if (deathmatch)
		{ // Ghosts last slightly less longer in DeathMatch
			mo->health = 85;
		}
		if (self->tracer)
		{
			mo->tracer = self->tracer;
			mo->flags |= MF_NOCLIP|MF_SKULLFLY;
			mo->flags &= ~MF_MISSILE;
		}
		SpawnSpiritTail (mo);
	}
	return 0;
}

//============================================================================
//
// SpawnSpiritTail
//
//============================================================================

void SpawnSpiritTail (AActor *spirit)
{
	AActor *tail, *next;
	int i;
	
	tail = Spawn ("HolyTail", spirit->Pos(), ALLOW_REPLACE);
	tail->target = spirit; // parent
	for (i = 1; i < 3; i++)
	{
		next = Spawn ("HolyTailTrail", spirit->Pos(), ALLOW_REPLACE);
		tail->tracer = next;
		tail = next;
	}
	tail->tracer = NULL; // last tail bit
}

//============================================================================
//
// A_CHolyAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyAttack)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	ACWeapWraithverge *weapon = static_cast<ACWeapWraithverge *> (self->player->ReadyWeapon);
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	AActor *missile = P_SpawnPlayerMissile (self, 0,0,0, PClass::FindActor("HolyMissile"), self->Angles.Yaw, &t);
	if (missile != NULL && !t.unlinked)
	{
		missile->tracer = t.linetarget;
	}

	weapon->CHolyCount = 3;
	S_Sound (self, CHAN_WEAPON, "HolySymbolFire", 1, ATTN_NORM);
	return 0;
}

//============================================================================
//
// A_CHolyPalette
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyPalette)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != NULL)
	{
		ACWeapWraithverge *weapon = static_cast<ACWeapWraithverge *> (self->player->ReadyWeapon);
		if (weapon != NULL && weapon->CHolyCount != 0)
		{
			weapon->CHolyCount--;
		}
	}
	return 0;
}

//============================================================================
//
// CHolyTailFollow
//
//============================================================================

static void CHolyTailFollow(AActor *actor, double dist)
{
	AActor *child;
	DAngle an;
	double oldDistance, newDistance;

	while (actor)
	{
		child = actor->tracer;
		if (child)
		{
			an = actor->AngleTo(child);
			oldDistance = child->Distance2D(actor);
			if (P_TryMove(child, actor->Pos().XY() + an.ToVector(dist), true))
			{
				newDistance = child->Distance2D(actor) - 1;
				if (oldDistance < 1)
				{
					if (child->Z() < actor->Z())
					{
						child->SetZ(actor->Z() - dist);
					}
					else
					{
						child->SetZ(actor->Z() + dist);
					}
				}
				else
				{
					child->SetZ(actor->Z() + (newDistance * (child->Z() - actor->Z()) / oldDistance));
				}
			}
		}
		actor = child;
		dist -= 1;
	}
}

//============================================================================
//
// CHolyTailRemove
//
//============================================================================

static void CHolyTailRemove (AActor *actor)
{
	AActor *next;

	while (actor)
	{
		next = actor->tracer;
		actor->Destroy ();
		actor = next;
	}
}

//============================================================================
//
// A_CHolyTail
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyTail)
{
	PARAM_ACTION_PROLOGUE;

	AActor *parent;

	parent = self->target;

	if (parent == NULL || parent->health <= 0)	// better check for health than current state - it's safer!
	{ // Ghost removed, so remove all tail parts
		CHolyTailRemove (self);
		return 0;
	}
	else
	{
		if (P_TryMove(self, parent->Vec2Angle(14., parent->Angles.Yaw, true), true))
		{
			self->SetZ(parent->Z() - 5.);
		}
		CHolyTailFollow(self, 10);
	}
	return 0;
}

//============================================================================
//
// CHolyFindTarget
//
//============================================================================

static void CHolyFindTarget (AActor *actor)
{
	AActor *target;

	if ( (target = P_RoughMonsterSearch (actor, 6, true)) )
	{
		actor->tracer = target;
		actor->flags |= MF_NOCLIP|MF_SKULLFLY;
		actor->flags &= ~MF_MISSILE;
	}
}

//============================================================================
//
// CHolySeekerMissile
//
// 	 Similar to P_SeekerMissile, but seeks to a random Z on the target
//============================================================================

static void CHolySeekerMissile (AActor *actor, DAngle thresh, DAngle turnMax)
{
	int dir;
	DAngle delta;
	AActor *target;
	double newZ;
	double deltaZ;

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	if (!(target->flags&MF_SHOOTABLE)
		|| (!(target->flags3&MF3_ISMONSTER) && !target->player))
	{ // Target died/target isn't a player or creature
		actor->tracer = NULL;
		actor->flags &= ~(MF_NOCLIP | MF_SKULLFLY);
		actor->flags |= MF_MISSILE;
		CHolyFindTarget(actor);
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta /= 2;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->Angles.Yaw += delta;
	}
	else
	{ // Turn counter clockwise
		actor->Angles.Yaw -= delta;
	}
	actor->VelFromAngle();

	if (!(level.time&15) 
		|| actor->Z() > target->Top()
		|| actor->Top() < target->Z())
	{
		newZ = target->Z() + ((pr_holyseeker()*target->Height) / 256.);
		deltaZ = newZ - actor->Z();
		if (fabs(deltaZ) > 15)
		{
			if (deltaZ > 0)
			{
				deltaZ = 15;
			}
			else
			{
				deltaZ = -15;
			}
		}
		actor->Vel.Z = deltaZ / actor->DistanceBySpeed(target, actor->Speed);
	}
	return;
}

//============================================================================
//
// A_CHolySeek
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolySeek)
{
	PARAM_ACTION_PROLOGUE;

	self->health--;
	if (self->health <= 0)
	{
		self->Vel.X /= 4;
		self->Vel.Y /= 4;
		self->Vel.Z = 0;
		self->SetState (self->FindState(NAME_Death));
		self->tics -= pr_holyseek()&3;
		return 0;
	}
	if (self->tracer)
	{
		CHolySeekerMissile (self, (double)self->args[0], self->args[0]*2.);
		if (!((level.time+7)&15))
		{
			self->args[0] = 5+(pr_holyseek()/20);
		}
	}

	int xyspeed = (pr_holyweave() % 5);
	int zspeed = (pr_holyweave() % 5);
	A_Weave(self, xyspeed, zspeed, 4., 2.);
	return 0;
}

//============================================================================
//
// A_CHolyCheckScream
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CHolyCheckScream)
{
	PARAM_ACTION_PROLOGUE;

	CALL_ACTION(A_CHolySeek, self);
	if (pr_checkscream() < 20)
	{
		S_Sound (self, CHAN_VOICE, "SpiritActive", 1, ATTN_NORM);
	}
	if (!self->tracer)
	{
		CHolyFindTarget(self);
	}
	return 0;
}

//============================================================================
//
// A_ClericAttack
// (for the ClericBoss)
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClericAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target) return 0;

	AActor * missile = P_SpawnMissileZ (self, self->Z() + 40., self->target, PClass::FindActor ("HolyMissile"));
	if (missile != NULL) missile->tracer = NULL;	// No initial target
	S_Sound (self, CHAN_WEAPON, "HolySymbolFire", 1, ATTN_NORM);
	return 0;
}

