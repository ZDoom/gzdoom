#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"

static FRandom pr_centaurdefend ("CentaurDefend");

static FRandom pr_reflect ("CentaurDeflect");
static FRandom pr_centaurattack ("CentaurAttack");
static FRandom pr_centaurdrop ("CentaurDrop");

void A_CentaurDefend (AActor *);
void A_CentaurAttack (AActor *);
void A_CentaurAttack2 (AActor *);
void A_CentaurDropStuff (AActor *);

// Centaur ------------------------------------------------------------------

FState ACentaur::States[] =
{
#define S_CENTAUR_LOOK1 0
	S_NORMAL (CENT, 'A',   10, A_Look				    , &States[S_CENTAUR_LOOK1+1]),
	S_NORMAL (CENT, 'B',   10, A_Look				    , &States[S_CENTAUR_LOOK1]),

#define S_CENTAUR_WALK1 (S_CENTAUR_LOOK1+2)
	S_NORMAL (CENT, 'A',	4, A_Chase				    , &States[S_CENTAUR_WALK1+1]),
	S_NORMAL (CENT, 'B',	4, A_Chase				    , &States[S_CENTAUR_WALK1+2]),
	S_NORMAL (CENT, 'C',	4, A_Chase				    , &States[S_CENTAUR_WALK1+3]),
	S_NORMAL (CENT, 'D',	4, A_Chase				    , &States[S_CENTAUR_WALK1]),

#define S_CENTAUR_PAIN1 (S_CENTAUR_WALK1+4)
	S_NORMAL (CENT, 'G',	6, A_Pain				    , &States[S_CENTAUR_PAIN1+1]),
	S_NORMAL (CENT, 'G',	6, A_SetReflectiveInvulnerable, &States[S_CENTAUR_PAIN1+2]),
	S_NORMAL (CENT, 'E',   15, A_CentaurDefend		    , &States[S_CENTAUR_PAIN1+3]),
	S_NORMAL (CENT, 'E',   15, A_CentaurDefend		    , &States[S_CENTAUR_PAIN1+4]),
	S_NORMAL (CENT, 'E',   15, A_CentaurDefend		    , &States[S_CENTAUR_PAIN1+5]),
	S_NORMAL (CENT, 'E',	1, A_UnSetReflectiveInvulnerable, &States[S_CENTAUR_WALK1]),

#define S_CENTAUR_ATK1 (S_CENTAUR_PAIN1+6)
	S_NORMAL (CENT, 'H',	5, A_FaceTarget			    , &States[S_CENTAUR_ATK1+1]),
	S_NORMAL (CENT, 'I',	4, A_FaceTarget			    , &States[S_CENTAUR_ATK1+2]),
	S_NORMAL (CENT, 'J',	7, A_CentaurAttack		    , &States[S_CENTAUR_WALK1]),

#define S_CENTAUR_MISSILE1 (S_CENTAUR_ATK1+3)
	S_NORMAL (CENT, 'E',   10, A_FaceTarget			    , &States[S_CENTAUR_MISSILE1+1]),
	S_BRIGHT (CENT, 'F',	8, A_CentaurAttack2		    , &States[S_CENTAUR_MISSILE1+2]),
	S_NORMAL (CENT, 'E',   10, A_FaceTarget			    , &States[S_CENTAUR_MISSILE1+3]),
	S_BRIGHT (CENT, 'F',	8, A_CentaurAttack2		    , &States[S_CENTAUR_WALK1]),

#define S_CENTAUR_DEATH1 (S_CENTAUR_MISSILE1+4)
	S_NORMAL (CENT, 'K',	4, NULL					    , &States[S_CENTAUR_DEATH1+1]),
	S_NORMAL (CENT, 'L',	4, A_Scream				    , &States[S_CENTAUR_DEATH1+2]),
	S_NORMAL (CENT, 'M',	4, NULL					    , &States[S_CENTAUR_DEATH1+3]),
	S_NORMAL (CENT, 'N',	4, NULL					    , &States[S_CENTAUR_DEATH1+4]),
	S_NORMAL (CENT, 'O',	4, A_NoBlocking			    , &States[S_CENTAUR_DEATH1+5]),
	S_NORMAL (CENT, 'P',	4, NULL					    , &States[S_CENTAUR_DEATH1+6]),
	S_NORMAL (CENT, 'Q',	4, NULL					    , &States[S_CENTAUR_DEATH1+7]),
	S_NORMAL (CENT, 'R',	4, A_QueueCorpse		    , &States[S_CENTAUR_DEATH1+8]),
	S_NORMAL (CENT, 'S',	4, NULL					    , &States[S_CENTAUR_DEATH1+9]),
	S_NORMAL (CENT, 'T',   -1, NULL					    , NULL),

#define S_CENTAUR_DEATH_X1 (S_CENTAUR_DEATH1+10)
	S_NORMAL (CTXD, 'A',	4, NULL					    , &States[S_CENTAUR_DEATH_X1+1]),
	S_NORMAL (CTXD, 'B',	4, A_NoBlocking			    , &States[S_CENTAUR_DEATH_X1+2]),
	S_NORMAL (CTXD, 'C',	4, A_CentaurDropStuff	    , &States[S_CENTAUR_DEATH_X1+3]),
	S_NORMAL (CTXD, 'D',	3, A_Scream				    , &States[S_CENTAUR_DEATH_X1+4]),
	S_NORMAL (CTXD, 'E',	4, A_QueueCorpse		    , &States[S_CENTAUR_DEATH_X1+5]),
	S_NORMAL (CTXD, 'F',	3, NULL					    , &States[S_CENTAUR_DEATH_X1+6]),
	S_NORMAL (CTXD, 'G',	4, NULL					    , &States[S_CENTAUR_DEATH_X1+7]),
	S_NORMAL (CTXD, 'H',	3, NULL					    , &States[S_CENTAUR_DEATH_X1+8]),
	S_NORMAL (CTXD, 'I',	4, NULL					    , &States[S_CENTAUR_DEATH_X1+9]),
	S_NORMAL (CTXD, 'J',	3, NULL					    , &States[S_CENTAUR_DEATH_X1+10]),
	S_NORMAL (CTXD, 'K',   -1, NULL					    , NULL),

#define S_CENTAUR_ICE1 (S_CENTAUR_DEATH_X1+11)
	S_NORMAL (CENT, 'U',	5, A_FreezeDeath		    , &States[S_CENTAUR_ICE1+1]),
	S_NORMAL (CENT, 'U',	1, A_FreezeDeathChunks	    , &States[S_CENTAUR_ICE1+1]),

};

IMPLEMENT_ACTOR (ACentaur, Hexen, 107, 1)
	PROP_SpawnHealth (200)
	PROP_PainChance (135)
	PROP_SpeedFixed (13)
	PROP_HeightFixed (64)
	PROP_Mass (120)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)

	PROP_SpawnState (S_CENTAUR_LOOK1)
	PROP_SeeState (S_CENTAUR_WALK1)
	PROP_PainState (S_CENTAUR_PAIN1)
	PROP_MeleeState (S_CENTAUR_ATK1)
	PROP_DeathState (S_CENTAUR_DEATH1)
	PROP_XDeathState (S_CENTAUR_DEATH_X1)
	PROP_IDeathState (S_CENTAUR_ICE1)

	PROP_SeeSound ("CentaurSight")
	PROP_AttackSound ("CentaurAttack")
	PROP_PainSound ("CentaurPain")
	PROP_DeathSound ("CentaurDeath")
	PROP_ActiveSound ("CentaurActive")
END_DEFAULTS

void ACentaur::Howl ()
{
	int howl = S_FindSound ("PuppyBeat");
	if (!S_GetSoundPlayingInfo (this, howl))
	{
		S_SoundID (this, CHAN_BODY, howl, 1, ATTN_NORM);
	}
}

bool ACentaur::AdjustReflectionAngle (AActor *thing, angle_t &angle)
{
	if (abs (angle-BlockingMobj->angle)>>24 > 45)
		return true;	// Let missile explode

	if (thing->IsKindOf (RUNTIME_CLASS(AHolySpirit)))
		return true;

	if (pr_reflect () < 128)
		angle += ANGLE_45;
	else
		angle -= ANGLE_45;

	return false;
}

// Centaur Leader -----------------------------------------------------------

class ACentaurLeader : public ACentaur
{
	DECLARE_STATELESS_ACTOR (ACentaurLeader, ACentaur)
};

IMPLEMENT_STATELESS_ACTOR (ACentaurLeader, Hexen, 115, 2)
	PROP_SpawnHealth (250)
	PROP_PainChance (96)
	PROP_SpeedFixed (10)

	PROP_MissileState (S_CENTAUR_MISSILE1)
END_DEFAULTS

// Mashed centaur -----------------------------------------------------------
//
// The mashed centaur is only placed through ACS. Nowhere in the game source
// is it ever referenced.

class ACentaurMash : public ACentaur
{
	DECLARE_STATELESS_ACTOR (ACentaurMash, ACentaur)
};

IMPLEMENT_STATELESS_ACTOR (ACentaurMash, Hexen, -1, 103)
	PROP_FlagsSet (MF_NOBLOOD)
	PROP_Flags2Set (MF2_BLASTED)
	PROP_Flags2Clear (MF2_TELESTOMP)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_DeathState (~0)
	PROP_XDeathState (~0)
	PROP_IDeathState (~0)
END_DEFAULTS

// Centaur projectile -------------------------------------------------------

class ACentaurFX : public AActor
{
	DECLARE_ACTOR (ACentaurFX, AActor)
};

FState ACentaurFX::States[] =
{
#define S_CENTAUR_FX1 0
	S_BRIGHT (CTFX, 'A',   -1, NULL						, NULL),

#define S_CENTAUR_FX_X1 (S_CENTAUR_FX1+1)
	S_BRIGHT (CTFX, 'B',	4, NULL						, &States[S_CENTAUR_FX_X1+1]),
	S_BRIGHT (CTFX, 'C',	3, NULL						, &States[S_CENTAUR_FX_X1+2]),
	S_BRIGHT (CTFX, 'D',	4, NULL						, &States[S_CENTAUR_FX_X1+3]),
	S_BRIGHT (CTFX, 'E',	3, NULL						, &States[S_CENTAUR_FX_X1+4]),
	S_BRIGHT (CTFX, 'F',	2, NULL						, NULL),
};

IMPLEMENT_ACTOR (ACentaurFX, Hexen, -1, 0)
	PROP_SpeedFixed (20)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CENTAUR_FX1)
	PROP_DeathState (S_CENTAUR_FX_X1)

	PROP_DeathSound ("CentaurMissileExplode")
END_DEFAULTS

// Centaur shield (debris) --------------------------------------------------

class ACentaurShield : public AActor
{
	DECLARE_ACTOR (ACentaurShield, AActor)
};

FState ACentaurShield::States[] =
{
#define S_CENTAUR_SHIELD1 0
	S_NORMAL (CTDP, 'A',	3, NULL						, &States[S_CENTAUR_SHIELD1+1]),
	S_NORMAL (CTDP, 'B',	3, NULL						, &States[S_CENTAUR_SHIELD1+2]),
	S_NORMAL (CTDP, 'C',	3, NULL						, &States[S_CENTAUR_SHIELD1+3]),
	S_NORMAL (CTDP, 'D',	3, NULL						, &States[S_CENTAUR_SHIELD1+4]),
	S_NORMAL (CTDP, 'E',	3, NULL						, &States[S_CENTAUR_SHIELD1+5]),
	S_NORMAL (CTDP, 'F',	3, NULL						, &States[S_CENTAUR_SHIELD1+2]),

#define S_CENTAUR_SHIELD_X1 (S_CENTAUR_SHIELD1+6)
	S_NORMAL (CTDP, 'G',	4, NULL						, &States[S_CENTAUR_SHIELD_X1+1]),
	S_NORMAL (CTDP, 'H',	4, A_QueueCorpse			, &States[S_CENTAUR_SHIELD_X1+2]),
	S_NORMAL (CTDP, 'I',	4, NULL						, &States[S_CENTAUR_SHIELD_X1+3]),
	S_NORMAL (CTDP, 'J',   -1, NULL						, NULL),
};

IMPLEMENT_ACTOR (ACentaurShield, Hexen, -1, 0)
	PROP_Flags (MF_DROPOFF|MF_CORPSE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_CENTAUR_SHIELD1)
	PROP_CrashState (S_CENTAUR_SHIELD_X1)
END_DEFAULTS

// Centaur sword (debris) ---------------------------------------------------

class ACentaurSword : public AActor
{
	DECLARE_ACTOR (ACentaurSword, AActor)
};

FState ACentaurSword::States[] =
{
#define S_CENTAUR_SWORD1 0
	S_NORMAL (CTDP, 'K',	3, NULL						, &States[S_CENTAUR_SWORD1+1]),
	S_NORMAL (CTDP, 'L',	3, NULL						, &States[S_CENTAUR_SWORD1+2]),
	S_NORMAL (CTDP, 'M',	3, NULL						, &States[S_CENTAUR_SWORD1+3]),
	S_NORMAL (CTDP, 'N',	3, NULL						, &States[S_CENTAUR_SWORD1+4]),
	S_NORMAL (CTDP, 'O',	3, NULL						, &States[S_CENTAUR_SWORD1+5]),
	S_NORMAL (CTDP, 'P',	3, NULL						, &States[S_CENTAUR_SWORD1+6]),
	S_NORMAL (CTDP, 'Q',	3, NULL						, &States[S_CENTAUR_SWORD1+2]),

#define S_CENTAUR_SWORD_X1 (S_CENTAUR_SWORD1+7)
	S_NORMAL (CTDP, 'R',	4, NULL						, &States[S_CENTAUR_SWORD_X1+1]),
	S_NORMAL (CTDP, 'S',	4, A_QueueCorpse			, &States[S_CENTAUR_SWORD_X1+2]),
	S_NORMAL (CTDP, 'T',   -1, NULL						, NULL),
};

IMPLEMENT_ACTOR (ACentaurSword, Hexen, -1, 0)
	PROP_Flags (MF_DROPOFF|MF_CORPSE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_CENTAUR_SWORD1)
	PROP_CrashState (S_CENTAUR_SWORD_X1)
END_DEFAULTS

//============================================================================
//
// A_CentaurAttack
//
//============================================================================

void A_CentaurAttack (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	if (P_CheckMeleeRange (actor))
	{
		int damage = pr_centaurattack()%7+3;
		P_DamageMobj (actor->target, actor, actor, damage);
		P_TraceBleed (damage, actor->target, actor);
	}
}

//============================================================================
//
// A_CentaurAttack2
//
//============================================================================

void A_CentaurAttack2 (AActor *actor)
{
	if (!actor->target)
	{
		return;
	}
	P_SpawnMissileZ (actor, actor->z + 45*FRACUNIT,
		actor->target, RUNTIME_CLASS(ACentaurFX));
	S_Sound (actor, CHAN_WEAPON, "CentaurLeaderAttack", 1, ATTN_NORM);
}

//============================================================================
//
// A_CentaurDropStuff
//
// 	Spawn shield/sword sprites when the centaur pulps
//============================================================================

void A_CentaurDropStuff (AActor *actor)
{
	const TypeInfo *const DropTypes[] =
	{
		RUNTIME_CLASS(ACentaurSword),
		RUNTIME_CLASS(ACentaurShield)
	};

	for (int i = sizeof(DropTypes)/sizeof(DropTypes[0])-1; i >= 0; --i)
	{
		AActor *mo;

		mo = Spawn (DropTypes[i], actor->x, actor->y, actor->z+45*FRACUNIT);
		if (mo)
		{
			angle_t angle = actor->angle + (i ? ANGLE_90 : ANGLE_270);
			mo->momz = FRACUNIT*8+(pr_centaurdrop()<<10);
			mo->momx = FixedMul(((pr_centaurdrop()-128)<<11)+FRACUNIT,
				finecosine[angle>>ANGLETOFINESHIFT]);
			mo->momy = FixedMul(((pr_centaurdrop()-128)<<11)+FRACUNIT, 
				finesine[angle>>ANGLETOFINESHIFT]);
			mo->target = actor;
		}
	}
}

//============================================================================
//
// A_CentaurDefend
//
//============================================================================

void A_CentaurDefend (AActor *actor)
{
	A_FaceTarget (actor);
	if (P_CheckMeleeRange(actor) && pr_centaurdefend() < 32)
	{
		A_UnSetInvulnerable (actor);
		actor->SetState (actor->MeleeState);
	}
}
