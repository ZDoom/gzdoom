#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

// Ettin --------------------------------------------------------------------

void A_EttinAttack (AActor *);
void A_DropMace (AActor *);

class AEttin : public AActor
{
	DECLARE_ACTOR (AEttin, AActor)
public:
	void Howl ();
};

FState AEttin::States[] =
{
#define S_ETTIN_LOOK 0
	S_NORMAL (ETTN, 'A',   10, A_Look					, &States[S_ETTIN_LOOK+1]),
	S_NORMAL (ETTN, 'A',   10, A_Look					, &States[S_ETTIN_LOOK+0]),

#define S_ETTIN_CHASE (S_ETTIN_LOOK+2)
	S_NORMAL (ETTN, 'A',	5, A_Chase					, &States[S_ETTIN_CHASE+1]),
	S_NORMAL (ETTN, 'B',	5, A_Chase					, &States[S_ETTIN_CHASE+2]),
	S_NORMAL (ETTN, 'C',	5, A_Chase					, &States[S_ETTIN_CHASE+3]),
	S_NORMAL (ETTN, 'D',	5, A_Chase					, &States[S_ETTIN_CHASE+0]),

#define S_ETTIN_PAIN (S_ETTIN_CHASE+4)
	S_NORMAL (ETTN, 'H',	7, A_Pain					, &States[S_ETTIN_CHASE+0]),

#define S_ETTIN_ATK1 (S_ETTIN_PAIN+1)
	S_NORMAL (ETTN, 'E',	6, A_FaceTarget 			, &States[S_ETTIN_ATK1+1]),
	S_NORMAL (ETTN, 'F',	6, A_FaceTarget 			, &States[S_ETTIN_ATK1+2]),
	S_NORMAL (ETTN, 'G',	8, A_EttinAttack			, &States[S_ETTIN_CHASE+0]),

#define S_ETTIN_DEATH1 (S_ETTIN_ATK1+3)
	S_NORMAL (ETTN, 'I',	4, NULL 					, &States[S_ETTIN_DEATH1+1]),
	S_NORMAL (ETTN, 'J',	4, NULL 					, &States[S_ETTIN_DEATH1+2]),
	S_NORMAL (ETTN, 'K',	4, A_Scream 				, &States[S_ETTIN_DEATH1+3]),
	S_NORMAL (ETTN, 'L',	4, A_NoBlocking 			, &States[S_ETTIN_DEATH1+4]),
	S_NORMAL (ETTN, 'M',	4, A_QueueCorpse			, &States[S_ETTIN_DEATH1+5]),
	S_NORMAL (ETTN, 'N',	4, NULL 					, &States[S_ETTIN_DEATH1+6]),
	S_NORMAL (ETTN, 'O',	4, NULL 					, &States[S_ETTIN_DEATH1+7]),
	S_NORMAL (ETTN, 'P',	4, NULL 					, &States[S_ETTIN_DEATH1+8]),
	S_NORMAL (ETTN, 'Q',   -1, NULL 					, NULL),

#define S_ETTIN_DEATH2 (S_ETTIN_DEATH1+9)
	S_NORMAL (ETTB, 'A',	4, NULL 					, &States[S_ETTIN_DEATH2+1]),
	S_NORMAL (ETTB, 'B',	4, A_NoBlocking 			, &States[S_ETTIN_DEATH2+2]),
	S_NORMAL (ETTB, 'C',	4, A_DropMace				, &States[S_ETTIN_DEATH2+3]),
	S_NORMAL (ETTB, 'D',	4, A_Scream 				, &States[S_ETTIN_DEATH2+4]),
	S_NORMAL (ETTB, 'E',	4, A_QueueCorpse			, &States[S_ETTIN_DEATH2+5]),
	S_NORMAL (ETTB, 'F',	4, NULL 					, &States[S_ETTIN_DEATH2+6]),
	S_NORMAL (ETTB, 'G',	4, NULL 					, &States[S_ETTIN_DEATH2+7]),
	S_NORMAL (ETTB, 'H',	4, NULL 					, &States[S_ETTIN_DEATH2+8]),
	S_NORMAL (ETTB, 'I',	4, NULL 					, &States[S_ETTIN_DEATH2+9]),
	S_NORMAL (ETTB, 'J',	4, NULL 					, &States[S_ETTIN_DEATH2+10]),
	S_NORMAL (ETTB, 'K',	4, NULL 					, &States[S_ETTIN_DEATH2+11]),
	S_NORMAL (ETTB, 'L',   -1, NULL 					, NULL),

#define S_ETTIN_ICE (S_ETTIN_DEATH2+12)
	S_NORMAL (ETTN, 'R',	5, A_FreezeDeath			, &States[S_ETTIN_ICE+1]),
	S_NORMAL (ETTN, 'R',	1, A_FreezeDeathChunks		, &States[S_ETTIN_ICE+1])
};
IMPLEMENT_ACTOR (AEttin, Hexen, 10030, 4)
	PROP_SpawnHealth (175)
	PROP_RadiusFixed (25)
	PROP_HeightFixed (68)
	PROP_Mass (175)
	PROP_SpeedFixed (13)
	PROP_Damage (3)
	PROP_PainChance (60)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PUSHWALL|MF2_MCROSS|MF2_TELESTOMP)

	PROP_SpawnState (S_ETTIN_LOOK)
	PROP_SeeState (S_ETTIN_CHASE)
	PROP_PainState (S_ETTIN_PAIN)
	PROP_MeleeState (S_ETTIN_ATK1)
	PROP_DeathState (S_ETTIN_DEATH1)
	PROP_XDeathState (S_ETTIN_DEATH2)
	PROP_IDeathState (S_ETTIN_ICE)

	PROP_SeeSound ("EttinSight")
	PROP_AttackSound ("EttinAttack")
	PROP_PainSound ("EttinPain")
	PROP_DeathSound ("EttinDeath")
	PROP_ActiveSound ("EttinActive")
END_DEFAULTS

void AEttin::Howl ()
{
	int howl = S_FindSound ("PuppyBeat");
	if (!S_GetSoundPlayingInfo (this, howl))
	{
		S_SoundID (this, CHAN_BODY, howl, 1, ATTN_NORM);
	}
}

// Ettin mace ---------------------------------------------------------------

class AEttinMace : public AActor
{
	DECLARE_ACTOR (AEttinMace, AActor)
};

FState AEttinMace::States[] =
{
	S_NORMAL (ETTB, 'M',	5, A_CheckFloor 			, &States[1]),
	S_NORMAL (ETTB, 'N',	5, A_CheckFloor 			, &States[2]),
	S_NORMAL (ETTB, 'O',	5, A_CheckFloor 			, &States[3]),
	S_NORMAL (ETTB, 'P',	5, A_CheckFloor 			, &States[0]),
	S_NORMAL (ETTB, 'Q',	5, NULL 					, &States[5]),
	S_NORMAL (ETTB, 'R',	5, A_QueueCorpse			, &States[6]),
	S_NORMAL (ETTB, 'S',   -1, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AEttinMace, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_DROPOFF|MF_CORPSE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FLOORCLIP)

	PROP_SpawnState (0)
	PROP_DeathState (4)
END_DEFAULTS

// Ettin mash ---------------------------------------------------------------

class AEttinMash : public AEttin
{
	DECLARE_STATELESS_ACTOR (AEttinMash, AEttin)
};

IMPLEMENT_STATELESS_ACTOR (AEttinMash, Hexen, -1, 102)
	PROP_FlagsSet (MF_NOBLOOD)
	PROP_Flags2Set (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_MCROSS|MF2_PUSHWALL|MF2_BLASTED)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_DeathState (~0)
	PROP_XDeathState (~0)
END_DEFAULTS

//============================================================================
// Ettin AI
//============================================================================

void A_EttinAttack (AActor *actor)
{
	if (P_CheckMeleeRange(actor))
	{
		P_DamageMobj (actor->target, actor, actor, HITDICE(2));
	}
}


void A_DropMace (AActor *actor)
{
	AEttinMace *mo;

	mo = Spawn<AEttinMace> (actor->x, actor->y,
		actor->z + (actor->height>>1));
	if (mo)
	{
		mo->momx = (P_Random()-128) << 11;
		mo->momy = (P_Random()-128) << 11;
		mo->momz = FRACUNIT*10+(P_Random()<<10);
		mo->target = actor;
	}
}

