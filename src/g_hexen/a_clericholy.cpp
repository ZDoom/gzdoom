#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_hexenglobal.h"
#include "gstrings.h"

#define BLAST_FULLSTRENGTH	255

static FRandom pr_holyatk2 ("CHolyAtk2");
static FRandom pr_holyseeker ("CHolySeeker");
static FRandom pr_holyweave ("CHolyWeave");
static FRandom pr_holyseek ("CHolySeek");
static FRandom pr_checkscream ("CCheckScream");
static FRandom pr_spiritslam ("CHolySlam");
static FRandom pr_wraithvergedrop ("WraithvergeDrop");

void A_CHolySpawnPuff (AActor *);
void A_CHolyAttack2 (AActor *);
void A_CHolyTail (AActor *);
void A_CHolySeek (AActor *);
void A_CHolyCheckScream (AActor *);
void A_DropWraithvergePieces (AActor *);

void A_CHolyAttack (AActor *);
void A_CHolyPalette (AActor *);

void SpawnSpiritTail (AActor *spirit);

//==========================================================================

class AClericWeaponPiece : public AFourthWeaponPiece
{
	DECLARE_STATELESS_ACTOR (AClericWeaponPiece, AFourthWeaponPiece)
public:
	void BeginPlay ();
protected:
	bool MatchPlayerClass (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (AClericWeaponPiece, Hexen, -1, 0)
	PROP_Inventory_PickupMessage("$TXT_WRAITHVERGE_PIECE")
END_DEFAULTS

bool AClericWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return !toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)) &&
		   !toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer));
}

//==========================================================================

class ACWeaponPiece1 : public AClericWeaponPiece
{
	DECLARE_ACTOR (ACWeaponPiece1, AClericWeaponPiece)
public:
	void BeginPlay ();
};

FState ACWeaponPiece1::States[] =
{
	S_BRIGHT (WCH1, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACWeaponPiece1, Hexen, 18, 33)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void ACWeaponPiece1::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE1<<3;
}

//==========================================================================

class ACWeaponPiece2 : public AClericWeaponPiece
{
	DECLARE_ACTOR (ACWeaponPiece2, AClericWeaponPiece)
public:
	void BeginPlay ();
};

FState ACWeaponPiece2::States[] =
{
	S_BRIGHT (WCH2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACWeaponPiece2, Hexen, 19, 34)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void ACWeaponPiece2::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE2<<3;
}

//==========================================================================

class ACWeaponPiece3 : public AClericWeaponPiece
{
	DECLARE_ACTOR (ACWeaponPiece3, AClericWeaponPiece)
public:
	void BeginPlay ();
};

FState ACWeaponPiece3::States[] =
{
	S_BRIGHT (WCH3, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACWeaponPiece3, Hexen, 20, 35)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void ACWeaponPiece3::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE3<<3;
}

// An actor that spawns the three pieces of the cleric's fourth weapon ------

// This gets spawned if weapon drop is on so that other players can pick up
// this player's weapon.

class AWraithvergeDrop : public AActor
{
	DECLARE_ACTOR (AWraithvergeDrop, AActor)
};

FState AWraithvergeDrop::States[] =
{
	S_NORMAL (TNT1, 'A', 1, NULL, &States[1]),
	S_NORMAL (TNT1, 'A', 1, A_DropWraithvergePieces, NULL)
};

IMPLEMENT_ACTOR (AWraithvergeDrop, Hexen, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS


// Cleric's Wraithverge (Holy Symbol?) --------------------------------------

class ACWeapWraithverge : public AClericWeapon
{
	DECLARE_ACTOR (ACWeapWraithverge, AClericWeapon)
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

FState ACWeapWraithverge::States[] =
{
	// Dummy state, because the fourth weapon does not appear in a level directly.
	S_NORMAL (TNT1, 'A',   -1, NULL					    , NULL),

#define S_CHOLYREADY 1
	S_NORMAL (CHLY, 'A',	1, A_WeaponReady		    , &States[S_CHOLYREADY]),

#define S_CHOLYDOWN (S_CHOLYREADY+1)
	S_NORMAL (CHLY, 'A',	1, A_Lower				    , &States[S_CHOLYDOWN]),

#define S_CHOLYUP (S_CHOLYDOWN+1)
	S_NORMAL (CHLY, 'A',	1, A_Raise				    , &States[S_CHOLYUP]),

#define S_CHOLYATK (S_CHOLYUP+1)
	S_BRIGHT2 (CHLY, 'A',	1, NULL					    , &States[S_CHOLYATK+1], 0, 40),
	S_BRIGHT2 (CHLY, 'B',	1, NULL					    , &States[S_CHOLYATK+2], 0, 40),
	S_BRIGHT2 (CHLY, 'C',	2, NULL					    , &States[S_CHOLYATK+3], 0, 43),
	S_BRIGHT2 (CHLY, 'D',	2, NULL					    , &States[S_CHOLYATK+4], 0, 43),
	S_BRIGHT2 (CHLY, 'E',	2, NULL					    , &States[S_CHOLYATK+5], 0, 45),
	S_BRIGHT2 (CHLY, 'F',	6, A_CHolyAttack		    , &States[S_CHOLYATK+6], 0, 48),
	S_BRIGHT2 (CHLY, 'G',	2, A_CHolyPalette		    , &States[S_CHOLYATK+7], 0, 40),
	S_BRIGHT2 (CHLY, 'G',	2, A_CHolyPalette		    , &States[S_CHOLYATK+8], 0, 40),
	S_BRIGHT2 (CHLY, 'G',	2, A_CHolyPalette		    , &States[S_CHOLYREADY], 0, 36)
};

IMPLEMENT_ACTOR (ACWeapWraithverge, Hexen, -1, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)

	PROP_Weapon_SelectionOrder (3000)
	PROP_Weapon_Flags (WIF_PRIMARY_USES_BOTH)
	PROP_Weapon_AmmoUse1 (18)
	PROP_Weapon_AmmoUse2 (18)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AmmoGive2 (0)
	PROP_Weapon_UpState (S_CHOLYUP)
	PROP_Weapon_DownState (S_CHOLYDOWN)
	PROP_Weapon_ReadyState (S_CHOLYREADY)
	PROP_Weapon_AtkState (S_CHOLYATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_MoveCombatDist (22000000)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Weapon_AmmoType2 ("Mana2")
	PROP_Weapon_ProjectileType ("HolyMissile")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_C4")
END_DEFAULTS

// Holy Missile -------------------------------------------------------------

class AHolyMissile : public AActor
{
	DECLARE_ACTOR (AHolyMissile, AActor)
};

FState AHolyMissile::States[] =
{
	S_BRIGHT (SPIR, 'P',	3, A_CHolySpawnPuff		    , &States[1]),
	S_BRIGHT (SPIR, 'P',	3, A_CHolySpawnPuff		    , &States[2]),
	S_BRIGHT (SPIR, 'P',	3, A_CHolySpawnPuff		    , &States[3]),
	S_BRIGHT (SPIR, 'P',	3, A_CHolySpawnPuff		    , &States[4]),
	S_BRIGHT (SPIR, 'P',	1, A_CHolyAttack2		    , NULL),
};

IMPLEMENT_ACTOR (AHolyMissile, Hexen, -1, 0)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (8)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)	
	PROP_Flags4 (MF4_EXTREMEDEATH)

	PROP_SpawnState (0)
	PROP_DeathState (4)
END_DEFAULTS

// Holy Missile Puff --------------------------------------------------------

class AHolyMissilePuff : public AActor
{
	DECLARE_ACTOR (AHolyMissilePuff, AActor)
};

FState AHolyMissilePuff::States[] =
{
	S_NORMAL (SPIR, 'Q',	3, NULL					    , &States[1]),
	S_NORMAL (SPIR, 'R',	3, NULL					    , &States[2]),
	S_NORMAL (SPIR, 'S',	3, NULL					    , &States[3]),
	S_NORMAL (SPIR, 'T',	3, NULL					    , &States[4]),
	S_NORMAL (SPIR, 'U',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHolyMissilePuff, Hexen, -1, 0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Holy Puff ----------------------------------------------------------------

class AHolyPuff : public AActor
{
	DECLARE_ACTOR (AHolyPuff, AActor)
};

FState AHolyPuff::States[] =
{
	S_NORMAL (SPIR, 'K',	3, NULL					    , &States[1]),
	S_NORMAL (SPIR, 'L',	3, NULL					    , &States[2]),
	S_NORMAL (SPIR, 'M',	3, NULL					    , &States[3]),
	S_NORMAL (SPIR, 'N',	3, NULL					    , &States[4]),
	S_NORMAL (SPIR, 'O',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHolyPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
	PROP_SpawnState (0)
END_DEFAULTS


// Holy Spirit --------------------------------------------------------------

FState AHolySpirit::States[] =
{
#define S_HOLY_FX1 0
	S_NORMAL (SPIR, 'A',	2, A_CHolySeek			    , &States[S_HOLY_FX1+1]),
	S_NORMAL (SPIR, 'A',	2, A_CHolySeek			    , &States[S_HOLY_FX1+2]),
	S_NORMAL (SPIR, 'B',	2, A_CHolySeek			    , &States[S_HOLY_FX1+3]),
	S_NORMAL (SPIR, 'B',	2, A_CHolyCheckScream	    , &States[S_HOLY_FX1]),

#define S_HOLY_FX_X1 (S_HOLY_FX1+4)
	S_NORMAL (SPIR, 'D',	4, NULL					    , &States[S_HOLY_FX_X1+1]),
	S_NORMAL (SPIR, 'E',	4, A_Scream				    , &States[S_HOLY_FX_X1+2]),
	S_NORMAL (SPIR, 'F',	4, NULL					    , &States[S_HOLY_FX_X1+3]),
	S_NORMAL (SPIR, 'G',	4, NULL					    , &States[S_HOLY_FX_X1+4]),
	S_NORMAL (SPIR, 'H',	4, NULL					    , &States[S_HOLY_FX_X1+5]),
	S_NORMAL (SPIR, 'I',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHolySpirit, Hexen, -1, 0)
	PROP_SpawnHealth (105)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_RIP|MF2_IMPACT|MF2_PCROSS|MF2_SEEKERMISSILE)
	PROP_Flags3 (MF3_FOILINVUL|MF3_SKYEXPLODE|MF3_NOEXPLODEFLOOR|MF3_CANBLAST)
	PROP_Flags4 (MF4_EXTREMEDEATH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (S_HOLY_FX1)
	PROP_DeathState (S_HOLY_FX_X1)

	PROP_DeathSound ("SpiritDie")
END_DEFAULTS

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
				Spawn<AHolyPuff> (x, y, z, ALLOW_REPLACE);
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


// Holy Tail ----------------------------------------------------------------

class AHolyTail : public AActor
{
	DECLARE_ACTOR (AHolyTail, AActor)
};

FState AHolyTail::States[] =
{
	S_NORMAL (SPIR, 'C',	1, A_CHolyTail			    , &States[0]),
	S_NORMAL (SPIR, 'D',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AHolyTail, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_NOCLIP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Holy Tail Trail ---------------------------------------------------------

class AHolyTailTrail : public AHolyTail
{
	DECLARE_STATELESS_ACTOR (AHolyTailTrail, AHolyTail)
};


IMPLEMENT_STATELESS_ACTOR (AHolyTailTrail, Hexen, -1, 0)
	PROP_SpawnState (1)
END_DEFAULTS

//============================================================================
//
// A_CHolyAttack3
//
// 	Spawns the spirits
//============================================================================

void A_CHolyAttack3 (AActor *actor)
{
	AActor * missile = P_SpawnMissileZ (actor, actor->z + 40*FRACUNIT, actor->target, RUNTIME_CLASS(AHolyMissile));
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
	
	tail = Spawn<AHolyTail> (spirit->x, spirit->y, spirit->z, ALLOW_REPLACE);
	tail->target = spirit; // parent
	for (i = 1; i < 3; i++)
	{
		next = Spawn<AHolyTailTrail> (spirit->x, spirit->y, spirit->z, ALLOW_REPLACE);
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
	AActor * missile = P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(AHolyMissile), actor->angle, &linetarget);
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
// A_CHolySpawnPuff
//
//============================================================================

void A_CHolySpawnPuff (AActor *actor)
{
	Spawn<AHolyMissilePuff> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
}

void AClericWeaponPiece::BeginPlay ()
{
	Super::BeginPlay ();
	FourthWeaponClass = RUNTIME_CLASS(ACWeapWraithverge);
}

//============================================================================
//
// A_DropWraithvergePieces
//
//============================================================================

void A_DropWraithvergePieces (AActor *actor)
{
	static const PClass *pieces[3] =
	{
		RUNTIME_CLASS(ACWeaponPiece1),
		RUNTIME_CLASS(ACWeaponPiece2),
		RUNTIME_CLASS(ACWeaponPiece3)
	};

	for (int i = 0, j = 0, fineang = 0; i < 3; ++i)
	{
		AActor *piece = Spawn (pieces[j], actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (piece != NULL)
		{
			piece->momx = actor->momx + finecosine[fineang];
			piece->momy = actor->momy + finesine[fineang];
			piece->momz = actor->momz;
			piece->flags |= MF_DROPPED;
			fineang += FINEANGLES/3;
			j = (j == 0) ? (pr_wraithvergedrop() & 1) + 1 : 3-j;
		}
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