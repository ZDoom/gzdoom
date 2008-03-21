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

// Degnin Ore ---------------------------------------------------------------

void A_RemoveForceField (AActor *);

FState ADegninOre::States[] =
{
	S_NORMAL (XPRK, 'A', -1, NULL,						NULL),

	S_NORMAL (XPRK, 'A',  1, A_RemoveForceField,		&States[2]),
	S_BRIGHT (BNG3, 'A',  3, A_ExplodeAndAlert,			&States[3]),
	S_BRIGHT (BNG3, 'B',  3, NULL,						&States[4]),
	S_BRIGHT (BNG3, 'C',  3, NULL,						&States[5]),
	S_BRIGHT (BNG3, 'D',  3, NULL,						&States[6]),
	S_BRIGHT (BNG3, 'E',  3, NULL,						&States[7]),
	S_BRIGHT (BNG3, 'F',  3, NULL,						&States[8]),
	S_BRIGHT (BNG3, 'G',  3, NULL,						&States[9]),
	S_BRIGHT (BNG3, 'H',  3, NULL,						NULL),
};

IMPLEMENT_ACTOR (ADegninOre, Strife, 59, 0)
	PROP_StrifeType (128)
	PROP_StrifeTeaserType (124)
	PROP_StrifeTeaserType2 (127)
	PROP_SpawnHealth (10)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Inventory_MaxAmount (10)
	PROP_Flags (MF_SPECIAL|MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("Degnin_Ore")		// "Thalite_Ore" in the Teaser
	PROP_DeathSound ("ore/explode")
	PROP_Inventory_Icon ("I_XPRK")
	PROP_Inventory_PickupMessage("$TXT_DEGNINORE")
END_DEFAULTS

void ADegninOre::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = dist = 192;
	RenderStyle = STYLE_Add;	// [RH] Make the explosion glow

	// Does strife automatically play the death sound on death?
	S_SoundID (this, CHAN_BODY, DeathSound, 1, ATTN_NORM);
}

void A_RemoveForceField (AActor *self)
{
	self->flags &= ~MF_SPECIAL;

	for (int i = 0; i < self->Sector->linecount; ++i)
	{
		line_t *line = self->Sector->lines[i];
		if (line->backsector != NULL && line->special == ForceField)
		{
			line->flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
			line->special = 0;
			sides[line->sidenum[0]].SetTexture(side_t::mid, 0);
			sides[line->sidenum[1]].SetTexture(side_t::mid, 0);
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

// Gun Training -------------------------------------------------------------

class AGunTraining : public AInventory
{
	DECLARE_ACTOR (AGunTraining, AInventory)
};

FState AGunTraining::States[] =
{
	S_NORMAL (GUNT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGunTraining, Strife, -1, 0)
	PROP_StrifeType (310)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_UNDROPPABLE)
	PROP_Inventory_MaxAmount (100)
	PROP_Tag ("Accuracy")
	PROP_Inventory_Icon ("I_GUNT")
END_DEFAULTS

// Health Training ----------------------------------------------------------

class AHealthTraining : public AInventory
{
	DECLARE_ACTOR (AHealthTraining, AInventory)
public:
	bool TryPickup (AActor *toucher);
};

FState AHealthTraining::States[] =
{
	S_NORMAL (HELT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AHealthTraining, Strife, -1, 0)
	PROP_StrifeType (309)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_UNDROPPABLE)
	PROP_Inventory_MaxAmount (100)
	PROP_Tag ("Toughness")
	PROP_Inventory_Icon ("I_HELT")
END_DEFAULTS

bool AHealthTraining::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (RUNTIME_CLASS(AGunTraining));
		AInventory *coin = Spawn<ACoin> (0,0,0, NO_REPLACE);
		if (coin != NULL)
		{
			coin->Amount = toucher->player->accuracy*5 + 300;
			if (!coin->TryPickup (toucher))
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
	DECLARE_ACTOR (AScanner, APowerupGiver)
public:
	bool Use (bool pickup);
};

FState AScanner::States[] =
{
	S_BRIGHT (PMUP, 'A', 6, NULL, &States[1]),
	S_BRIGHT (PMUP, 'B', 6, NULL, &States[0])
};

IMPLEMENT_ACTOR (AScanner, Strife, 2027, 0)
	PROP_StrifeType (165)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Inventory_MaxAmount (1)
	PROP_Inventory_FlagsClear (IF_FANCYPICKUPSOUND)
	PROP_Tag ("scanner")
	PROP_Inventory_Icon ("I_PMUP")
	PROP_PowerupGiver_Powerup ("PowerScanner")
	PROP_Inventory_PickupSound ("misc/i_pkup")
	PROP_Inventory_PickupMessage("$TXT_SCANNER")
END_DEFAULTS

bool AScanner::Use (bool pickup)
{
	if (!(level.flags & LEVEL_ALLMAP))
	{
		if (Owner->CheckLocalView (consoleplayer))
		{
			C_MidPrint(GStrings("TXT_NEEDMAP"));
		}
		return false;
	}
	return Super::Use (pickup);
}

// Prison Pass --------------------------------------------------------------

class APrisonPass : public AKey
{
	DECLARE_ACTOR (APrisonPass, AKey)
public:
	bool TryPickup (AActor *toucher);
	bool SpecialDropAction (AActor *dropper);
};

FState APrisonPass::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APrisonPass, Strife, -1, 0)
	PROP_StrifeType (304)
	PROP_StrifeTeaserType (286)
	PROP_StrifeTeaserType2 (303)
	PROP_SpawnState (0)
	PROP_Inventory_Icon ("I_TOKN")
	PROP_Tag ("Prison_pass")
	PROP_Inventory_PickupMessage("$TXT_PRISONPASS")
END_DEFAULTS

bool APrisonPass::TryPickup (AActor *toucher)
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

FState ADummyStrifeItem::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADummyStrifeItem, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
END_DEFAULTS

// Sound the alarm! ---------------------------------------------------------

class ARaiseAlarm : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (ARaiseAlarm, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_STATELESS_ACTOR (ARaiseAlarm, Strife, -1, 0)
	PROP_StrifeType (301)
	PROP_StrifeTeaserType (283)
	PROP_StrifeTeaserType2 (300)
	PROP_Tag ("alarm")
END_DEFAULTS

bool ARaiseAlarm::TryPickup (AActor *toucher)
{
	P_NoiseAlert (toucher, toucher);
	// A_WakeOracleSpectre (dword312F4);
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
	DECLARE_STATELESS_ACTOR (AOpenDoor222, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (AOpenDoor222, Strife, -1, 0)
	PROP_StrifeType (302)
	PROP_StrifeTeaserType (284)
	PROP_StrifeTeaserType2 (301)
END_DEFAULTS

bool AOpenDoor222::TryPickup (AActor *toucher)
{
	EV_DoDoor (DDoor::doorOpen, NULL, toucher, 222, 2*FRACUNIT, 0, 0, 0);
	GoAwayAndDie ();
	return true;
}

// Close door tag 222 -------------------------------------------------------

class ACloseDoor222 : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (ACloseDoor222, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_STATELESS_ACTOR (ACloseDoor222, Strife, -1, 0)
	PROP_StrifeType (303)
	PROP_StrifeTeaserType (285)
	PROP_StrifeTeaserType2 (302)
END_DEFAULTS

bool ACloseDoor222::TryPickup (AActor *toucher)
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
	DECLARE_STATELESS_ACTOR (AOpenDoor224, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
	bool SpecialDropAction (AActor *dropper);
};

IMPLEMENT_STATELESS_ACTOR (AOpenDoor224, Strife, -1, 0)
	PROP_StrifeType (305)
END_DEFAULTS

bool AOpenDoor224::TryPickup (AActor *toucher)
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
	DECLARE_STATELESS_ACTOR (AAmmoFillup, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (AAmmoFillup, Strife, -1, 0)
	PROP_StrifeType (298)
	PROP_StrifeTeaserType (280)
	PROP_StrifeTeaserType2 (297)
	PROP_Tag ("Ammo")
END_DEFAULTS

bool AAmmoFillup::TryPickup (AActor *toucher)
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
	DECLARE_STATELESS_ACTOR (AHealthFillup, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (AHealthFillup, Strife, -1, 0)
	PROP_StrifeType (299)
	PROP_StrifeTeaserType (281)
	PROP_StrifeTeaserType2 (298)
	PROP_Tag ("Health")
END_DEFAULTS

bool AHealthFillup::TryPickup (AActor *toucher)
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

IMPLEMENT_STATELESS_ACTOR (AUpgradeStamina, Strife, -1, 0)
	PROP_StrifeType (306)
	PROP_StrifeTeaserType (287)
	PROP_StrifeTeaserType2 (307)
	PROP_Inventory_Amount (10)
	PROP_Inventory_MaxAmount (100)
END_DEFAULTS

bool AUpgradeStamina::TryPickup (AActor *toucher)
{
	if (toucher->player == NULL)
		return false;
		
	toucher->player->stamina += Amount;
	if (toucher->player->stamina >= MaxAmount)
		toucher->player->stamina = MaxAmount;
		
	P_GiveBody (toucher, -100);
	GoAwayAndDie ();
	return true;
}

// Upgrade Accuracy ---------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AUpgradeAccuracy, Strife, -1, 0)
	PROP_StrifeType (307)
	PROP_StrifeTeaserType (288)
	PROP_StrifeTeaserType2 (308)
END_DEFAULTS

bool AUpgradeAccuracy::TryPickup (AActor *toucher)
{
	if (toucher->player == NULL || toucher->player->accuracy >= 100)
		return false;
	toucher->player->accuracy += 10;
	GoAwayAndDie ();
	return true;
}

// Start a slideshow --------------------------------------------------------

class ASlideshowStarter : public ADummyStrifeItem
{
	DECLARE_STATELESS_ACTOR (ASlideshowStarter, ADummyStrifeItem)
public:
	bool TryPickup (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (ASlideshowStarter, Strife, -1, 0)
	PROP_StrifeType (343)
END_DEFAULTS

bool ASlideshowStarter::TryPickup (AActor *toucher)
{
	gameaction = ga_slideshow;
	if (level.levelnum == 10)
	{
		toucher->GiveInventoryType (QuestItemClasses[16]);
	}
	GoAwayAndDie ();
	return true;
}
