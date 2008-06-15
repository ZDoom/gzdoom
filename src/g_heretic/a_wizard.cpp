#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_hereticglobal.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_wizatk3 ("WizAtk3");

void A_WizAtk1 (AActor *);
void A_WizAtk2 (AActor *);
void A_WizAtk3 (AActor *);
void A_GhostOff (AActor *);

// Class definitions --------------------------------------------------------

FState AWizard::States[] =
{
#define S_WIZARD_LOOK 0
	S_NORMAL (WZRD, 'A',   10, A_Look						, &States[S_WIZARD_LOOK+1]),
	S_NORMAL (WZRD, 'B',   10, A_Look						, &States[S_WIZARD_LOOK+0]),

#define S_WIZARD_WALK (S_WIZARD_LOOK+2)
	S_NORMAL (WZRD, 'A',	3, A_Chase						, &States[S_WIZARD_WALK+1]),
	S_NORMAL (WZRD, 'A',	4, A_Chase						, &States[S_WIZARD_WALK+2]),
	S_NORMAL (WZRD, 'A',	3, A_Chase						, &States[S_WIZARD_WALK+3]),
	S_NORMAL (WZRD, 'A',	4, A_Chase						, &States[S_WIZARD_WALK+4]),
	S_NORMAL (WZRD, 'B',	3, A_Chase						, &States[S_WIZARD_WALK+5]),
	S_NORMAL (WZRD, 'B',	4, A_Chase						, &States[S_WIZARD_WALK+6]),
	S_NORMAL (WZRD, 'B',	3, A_Chase						, &States[S_WIZARD_WALK+7]),
	S_NORMAL (WZRD, 'B',	4, A_Chase						, &States[S_WIZARD_WALK+0]),

#define S_WIZARD_ATK (S_WIZARD_WALK+8)
	S_NORMAL (WZRD, 'C',	4, A_WizAtk1					, &States[S_WIZARD_ATK+1]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk2					, &States[S_WIZARD_ATK+2]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk1					, &States[S_WIZARD_ATK+3]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk2					, &States[S_WIZARD_ATK+4]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk1					, &States[S_WIZARD_ATK+5]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk2					, &States[S_WIZARD_ATK+6]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk1					, &States[S_WIZARD_ATK+7]),
	S_NORMAL (WZRD, 'C',	4, A_WizAtk2					, &States[S_WIZARD_ATK+8]),
	S_NORMAL (WZRD, 'D',   12, A_WizAtk3					, &States[S_WIZARD_WALK+0]),

#define S_WIZARD_PAIN (S_WIZARD_ATK+9)
	S_NORMAL (WZRD, 'E',	3, A_GhostOff					, &States[S_WIZARD_PAIN+1]),
	S_NORMAL (WZRD, 'E',	3, A_Pain						, &States[S_WIZARD_WALK+0]),

#define S_WIZARD_DIE (S_WIZARD_PAIN+2)
	S_NORMAL (WZRD, 'F',	6, A_GhostOff					, &States[S_WIZARD_DIE+1]),
	S_NORMAL (WZRD, 'G',	6, A_Scream 					, &States[S_WIZARD_DIE+2]),
	S_NORMAL (WZRD, 'H',	6, NULL 						, &States[S_WIZARD_DIE+3]),
	S_NORMAL (WZRD, 'I',	6, NULL 						, &States[S_WIZARD_DIE+4]),
	S_NORMAL (WZRD, 'J',	6, A_NoBlocking 				, &States[S_WIZARD_DIE+5]),
	S_NORMAL (WZRD, 'K',	6, NULL 						, &States[S_WIZARD_DIE+6]),
	S_NORMAL (WZRD, 'L',	6, NULL 						, &States[S_WIZARD_DIE+7]),
	S_NORMAL (WZRD, 'M',   -1, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AWizard, Heretic, 15, 19)
	PROP_SpawnHealth (180)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (68)
	PROP_Mass (100)
	PROP_SpeedFixed (12)
	PROP_PainChance (64)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_FLOAT|MF_NOGRAVITY)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTOVERLAP)

	PROP_SpawnState (S_WIZARD_LOOK)
	PROP_SeeState (S_WIZARD_WALK)
	PROP_PainState (S_WIZARD_PAIN)
	PROP_MissileState (S_WIZARD_ATK)
	PROP_DeathState (S_WIZARD_DIE)

	PROP_SeeSound ("wizard/sight")
	PROP_AttackSound ("wizard/attack")
	PROP_PainSound ("wizard/pain")
	PROP_DeathSound ("wizard/death")
	PROP_ActiveSound ("wizard/active")
	PROP_Obituary("$OB_WIZARD")
	PROP_HitObituary("$OB_WIZARDHIT")
END_DEFAULTS

void AWizard::NoBlockingSet ()
{
	P_DropItem (this, "BlasterAmmo", 10, 84);
	P_DropItem (this, "ArtiTomeOfPower", 0, 4);
}

class AWizardFX1 : public AActor
{
	DECLARE_ACTOR (AWizardFX1, AActor)
};

FState AWizardFX1::States[] =
{
#define S_WIZFX1 0
	S_BRIGHT (FX11, 'A',	6, NULL 						, &States[S_WIZFX1+1]),
	S_BRIGHT (FX11, 'B',	6, NULL 						, &States[S_WIZFX1+0]),

#define S_WIZFXI1 (S_WIZFX1+2)
	S_BRIGHT (FX11, 'C',	5, NULL 						, &States[S_WIZFXI1+1]),
	S_BRIGHT (FX11, 'D',	5, NULL 						, &States[S_WIZFXI1+2]),
	S_BRIGHT (FX11, 'E',	5, NULL 						, &States[S_WIZFXI1+3]),
	S_BRIGHT (FX11, 'F',	5, NULL 						, &States[S_WIZFXI1+4]),
	S_BRIGHT (FX11, 'G',	5, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AWizardFX1, Heretic, -1, 140)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (18)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_WIZFX1)
	PROP_DeathState (S_WIZFXI1)
END_DEFAULTS

AT_SPEED_SET (WizardFX1, speed)
{
	SimpleSpeedSetter (AWizardFX1, 18*FRACUNIT, 24*FRACUNIT, speed);
}

// --- Action functions -----------------------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_GhostOff
//
//----------------------------------------------------------------------------

void A_GhostOff (AActor *actor)
{
	actor->RenderStyle = STYLE_Normal;
	actor->flags3 &= ~MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk1
//
//----------------------------------------------------------------------------

void A_WizAtk1 (AActor *actor)
{
	A_FaceTarget (actor);
	A_GhostOff (actor);
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk2
//
//----------------------------------------------------------------------------

void A_WizAtk2 (AActor *actor)
{
	A_FaceTarget (actor);
	actor->alpha = HR_SHADOW;
	actor->RenderStyle = STYLE_Translucent;
	actor->flags3 |= MF3_GHOST;
}

//----------------------------------------------------------------------------
//
// PROC A_WizAtk3
//
//----------------------------------------------------------------------------

void A_WizAtk3 (AActor *actor)
{
	AActor *mo;

	A_GhostOff (actor);
	if (!actor->target)
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);
	if (actor->CheckMeleeRange())
	{
		int damage = pr_wizatk3.HitDice (4);
		P_DamageMobj (actor->target, actor, actor, damage, NAME_Melee);
		P_TraceBleed (damage, actor->target, actor);
		return;
	}
	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(AWizardFX1));
	if (mo != NULL)
	{
		P_SpawnMissileAngle(actor, RUNTIME_CLASS(AWizardFX1), mo->angle-(ANG45/8), mo->momz);
		P_SpawnMissileAngle(actor, RUNTIME_CLASS(AWizardFX1), mo->angle+(ANG45/8), mo->momz);
	}
}
