#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "doomdata.h"
#include "r_translate.h"

void A_BeShadowyFoe (AActor *);
void A_AcolyteBits (AActor *);
void A_AcolyteDie (AActor *);
void A_HideDecepticon (AActor *);

void A_ShootGun (AActor *);
void A_TossGib (AActor *);
void A_ClearShadow (AActor *);
void A_SetShadow (AActor *);

// Base class for the acolytes ----------------------------------------------

class AAcolyte : public AStrifeHumanoid
{
	DECLARE_ACTOR (AAcolyte, AStrifeHumanoid)
};

FState AAcolyte::States[] =
{
#define S_ACOLYTE_STND 0
	S_NORMAL (AGRD, 'A',	5, A_Look2,				&States[S_ACOLYTE_STND]),
	S_NORMAL (AGRD, 'B',	8, A_ClearShadow,		&States[S_ACOLYTE_STND]),
	S_NORMAL (AGRD, 'D',	8, NULL,				&States[S_ACOLYTE_STND]),

#define S_ACOLYTE_WANDER (S_ACOLYTE_STND+3)
	S_NORMAL (AGRD, 'A',	5, A_Wander,			&States[S_ACOLYTE_WANDER+1]),
	S_NORMAL (AGRD, 'B',	5, A_Wander,			&States[S_ACOLYTE_WANDER+2]),
	S_NORMAL (AGRD, 'C',	5, A_Wander,			&States[S_ACOLYTE_WANDER+3]),
	S_NORMAL (AGRD, 'D',	5, A_Wander,			&States[S_ACOLYTE_WANDER+4]),
	S_NORMAL (AGRD, 'A',	5, A_Wander,			&States[S_ACOLYTE_WANDER+5]),
	S_NORMAL (AGRD, 'B',	5, A_Wander,			&States[S_ACOLYTE_WANDER+6]),
	S_NORMAL (AGRD, 'C',	5, A_Wander,			&States[S_ACOLYTE_WANDER+7]),
	S_NORMAL (AGRD, 'D',	5, A_Wander,			&States[S_ACOLYTE_STND]),

#define S_ACOLYTE_ALTCHASE (S_ACOLYTE_WANDER+8)
	S_NORMAL (AGRD, 'A',	6, A_BeShadowyFoe,		&States[S_ACOLYTE_ALTCHASE+2]),

#define S_ACOLYTE_CHASE (S_ACOLYTE_ALTCHASE+1)
	S_NORMAL (AGRD, 'A',	6, A_AcolyteBits,		&States[S_ACOLYTE_CHASE+1]),
	S_NORMAL (AGRD, 'B',	6, A_Chase,				&States[S_ACOLYTE_CHASE+2]),
	S_NORMAL (AGRD, 'C',	6, A_Chase,				&States[S_ACOLYTE_CHASE+3]),
	S_NORMAL (AGRD, 'D',	6, A_Chase,				&States[S_ACOLYTE_CHASE]),

#define S_ACOLYTE_ATK (S_ACOLYTE_CHASE+4)
	S_NORMAL (AGRD, 'E',	8, A_FaceTarget,		&States[S_ACOLYTE_ATK+1]),
	S_NORMAL (AGRD, 'F',	4, A_ShootGun,			&States[S_ACOLYTE_ATK+2]),
	S_NORMAL (AGRD, 'E',	4, A_ShootGun,			&States[S_ACOLYTE_ATK+3]),
	S_NORMAL (AGRD, 'F',	6, A_ShootGun,			&States[S_ACOLYTE_CHASE]),

#define S_ACOLYTE_ALTPAIN (S_ACOLYTE_ATK+4)
	S_NORMAL (AGRD, 'O',	0, A_SetShadow,			&States[S_ACOLYTE_ALTPAIN+1]),
	S_NORMAL (AGRD, 'O',	8, A_Pain,				&States[S_ACOLYTE_ALTCHASE]),

#define S_ACOLYTE_PAIN (S_ACOLYTE_ALTPAIN+2)
	S_NORMAL (AGRD, 'O',	8, A_Pain,				&States[S_ACOLYTE_CHASE]),

#define S_ACOLYTE_DIE (S_ACOLYTE_PAIN+1)
	S_NORMAL (AGRD, 'G',	4, NULL,				&States[S_ACOLYTE_DIE+1]),
	S_NORMAL (AGRD, 'H',	4, A_Scream,			&States[S_ACOLYTE_DIE+2]),
	S_NORMAL (AGRD, 'I',	4, NULL,				&States[S_ACOLYTE_DIE+3]),
	S_NORMAL (AGRD, 'J',	3, NULL,				&States[S_ACOLYTE_DIE+4]),
	S_NORMAL (AGRD, 'K',	3, A_NoBlocking,		&States[S_ACOLYTE_DIE+5]),
	S_NORMAL (AGRD, 'L',	3, NULL,				&States[S_ACOLYTE_DIE+6]),
	S_NORMAL (AGRD, 'M',	3, A_AcolyteDie,		&States[S_ACOLYTE_DIE+7]),
	S_NORMAL (AGRD, 'N', 1400, NULL,				NULL),

#define S_ACOLYTE_XDIE (S_ACOLYTE_DIE+8)
	S_NORMAL (GIBS, 'A',	5, A_NoBlocking,		&States[S_ACOLYTE_XDIE+1]),
	S_NORMAL (GIBS, 'B',	5, A_TossGib,			&States[S_ACOLYTE_XDIE+2]),
	S_NORMAL (GIBS, 'C',	5, A_TossGib,			&States[S_ACOLYTE_XDIE+3]),
	S_NORMAL (GIBS, 'D',	4, A_TossGib,			&States[S_ACOLYTE_XDIE+4]),
	S_NORMAL (GIBS, 'E',	4, A_XScream,			&States[S_ACOLYTE_XDIE+5]),
	S_NORMAL (GIBS, 'F',	4, A_TossGib,			&States[S_ACOLYTE_XDIE+6]),
	S_NORMAL (GIBS, 'G',	4, NULL,				&States[S_ACOLYTE_XDIE+7]),
	S_NORMAL (GIBS, 'H',	4, NULL,				&States[S_ACOLYTE_XDIE+8]),
	S_NORMAL (GIBS, 'I',	5, NULL,				&States[S_ACOLYTE_XDIE+9]),
	S_NORMAL (GIBS, 'J',	5, A_AcolyteDie,		&States[S_ACOLYTE_XDIE+10]),
	S_NORMAL (GIBS, 'K',	5, NULL,				&States[S_ACOLYTE_XDIE+11]),
	S_NORMAL (GIBS, 'L', 1400, NULL,				NULL),
};

IMPLEMENT_ACTOR (AAcolyte, Strife, -1, 0)
	PROP_SpawnState (S_ACOLYTE_STND)
	PROP_SeeState (S_ACOLYTE_CHASE)
	PROP_PainState (S_ACOLYTE_PAIN)
	PROP_MissileState (S_ACOLYTE_ATK)
	PROP_DeathState (S_ACOLYTE_DIE)
	PROP_XDeathState (S_ACOLYTE_XDIE)

	PROP_SpawnHealth (70)
	PROP_PainChance (150)
	PROP_SpeedFixed (7)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (64)
	PROP_Mass (400)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_MinMissileChance (150)
	PROP_Tag ("ACOLYTE")

	PROP_SeeSound ("acolyte/sight")
	PROP_PainSound ("acolyte/pain")
	PROP_AttackSound ("acolyte/rifle")
	PROP_DeathSound ("acolyte/death")
	PROP_ActiveSound ("acolyte/active")
	PROP_Obituary ("$OB_ACOLYTE")
END_DEFAULTS

// Acolyte 1 ----------------------------------------------------------------

class AAcolyteTan : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteTan, AAcolyte)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteTan, Strife, 3002, 0)
	PROP_StrifeType (53)
	PROP_StrifeTeaserType (52)
	PROP_StrifeTeaserType2 (53)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

//============================================================================
//
// AAcolyteTan :: NoBlockingSet
//
// This and the shadow acolyte are the only ones that drop clips by default.
//
//============================================================================

void AAcolyteTan::NoBlockingSet ()
{
	P_DropItem (this, "ClipOfBullets", -1, 256);
}

// Acolyte 2 ----------------------------------------------------------------

class AAcolyteRed : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteRed, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteRed, Strife, 142, 0)
	PROP_Translation (TRANSLATION_Standard, 0)
	PROP_StrifeType (54)
	PROP_StrifeTeaserType (53)
	PROP_StrifeTeaserType2 (54)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 3 ----------------------------------------------------------------

class AAcolyteRust : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteRust, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteRust, Strife, 143, 0)
	PROP_Translation (TRANSLATION_Standard, 1)
	PROP_StrifeType (55)
	PROP_StrifeTeaserType (54)
	PROP_StrifeTeaserType2 (55)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 4 ----------------------------------------------------------------

class AAcolyteGray : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteGray, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteGray, Strife, 146, 0)
	PROP_Translation (TRANSLATION_Standard, 2)
	PROP_StrifeType (56)
	PROP_StrifeTeaserType (55)
	PROP_StrifeTeaserType2 (56)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 5 ----------------------------------------------------------------

class AAcolyteDGreen : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteDGreen, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteDGreen, Strife, 147, 0)
	PROP_Translation (TRANSLATION_Standard, 3)
	PROP_StrifeType (57)
	PROP_StrifeTeaserType (56)
	PROP_StrifeTeaserType2 (57)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 6 ----------------------------------------------------------------

class AAcolyteGold : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteGold, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteGold, Strife, 148, 0)
	PROP_Translation (TRANSLATION_Standard, 4)
	PROP_StrifeType (58)
	PROP_StrifeTeaserType (57)
	PROP_StrifeTeaserType2 (58)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_MISSILEEVENMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 7 ----------------------------------------------------------------

class AAcolyteLGreen : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteLGreen, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteLGreen, Strife, 232, 0)
	PROP_Translation (TRANSLATION_Standard, 5)
	PROP_SpawnHealth (60)
	PROP_StrifeType (59)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Acolyte 8 ----------------------------------------------------------------

class AAcolyteBlue : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteBlue, AAcolyte)
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteBlue, Strife, 231, 0)
	PROP_Translation (TRANSLATION_Standard, 6)
	PROP_SpawnHealth (60)
	PROP_StrifeType (60)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

// Shadow Acolyte -----------------------------------------------------------

class AAcolyteShadow : public AAcolyte
{
	DECLARE_STATELESS_ACTOR (AAcolyteShadow, AAcolyte)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (AAcolyteShadow, Strife, 58, 0)
	PROP_SeeState (S_ACOLYTE_ALTCHASE)
	PROP_PainState (S_ACOLYTE_ALTPAIN)
	PROP_StrifeType (61)
	PROP_StrifeTeaserType (58)
	PROP_StrifeTeaserType2 (59)
	PROP_Flags4 (MF4_MISSILEMORE|MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_Tag ("ACOLYTE")
END_DEFAULTS

//============================================================================
//
// AAcolyteShadow :: NoBlockingSet
//
// This and the tan acolyte are the only ones that drop clips by default.
//
//============================================================================

void AAcolyteShadow::NoBlockingSet ()
{
	P_DropItem (this, "ClipOfBullets", -1, 256);
}

// Some guy turning into an acolyte -----------------------------------------

class AAcolyteToBe : public AAcolyte
{
	DECLARE_ACTOR (AAcolyteToBe, AAcolyte)
};

FState AAcolyteToBe::States[] =
{
#define S_BECOMING_STND 0
	S_NORMAL (ARMR, 'A',   -1, NULL,				NULL),

#define S_BECOMING_PAIN (S_BECOMING_STND+1)
	S_NORMAL (ARMR, 'A',   -1, A_HideDecepticon,	NULL),

#define S_BECOMING_DIEJUMP (S_BECOMING_PAIN+1)
	S_NORMAL (GIBS, 'A',	0, NULL,				&AAcolyte::States[S_ACOLYTE_XDIE]),
};

IMPLEMENT_ACTOR (AAcolyteToBe, Strife, 201, 0)
	PROP_SpawnState (S_BECOMING_STND)
	PROP_PainState (S_BECOMING_PAIN)
	PROP_DeathState (S_BECOMING_DIEJUMP)

	PROP_FlagsClear (MF_COUNTKILL)

	PROP_SpawnHealth (61)
	PROP_PainChance (255)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_StrifeType (29)

	PROP_DeathSound ("becoming/death")
END_DEFAULTS

//============================================================================
//
// A_HideDecepticon
//
// Hide the Acolyte-to-be							->
// Hide the guy transforming into an Acolyte		->
// Hide the transformer								->
// Transformers are Autobots and Decepticons, and
// Decepticons are the bad guys, so...				->
//
// Hide the Decepticon!
//
//============================================================================

void A_HideDecepticon (AActor *self)
{
	EV_DoDoor (DDoor::doorClose, NULL, self, 999, 8*FRACUNIT, 0, 0, 0);
	if (self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert (self->target, self);
	}
}

//============================================================================
//
// A_AcolyteDie
//
//============================================================================

void A_AcolyteDie (AActor *self)
{
	int i;

	// [RH] Disable translucency here.
	self->RenderStyle = STYLE_Normal;

	// Only the Blue Acolyte does extra stuff on death.
	if (!self->IsKindOf (RUNTIME_CLASS(AAcolyteBlue)))
		return;

	// Make sure somebody is still alive
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
			break;
	}
	if (i == MAXPLAYERS)
		return;

	// Make sure all the other blue acolytes are dead.
	TThinkerIterator<AAcolyteBlue> iterator;
	AActor *other;

	while ( (other = iterator.Next ()) )
	{
		if (other != self && other->health > 0)
		{ // Found a living one
			return;
		}
	}

	players[0].mo->GiveInventoryType (QuestItemClasses[6]);
	players[0].SetLogNumber (14);
	S_StopSound ((fixed_t *)NULL, CHAN_VOICE);
	S_Sound (CHAN_VOICE, "svox/voc14", 1, ATTN_NORM);
}

//============================================================================
//
// A_BeShadowyFoe
//
//============================================================================

void A_BeShadowyFoe (AActor *self)
{
	self->RenderStyle = STYLE_Translucent;
	self->alpha = HR_SHADOW;
	self->flags &= ~MF_FRIENDLY;
}

//============================================================================
//
// A_AcolyteBits
//
//============================================================================

void A_AcolyteBits (AActor *self)
{
	if (self->SpawnFlags & MTF_SHADOW)
	{
		A_BeShadowyFoe (self);
	}
	if (self->SpawnFlags & MTF_ALTSHADOW)
	{
		//self->flags |= MF_STRIFEx8000000;
		if (self->flags & MF_SHADOW)
		{
			// I dunno.
		}
		else
		{
			self->RenderStyle = STYLE_None;
		}
	}
}
