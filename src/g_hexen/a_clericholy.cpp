#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_hexenglobal.h"
#include "gstrings.h"
#include "a_weaponpiece.h"

#define BLAST_FULLSTRENGTH	255

static FRandom pr_holyatk2 ("CHolyAtk2");
static FRandom pr_holyseeker ("CHolySeeker");
static FRandom pr_holyweave ("CHolyWeave");
static FRandom pr_holyseek ("CHolySeek");
static FRandom pr_checkscream ("CCheckScream");
static FRandom pr_spiritslam ("CHolySlam");
static FRandom pr_wraithvergedrop ("WraithvergeDrop");

void A_CHolyAttack2 (AActor *);
void A_CHolyTail (AActor *);
void A_CHolySeek (AActor *);
void A_CHolyCheckScream (AActor *);
void A_DropWraithvergePieces (AActor *);

void A_CHolyAttack (AActor *);
void A_CHolyPalette (AActor *);

void SpawnSpiritTail (AActor *spirit);

//==========================================================================

class AClericWeaponPiece : public AWeaponPiece
{
	DECLARE_CLASS (AClericWeaponPiece, AWeaponPiece)
protected:
	bool TryPickup (AActor *toucher);
};

IMPLEMENT_CLASS (AClericWeaponPiece)

bool AClericWeaponPiece::TryPickup (AActor *toucher)
{
	if (!toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer)) &&
		!toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)))
	{
		return Super::TryPickup(toucher);
	}
	else
	{ // Wrong class, but try to pick up for ammo
		if (ShouldStay())
		{
			// Can't pick up weapons for other classes in coop netplay
			return false;
		}

		AWeapon * Defaults=(AWeapon*)GetDefaultByType(WeaponClass);

		bool gaveSome = !!(toucher->GiveAmmo (Defaults->AmmoType1, Defaults->AmmoGive1) +
						   toucher->GiveAmmo (Defaults->AmmoType2, Defaults->AmmoGive2));

		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
}

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
		return PalEntry (CHolyCount * 128 / 3, 131, 131, 131);
	}
	BYTE CHolyCount;
};

IMPLEMENT_CLASS (ACWeapWraithverge)

// Holy Spirit --------------------------------------------------------------

IMPLEMENT_CLASS (AHolySpirit)

bool AHolySpirit::Slam (AActor *thing)
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
			P_DamageMobj (thing, this, target, dam, NAME_Melee);
			if (pr_spiritslam() < 128)
			{
				Spawn ("HolyPuff", x, y, z, ALLOW_REPLACE);
				S_Sound (this, CHAN_WEAPON, "SpiritAttack", 1, ATTN_NORM);
				if (thing->flags3&MF3_ISMONSTER && pr_spiritslam() < 128)
				{
					thing->Howl ();
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

bool AHolySpirit::SpecialBlastHandling (AActor *source, fixed_t strength)
{
	if (strength == BLAST_FULLSTRENGTH && tracer == source)
	{
		tracer = target;
		target = source;
		GC::WriteBarrier(this, source);
	}
	return true;
}

bool AHolySpirit::IsOkayToAttack (AActor *link)
{
	if ((link->flags3&MF3_ISMONSTER ||
		(link->player && link != target))
		&& !(link->flags2&MF2_DORMANT))
	{
		if (target != NULL && link->IsFriend(target))
		{
			return false;
		}
		if (!(link->flags&MF_SHOOTABLE))
		{
			return false;
		}
		if (multiplayer && !deathmatch && link->player && target != NULL && target->player)
		{
			return false;
		}
		if (link == target)
		{
			return false;
		}
		else if (P_CheckSight (this, link))
		{
			return true;
		}
	}
	return false;
}

//============================================================================
//
// A_CHolyAttack3
//
// 	Spawns the spirits
//============================================================================

void A_CHolyAttack3 (AActor *actor)
{
	AActor * missile = P_SpawnMissileZ (actor, actor->z + 40*FRACUNIT, actor->target, PClass::FindClass ("HolyMissile"));
	if (missile != NULL) missile->tracer = NULL;	// No initial target
	S_Sound (actor, CHAN_WEAPON, "HolySymbolFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CHolyAttack2 
//
// 	Spawns the spirits
//============================================================================

void A_CHolyAttack2 (AActor *actor)
{
	int j;
	int i;
	AActor *mo;

	for (j = 0; j < 4; j++)
	{
		mo = Spawn<AHolySpirit> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (!mo)
		{
			continue;
		}
		switch (j)
		{ // float bob index
			case 0:
				mo->special2 = pr_holyatk2()&7; // upper-left
				break;
			case 1:
				mo->special2 = 32+(pr_holyatk2()&7); // upper-right
				break;
			case 2:
				mo->special2 = (32+(pr_holyatk2()&7))<<16; // lower-left
				break;
			case 3:
				i = pr_holyatk2();
				mo->special2 = ((32+(i&7))<<16)+32+(pr_holyatk2()&7);
				break;
		}
		mo->z = actor->z;
		mo->angle = actor->angle+(ANGLE_45+ANGLE_45/2)-ANGLE_45*j;
		P_ThrustMobj(mo, mo->angle, mo->Speed);
		mo->target = actor->target;
		mo->args[0] = 10; // initial turn value
		mo->args[1] = 0; // initial look angle
		if (deathmatch)
		{ // Ghosts last slightly less longer in DeathMatch
			mo->health = 85;
		}
		if (actor->tracer)
		{
			mo->tracer = actor->tracer;
			mo->flags |= MF_NOCLIP|MF_SKULLFLY;
			mo->flags &= ~MF_MISSILE;
		}
		SpawnSpiritTail (mo);
	}
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
	
	tail = Spawn ("HolyTail", spirit->x, spirit->y, spirit->z, ALLOW_REPLACE);
	tail->target = spirit; // parent
	for (i = 1; i < 3; i++)
	{
		next = Spawn ("HolyTailTrail", spirit->x, spirit->y, spirit->z, ALLOW_REPLACE);
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

void A_CHolyAttack (AActor *actor)
{
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}
	ACWeapWraithverge *weapon = static_cast<ACWeapWraithverge *> (actor->player->ReadyWeapon);
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	AActor * missile = P_SpawnPlayerMissile (actor, 0,0,0, PClass::FindClass ("HolyMissile"), actor->angle, &linetarget);
	if (missile != NULL) missile->tracer = linetarget;

	weapon->CHolyCount = 3;
	S_Sound (actor, CHAN_WEAPON, "HolySymbolFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CHolyPalette
//
//============================================================================

void A_CHolyPalette (AActor *actor)
{
	if (actor->player != NULL)
	{
		ACWeapWraithverge *weapon = static_cast<ACWeapWraithverge *> (actor->player->ReadyWeapon);
		if (weapon != NULL && weapon->CHolyCount != 0)
		{
			weapon->CHolyCount--;
		}
	}
}

//============================================================================
//
// CHolyTailFollow
//
//============================================================================

static void CHolyTailFollow (AActor *actor, fixed_t dist)
{
	AActor *child;
	int an;
	fixed_t oldDistance, newDistance;

	while (actor)
	{
		child = actor->tracer;
		if (child)
		{
			an = R_PointToAngle2(actor->x, actor->y, child->x, 
				child->y)>>ANGLETOFINESHIFT;
			oldDistance = P_AproxDistance (child->x-actor->x, child->y-actor->y);
			if (P_TryMove (child, actor->x+FixedMul(dist, finecosine[an]), 
				actor->y+FixedMul(dist, finesine[an]), true))
			{
				newDistance = P_AproxDistance (child->x-actor->x, 
					child->y-actor->y)-FRACUNIT;
				if (oldDistance < FRACUNIT)
				{
					if (child->z < actor->z)
					{
						child->z = actor->z-dist;
					}
					else
					{
						child->z = actor->z+dist;
					}
				}
				else
				{
					child->z = actor->z + Scale (newDistance, child->z-actor->z, oldDistance);
				}
			}
		}
		actor = child;
		dist -= FRACUNIT;
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

void A_CHolyTail (AActor *actor)
{
	AActor *parent;

	parent = actor->target;

	if (parent == NULL || parent->health <= 0)	// better check for health than current state - it's safer!
	{ // Ghost removed, so remove all tail parts
		CHolyTailRemove (actor);
		return;
	}
	else
	{
		if (P_TryMove (actor,
			parent->x - 14*finecosine[parent->angle>>ANGLETOFINESHIFT],
			parent->y - 14*finesine[parent->angle>>ANGLETOFINESHIFT], true))
		{
			actor->z = parent->z-5*FRACUNIT;
		}
		CHolyTailFollow (actor, 10*FRACUNIT);
	}
}

//============================================================================
//
// CHolyFindTarget
//
//============================================================================

static void CHolyFindTarget (AActor *actor)
{
	AActor *target;

	if ( (target = P_RoughMonsterSearch (actor, 6)) )
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

static void CHolySeekerMissile (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	fixed_t newZ;
	fixed_t deltaZ;

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	if(!(target->flags&MF_SHOOTABLE) 
	|| (!(target->flags3&MF3_ISMONSTER) && !target->player))
	{ // Target died/target isn't a player or creature
		actor->tracer = NULL;
		actor->flags &= ~(MF_NOCLIP|MF_SKULLFLY);
		actor->flags |= MF_MISSILE;
		CHolyFindTarget (actor);
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[angle]);
	actor->momy = FixedMul (actor->Speed, finesine[angle]);
	if (!(level.time&15) 
		|| actor->z > target->z+(target->height)
		|| actor->z+actor->height < target->z)
	{
		newZ = target->z+((pr_holyseeker()*target->height)>>8);
		deltaZ = newZ-actor->z;
		if (abs(deltaZ) > 15*FRACUNIT)
		{
			if (deltaZ > 0)
			{
				deltaZ = 15*FRACUNIT;
			}
			else
			{
				deltaZ = -15*FRACUNIT;
			}
		}
		dist = P_AproxDistance (target->x-actor->x, target->y-actor->y);
		dist = dist / actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->momz = deltaZ/dist;
	}
	return;
}

//============================================================================
//
// A_CHolyWeave
//
//============================================================================

static void CHolyWeave (AActor *actor)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;

	weaveXY = actor->special2>>16;
	weaveZ = actor->special2&0xFFFF;
	angle = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	newX = actor->x-FixedMul(finecosine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	newY = actor->y-FixedMul(finesine[angle],
		FloatBobOffsets[weaveXY]<<2);
	weaveXY = (weaveXY+(pr_holyweave()%5))&63;
	newX += FixedMul(finecosine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	newY += FixedMul(finesine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	P_TryMove(actor, newX, newY, true);
	actor->z -= FloatBobOffsets[weaveZ]<<1;
	weaveZ = (weaveZ+(pr_holyweave()%5))&63;
	actor->z += FloatBobOffsets[weaveZ]<<1;	
	actor->special2 = weaveZ+(weaveXY<<16);
}

//============================================================================
//
// A_CHolySeek
//
//============================================================================

void A_CHolySeek (AActor *actor)
{
	actor->health--;
	if (actor->health <= 0)
	{
		actor->momx >>= 2;
		actor->momy >>= 2;
		actor->momz = 0;
		actor->SetState (actor->FindState(NAME_Death));
		actor->tics -= pr_holyseek()&3;
		return;
	}
	if (actor->tracer)
	{
		CHolySeekerMissile (actor, actor->args[0]*ANGLE_1,
			actor->args[0]*ANGLE_1*2);
		if (!((level.time+7)&15))
		{
			actor->args[0] = 5+(pr_holyseek()/20);
		}
	}
	CHolyWeave (actor);
}

//============================================================================
//
// A_CHolyCheckScream
//
//============================================================================

void A_CHolyCheckScream (AActor *actor)
{
	A_CHolySeek (actor);
	if (pr_checkscream() < 20)
	{
		S_Sound (actor, CHAN_VOICE, "SpiritActive", 1, ATTN_NORM);
	}
	if (!actor->tracer)
	{
		CHolyFindTarget(actor);
	}
}

//============================================================================
//
// A_ClericAttack
// (for the ClericBoss)
//
//============================================================================

void A_ClericAttack (AActor *actor)
{
	extern void A_CHolyAttack3 (AActor *actor);

	if (!actor->target) return;
	A_CHolyAttack3 (actor);
}
