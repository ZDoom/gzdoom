#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "a_pickups.h"
#include "p_local.h"
#include "a_hereticglobal.h"
#include "a_sharedglobal.h"
#include "p_enemy.h"
#include "d_event.h"
#include "ravenshared.h"
#include "gstrings.h"

static FRandom pr_snoutattack ("SnoutAttack");
static FRandom pr_pigattack ("PigAttack");

extern void AdjustPlayerAngle (AActor *);

void A_SnoutAttack (AActor *actor, pspdef_t *);

void A_PigPain (AActor *);
void A_PigLook (AActor *);
void A_PigChase (AActor *);
void A_PigAttack (AActor *);

// Snout puff ---------------------------------------------------------------

class ASnoutPuff : public AActor
{
	DECLARE_ACTOR (ASnoutPuff, AActor)
};

FState ASnoutPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL 					, &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL 					, &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL 					, &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL 					, &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ASnoutPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
	PROP_SpawnState (0)
END_DEFAULTS

// Snout --------------------------------------------------------------------

class ASnout : public AWeapon
{
	DECLARE_ACTOR (ASnout, AWeapon)
public:
	weapontype_t OldStyleID () const
	{
		return wp_snout;
	}
	static FWeaponInfo WeaponInfo;
};

FState ASnout::States[] =
{
#define S_SNOUTREADY 0
	S_NORMAL (WPIG, 'A',	1, A_WeaponReady		    , &States[S_SNOUTREADY]),

#define S_SNOUTDOWN 1
	S_NORMAL (WPIG, 'A',	1, A_Lower				    , &States[S_SNOUTDOWN]),

#define S_SNOUTUP 2
	S_NORMAL (WPIG, 'A',	1, A_Raise				    , &States[S_SNOUTUP]),

#define S_SNOUTATK 3
	S_NORMAL (WPIG, 'A',	4, A_SnoutAttack		    , &States[S_SNOUTATK+1]),
	S_NORMAL (WPIG, 'B',	8, A_SnoutAttack		    , &States[S_SNOUTREADY])
};

FWeaponInfo ASnout::WeaponInfo =
{
	WIF_DONTBOB,
	am_noammo,
	am_noammo,
	0,
	0,
	&States[S_SNOUTUP],
	&States[S_SNOUTDOWN],
	&States[S_SNOUTREADY],
	&States[S_SNOUTATK],
	&States[S_SNOUTATK],
	NULL,
	NULL,
	150,
	10*FRACUNIT,
	NULL,
	NULL,
	RUNTIME_CLASS(ASnout),
	-1
};

IMPLEMENT_ACTOR (ASnout, Hexen, -1, 0)
END_DEFAULTS

WEAPON1 (wp_snout, ASnout)

// Pig player ---------------------------------------------------------------

class APigPlayer : public APlayerPawn
{
	DECLARE_ACTOR (APigPlayer, APlayerPawn)
public:
	fixed_t GetJumpZ () { return 6*FRACUNIT; }
	void TweakSpeeds (int &forward, int &side);
	int GetArmorMax () { return 1; }
	int GetAutoArmorSave () { return 0; }
	fixed_t GetArmorIncrement (int armortype) { return 0; }
};

FState APigPlayer::States[] =
{
#define S_PIGPLAY 0
	S_NORMAL (PIGY, 'A',   -1, NULL					    , NULL),

#define S_PIGPLAY_RUN1 (S_PIGPLAY+1)
	S_NORMAL (PIGY, 'A',	3, NULL					    , &States[S_PIGPLAY_RUN1+1]),
	S_NORMAL (PIGY, 'B',	3, NULL					    , &States[S_PIGPLAY_RUN1+2]),
	S_NORMAL (PIGY, 'C',	3, NULL					    , &States[S_PIGPLAY_RUN1+3]),
	S_NORMAL (PIGY, 'D',	3, NULL					    , &States[S_PIGPLAY_RUN1]),

#define S_PIGPLAY_PAIN (S_PIGPLAY_RUN1+4)
	S_NORMAL (PIGY, 'D',	4, A_PigPain			    , &States[S_PIGPLAY]),

#define S_PIGPLAY_ATK1 (S_PIGPLAY_PAIN+1)
	S_NORMAL (PIGY, 'A',   12, NULL					    , &States[S_PIGPLAY]),

#define S_PIGPLAY_DIE1 (S_PIGPLAY_ATK1+1)
	S_NORMAL (PIGY, 'E',	4, A_Scream				    , &States[S_PIGPLAY_DIE1+1]),
	S_NORMAL (PIGY, 'F',	3, A_NoBlocking			    , &States[S_PIGPLAY_DIE1+2]),
	S_NORMAL (PIGY, 'G',	4, NULL					    , &States[S_PIGPLAY_DIE1+3]),
	S_NORMAL (PIGY, 'H',	3, NULL					    , &States[S_PIGPLAY_DIE1+4]),
	S_NORMAL (PIGY, 'I',	4, NULL					    , &States[S_PIGPLAY_DIE1+5]),
	S_NORMAL (PIGY, 'J',	4, NULL					    , &States[S_PIGPLAY_DIE1+6]),
	S_NORMAL (PIGY, 'K',	4, NULL					    , &States[S_PIGPLAY_DIE1+7]),
	S_NORMAL (PIGY, 'L',   -1, NULL					    , NULL),

#define S_PIGPLAY_ICE (S_PIGPLAY_DIE1+8)
	S_NORMAL (PIGY, 'M',	5, A_FreezeDeath		    , &States[S_PIGPLAY_ICE+1]),
	S_NORMAL (PIGY, 'M',	1, A_FreezeDeathChunks	    , &States[S_PIGPLAY_ICE+1]),

};

IMPLEMENT_ACTOR (APigPlayer, Hexen, -1, 0)
	PROP_SpawnHealth (100)
	PROP_ReactionTime (0)
	PROP_PainChance (255)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (24)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_NOTDMATCH)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)

	PROP_SpawnState (S_PIGPLAY)
	PROP_SeeState (S_PIGPLAY_RUN1)
	PROP_PainState (S_PIGPLAY_PAIN)
	PROP_MissileState (S_PIGPLAY_ATK1)
	PROP_DeathState (S_PIGPLAY_DIE1)
	PROP_IDeathState (S_PIGPLAY_ICE)

	PROP_PainSound ("PigPain")
	PROP_DeathSound ("PigDeath")
END_DEFAULTS

void APigPlayer::TweakSpeeds (int &forward, int &side)
{
	// Yes, the pig is faster than a mage.
	if ((unsigned int)(forward + 0x31ff) < 0x63ff)
	{
		forward = forward * 0x18 / 0x19;
	}
	else
	{
		forward = forward * 0x31 / 0x32;
	}
	if ((unsigned int)(side + 0x27ff) < 0x4fff)
	{
		side = side * 0x17 / 0x18;
	}
	else
	{
		side = side * 0x27 / 0x28;
	}
}

// Pig (non-player) ---------------------------------------------------------

class APig : public AActor
{
	DECLARE_ACTOR (APig, AActor)
public:
	void Destroy ();
	const char *GetObituary ();
};

FState APig::States[] =
{
#define S_PIG_LOOK1 0
	S_NORMAL (PIGY, 'B',   10, A_PigLook			    , &States[S_PIG_LOOK1]),

#define S_PIG_WALK1 (S_PIG_LOOK1+1)
	S_NORMAL (PIGY, 'A',	3, A_PigChase			    , &States[S_PIG_WALK1+1]),
	S_NORMAL (PIGY, 'B',	3, A_PigChase			    , &States[S_PIG_WALK1+2]),
	S_NORMAL (PIGY, 'C',	3, A_PigChase			    , &States[S_PIG_WALK1+3]),
	S_NORMAL (PIGY, 'D',	3, A_PigChase			    , &States[S_PIG_WALK1]),

#define S_PIG_PAIN (S_PIG_WALK1+4)
	S_NORMAL (PIGY, 'D',	4, A_PigPain			    , &States[S_PIG_WALK1]),

#define S_PIG_ATK1 (S_PIG_PAIN+1)
	S_NORMAL (PIGY, 'A',	5, A_FaceTarget			    , &States[S_PIG_ATK1+1]),
	S_NORMAL (PIGY, 'A',   10, A_PigAttack			    , &States[S_PIG_WALK1]),

#define S_PIG_DIE1 (S_PIG_ATK1+2)
	S_NORMAL (PIGY, 'E',	4, A_Scream				    , &States[S_PIG_DIE1+1]),
	S_NORMAL (PIGY, 'F',	3, A_NoBlocking			    , &States[S_PIG_DIE1+2]),
	S_NORMAL (PIGY, 'G',	4, A_QueueCorpse		    , &States[S_PIG_DIE1+3]),
	S_NORMAL (PIGY, 'H',	3, NULL					    , &States[S_PIG_DIE1+4]),
	S_NORMAL (PIGY, 'I',	4, NULL					    , &States[S_PIG_DIE1+5]),
	S_NORMAL (PIGY, 'J',	4, NULL					    , &States[S_PIG_DIE1+6]),
	S_NORMAL (PIGY, 'K',	4, NULL					    , &States[S_PIG_DIE1+7]),
	S_NORMAL (PIGY, 'L',   -1, NULL					    , NULL),

#define S_PIG_ICE (S_PIG_DIE1+8)
	S_NORMAL (PIGY, 'M',	5, A_FreezeDeath		    , &States[S_PIG_ICE+1]),
	S_NORMAL (PIGY, 'M',	1, A_FreezeDeathChunks	    , &States[S_PIG_ICE+1]),

};

IMPLEMENT_ACTOR (APig, Hexen, -1, 0)
	PROP_SpawnHealth (25)
	PROP_PainChance (128)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (22)
	PROP_Mass (60)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH)

	PROP_SpawnState (S_PIG_LOOK1)
	PROP_SeeState (S_PIG_WALK1)
	PROP_PainState (S_PIG_PAIN)
	PROP_MeleeState (S_PIG_ATK1)
	PROP_DeathState (S_PIG_DIE1)
	PROP_IDeathState (S_PIG_ICE)

	PROP_SeeSound ("PigActive1")
	PROP_PainSound ("PigPain")
	PROP_DeathSound ("PigDeath")
	PROP_ActiveSound ("PigActive1")
END_DEFAULTS

void APig::Destroy ()
{
	if (tracer != NULL)
	{
		tracer->Destroy ();
	}
	Super::Destroy ();
}

const char *APig::GetObituary ()
{
	//return GStrings(OB_CHICKEN);
	return Super::GetObituary ();
}

//============================================================================
//
// A_SnoutAttack
//
//============================================================================

void A_SnoutAttack (AActor *actor, pspdef_t *psp)
{
	angle_t angle;
	int damage;
	int slope;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	damage = 3+(pr_snoutattack()&3);
	angle = player->mo->angle;
	slope = P_AimLineAttack(player->mo, angle, MELEERANGE);
	PuffType = RUNTIME_CLASS(ASnoutPuff);
	PuffSpawned = NULL;
	P_LineAttack(player->mo, angle, MELEERANGE, slope, damage);
	S_Sound(player->mo, CHAN_VOICE, "PigActive", 1, ATTN_NORM);
	if(linetarget)
	{
		AdjustPlayerAngle(player->mo);
		if(PuffSpawned)
		{ // Bit something
			S_Sound(player->mo, CHAN_VOICE, "PigAttack", 1, ATTN_NORM);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_PigLook
//
//----------------------------------------------------------------------------

void A_PigLook (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 10))
	{
		return;
	}
	A_Look (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_PigChase
//
//----------------------------------------------------------------------------

void A_PigChase (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 3))
	{
		return;
	}
	A_Chase(actor);
}

//============================================================================
//
// A_PigAttack
//
//============================================================================

void A_PigAttack (AActor *actor)
{
	if (P_UpdateMorphedMonster (actor, 18))
	{
		return;
	}
	if (!actor->target)
	{
		return;
	}
	if (P_CheckMeleeRange (actor))
	{
		P_DamageMobj(actor->target, actor, actor, 2+(pr_pigattack()&1));
		S_Sound(actor, CHAN_WEAPON, "PigAttack", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_PigPain
//
//============================================================================

void A_PigPain (AActor *actor)
{
	A_Pain (actor);
	if (actor->z <= actor->floorz)
	{
		actor->momz = FRACUNIT*7/2;
	}
}
