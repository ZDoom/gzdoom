/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

// Tome of power ------------------------------------------------------------

class AArtiTomeOfPower : public APowerupGiver
{
	DECLARE_CLASS (AArtiTomeOfPower, APowerupGiver)
public:
	bool Use (bool pickup);
};


IMPLEMENT_CLASS (AArtiTomeOfPower)

bool AArtiTomeOfPower::Use (bool pickup)
{
	if (Owner->player->morphTics && (Owner->player->MorphStyle & MORPH_UNDOBYTOMEOFPOWER))
	{ // Attempt to undo chicken
		if (!P_UndoPlayerMorph (Owner->player, Owner->player, MORPH_UNDOBYTOMEOFPOWER))
		{ // Failed
			if (!(Owner->player->MorphStyle & MORPH_FAILNOTELEFRAG))
			{
				P_DamageMobj (Owner, NULL, NULL, TELEFRAG_DAMAGE, NAME_Telefrag);
			}
		}
		else
		{ // Succeeded
			S_Sound (Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_IDLE);
		}
		return true;
	}
	else
	{
		return Super::Use (pickup);
	}
}

// Time bomb ----------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_TimeBomb)
{
	PARAM_ACTION_PROLOGUE;

	self->AddZ(32, false);
	self->RenderStyle = STYLE_Add;
	self->Alpha = 1.;
	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, RADF_HURTSOURCE);
	P_CheckSplash(self, 128);
	return 0;
}

class AArtiTimeBomb : public AInventory
{
	DECLARE_CLASS (AArtiTimeBomb, AInventory)
public:
	bool Use (bool pickup);
};


IMPLEMENT_CLASS (AArtiTimeBomb)

bool AArtiTimeBomb::Use (bool pickup)
{
	AActor *mo = Spawn("ActivatedTimeBomb",
		Owner->Vec3Angle(24., Owner->Angles.Yaw, - Owner->Floorclip), ALLOW_REPLACE);
	mo->target = Owner;
	return true;
}

