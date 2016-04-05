/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "a_hexenglobal.h"
#include "gstrings.h"
#include "a_weaponpiece.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_mstafftrack ("MStaffTrack");
static FRandom pr_bloodscourgedrop ("BloodScourgeDrop");

void A_MStaffTrack (AActor *);
void A_DropBloodscourgePieces (AActor *);
void A_MStaffAttack (AActor *actor);
void A_MStaffPalette (AActor *actor);

static AActor *FrontBlockCheck (AActor *mo, int index, void *);
static divline_t BlockCheckLine;

//==========================================================================
// The Mages's Staff (Bloodscourge) -----------------------------------------

class AMWeapBloodscourge : public AMageWeapon
{
	DECLARE_CLASS (AMWeapBloodscourge, AMageWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << MStaffCount;
	}
	PalEntry GetBlend ()
	{
		if (paletteflash & PF_HEXENWEAPONS)
		{
			if (MStaffCount == 3)
				return PalEntry(128, 100, 73, 0);
			else if (MStaffCount == 2)
				return PalEntry(128, 125, 92, 0);
			else if (MStaffCount == 1)
				return PalEntry(128, 150, 110, 0);
			else
				return PalEntry(0, 0, 0, 0);
		}
		else
		{
			return PalEntry (MStaffCount * 128 / 3, 151, 110, 0);
		}
	}
	BYTE MStaffCount;
};

IMPLEMENT_CLASS (AMWeapBloodscourge)

// Mage Staff FX2 (Bloodscourge) --------------------------------------------

class AMageStaffFX2 : public AActor
{
	DECLARE_CLASS(AMageStaffFX2, AActor)
public:
	int SpecialMissileHit (AActor *victim);
	bool SpecialBlastHandling (AActor *source, double strength);
};

IMPLEMENT_CLASS (AMageStaffFX2)

int AMageStaffFX2::SpecialMissileHit (AActor *victim)
{
	if (victim != target &&
		!victim->player &&
		!(victim->flags2 & MF2_BOSS))
	{
		P_DamageMobj (victim, this, target, 10, NAME_Fire);
		return 1;	// Keep going
	}
	return -1;
}

bool AMageStaffFX2::SpecialBlastHandling (AActor *source, double strength)
{
	// Reflect to originator
	tracer = target;	
	target = source;
	return true;
}

//============================================================================

//============================================================================
//
// MStaffSpawn
//
//============================================================================

void MStaffSpawn (AActor *pmo, DAngle angle, AActor *alttarget)
{
	AActor *mo;
	FTranslatedLineTarget t;

	mo = P_SpawnPlayerMissile (pmo, 0, 0, 8, RUNTIME_CLASS(AMageStaffFX2), angle, &t);
	if (mo)
	{
		mo->target = pmo;
		if (t.linetarget && !t.unlinked)
			mo->tracer = t.linetarget;
		else
			mo->tracer = alttarget;
	}
}

//============================================================================
//
// A_MStaffAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffAttack)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	player_t *player;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (self->player->ReadyWeapon);
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	angle = self->Angles.Yaw;
	
	// [RH] Let's try and actually track what the player aimed at
	P_AimLineAttack (self, angle, PLAYERMISSILERANGE, &t, 32.);
	if (t.linetarget == NULL)
	{
		BlockCheckLine.x = self->X();
		BlockCheckLine.y = self->Y();
		BlockCheckLine.dx = -angle.Sin();
		BlockCheckLine.dy = -angle.Cos();
		t.linetarget = P_BlockmapSearch (self, 10, FrontBlockCheck);
	}
	MStaffSpawn (self, angle, t.linetarget);
	MStaffSpawn (self, angle-5, t.linetarget);
	MStaffSpawn (self, angle+5, t.linetarget);
	S_Sound (self, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
	weapon->MStaffCount = 3;
	return 0;
}

//============================================================================
//
// A_MStaffPalette
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffPalette)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != NULL)
	{
		AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (self->player->ReadyWeapon);
		if (weapon != NULL && weapon->MStaffCount != 0)
		{
			weapon->MStaffCount--;
		}
	}
	return 0;
}

//============================================================================
//
// A_MStaffTrack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MStaffTrack)
{
	PARAM_ACTION_PROLOGUE;

	if ((self->tracer == 0) && (pr_mstafftrack()<50))
	{
		self->tracer = P_RoughMonsterSearch (self, 10, true);
	}
	P_SeekerMissile(self, 2, 10);
	return 0;
}

//============================================================================
//
// FrontBlockCheck
//
// [RH] Like RoughBlockCheck, but it won't return anything behind a line.
//
//============================================================================

static AActor *FrontBlockCheck (AActor *mo, int index, void *)
{
	FBlockNode *link;

	for (link = blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (P_PointOnDivlineSide(link->Me->X(), link->Me->Y(), &BlockCheckLine) == 0 &&
				mo->IsOkayToAttack (link->Me))
			{
				return link->Me;
			}
		}
	}
	return NULL;
}

//============================================================================
//
// MStaffSpawn2 - for use by mage class boss
//
//============================================================================

void MStaffSpawn2 (AActor *actor, DAngle angle)
{
	AActor *mo;

	mo = P_SpawnMissileAngleZ (actor, actor->Z()+40, RUNTIME_CLASS(AMageStaffFX2), angle, 0.);
	if (mo)
	{
		mo->target = actor;
		mo->tracer = P_BlockmapSearch (mo, 10, FrontBlockCheck);
	}
}

//============================================================================
//
// A_MStaffAttack2 - for use by mage class boss
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MageAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
	{
		return 0;
	}
	DAngle angle = self->Angles.Yaw;
	MStaffSpawn2(self, angle);
	MStaffSpawn2(self, angle - 5);
	MStaffSpawn2(self, angle + 5);
	S_Sound(self, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
	return 0;
}
