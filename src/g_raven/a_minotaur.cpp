#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "ravenshared.h"
#include "a_action.h"
#include "gi.h"
#include "w_wad.h"

#define MAULATORTICS (25*35)

static FRandom pr_minotauratk1 ("MinotaurAtk1");
static FRandom pr_minotaurdecide ("MinotaurDecide");
static FRandom pr_atk ("MinotaurAtk2");
static FRandom pr_minotauratk3 ("MinotaurAtk3");
static FRandom pr_fire ("MntrFloorFire");
static FRandom pr_minotaurslam ("MinotaurSlam");
static FRandom pr_minotaurroam ("MinotaurRoam");
static FRandom pr_minotaurchase ("MinotaurChase");

void A_MinotaurLook (AActor *);

void P_MinotaurSlam (AActor *source, AActor *target);

IMPLEMENT_CLASS(AMinotaur)

void AMinotaur::Tick ()
{
	Super::Tick ();
	
	// The unfriendly Minotaur (Heretic's) is invulnerable while charging
	if (!(flags5 & MF5_SUMMONEDMONSTER))
	{
		// Get MF_SKULLFLY bit and shift it so it matches MF2_INVULNERABLE
		DWORD flying = (flags & MF_SKULLFLY) << 3;
		if ((flags2 & MF2_INVULNERABLE) != flying)
		{
			flags2 ^= MF2_INVULNERABLE;
		}
	}
}

bool AMinotaur::Slam (AActor *thing)
{
	// Slamming minotaurs shouldn't move non-creatures
	if (!(thing->flags3&MF3_ISMONSTER) && !thing->player)
	{
		return false;
	}
	return Super::Slam (thing);
}

int AMinotaur::DoSpecialDamage (AActor *target, int damage)
{
	damage = Super::DoSpecialDamage (target, damage);
	if ((damage != -1) && (flags & MF_SKULLFLY))
	{ // Slam only when in charge mode
		P_MinotaurSlam (this, target);
		return -1;
	}
	return damage;
}

// Minotaur Friend ----------------------------------------------------------

IMPLEMENT_CLASS(AMinotaurFriend)

void AMinotaurFriend::BeginPlay ()
{
	Super::BeginPlay ();
	StartTime = -1;
}

void AMinotaurFriend::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << StartTime;
}

bool AMinotaurFriend::IsOkayToAttack (AActor *link)
{
	if ((link->flags3&MF3_ISMONSTER) && (link != tracer))
	{
		if (!((link->flags ^ flags) & MF_FRIENDLY))
		{ // Don't attack friends
			if (link->flags & MF_FRIENDLY)
			{
				if (!deathmatch || link->FriendPlayer == 0 || FriendPlayer == 0 ||
					link->FriendPlayer != FriendPlayer)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		if (!(link->flags&MF_SHOOTABLE))
		{
			return false;
		}
		if (link->flags2&MF2_DORMANT)
		{
			return false;
		}
		if ((link->flags5 & MF5_SUMMONEDMONSTER) &&	(link->tracer == tracer))
		{
			return false;
		}
		if (multiplayer && !deathmatch && link->player)
		{
			return false;
		}
		if (P_CheckSight (this, link))
		{
			return true;
		}
	}
	return false;
}

void AMinotaurFriend::Die (AActor *source, AActor *inflictor)
{
	Super::Die (source, inflictor);

	if (tracer && tracer->health > 0 && tracer->player)
	{
		// Search thinker list for minotaur
		TThinkerIterator<AMinotaurFriend> iterator;
		AMinotaurFriend *mo;

		while ((mo = iterator.Next()) != NULL)
		{
			if (mo->health <= 0) continue;
			// [RH] Minotaurs can't be morphed, so this isn't needed
			//if (!(mo->flags&MF_COUNTKILL)) continue;		// for morphed minotaurs
			if (mo->flags&MF_CORPSE) continue;
			if (mo->StartTime >= 0 && (level.maptime - StartTime) >= MAULATORTICS) continue;
			if (mo->tracer != NULL && mo->tracer->player == tracer->player) break;
		}

		if (mo == NULL)
		{
			AInventory *power = tracer->FindInventory (PClass::FindClass("PowerMinotaur"));
			if (power != NULL)
			{
				power->Destroy ();
			}
		}
	}
}

bool AMinotaurFriend::OkayToSwitchTarget (AActor *other)
{
	if (other == tracer) return false;		// Do not target the master
	return Super::OkayToSwitchTarget (other);
}

// Action functions for the minotaur ----------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk1
//
// Melee attack.
//
//----------------------------------------------------------------------------

void A_MinotaurDeath (AActor *actor)
{
	if (Wads.CheckNumForName ("MNTRF1", ns_sprites) < 0 &&
		Wads.CheckNumForName ("MNTRF0", ns_sprites) < 0)
		actor->SetState(actor->FindState ("FadeOut"));
}

void A_MinotaurAtk1 (AActor *actor)
{
	player_t *player;

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, "minotaur/melee", 1, ATTN_NORM);
	if (actor->CheckMeleeRange())
	{
		int damage = pr_minotauratk1.HitDice (4);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		if ((player = actor->target->player) != NULL &&
			player->mo == actor->target)
		{ // Squish the player
			player->deltaviewheight = -16*FRACUNIT;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurDecide
//
// Choose a missile attack.
//
//----------------------------------------------------------------------------

#define MNTR_CHARGE_SPEED (13*FRACUNIT)

void A_MinotaurDecide (AActor *actor)
{
	bool friendly = !!(actor->flags5 & MF5_SUMMONEDMONSTER);
	angle_t angle;
	AActor *target;
	int dist;

	target = actor->target;
	if (!target)
	{
		return;
	}
	if (!friendly)
	{
		S_Sound (actor, CHAN_WEAPON, "minotaur/sight", 1, ATTN_NORM);
	}
	dist = P_AproxDistance (actor->x-target->x, actor->y-target->y);
	if (target->z+target->height > actor->z
		&& target->z+target->height < actor->z+actor->height
		&& dist < (friendly ? 16*64*FRACUNIT : 8*64*FRACUNIT)
		&& dist > 1*64*FRACUNIT
		&& pr_minotaurdecide() < 150)
	{ // Charge attack
		// Don't call the state function right away
		actor->SetStateNF (actor->FindState ("Charge"));
		actor->flags |= MF_SKULLFLY;
		if (!friendly)
		{ // Heretic's Minotaur is invulnerable during charge attack
			actor->flags2 |= MF2_INVULNERABLE;
		}
		A_FaceTarget (actor);
		angle = actor->angle>>ANGLETOFINESHIFT;
		actor->momx = FixedMul (MNTR_CHARGE_SPEED, finecosine[angle]);
		actor->momy = FixedMul (MNTR_CHARGE_SPEED, finesine[angle]);
		actor->special1 = TICRATE/2; // Charge duration
	}
	else if (target->z == target->floorz
		&& dist < 9*64*FRACUNIT
		&& pr_minotaurdecide() < (friendly ? 100 : 220))
	{ // Floor fire attack
		actor->SetState (actor->FindState ("Hammer"));
		actor->special2 = 0;
	}
	else
	{ // Swing attack
		A_FaceTarget (actor);
		// Don't need to call P_SetMobjState because the current state
		// falls through to the swing attack
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurCharge
//
//----------------------------------------------------------------------------

void A_MinotaurCharge (AActor *actor)
{
	AActor *puff;

	if (!actor->target) return;

	if (actor->special1 > 0)
	{
		const PClass *type;

		if (gameinfo.gametype == GAME_Heretic)
		{
			type = PClass::FindClass ("PhoenixPuff");
		}
		else
		{
			type = PClass::FindClass ("PunchPuff");
		}
		puff = Spawn (type, actor->x, actor->y, actor->z, ALLOW_REPLACE);
		puff->momz = 2*FRACUNIT;
		actor->special1--;
	}
	else
	{
		actor->flags &= ~MF_SKULLFLY;
		actor->flags2 &= ~MF2_INVULNERABLE;
		actor->SetState (actor->SeeState);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk2
//
// Swing attack.
//
//----------------------------------------------------------------------------

void A_MinotaurAtk2 (AActor *actor)
{
	AActor *mo;
	angle_t angle;
	fixed_t momz;
	fixed_t z;
	bool friendly = !!(actor->flags5 & MF5_SUMMONEDMONSTER);

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM);
	if (actor->CheckMeleeRange())
	{
		int damage;
		damage = pr_atk.HitDice (friendly ? 3 : 5);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	z = actor->z + 40*FRACUNIT;
	const PClass *fx = PClass::FindClass("MinotaurFX1");
	if (fx)
	{
		mo = P_SpawnMissileZ (actor, z, actor->target, fx);
		if (mo != NULL)
		{
//			S_Sound (mo, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM);
			momz = mo->momz;
			angle = mo->angle;
			P_SpawnMissileAngleZ (actor, z, fx, angle-(ANG45/8), momz);
			P_SpawnMissileAngleZ (actor, z, fx, angle+(ANG45/8), momz);
			P_SpawnMissileAngleZ (actor, z, fx, angle-(ANG45/16), momz);
			P_SpawnMissileAngleZ (actor, z, fx, angle+(ANG45/16), momz);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk3
//
// Floor fire attack.
//
//----------------------------------------------------------------------------

void A_MinotaurAtk3 (AActor *actor)
{
	AActor *mo;
	player_t *player;
	bool friendly = !!(actor->flags5 & MF5_SUMMONEDMONSTER);

	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_VOICE, "minotaur/attack3", 1, ATTN_NORM);
	if (actor->CheckMeleeRange())
	{
		int damage;
		
		damage = pr_minotauratk3.HitDice (friendly ? 3 : 5);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		if ((player = actor->target->player) != NULL &&
			player->mo == actor->target)
		{ // Squish the player
			player->deltaviewheight = -16*FRACUNIT;
		}
	}
	else
	{
		mo = P_SpawnMissile (actor, actor->target, PClass::FindClass("MinotaurFX2"));
		if (mo != NULL)
		{
			S_Sound (mo, CHAN_WEAPON, "minotaur/attack1", 1, ATTN_NORM);
		}
	}
	if (pr_minotauratk3() < 192 && actor->special2 == 0)
	{
		actor->SetState (actor->FindState ("Hammer"));
		actor->special2 = 1;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MntrFloorFire
//
//----------------------------------------------------------------------------

void A_MntrFloorFire (AActor *actor)
{
	AActor *mo;
	fixed_t x, y;

	actor->z = actor->floorz;
	x = actor->x + (pr_fire.Random2 () << 10);
	y = actor->y + (pr_fire.Random2 () << 10);
	mo = Spawn("MinotaurFX3", x, y, ONFLOORZ, ALLOW_REPLACE);
	mo->target = actor->target;
	mo->momx = 1; // Force block checking
	P_CheckMissileSpawn (mo);
}

//---------------------------------------------------------------------------
//
// FUNC P_MinotaurSlam
//
//---------------------------------------------------------------------------

void P_MinotaurSlam (AActor *source, AActor *target)
{
	angle_t angle;
	fixed_t thrust;
	int damage;

	angle = R_PointToAngle2 (source->x, source->y, target->x, target->y);
	angle >>= ANGLETOFINESHIFT;
	thrust = 16*FRACUNIT+(pr_minotaurslam()<<10);
	target->momx += FixedMul (thrust, finecosine[angle]);
	target->momy += FixedMul (thrust, finesine[angle]);
	damage = pr_minotaurslam.HitDice (static_cast<AMinotaur *>(source) ? 4 : 6);
	P_DamageMobj (target, NULL, NULL, damage, NAME_Melee);
	P_TraceBleed (damage, target, angle, 0);
	if (target->player)
	{
		target->reactiontime = 14+(pr_minotaurslam()&7);
	}
}

//----------------------------------------------------------------------------
//
// Minotaur variables
//
//	special1		charge duration countdown
//	special2		internal to minotaur AI
//	StartTime		minotaur start time
// 	tracer			pointer to player that spawned it
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//
// A_MinotaurRoam
//
//----------------------------------------------------------------------------

void A_MinotaurRoam (AActor *actor)
{
	AMinotaurFriend *self = static_cast<AMinotaurFriend *> (actor);

	// In case pain caused him to skip his fade in.
	actor->RenderStyle = STYLE_Normal;

	if (self->StartTime >= 0 && (level.maptime - self->StartTime) >= MAULATORTICS)
	{
		P_DamageMobj (actor, NULL, NULL, 1000000, NAME_None);
		return;
	}

	if (pr_minotaurroam() < 30)
		A_MinotaurLook (actor);		// adjust to closest target

	if (pr_minotaurroam() < 6)
	{
		//Choose new direction
		actor->movedir = pr_minotaurroam() % 8;
		FaceMovementDirection (actor);
	}
	if (!P_Move(actor))
	{
		// Turn
		if (pr_minotaurroam() & 1)
			actor->movedir = (++actor->movedir)%8;
		else
			actor->movedir = (actor->movedir+7)%8;
		FaceMovementDirection (actor);
	}
}


//----------------------------------------------------------------------------
//
//	PROC A_MinotaurLook
//
// Look for enemy of player
//----------------------------------------------------------------------------
#define MINOTAUR_LOOK_DIST		(16*54*FRACUNIT)

void A_MinotaurLook (AActor *actor)
{
	if (!actor->IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		A_Look (actor);
		return;
	}

	AActor *mo = NULL;
	player_t *player;
	fixed_t dist;
	int i;
	AActor *master = actor->tracer;

	actor->target = NULL;
	if (deathmatch)					// Quick search for players
	{
    	for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i]) continue;
			player = &players[i];
			mo = player->mo;
			if (mo == master) continue;
			if (mo->health <= 0) continue;
			dist = P_AproxDistance(actor->x - mo->x, actor->y - mo->y);
			if (dist > MINOTAUR_LOOK_DIST) continue;
			actor->target = mo;
			break;
		}
	}

	if (!actor->target)				// Near player monster search
	{
		if (master && (master->health>0) && (master->player))
			mo = P_RoughMonsterSearch(master, 20);
		else
			mo = P_RoughMonsterSearch(actor, 20);
		actor->target = mo;
	}

	if (!actor->target)				// Normal monster search
	{
		FActorIterator iterator (0);

		while ((mo = iterator.Next()) != NULL)
		{
			if (!(mo->flags3&MF3_ISMONSTER)) continue;
			if (mo->health <= 0) continue;
			if (!(mo->flags&MF_SHOOTABLE)) continue;
			dist = P_AproxDistance (actor->x - mo->x, actor->y - mo->y);
			if (dist > MINOTAUR_LOOK_DIST) continue;
			if ((mo == master) || (mo == actor)) continue;
			if ((mo->flags5 & MF5_SUMMONEDMONSTER) && (mo->tracer == master)) continue;
			actor->target = mo;
			break;			// Found actor to attack
		}
	}

	if (actor->target)
	{
		actor->SetStateNF (actor->SeeState);
	}
	else
	{
		actor->SetStateNF (actor->FindState ("Roam"));
	}
}

void A_MinotaurChase (AActor *actor)
{
	if (!actor->IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		A_Chase (actor);
		return;
	}

	AMinotaurFriend *self = static_cast<AMinotaurFriend *> (actor);

	// In case pain caused him to skip his fade in.
	actor->RenderStyle = STYLE_Normal;

	if (self->StartTime >= 0 && (level.maptime - self->StartTime) >= MAULATORTICS)
	{
		P_DamageMobj (actor, NULL, NULL, 1000000, NAME_None);
		return;
	}

	if (pr_minotaurchase() < 30)
		A_MinotaurLook (actor);		// adjust to closest target

	if (!actor->target || (actor->target->health <= 0) ||
		!(actor->target->flags&MF_SHOOTABLE))
	{ // look for a new target
		actor->SetState (actor->FindState ("Spawn"));
		return;
	}

	FaceMovementDirection (actor);
	actor->reactiontime = 0;

	// Melee attack
	if (actor->MeleeState && actor->CheckMeleeRange ())
	{
		if (actor->AttackSound)
		{
			S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
		}
		actor->SetState (actor->MeleeState);
		return;
	}

	// Missile attack
	if (actor->MissileState && P_CheckMissileRange(actor))
	{
		actor->SetState (actor->MissileState);
		return;
	}

	// chase towards target
	if (!P_Move (actor))
	{
		P_NewChaseDir (actor);
		FaceMovementDirection (actor);
	}

	// Active sound
	if (pr_minotaurchase() < 6)
	{
		actor->PlayActiveSound ();
	}
}

