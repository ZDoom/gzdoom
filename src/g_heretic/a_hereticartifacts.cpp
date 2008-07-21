#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"

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
		if (!P_UndoPlayerMorph (Owner->player, Owner->player))
		{ // Failed
			if (!(Owner->player->MorphStyle & MORPH_FAILNOTELEFRAG))
			{
				P_DamageMobj (Owner, NULL, NULL, 1000000, NAME_Telefrag);
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

void A_TimeBomb(AActor *self)
{
	self->z += 32*FRACUNIT;
	self->RenderStyle = STYLE_Add;
	self->alpha = FRACUNIT;
	A_Explode(self);
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
	angle_t angle = Owner->angle >> ANGLETOFINESHIFT;
	AActor *mo = Spawn("ActivatedTimeBomb",
		Owner->x + 24*finecosine[angle],
		Owner->y + 24*finesine[angle],
		Owner->z - Owner->floorclip, ALLOW_REPLACE);
	mo->target = Owner;
	return true;
}

