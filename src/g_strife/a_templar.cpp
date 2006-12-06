#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"

static FRandom pr_templar ("Templar");

void A_ReaverMelee (AActor *);
void A_TossGib (AActor *);
void A_1fce8 (AActor *);

// Templar ------------------------------------------------------------------

class ATemplar : public AActor
{
	DECLARE_ACTOR (ATemplar, AActor)
public:
	void NoBlockingSet ();
};

FState ATemplar::States[] =
{
#define S_TEMPLAR_STAND 0
	S_NORMAL (PGRD, 'A',  5, A_Look2,				&States[S_TEMPLAR_STAND]),
	S_NORMAL (PGRD, 'B', 10, NULL,					&States[S_TEMPLAR_STAND]),
	S_NORMAL (PGRD, 'C', 10, NULL,					&States[S_TEMPLAR_STAND]),
	S_NORMAL (PGRD, 'B', 10, A_Wander,				&States[S_TEMPLAR_STAND]),

#define S_TEMPLAR_CHASE (S_TEMPLAR_STAND+4)
	S_NORMAL (PGRD, 'A',  3, A_Chase,				&States[S_TEMPLAR_CHASE+1]),
	S_NORMAL (PGRD, 'A',  3, A_Chase,				&States[S_TEMPLAR_CHASE+2]),
	S_NORMAL (PGRD, 'B',  3, A_Chase,				&States[S_TEMPLAR_CHASE+3]),
	S_NORMAL (PGRD, 'B',  3, A_Chase,				&States[S_TEMPLAR_CHASE+4]),
	S_NORMAL (PGRD, 'C',  3, A_Chase,				&States[S_TEMPLAR_CHASE+5]),
	S_NORMAL (PGRD, 'C',  3, A_Chase,				&States[S_TEMPLAR_CHASE+6]),
	S_NORMAL (PGRD, 'D',  3, A_Chase,				&States[S_TEMPLAR_CHASE+7]),
	S_NORMAL (PGRD, 'D',  3, A_Chase,				&States[S_TEMPLAR_CHASE]),

#define S_TEMPLAR_MELEE (S_TEMPLAR_CHASE+8)
	S_NORMAL (PGRD, 'E',  8, A_FaceTarget,			&States[S_TEMPLAR_MELEE+1]),
	S_NORMAL (PGRD, 'F',  8, A_ReaverMelee,			&States[S_TEMPLAR_CHASE]),

#define S_TEMPLAR_MISSILE (S_TEMPLAR_MELEE+2)
	S_BRIGHT (PGRD, 'G',  8, A_FaceTarget,			&States[S_TEMPLAR_MISSILE+1]),
	S_BRIGHT (PGRD, 'H',  8, A_1fce8,				&States[S_TEMPLAR_CHASE]),

#define S_TEMPLAR_PAIN (S_TEMPLAR_MISSILE+2)
	S_NORMAL (PGRD, 'A',  2, NULL,					&States[S_TEMPLAR_PAIN+1]),
	S_NORMAL (PGRD, 'A',  2, A_Pain,				&States[S_TEMPLAR_CHASE]),

#define S_TEMPLAR_DIE (S_TEMPLAR_PAIN+2)
	S_BRIGHT (PGRD, 'I',  4, A_TossGib,				&States[S_TEMPLAR_DIE+1]),
	S_BRIGHT (PGRD, 'J',  4, A_Scream,				&States[S_TEMPLAR_DIE+2]),
	S_BRIGHT (PGRD, 'K',  4, A_TossGib,				&States[S_TEMPLAR_DIE+3]),
	S_BRIGHT (PGRD, 'L',  4, A_NoBlocking,			&States[S_TEMPLAR_DIE+4]),
	S_BRIGHT (PGRD, 'M',  4, NULL,					&States[S_TEMPLAR_DIE+5]),
	S_BRIGHT (PGRD, 'N',  4, NULL,					&States[S_TEMPLAR_DIE+6]),
	S_NORMAL (PGRD, 'O',  4, A_TossGib,				&States[S_TEMPLAR_DIE+7]),
	S_NORMAL (PGRD, 'P',  4, NULL,					&States[S_TEMPLAR_DIE+8]),
	S_NORMAL (PGRD, 'Q',  4, NULL,					&States[S_TEMPLAR_DIE+9]),
	S_NORMAL (PGRD, 'R',  4, NULL,					&States[S_TEMPLAR_DIE+10]),
	S_NORMAL (PGRD, 'S',  4, NULL,					&States[S_TEMPLAR_DIE+11]),
	S_NORMAL (PGRD, 'T',  4, NULL,					&States[S_TEMPLAR_DIE+12]),
	S_NORMAL (PGRD, 'U',  4, NULL,					&States[S_TEMPLAR_DIE+13]),
	S_NORMAL (PGRD, 'V',  4, NULL,					&States[S_TEMPLAR_DIE+14]),
	S_NORMAL (PGRD, 'W',  4, NULL,					&States[S_TEMPLAR_DIE+15]),
	S_NORMAL (PGRD, 'X',  4, NULL,					&States[S_TEMPLAR_DIE+16]),
	S_NORMAL (PGRD, 'Y',  4, NULL,					&States[S_TEMPLAR_DIE+17]),
   	S_NORMAL (PGRD, 'Z',  4, NULL,					&States[S_TEMPLAR_DIE+18]),
	S_NORMAL (PGRD, '[',  4, NULL,					&States[S_TEMPLAR_DIE+19]),
	S_NORMAL (PGRD, '\\',-1, NULL,					NULL),
};

IMPLEMENT_ACTOR (ATemplar, Strife, 3003, 0)
	PROP_StrifeType (62)
	PROP_StrifeTeaserType (61)
	PROP_StrifeTeaserType2 (62)
	PROP_SpawnHealth (300)
	PROP_SpawnState (S_TEMPLAR_STAND)
	PROP_SeeState (S_TEMPLAR_CHASE)
	PROP_PainState (S_TEMPLAR_PAIN)
	PROP_PainChance (100)
	PROP_MeleeState (S_TEMPLAR_MELEE)
	PROP_MissileState (S_TEMPLAR_MISSILE)
	PROP_DeathState (S_TEMPLAR_DIE)
	PROP_SpeedFixed (8)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (60)
	PROP_Mass (500)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags4 (MF4_SEESDAGGERS|MF4_NOSPLASHALERT)
	PROP_MaxDropOffHeight (32)
	PROP_MinMissileChance (150)
	PROP_SeeSound ("templar/sight")
	PROP_PainSound ("templar/pain")
	PROP_DeathSound ("templar/death")
	PROP_ActiveSound ("templar/active")
	PROP_Tag ("TEMPLAR")	// Known as "Enforcer" in earlier versions.
	PROP_HitObituary ("$OB_TEMPLARHIT")
	PROP_Obituary ("$OB_TEMPLAR")
END_DEFAULTS

//============================================================================
//
// ATemplar :: NoBlockingSet
//
//============================================================================

void ATemplar::NoBlockingSet ()
{
	P_DropItem (this, "EnergyPod", 20, 256);
}

class AMaulerPuff : public AActor
{
	DECLARE_ACTOR (AMaulerPuff, AActor)
};

void A_1fce8 (AActor *self)
{
	int damage;
	angle_t angle;
	int pitch;
	int pitchdiff;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "templar/shoot", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);

	for (int i = 0; i < 10; ++i)
	{
		damage = (pr_templar() & 4) * 2;
		angle = self->angle + (pr_templar.Random2() << 19);
		pitchdiff = pr_templar.Random2() * 332063;
		P_LineAttack (self, angle, MISSILERANGE+64*FRACUNIT, pitch+pitchdiff, damage, NAME_Disintegrate, RUNTIME_CLASS(AMaulerPuff));
	}
}
