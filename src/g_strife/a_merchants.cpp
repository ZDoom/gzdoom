#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

void A_CloseUpShop (AActor *);
void A_ClearSoundTarget (AActor *);
void A_PlayActiveSound (AActor *);

// Base class for the merchants ---------------------------------------------

class AMerchant : public AActor
{
	DECLARE_ACTOR (AMerchant, AActor)
public:
	void ConversationAnimation (int animnum);
};

void A_PainCatch (AActor *m)
{
	m=m;
}

FState AMerchant::States[] =
{
#define S_MERCHANT_YES 0
	S_NORMAL (MRYS, 'A',   30, NULL,				&States[S_MERCHANT_YES+6]),

#define S_MERCHANT_NO (S_MERCHANT_YES+1)
	S_NORMAL (MRNO, 'A',	6, NULL,				&States[S_MERCHANT_NO+1]),
	S_NORMAL (MRNO, 'B',	6, NULL,				&States[S_MERCHANT_NO+2]),
	S_NORMAL (MRNO, 'C',   10, NULL,				&States[S_MERCHANT_NO+3]),
	S_NORMAL (MRNO, 'B',	6, NULL,				&States[S_MERCHANT_NO+4]),
	S_NORMAL (MRNO, 'A',	6, NULL,				&States[S_MERCHANT_NO+5]),
	
#define S_MERCHANT_STND (S_MERCHANT_NO+5)
	S_NORMAL (MRST, 'A',   10, A_Look2,				&States[S_MERCHANT_STND]),

#define S_MERCHANT_LOOK1 (S_MERCHANT_STND+1)
	// This actually uses A_LoopActiveSound, but since that doesn't actually
	// loop in Strife, this is probably better, eh?
	S_NORMAL (MRLK, 'A',   30, A_PlayActiveSound,	&States[S_MERCHANT_STND]),

#define S_MERCHANT_LOOK2 (S_MERCHANT_LOOK1+1)
	S_NORMAL (MRLK, 'B',   30, NULL,				&States[S_MERCHANT_STND]),

#define S_MERCHANT_BD (S_MERCHANT_LOOK2+1)
	S_NORMAL (MRBD, 'A',	4, NULL,				&States[S_MERCHANT_BD+1]),
	S_NORMAL (MRBD, 'B',	4, NULL,				&States[S_MERCHANT_BD+2]),
	S_NORMAL (MRBD, 'C',	4, NULL,				&States[S_MERCHANT_BD+3]),
	S_NORMAL (MRBD, 'D',	4, NULL,				&States[S_MERCHANT_BD+4]),
	S_NORMAL (MRBD, 'E',	4, NULL,				&States[S_MERCHANT_BD+5]),
	S_NORMAL (MRBD, 'D',	4, NULL,				&States[S_MERCHANT_BD+6]),
	S_NORMAL (MRBD, 'C',	4, NULL,				&States[S_MERCHANT_BD+7]),
	S_NORMAL (MRBD, 'B',	4, NULL,				&States[S_MERCHANT_BD+8]),
	S_NORMAL (MRBD, 'A',	5, NULL,				&States[S_MERCHANT_BD+9]),
	S_NORMAL (MRBD, 'F',	6, NULL,				&States[S_MERCHANT_STND]),

#define S_MERCHANT_PAIN (S_MERCHANT_BD+10)
	S_NORMAL (MRPN, 'A',	3, A_PainCatch,				&States[S_MERCHANT_PAIN+1]),
	S_NORMAL (MRPN, 'B',	3, A_Pain,				&States[S_MERCHANT_PAIN+2]),
	S_NORMAL (MRPN, 'C',	3, NULL,				&States[S_MERCHANT_PAIN+3]),
	S_NORMAL (MRPN, 'D',	9, A_CloseUpShop,		&States[S_MERCHANT_PAIN+4]),
	S_NORMAL (MRPN, 'C',	4, NULL,				&States[S_MERCHANT_PAIN+5]),
	S_NORMAL (MRPN, 'B',	3, NULL,				&States[S_MERCHANT_PAIN+6]),
	S_NORMAL (MRPN, 'A',	3, A_ClearSoundTarget,	&States[S_MERCHANT_STND]),

#define S_MERCHANT_GT (S_MERCHANT_PAIN+7)
	S_NORMAL (MRGT, 'A',	5, NULL,				&States[S_MERCHANT_GT+1]),
	S_NORMAL (MRGT, 'B',	5, NULL,				&States[S_MERCHANT_GT+2]),
	S_NORMAL (MRGT, 'C',	5, NULL,				&States[S_MERCHANT_GT+3]),
	S_NORMAL (MRGT, 'D',	5, NULL,				&States[S_MERCHANT_GT+4]),
	S_NORMAL (MRGT, 'E',	5, NULL,				&States[S_MERCHANT_GT+5]),
	S_NORMAL (MRGT, 'F',	5, NULL,				&States[S_MERCHANT_GT+6]),
	S_NORMAL (MRGT, 'G',	5, NULL,				&States[S_MERCHANT_GT+7]),
	S_NORMAL (MRGT, 'H',	5, NULL,				&States[S_MERCHANT_GT+8]),
	S_NORMAL (MRGT, 'I',	5, NULL,				&States[S_MERCHANT_STND])
};

IMPLEMENT_ACTOR (AMerchant, Strife, -1, 0)
	PROP_SpawnState (S_MERCHANT_STND)
	PROP_SeeState (S_MERCHANT_PAIN)
	PROP_PainState (S_MERCHANT_PAIN)

	PROP_SpawnHealthLong (10000000)
	PROP_PainChance (150)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Mass (5000)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOTDMATCH)
	PROP_Flags4 (MF4_NOSPLASHALERT)
END_DEFAULTS

void AMerchant::ConversationAnimation (int animnum)
{
	switch (animnum)
	{
	case 0:
		SetState (&States[S_MERCHANT_GT]);
		break;
	case 1:
		SetState (&States[S_MERCHANT_YES]);
		break;
	case 2:
		SetState (&States[S_MERCHANT_NO]);
		break;
	}
}

// Weapon Smith -------------------------------------------------------------

class AWeaponSmith : public AMerchant
{
	DECLARE_STATELESS_ACTOR (AWeaponSmith, AMerchant)
};

IMPLEMENT_STATELESS_ACTOR (AWeaponSmith, Strife, 116, 0)
	PROP_StrifeType (2)
	PROP_StrifeTeaserType (2)
	PROP_StrifeTeaserType2 (2)
	PROP_PainSound ("smith/pain")
	PROP_Tag ("Weapon_Smith")
END_DEFAULTS

// Bar Keep -----------------------------------------------------------------

class ABarKeep : public AMerchant
{
	DECLARE_STATELESS_ACTOR (ABarKeep, AMerchant)
};

IMPLEMENT_STATELESS_ACTOR (ABarKeep, Strife, 72, 0)
	PROP_Translation (TRANSLATION_Standard, 4)
	PROP_StrifeType (3)
	PROP_StrifeTeaserType (3)
	PROP_StrifeTeaserType2 (3)
	PROP_PainSound ("barkeep/pain")
	PROP_ActiveSound ("barkeep/active")
	PROP_Tag ("Bar_Keep")
END_DEFAULTS

// Armorer ------------------------------------------------------------------

class AArmorer : public AMerchant
{
	DECLARE_STATELESS_ACTOR (AArmorer, AMerchant)
};

IMPLEMENT_STATELESS_ACTOR (AArmorer, Strife, 73, 0)
	PROP_Translation (TRANSLATION_Standard, 5)
	PROP_StrifeType (4)
	PROP_StrifeTeaserType (4)
	PROP_StrifeTeaserType2 (4)
	PROP_PainSound ("armorer/pain")
	PROP_Tag ("Aromorer")
END_DEFAULTS

// Medic --------------------------------------------------------------------

class AMedic : public AMerchant
{
	DECLARE_STATELESS_ACTOR (AMedic, AMerchant)
};

IMPLEMENT_STATELESS_ACTOR (AMedic, Strife, 74, 0)
	PROP_Translation (TRANSLATION_Standard, 6)
	PROP_StrifeType (5)
	PROP_StrifeTeaserType (5)
	PROP_StrifeTeaserType2 (5)
	PROP_PainSound ("medic/pain")
	PROP_Tag ("Medic")
END_DEFAULTS


//============================================================================
//
// A_CloseUpShop
//
//============================================================================

void A_CloseUpShop (AActor *self)
{
	EV_DoDoor (DDoor::doorCloseWaitOpen, NULL, self, 999, 8*FRACUNIT, 120*TICRATE, 0, 0);
	if (self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert (self->target, self);
	}
}

//============================================================================
//
// A_ClearSoundTarget
//
//============================================================================

void A_ClearSoundTarget (AActor *self)
{
	AActor *actor;

	for (actor = self->Sector->thinglist; actor != NULL; actor = actor->snext)
	{
		actor->LastHeard = NULL;
	}
}

//============================================================================
//
// A_PlayActiveSound
//
//============================================================================

void A_PlayActiveSound (AActor *self)
{
	S_SoundID (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
}
