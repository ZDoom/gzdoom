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

void A_MinotaurDecide (AActor *);
void A_MinotaurAtk1 (AActor *);
void A_MinotaurAtk2 (AActor *);
void A_MinotaurAtk3 (AActor *);
void A_MinotaurCharge (AActor *);
void A_MntrFloorFire (AActor *);
void A_MinotaurFade0 (AActor *);
void A_MinotaurFade1 (AActor *);
void A_MinotaurFade2 (AActor *);
void A_MinotaurLook (AActor *);
void A_MinotaurRoam (AActor *);
void A_SmokePuffExit (AActor *);
void A_MinotaurChase (AActor *);

void P_MinotaurSlam (AActor *source, AActor *target);

// Class definitions --------------------------------------------------------

FState AMinotaur::States[] =
{
#define S_MNTR_LOOK 0
	S_NORMAL (MNTR, 'A',   10, A_MinotaurLook				, &States[S_MNTR_LOOK+1]),
	S_NORMAL (MNTR, 'B',   10, A_MinotaurLook				, &States[S_MNTR_LOOK+0]),

#define S_MNTR_ROAM (S_MNTR_LOOK+2)
	S_NORMAL (MNTR, 'A',	5, A_MinotaurRoam				, &States[S_MNTR_ROAM+1]),
	S_NORMAL (MNTR, 'B',	5, A_MinotaurRoam				, &States[S_MNTR_ROAM+2]),
	S_NORMAL (MNTR, 'C',	5, A_MinotaurRoam				, &States[S_MNTR_ROAM+3]),
	S_NORMAL (MNTR, 'D',	5, A_MinotaurRoam				, &States[S_MNTR_ROAM]),

#define S_MNTR_WALK (S_MNTR_ROAM+4)
	S_NORMAL (MNTR, 'A',	5, A_MinotaurChase				, &States[S_MNTR_WALK+1]),
	S_NORMAL (MNTR, 'B',	5, A_MinotaurChase				, &States[S_MNTR_WALK+2]),
	S_NORMAL (MNTR, 'C',	5, A_MinotaurChase				, &States[S_MNTR_WALK+3]),
	S_NORMAL (MNTR, 'D',	5, A_MinotaurChase				, &States[S_MNTR_WALK+0]),

#define S_MNTR_ATK1 (S_MNTR_WALK+4)
	S_NORMAL (MNTR, 'V',   10, A_FaceTarget 				, &States[S_MNTR_ATK1+1]),
	S_NORMAL (MNTR, 'W',	7, A_FaceTarget 				, &States[S_MNTR_ATK1+2]),
	S_NORMAL (MNTR, 'X',   12, A_MinotaurAtk1				, &States[S_MNTR_WALK+0]),

#define S_MNTR_ATK2 (S_MNTR_ATK1+3)
	S_NORMAL (MNTR, 'V',   10, A_MinotaurDecide 			, &States[S_MNTR_ATK2+1]),
	S_NORMAL (MNTR, 'Y',	4, A_FaceTarget 				, &States[S_MNTR_ATK2+2]),
	S_NORMAL (MNTR, 'Z',	9, A_MinotaurAtk2				, &States[S_MNTR_WALK+0]),

#define S_MNTR_ATK3 (S_MNTR_ATK2+3)
	S_NORMAL (MNTR, 'V',   10, A_FaceTarget 				, &States[S_MNTR_ATK3+1]),
	S_NORMAL (MNTR, 'W',	7, A_FaceTarget 				, &States[S_MNTR_ATK3+2]),
	S_NORMAL (MNTR, 'X',   12, A_MinotaurAtk3				, &States[S_MNTR_WALK+0]),
	S_NORMAL (MNTR, 'X',   12, NULL 						, &States[S_MNTR_ATK3+0]),

#define S_MNTR_ATK4 (S_MNTR_ATK3+4)
	S_NORMAL (MNTR, 'U',	2, A_MinotaurCharge 			, &States[S_MNTR_ATK4+0]),

#define S_MNTR_PAIN (S_MNTR_ATK4+1)
	S_NORMAL (MNTR, 'E',	3, NULL 						, &States[S_MNTR_PAIN+1]),
	S_NORMAL (MNTR, 'E',	6, A_Pain						, &States[S_MNTR_WALK+0]),

#define S_MNTR_DIE (S_MNTR_PAIN+2)
	S_NORMAL (MNTR, 'F',	6, NULL 						, &States[S_MNTR_DIE+1]),
	S_NORMAL (MNTR, 'G',	5, NULL 						, &States[S_MNTR_DIE+2]),
	S_NORMAL (MNTR, 'H',	6, A_Scream 					, &States[S_MNTR_DIE+3]),
	S_NORMAL (MNTR, 'I',	5, NULL 						, &States[S_MNTR_DIE+4]),
	S_NORMAL (MNTR, 'J',	6, NULL 						, &States[S_MNTR_DIE+5]),
	S_NORMAL (MNTR, 'K',	5, NULL 						, &States[S_MNTR_DIE+6]),
	S_NORMAL (MNTR, 'L',	6, NULL 						, &States[S_MNTR_DIE+7]),
	S_NORMAL (MNTR, 'M',	5, A_NoBlocking 				, &States[S_MNTR_DIE+8]),
	S_NORMAL (MNTR, 'N',	6, NULL 						, &States[S_MNTR_DIE+9]),
	S_NORMAL (MNTR, 'O',	5, NULL 						, &States[S_MNTR_DIE+10]),
	S_NORMAL (MNTR, 'P',	6, NULL 						, &States[S_MNTR_DIE+11]),
	S_NORMAL (MNTR, 'Q',	5, NULL 						, &States[S_MNTR_DIE+12]),
	S_NORMAL (MNTR, 'R',	6, NULL 						, &States[S_MNTR_DIE+13]),
	S_NORMAL (MNTR, 'S',	5, NULL 						, &States[S_MNTR_DIE+14]),
	S_NORMAL (MNTR, 'T',   -1, A_BossDeath					, NULL),

#define S_MNTR_FADEIN (S_MNTR_DIE+15)
	S_NORMAL (MNTR, 'A',   15, NULL					    , &States[S_MNTR_FADEIN+1]),
	S_NORMAL (MNTR, 'A',   15, A_MinotaurFade1		    , &States[S_MNTR_FADEIN+2]),
	S_NORMAL (MNTR, 'A',	3, A_MinotaurFade2		    , &States[S_MNTR_LOOK]),

#define S_MNTR_FADEOUT (S_MNTR_FADEIN+3)
	S_NORMAL (MNTR, 'E',	6, NULL					    , &States[S_MNTR_FADEOUT+1]),
	S_NORMAL (MNTR, 'E',	2, A_Scream				    , &States[S_MNTR_FADEOUT+2]),
	S_NORMAL (MNTR, 'E',	5, A_SmokePuffExit		    , &States[S_MNTR_FADEOUT+3]),
	S_NORMAL (MNTR, 'E',	5, NULL					    , &States[S_MNTR_FADEOUT+4]),
	S_NORMAL (MNTR, 'E',	5, A_NoBlocking			    , &States[S_MNTR_FADEOUT+5]),
	S_NORMAL (MNTR, 'E',	5, NULL					    , &States[S_MNTR_FADEOUT+6]),
	S_NORMAL (MNTR, 'E',	5, A_MinotaurFade1		    , &States[S_MNTR_FADEOUT+7]),
	S_NORMAL (MNTR, 'E',	5, A_MinotaurFade0		    , &States[S_MNTR_FADEOUT+8]),
	S_NORMAL (MNTR, 'E',   10, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMinotaur, Heretic, 9, 0)
	PROP_SpawnHealth (3000)
	PROP_RadiusFixed (28)
	PROP_HeightFixed (100)
	PROP_Mass (800)
	PROP_SpeedFixed (16)
	PROP_Damage (7)
	PROP_PainChance (25)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_DROPOFF)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_BOSS|MF2_PUSHWALL)
	PROP_Flags3 (MF3_NORADIUSDMG|MF3_DONTMORPH|MF3_NOTARGET)

	PROP_SpawnState (S_MNTR_LOOK)
	PROP_SeeState (S_MNTR_WALK)
	PROP_PainState (S_MNTR_PAIN)
	PROP_MeleeState (S_MNTR_ATK1)
	PROP_MissileState (S_MNTR_ATK2)
	PROP_DeathState (S_MNTR_DIE)

	PROP_SeeSound ("minotaur/sight")
	PROP_AttackSound ("minotaur/attack1")
	PROP_PainSound ("minotaur/pain")
	PROP_DeathSound ("minotaur/death")
	PROP_ActiveSound ("minotaur/active")
END_DEFAULTS

void AMinotaur::Tick ()
{
	Super::Tick ();
	
	// The unfriendly Minotaur (Heretic's) is invulnerable while charging
	if (!IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		// Get MF_SKULLFLY bit and shift it so it matches MF2_INVULNERABLE
		DWORD flying = (flags & MF_SKULLFLY) << 3;
		if ((flags2 & MF2_INVULNERABLE) != flying)
		{
			flags2 ^= MF2_INVULNERABLE;
		}
	}
}

void AMinotaur::NoBlockingSet ()
{
	P_DropItem (this, "ArtiSuperHealth", 0, 51);
	P_DropItem (this, "PhoenixRodAmmo", 10, 84);
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

IMPLEMENT_STATELESS_ACTOR (AMinotaurFriend, Hexen, -1, 0)
	PROP_SpawnState (S_MNTR_FADEIN)
	PROP_DeathState (S_MNTR_FADEOUT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (OPAQUE/3)

	PROP_SpawnHealth (2500)
	PROP_FlagsClear (MF_DROPOFF|MF_COUNTKILL)
	PROP_FlagsSet (MF_FRIENDLY)
	PROP_Flags2Clear (MF2_BOSS)
	PROP_Flags2Set (MF2_TELESTOMP)
	PROP_Flags3Set (MF3_STAYMORPHED)
	PROP_Flags3Clear (MF3_DONTMORPH)
	PROP_Flags4 (MF4_NOTARGETSWITCH)
END_DEFAULTS

AT_GAME_SET (Minotaur)
{
	// Modify the Minotaur to accomodate the difference in Hexen's graphics.
	// (Hexen's has fewer frames than Heretic's.) Sprites have not been loaded
	// yet, so check the wad for their presence.
	if (Wads.CheckNumForName ("MNTRZ1", ns_sprites) < 0 &&
		Wads.CheckNumForName ("MNTRZ0", ns_sprites) < 0)
	{
		AMinotaur::States[S_MNTR_ATK1+0].SetFrame ('G');
		AMinotaur::States[S_MNTR_ATK1+1].SetFrame ('H');
		AMinotaur::States[S_MNTR_ATK1+2].SetFrame ('I');
		AMinotaur::States[S_MNTR_ATK2+0].SetFrame ('G');
		AMinotaur::States[S_MNTR_ATK2+1].SetFrame ('J');
		AMinotaur::States[S_MNTR_ATK2+2].SetFrame ('K');
		AMinotaur::States[S_MNTR_ATK3+0].SetFrame ('G');
		AMinotaur::States[S_MNTR_ATK3+1].SetFrame ('H');
		AMinotaur::States[S_MNTR_ATK3+2].SetFrame ('I');
		AMinotaur::States[S_MNTR_ATK3+3].SetFrame ('I');
		AMinotaur::States[S_MNTR_ATK4+0].SetFrame ('F');

		RUNTIME_CLASS(AMinotaur)->ActorInfo->ChangeState(NAME_Death, &AMinotaur::States[S_MNTR_FADEOUT]);
	}
}

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
		if ((link->IsKindOf (RUNTIME_CLASS(AMinotaur))) &&
			(link->tracer == tracer))
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
			AInventory *power = tracer->FindInventory (RUNTIME_CLASS(APowerMinotaur));
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

void AMinotaurFriend::NoBlockingSet ()
{
	// Do not drop anything
}

// Minotaur FX 1 ------------------------------------------------------------

class AMinotaurFX1 : public AActor
{
	DECLARE_ACTOR (AMinotaurFX1, AActor)
};

FState AMinotaurFX1::States[] =
{
#define S_MNTRFX1 0
	S_BRIGHT (FX12, 'A',	6, NULL 						, &States[S_MNTRFX1+1]),
	S_BRIGHT (FX12, 'B',	6, NULL 						, &States[S_MNTRFX1+0]),

#define S_MNTRFXI1 (S_MNTRFX1+2)
	S_BRIGHT (FX12, 'C',	5, NULL 						, &States[S_MNTRFXI1+1]),
	S_BRIGHT (FX12, 'D',	5, NULL 						, &States[S_MNTRFXI1+2]),
	S_BRIGHT (FX12, 'E',	5, NULL 						, &States[S_MNTRFXI1+3]),
	S_BRIGHT (FX12, 'F',	5, NULL 						, &States[S_MNTRFXI1+4]),
	S_BRIGHT (FX12, 'G',	5, NULL 						, &States[S_MNTRFXI1+5]),
	S_BRIGHT (FX12, 'H',	5, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AMinotaurFX1, Raven, -1, 0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (20)
	PROP_Damage (3)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_MNTRFX1)
	PROP_DeathState (S_MNTRFXI1)
END_DEFAULTS

AT_SPEED_SET (MinotaurFX1, speed)
{
	SimpleSpeedSetter (AMinotaurFX1, 20*FRACUNIT, 26*FRACUNIT, speed);
}

// Minotaur FX 2 ------------------------------------------------------------

class AMinotaurFX2 : public AMinotaurFX1
{
	DECLARE_ACTOR (AMinotaurFX2, AMinotaurFX1)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource)
	{
		damage = 24;
	}
};

FState AMinotaurFX2::States[] =
{
#define S_MNTRFX2 0
	S_NORMAL (FX13, 'A',	2, A_MntrFloorFire				, &States[S_MNTRFX2+0]),

#define S_MNTRFXI2 (S_MNTRFX2+1)
	S_BRIGHT (FX13, 'I',	4, A_Explode					, &States[S_MNTRFXI2+1]),
	S_BRIGHT (FX13, 'J',	4, NULL 						, &States[S_MNTRFXI2+2]),
	S_BRIGHT (FX13, 'K',	4, NULL 						, &States[S_MNTRFXI2+3]),
	S_BRIGHT (FX13, 'L',	4, NULL 						, &States[S_MNTRFXI2+4]),
	S_BRIGHT (FX13, 'M',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AMinotaurFX2, Raven, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (12)
	PROP_SpeedFixed (14)
	PROP_Damage (4)
	PROP_Flags3 (MF3_FLOORHUGGER)

	PROP_SpawnState (S_MNTRFX2)
	PROP_DeathState (S_MNTRFXI2)

	PROP_DeathSound ("minotaur/fx2hit")
END_DEFAULTS

AT_SPEED_SET (MinotaurFX2, speed)
{
	SimpleSpeedSetter (AMinotaurFX2, 14*FRACUNIT, 20*FRACUNIT, speed);
}

// Minotaur FX 3 ------------------------------------------------------------

class AMinotaurFX3 : public AMinotaurFX2
{
	DECLARE_ACTOR (AMinotaurFX3, AMinotaurFX2)
public:
	void GetExplodeParms (int &damage, int &distance, bool &hurtSource)
	{
		damage = 128;
	}
};

FState AMinotaurFX3::States[] =
{
#define S_MNTRFX3 0
	S_BRIGHT (FX13, 'D',	4, NULL 						, &States[S_MNTRFX3+1]),
	S_BRIGHT (FX13, 'C',	4, NULL 						, &States[S_MNTRFX3+2]),
	S_BRIGHT (FX13, 'B',	5, NULL 						, &States[S_MNTRFX3+3]),
	S_BRIGHT (FX13, 'C',	5, NULL 						, &States[S_MNTRFX3+4]),
	S_BRIGHT (FX13, 'D',	5, NULL 						, &States[S_MNTRFX3+5]),
	S_BRIGHT (FX13, 'E',	5, NULL 						, &States[S_MNTRFX3+6]),
	S_BRIGHT (FX13, 'F',	4, NULL 						, &States[S_MNTRFX3+7]),
	S_BRIGHT (FX13, 'G',	4, NULL 						, &States[S_MNTRFX3+8]),
	S_BRIGHT (FX13, 'H',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AMinotaurFX3, Raven, -1, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (16)
	PROP_SpeedFixed (0)

	PROP_SpawnState (S_MNTRFX3)

	PROP_DeathSound ("minotaur/fx3hit")
END_DEFAULTS

// Minotaur Smoke Exit ------------------------------------------------------

class AMinotaurSmokeExit : public AActor
{
	DECLARE_ACTOR (AMinotaurSmokeExit, AActor)
};

FState AMinotaurSmokeExit::States[] =
{
	S_NORMAL (MNSM, 'A',	3, NULL					    , &States[1]),
	S_NORMAL (MNSM, 'B',	3, NULL					    , &States[2]),
	S_NORMAL (MNSM, 'C',	3, NULL					    , &States[3]),
	S_NORMAL (MNSM, 'D',	3, NULL					    , &States[4]),
	S_NORMAL (MNSM, 'E',	3, NULL					    , &States[5]),
	S_NORMAL (MNSM, 'F',	3, NULL					    , &States[6]),
	S_NORMAL (MNSM, 'G',	3, NULL					    , &States[7]),
	S_NORMAL (MNSM, 'H',	3, NULL					    , &States[8]),
	S_NORMAL (MNSM, 'I',	3, NULL					    , &States[9]),
	S_NORMAL (MNSM, 'J',	3, NULL					    , &States[10]),
	S_NORMAL (MNSM, 'I',	3, NULL					    , &States[11]),
	S_NORMAL (MNSM, 'H',	3, NULL					    , &States[12]),
	S_NORMAL (MNSM, 'G',	3, NULL					    , &States[13]),
	S_NORMAL (MNSM, 'F',	3, NULL					    , &States[14]),
	S_NORMAL (MNSM, 'E',	3, NULL					    , &States[15]),
	S_NORMAL (MNSM, 'D',	3, NULL					    , &States[16]),
	S_NORMAL (MNSM, 'C',	3, NULL					    , &States[17]),
	S_NORMAL (MNSM, 'B',	3, NULL					    , &States[18]),
	S_NORMAL (MNSM, 'A',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMinotaurSmokeExit, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Action functions for the minotaur ----------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk1
//
// Melee attack.
//
//----------------------------------------------------------------------------

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
	bool friendly = actor->IsKindOf(RUNTIME_CLASS(AMinotaurFriend));
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
		actor->SetStateNF (&AMinotaur::States[S_MNTR_ATK4]);
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
		actor->SetState (&AMinotaur::States[S_MNTR_ATK3]);
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
	bool friendly = actor->IsKindOf(RUNTIME_CLASS(AMinotaurFriend));

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
	mo = P_SpawnMissileZ (actor, z, actor->target, RUNTIME_CLASS(AMinotaurFX1));
	if (mo != NULL)
	{
//		S_Sound (mo, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM);
		momz = mo->momz;
		angle = mo->angle;
		P_SpawnMissileAngleZ (actor, z, RUNTIME_CLASS(AMinotaurFX1), angle-(ANG45/8), momz);
		P_SpawnMissileAngleZ (actor, z, RUNTIME_CLASS(AMinotaurFX1), angle+(ANG45/8), momz);
		P_SpawnMissileAngleZ (actor, z, RUNTIME_CLASS(AMinotaurFX1), angle-(ANG45/16), momz);
		P_SpawnMissileAngleZ (actor, z, RUNTIME_CLASS(AMinotaurFX1), angle+(ANG45/16), momz);
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
	bool friendly = actor->IsKindOf(RUNTIME_CLASS(AMinotaurFriend));

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
		mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(AMinotaurFX2));
		if (mo != NULL)
		{
			S_Sound (mo, CHAN_WEAPON, "minotaur/attack1", 1, ATTN_NORM);
		}
	}
	if (pr_minotauratk3() < 192 && actor->special2 == 0)
	{
		actor->SetState (&AMinotaur::States[S_MNTR_ATK3+3]);
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
	mo = Spawn<AMinotaurFX3> (x, y, ONFLOORZ, ALLOW_REPLACE);
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

void A_MinotaurFade0 (AActor *actor)
{
	actor->RenderStyle = STYLE_Translucent;
	actor->alpha = OPAQUE/3;
}

void A_MinotaurFade1 (AActor *actor)
{
	// Second level of transparency
	actor->RenderStyle = STYLE_Translucent;
	actor->alpha = OPAQUE*2/3;
}

void A_MinotaurFade2 (AActor *actor)
{
	// Make fully visible
	actor->RenderStyle = STYLE_Normal;
}

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
			if ((mo->IsKindOf (RUNTIME_CLASS(AMinotaur))) &&
				(mo->tracer == master)) continue;
			actor->target = mo;
			break;			// Found actor to attack
		}
	}

	if (actor->target)
	{
		actor->SetStateNF (&AMinotaur::States[S_MNTR_WALK]);
	}
	else
	{
		actor->SetStateNF (&AMinotaur::States[S_MNTR_ROAM]);
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
		actor->SetState (&AMinotaur::States[S_MNTR_LOOK]);
		return;
	}

	FaceMovementDirection (actor);
	actor->reactiontime = 0;

	// Melee attack
	if (actor->MeleeState && actor->CheckMeleeRange ())
	{
		if (actor->AttackSound)
		{
			S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
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

/*
void A_SmokePuffEntry(mobj_t *actor)
{
	P_SpawnMobj(actor->x, actor->y, actor->z, MT_MNTRSMOKE);
}
*/

void A_SmokePuffExit (AActor *actor)
{
	Spawn<AMinotaurSmokeExit> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
}
