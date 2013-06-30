/*
#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_spec.h"
#include "a_strifeglobal.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "d_event.h"
#include "a_keys.h"
#include "c_console.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
*/
// Degnin Ore ---------------------------------------------------------------

IMPLEMENT_CLASS(ADegninOre)

DEFINE_ACTION_FUNCTION(AActor, A_RemoveForceField)
{
	self->flags &= ~MF_SPECIAL;

	for (int i = 0; i < self->Sector->linecount; ++i)
	{
		line_t *line = self->Sector->lines[i];
		if (line->backsector != NULL && line->special == ForceField)
		{
			line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
			line->special = 0;
			line->sidedef[0]->SetTexture(side_t::mid, FNullTextureID());
			line->sidedef[1]->SetTexture(side_t::mid, FNullTextureID());
		}
	}
}

bool ADegninOre::Use (bool pickup)
{
	if (pickup)
	{
		return false;
	}
	else
	{
		AInventory *drop;

		// Increase the amount by one so that when DropInventory decrements it,
		// the actor will have the same number of beacons that he started with.
		// When we return to UseInventory, it will take care of decrementing
		// Amount again and disposing of this item if there are no more.
		Amount++;
		drop = Owner->DropInventory (this);
		if (drop == NULL)
		{
			Amount--;
			return false;
		}
		return true;
	}
}

// Health Training ----------------------------------------------------------

class AHealthTraining : public AInventory
{
	DECLARE_CLASS (AHealthTraining, AInventory)
public:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AHealthTraining)

bool AHealthTraining::TryPickup (AActor *&toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (PClass::FindClass("GunTraining"));
		AInventory *coin = Spawn<ACoin> (0,0,0, NO_REPLACE);
		if (coin != NULL)
		{
			coin->Amount = toucher->player->mo->accuracy*5 + 300;
			if (!coin->CallTryPickup (toucher))
			{
				coin->Destroy ();
			}
		}
		return true;
	}
	return false;
}

// Scanner ------------------------------------------------------------------

class AScanner : public APowerupGiver
{
	DECLARE_CLASS (AScanner, APowerupGiver)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AScanner)

bool AScanner::Use (bool pickup)
{
	if (!(level.flags2 & LEVEL2_ALLMAP))
	{
		if (Owner->CheckLocalView (consoleplayer))
		{
			C_MidPrint(SmallFont, GStrings("TXT_NEEDMAP"));
		}
		return false;
	}
	return Super::Use (pickup);
}

// Prison Pass --------------------------------------------------------------

class APrisonPass : public AKey
{
	DECLARE_CLASS (APrisonPass, AKey)
public:
	bool TryPickup (AActor *&toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_CLASS (APrisonPass)

bool APrisonPass::TryPickup (AActor *&toucher)
{
	Super::TryPickup (toucher);
	EV_DoDoor (DDoor::doorOpen, NULL, toucher, 223, 2*FRACUNIT, 0, 0, 0);
	toucher->GiveInventoryType (QuestItemClasses[9]);
	return true;
}

//============================================================================
//
// APrisonPass :: SpecialDropAction
//
// Trying to make a monster that drops a prison pass turns it into an
// OpenDoor223 item instead. That means the only way to get it in Strife
// is through dialog, which is why it doesn't have its own sprite.
//
//============================================================================

bool APrisonPass::SpecialDropAction (AActor *dropper)
{
	EV_DoDoor (DDoor::doorOpen, NULL, dropper, 223, 2*FRACUNIT, 0, 0, 0);
	Destroy ();
	return true;
}


//---------------------------------------------------------------------------
// Dummy items. They are just used by Strife to perform ---------------------
// actions and cannot be held. ----------------------------------------------
//---------------------------------------------------------------------------

IMPLEMENT_CLASS (ADummyStrifeItem)

// Sound the alarm! ---------------------------------------------------------

class ARaiseAlarm : public ADummyStrifeItem
{
	DECLARE_CLASS (ARaiseAlarm, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_CLASS (ARaiseAlarm)

bool ARaiseAlarm::TryPickup (AActor *&toucher)
{
	P_NoiseAlert (toucher, toucher);
	CALL_ACTION(A_WakeOracleSpectre, toucher);
	GoAwayAndDie ();
	return true;
}

bool ARaiseAlarm::SpecialDropAction (AActor *dropper)
{
	P_NoiseAlert (dropper->target, dropper->target);
	if (dropper->target->CheckLocalView (consoleplayer))
	{
		Printf ("You Fool!  You've set off the alarm.\n");
	}
	Destroy ();
	return true;
}

// Open door tag 222 --------------------------------------------------------

class AOpenDoor222 : public ADummyStrifeItem
{
	DECLARE_CLASS (AOpenDoor222, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AOpenDoor222)

bool AOpenDoor222::TryPickup (AActor *&toucher)
{
	EV_DoDoor (DDoor::doorOpen, NULL, toucher, 222, 2*FRACUNIT, 0, 0, 0);
	GoAwayAndDie ();
	return true;
}

// Close door tag 222 -------------------------------------------------------

class ACloseDoor222 : public ADummyStrifeItem
{
	DECLARE_CLASS (ACloseDoor222, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_CLASS (ACloseDoor222)

bool ACloseDoor222::TryPickup (AActor *&toucher)
{
	EV_DoDoor (DDoor::doorClose, NULL, toucher, 222, 2*FRACUNIT, 0, 0, 0);
	GoAwayAndDie ();
	return true;
}

bool ACloseDoor222::SpecialDropAction (AActor *dropper)
{
	EV_DoDoor (DDoor::doorClose, NULL, dropper, 222, 2*FRACUNIT, 0, 0, 0);
	if (dropper->target->CheckLocalView (consoleplayer))
	{
		Printf ("You're dead!  You set off the alarm.\n");
	}
	P_NoiseAlert (dropper->target, dropper->target);
	Destroy ();
	return true;
}

// Open door tag 224 --------------------------------------------------------

class AOpenDoor224 : public ADummyStrifeItem
{
	DECLARE_CLASS (AOpenDoor224, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_CLASS (AOpenDoor224)

bool AOpenDoor224::TryPickup (AActor *&toucher)
{
	EV_DoDoor (DDoor::doorOpen, NULL, toucher, 224, 2*FRACUNIT, 0, 0, 0);
	GoAwayAndDie ();
	return true;
}

bool AOpenDoor224::SpecialDropAction (AActor *dropper)
{
	EV_DoDoor (DDoor::doorOpen, NULL, dropper, 224, 2*FRACUNIT, 0, 0, 0);
	Destroy ();
	return true;
}

// Ammo ---------------------------------------------------------------------

class AAmmoFillup : public ADummyStrifeItem
{
	DECLARE_CLASS (AAmmoFillup, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AAmmoFillup)

bool AAmmoFillup::TryPickup (AActor *&toucher)
{
	const PClass * clip = PClass::FindClass(NAME_ClipOfBullets);
	if (clip != NULL)
	{
		AInventory *item = toucher->FindInventory(clip);
		if (item == NULL)
		{
			item = toucher->GiveInventoryType (clip);
			if (item != NULL)
			{
				item->Amount = 50;
			}
		}
		else if (item->Amount < 50)
		{
			item->Amount = 50;
		}
		else
		{
			return false;
		}
		GoAwayAndDie ();
	}
	return true;
}

// Health -------------------------------------------------------------------

class AHealthFillup : public ADummyStrifeItem
{
	DECLARE_CLASS (AHealthFillup, ADummyStrifeItem)
public:
	bool TryPickup (AActor *&toucher);
};

IMPLEMENT_CLASS (AHealthFillup)

bool AHealthFillup::TryPickup (AActor *&toucher)
{
	static const int skillhealths[5] = { -100, -75, -50, -50, -100 };

	int index = clamp<int>(gameskill, 0,4);
	if (!P_GiveBody (toucher, skillhealths[index]))
	{
		return false;
	}
	GoAwayAndDie ();
	return true;
}

// Upgrade Stamina ----------------------------------------------------------

IMPLEMENT_CLASS (AUpgradeStamina)

bool AUpgradeStamina::TryPickup (AActor *&toucher)
{
	if (toucher->player == NULL)
		return false;
		
	toucher->player->mo->stamina += Amount;
	if (toucher->player->mo->stamina >= MaxAmount)
		toucher->player->mo->stamina = MaxAmount;
		
	P_GiveBody (toucher, -100);
	GoAwayAndDie ();
	return true;
}

// Upgrade Accuracy ---------------------------------------------------------

IMPLEMENT_CLASS (AUpgradeAccuracy)

bool AUpgradeAccuracy::TryPickup (AActor *&toucher)
{
	if (toucher->player == NULL || toucher->player->mo->accuracy >= 100)
		return false;
	toucher->player->mo->accuracy += 10;
	GoAwayAndDie ();
	return true;
}

// Start a slideshow --------------------------------------------------------

IMPLEMENT_CLASS (ASlideshowStarter)

bool ASlideshowStarter::TryPickup (AActor *&toucher)
{
	gameaction = ga_slideshow;
	if (level.levelnum == 10)
	{
		toucher->GiveInventoryType (QuestItemClasses[16]);
	}
	GoAwayAndDie ();
	return true;
}
