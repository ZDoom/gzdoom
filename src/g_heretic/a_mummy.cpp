#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

void A_MummyAttack (AActor *);
void A_MummySoul (AActor *);
void A_MummyAttack2 (AActor *);
void A_MummyFXSound (AActor *);
void A_MummyFX1Seek (AActor *);

// Mummy --------------------------------------------------------------------

class AMummy : public AActor
{
	DECLARE_ACTOR (AMummy, AActor)
public:
	void NoBlockingSet ();
	const char *GetHitObituary ();
};

FState AMummy::States[] =
{
#define S_MUMMY_LOOK 0
	S_NORMAL (MUMM, 'A',   10, A_Look					, &States[S_MUMMY_LOOK+1]),
	S_NORMAL (MUMM, 'B',   10, A_Look					, &States[S_MUMMY_LOOK+0]),

#define S_MUMMY_WALK (S_MUMMY_LOOK+2)
	S_NORMAL (MUMM, 'A',	4, A_Chase					, &States[S_MUMMY_WALK+1]),
	S_NORMAL (MUMM, 'B',	4, A_Chase					, &States[S_MUMMY_WALK+2]),
	S_NORMAL (MUMM, 'C',	4, A_Chase					, &States[S_MUMMY_WALK+3]),
	S_NORMAL (MUMM, 'D',	4, A_Chase					, &States[S_MUMMY_WALK+0]),

#define S_MUMMY_ATK (S_MUMMY_WALK+4)
	S_NORMAL (MUMM, 'E',	6, A_FaceTarget 			, &States[S_MUMMY_ATK+1]),
	S_NORMAL (MUMM, 'F',	6, A_MummyAttack			, &States[S_MUMMY_ATK+2]),
	S_NORMAL (MUMM, 'G',	6, A_FaceTarget 			, &States[S_MUMMY_WALK+0]),

#define S_MUMMY_PAIN (S_MUMMY_ATK+3)
	S_NORMAL (MUMM, 'H',	4, NULL 					, &States[S_MUMMY_PAIN+1]),
	S_NORMAL (MUMM, 'H',	4, A_Pain					, &States[S_MUMMY_WALK+0]),

#define S_MUMMY_DIE (S_MUMMY_PAIN+2)
	S_NORMAL (MUMM, 'I',	5, NULL 					, &States[S_MUMMY_DIE+1]),
	S_NORMAL (MUMM, 'J',	5, A_Scream 				, &States[S_MUMMY_DIE+2]),
	S_NORMAL (MUMM, 'K',	5, A_MummySoul				, &States[S_MUMMY_DIE+3]),
	S_NORMAL (MUMM, 'L',	5, NULL 					, &States[S_MUMMY_DIE+4]),
	S_NORMAL (MUMM, 'M',	5, A_NoBlocking 			, &States[S_MUMMY_DIE+5]),
	S_NORMAL (MUMM, 'N',	5, NULL 					, &States[S_MUMMY_DIE+6]),
	S_NORMAL (MUMM, 'O',	5, NULL 					, &States[S_MUMMY_DIE+7]),
	S_NORMAL (MUMM, 'P',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AMummy, Heretic, 68, 0)
	PROP_SpawnHealth (80)
	PROP_RadiusFixed (22)
	PROP_HeightFixed (62)
	PROP_Mass (75)
	PROP_SpeedFixed (12)
	PROP_PainChance (128)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ)

	PROP_SpawnState (S_MUMMY_LOOK)
	PROP_SeeState (S_MUMMY_WALK)
	PROP_PainState (S_MUMMY_PAIN)
	PROP_MeleeState (S_MUMMY_ATK)
	PROP_DeathState (S_MUMMY_DIE)

	PROP_SeeSound ("mummy/sight")
	PROP_AttackSound ("mummy/attack1")
	PROP_PainSound ("mummy/pain")
	PROP_DeathSound ("mummy/death")
	PROP_ActiveSound ("mummy/active")
END_DEFAULTS

void AMummy::NoBlockingSet ()
{
	P_DropItem (this, "GoldWandAmmo", 3, 84);
}

const char *AMummy::GetHitObituary ()
{
	return GStrings(OB_MUMMY);
}

// Mummy leader -------------------------------------------------------------

class AMummyLeader : public AMummy
{
	DECLARE_ACTOR (AMummyLeader, AMummy)
public:
	const char *GetObituary ();
};

FState AMummyLeader::States[] =
{
#define S_MUMMYL_ATK 0
	S_NORMAL (MUMM, 'X',	5, A_FaceTarget 			, &States[S_MUMMYL_ATK+1]),
	S_BRIGHT (MUMM, 'Y',	5, A_FaceTarget 			, &States[S_MUMMYL_ATK+2]),
	S_NORMAL (MUMM, 'X',	5, A_FaceTarget 			, &States[S_MUMMYL_ATK+3]),
	S_BRIGHT (MUMM, 'Y',	5, A_FaceTarget 			, &States[S_MUMMYL_ATK+4]),
	S_NORMAL (MUMM, 'X',	5, A_FaceTarget 			, &States[S_MUMMYL_ATK+5]),
	S_BRIGHT (MUMM, 'Y',   15, A_MummyAttack2			, &AMummy::States[S_MUMMY_WALK+0]),

};

IMPLEMENT_ACTOR (AMummyLeader, Heretic, 45, 0)
	PROP_SpawnHealth (100)
	PROP_PainChance (64)
	PROP_MissileState (S_MUMMYL_ATK)
END_DEFAULTS

const char *AMummyLeader::GetObituary ()
{
	return GStrings (OB_MUMMYLEADER);
}

// Mummy ghost --------------------------------------------------------------

class AMummyGhost : public AMummy
{
	DECLARE_STATELESS_ACTOR (AMummyGhost, AMummy)
};

IMPLEMENT_STATELESS_ACTOR (AMummyGhost, Heretic, 69, 0)
	PROP_FlagsSet (MF_SHADOW)
	PROP_Flags3 (MF3_GHOST)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
END_DEFAULTS

// Mummy leader ghost -------------------------------------------------------

class AMummyLeaderGhost : public AMummyLeader
{
	DECLARE_STATELESS_ACTOR (AMummyLeaderGhost, AMummyLeader)
};

IMPLEMENT_STATELESS_ACTOR (AMummyLeaderGhost, Heretic, 46, 0)
	PROP_FlagsSet (MF_SHADOW)
	PROP_Flags3 (MF3_GHOST)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
END_DEFAULTS

// Mummy soul ---------------------------------------------------------------

class AMummySoul : public AActor
{
	DECLARE_ACTOR (AMummySoul, AActor)
};

FState AMummySoul::States[] =
{
#define S_MUMMY_SOUL 0
	S_NORMAL (MUMM, 'Q',	5, NULL 					, &States[S_MUMMY_SOUL+1]),
	S_NORMAL (MUMM, 'R',	5, NULL 					, &States[S_MUMMY_SOUL+2]),
	S_NORMAL (MUMM, 'S',	5, NULL 					, &States[S_MUMMY_SOUL+3]),
	S_NORMAL (MUMM, 'T',	9, NULL 					, &States[S_MUMMY_SOUL+4]),
	S_NORMAL (MUMM, 'U',	5, NULL 					, &States[S_MUMMY_SOUL+5]),
	S_NORMAL (MUMM, 'V',	5, NULL 					, &States[S_MUMMY_SOUL+6]),
	S_NORMAL (MUMM, 'W',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AMummySoul, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_SpawnState (S_MUMMY_SOUL)
END_DEFAULTS

// Mummy FX 1 (flying head) -------------------------------------------------

class AMummyFX1 : public AActor
{
	DECLARE_ACTOR (AMummyFX1, AActor)
};

FState AMummyFX1::States[] =
{
#define S_MUMMYFX1 0
	// [RH] Make the mummy scream right away
	S_BRIGHT (FX15, 'A',	1, NULL						, &States[S_MUMMYFX1+1]),
	S_BRIGHT (FX15, 'A',	5, A_MummyFXSound			, &States[S_MUMMYFX1+2]),
	S_BRIGHT (FX15, 'B',	5, A_MummyFX1Seek			, &States[S_MUMMYFX1+3]),
	S_BRIGHT (FX15, 'C',	5, NULL 					, &States[S_MUMMYFX1+4]),
	S_BRIGHT (FX15, 'B',	5, A_MummyFX1Seek			, &States[S_MUMMYFX1+1]),

#define S_MUMMYFXI1 (S_MUMMYFX1+5)
	S_BRIGHT (FX15, 'D',	5, NULL 					, &States[S_MUMMYFXI1+1]),
	S_BRIGHT (FX15, 'E',	5, NULL 					, &States[S_MUMMYFXI1+2]),
	S_BRIGHT (FX15, 'F',	5, NULL 					, &States[S_MUMMYFXI1+3]),
	S_BRIGHT (FX15, 'G',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AMummyFX1, Heretic, -1, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (14)
	PROP_SpeedFixed (9)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_MUMMYFX1)
	PROP_DeathState (S_MUMMYFXI1)
END_DEFAULTS

AT_SPEED_SET (MummyFX1, speed)
{
	SimpleSpeedSetter (AMummyFX1, 9*FRACUNIT, 18*FRACUNIT, speed);
}

//----------------------------------------------------------------------------
//
// PROC A_MummyAttack
//
//----------------------------------------------------------------------------

void A_MummyAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (P_CheckMeleeRange (actor))
	{
		P_DamageMobj (actor->target, actor, actor, HITDICE(2), MOD_HIT);
		S_Sound (actor, CHAN_WEAPON, "mummy/attack2", 1, ATTN_NORM);
		return;
	}
	S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_MummyAttack2
//
// Mummy leader missile attack.
//
//----------------------------------------------------------------------------

void A_MummyAttack2 (AActor *actor)
{
	AActor *mo;

	if (!actor->target)
	{
		return;
	}
	//S_StartSound(actor, actor->info->attacksound);
	if (P_CheckMeleeRange (actor))
	{
		P_DamageMobj (actor->target, actor, actor, HITDICE(2), MOD_HIT);
		return;
	}
	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(AMummyFX1));
	//mo = P_SpawnMissile(actor, actor->target, MT_EGGFX);
	if (mo != NULL)
	{
		mo->tracer = actor->target;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MummyFX1Seek
//
//----------------------------------------------------------------------------

void A_MummyFX1Seek (AActor *actor)
{
	P_SeekerMissile (actor, ANGLE_1*10, ANGLE_1*20);
}

//----------------------------------------------------------------------------
//
// PROC A_MummySoul
//
//----------------------------------------------------------------------------

void A_MummySoul (AActor *mummy)
{
	AActor *mo;

	mo = Spawn<AMummySoul> (mummy->x, mummy->y, mummy->z+10*FRACUNIT);
	mo->momz = FRACUNIT;
}

//----------------------------------------------------------------------------
//
// PROC A_MummyFXSound
//
//----------------------------------------------------------------------------

void A_MummyFXSound (AActor *self)
{
	S_Sound (self, CHAN_BODY, "mummy/head", 1, ATTN_IDLE);
}
