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
#include "a_strifeweaps.h"
#include "d_event.h"

// Med patch -----------------------------------------------------------------

class AMedPatch : public AHealthPickup
{
	DECLARE_ACTOR (AMedPatch, AHealthPickup);
protected:
	const char *PickupMessage ();
};

FState AMedPatch::States[] =
{
	S_NORMAL (STMP, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMedPatch, Strife, 2011, 0)
	PROP_SpawnHealth (10)
	PROP_SpawnState (0)
	PROP_StrifeType (125)
	PROP_StrifeTeaserType (121)
	PROP_StrifeTeaserType2 (124)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_MaxAmount (20)
	PROP_Tag ("Med_patch")
	PROP_Inventory_Icon ("I_STMP")
END_DEFAULTS

const char *AMedPatch::PickupMessage ()
{
	return "You picked up the Med patch.";
}

// Medical Kit ---------------------------------------------------------------

class AMedicalKit : public AHealthPickup
{
	DECLARE_ACTOR (AMedicalKit, AHealthPickup);
protected:
	const char *PickupMessage ();
};

FState AMedicalKit::States[] =
{
	S_NORMAL (MDKT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMedicalKit, Strife, 2012, 0)
	PROP_SpawnHealth (25)
	PROP_SpawnState (0)
	PROP_StrifeType (126)
	PROP_StrifeTeaserType (122)
	PROP_StrifeTeaserType2 (125)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_MaxAmount (15)
	PROP_Tag ("Medical_kit")
	PROP_Inventory_Icon ("I_MDKT")
END_DEFAULTS

const char *AMedicalKit::PickupMessage ()
{
	return "You picked up the Medical kit.";
}

// Surgery Kit --------------------------------------------------------------

class ASurgeryKit : public AHealthPickup
{
	DECLARE_ACTOR (ASurgeryKit, AHealthPickup);
protected:
	const char *PickupMessage ();
	bool Use (bool pickup);
};

FState ASurgeryKit::States[] =
{
	S_NORMAL (FULL, 'A', 35, NULL, &States[1]),
	S_NORMAL (FULL, 'B', 35, NULL, &States[0])
};

IMPLEMENT_ACTOR (ASurgeryKit, Strife, 83, 0)
	PROP_SpawnState (0)
	PROP_StrifeType (127)
	PROP_StrifeTeaserType (123)
	PROP_StrifeTeaserType2 (126)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_MaxAmount (5)
	PROP_Tag ("Surgery_Kit")	// "full_health" in the Teaser
	PROP_Inventory_Icon ("I_FULL")
END_DEFAULTS

const char *ASurgeryKit::PickupMessage ()
{
	return "You picked up the Surgery Kit.";
}

bool ASurgeryKit::Use (bool pickup)
{
	return P_GiveBody (Owner->player, -100);
}

// StrifeMap ----------------------------------------------------------------

class AStrifeMap : public AMapRevealer
{
	DECLARE_ACTOR (AStrifeMap, AMapRevealer)
protected:
	virtual const char *PickupMessage ()
	{
		return "You picked up the map";
	}
	// Which sound does it play in Strife? Powerup or normal pickup?
	// Does Strife even have a powerup sound?
};

FState AStrifeMap::States[] =
{
	S_BRIGHT (SMAP, 'A',	6, NULL 				, &States[1]),
	S_BRIGHT (SMAP, 'B',	6, NULL 				, &States[0]),
};

IMPLEMENT_ACTOR (AStrifeMap, Strife, 2026, 137)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_StrifeType (164)
	PROP_StrifeTeaserType (160)
	PROP_StrifeTeaserType2 (163)
END_DEFAULTS

// Degnin Ore ---------------------------------------------------------------

void A_RemoveForceField (AActor *);

FState ADegninOre::States[] =
{
	S_NORMAL (XPRK, 'A', -1, NULL,						NULL),

	S_NORMAL (XPRK, 'A',  1, A_RemoveForceField,		&States[2]),
	S_BRIGHT (BNG3, 'A',  3, A_Explode,					&States[3]),
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
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("Degnin_Ore")		// "Thalite_Ore" in the Teaser
	PROP_DeathSound ("ore/explode")
	PROP_Inventory_Icon ("I_XPRK")
END_DEFAULTS

const char *ADegninOre::PickupMessage ()
{
	return "You picked up the Degnin Ore.";
}

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
			sides[line->sidenum[0]].midtexture = 0;
			sides[line->sidenum[1]].midtexture = 0;
		}
	}
}

// Beldin's Ring ------------------------------------------------------------

class ABeldinsRing : public AInventory
{
	DECLARE_ACTOR (ABeldinsRing, AInventory)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState ABeldinsRing::States[] =
{
	S_NORMAL (RING, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABeldinsRing, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
	PROP_StrifeType (173)
	PROP_StrifeTeaserType (165)
	PROP_StrifeTeaserType2 (169)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("ring")
	PROP_Inventory_Icon ("I_RING")
END_DEFAULTS

bool ABeldinsRing::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[0]);
		return true;
	}
	return false;
}

const char *ABeldinsRing::PickupMessage ()
{
	return "You picked up the ring.";
}

// Offering Chalice ---------------------------------------------------------

class AOfferingChalice : public AInventory
{
	DECLARE_ACTOR (AOfferingChalice, AInventory)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState AOfferingChalice::States[] =
{
	S_BRIGHT (RELC, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOfferingChalice, Strife, 205, 0)
	PROP_StrifeType (174)
	PROP_StrifeTeaserType (166)
	PROP_StrifeTeaserType2 (170)
	PROP_SpawnState (0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_DROPPED)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("Offering_Chalice")
	PROP_Inventory_Icon ("I_RELC")
END_DEFAULTS

bool AOfferingChalice::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[1]);
		return true;
	}
	return false;
}

const char *AOfferingChalice::PickupMessage ()
{
	return "You picked up the Offering Chalice.";
}

// Ear ----------------------------------------------------------------------

class AEar : public AInventory
{
	DECLARE_ACTOR (AEar, AInventory)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState AEar::States[] =
{
	S_NORMAL (EARS, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AEar, Strife, -1, 0)
	PROP_StrifeType (175)
	PROP_StrifeTeaserType (167)
	PROP_StrifeTeaserType2 (171)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("ear")
	PROP_Inventory_Icon ("I_EARS")
END_DEFAULTS

bool AEar::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[8]);
		return true;
	}
	return false;
}

const char *AEar::PickupMessage ()
{
	return "You picked up the ear.";
}

// Broken Power Coupling ----------------------------------------------------

class ABrokenPowerCoupling : public AInventory
{
	DECLARE_ACTOR (ABrokenPowerCoupling, AInventory)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState ABrokenPowerCoupling::States[] =
{
	S_NORMAL (COUP, 'C', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABrokenPowerCoupling, Strife, 226, 0)
	PROP_StrifeType (289)
	PROP_SpawnHealth (40)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Inventory_MaxAmount (1)
	PROP_Flags (MF_SPECIAL|MF_DROPPED)
	PROP_Tag ("BROKEN_POWER_COUPLING")
	PROP_Inventory_Icon ("I_COUP")
END_DEFAULTS

bool ABrokenPowerCoupling::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[7]);
		return true;
	}
	return false;
}

const char *ABrokenPowerCoupling::PickupMessage ()
{
	return "You picked up the broken power coupling.";
}

// Shadow Armor -------------------------------------------------------------

void A_ClearShadow (AActor *);
void A_BeShadowyFoe (AActor *);
void A_SetShadow (AActor *);

class AShadowArmor : public APowerupGiver
{
	DECLARE_ACTOR (AShadowArmor, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AShadowArmor::States[] =
{
	S_BRIGHT (SHD1, 'A', 17, A_ClearShadow,		&States[1]),
	S_BRIGHT (SHD1, 'A', 17, A_BeShadowyFoe,	&States[2]),
	S_BRIGHT (SHD1, 'A', 17, A_SetShadow,		&States[3]),
	S_BRIGHT (SHD1, 'A', 17, A_BeShadowyFoe,	&States[0])
};

IMPLEMENT_ACTOR (AShadowArmor, Strife, 2024, 135)
	PROP_StrifeType (160)
	PROP_StrifeTeaserType (156)
	PROP_StrifeTeaserType2 (159)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Inventory_MaxAmount (2)
	PROP_PowerupGiver_Powerup ("PowerShadow")
	PROP_Tag ("Shadow_armor")
	PROP_Inventory_Icon ("I_SHD1")
END_DEFAULTS

const char *AShadowArmor::PickupMessage ()
{
	return "You picked up the Shadow armor.";
}

// Environmental suit -------------------------------------------------------

class AEnvironmentalSuit : public APowerupGiver
{
	DECLARE_ACTOR (AEnvironmentalSuit, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AEnvironmentalSuit::States[] =
{
	S_BRIGHT (MASK, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AEnvironmentalSuit, Strife, 2025, 136)
	PROP_StrifeType (161)
	PROP_StrifeTeaserType (157)
	PROP_StrifeTeaserType2 (160)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Inventory_MaxAmount (5)
	PROP_PowerupGiver_Powerup ("PowerMask")
	PROP_Tag ("Environmental_Suit")
	PROP_Inventory_Icon ("I_MASK")
END_DEFAULTS

const char *AEnvironmentalSuit::PickupMessage ()
{
	return "You picked up the Environmental Suit.";
}

// Guard Uniform ------------------------------------------------------------

class AGuardUniform : public AInventory
{
	DECLARE_ACTOR (AGuardUniform, AInventory)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState AGuardUniform::States[] =
{
	S_NORMAL (UNIF, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGuardUniform, Strife, 90, 0)
	PROP_StrifeType (162)
	PROP_StrifeTeaserType (158)
	PROP_StrifeTeaserType2 (161)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("Guard_Uniform")
	PROP_Inventory_Icon ("I_UNIF")
END_DEFAULTS

bool AGuardUniform::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[14]);
		return true;
	}
	return false;
}

const char *AGuardUniform::PickupMessage ()
{
	return "You picked up the Guard Uniform.";
}

// Officer's Uniform --------------------------------------------------------

class AOfficersUniform : public AInventory
{
	DECLARE_ACTOR (AOfficersUniform, AInventory)
public:
	const char *PickupMessage ();
};

FState AOfficersUniform::States[] =
{
	S_NORMAL (OFIC, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOfficersUniform, Strife, 52, 0)
	PROP_StrifeType (163)
	PROP_StrifeTeaserType (159)
	PROP_StrifeTeaserType2 (162)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("Officer's_Uniform")
	PROP_Inventory_Icon ("I_OFIC")
END_DEFAULTS

const char *AOfficersUniform::PickupMessage ()
{
	return "You picked up the Officer's Uniform.";
}

// InterrogatorReport -------------------------------------------------------
// SCRIPT32 in strife0.wad has an Acolyte that drops this, but I couldn't
// find that Acolyte in the map. It seems to be totally unused in the
// final game.

class AInterrogatorReport : public AInventory
{
	DECLARE_ACTOR (AInterrogatorReport, AInventory)
public:
	const char *PickupMessage ();
};

FState AInterrogatorReport::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AInterrogatorReport, Strife, -1, 0)
	PROP_StrifeType (308)
	PROP_StrifeTeaserType (289)
	PROP_StrifeTeaserType2 (306)
	PROP_Flags (MF_SPECIAL)
	PROP_Tag ("report")
END_DEFAULTS

const char *AInterrogatorReport::PickupMessage ()
{
	return "You picked up the report.";
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
	PROP_Inventory_FlagsSet (IF_INVBAR)
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
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_MaxAmount (100)
	PROP_Tag ("Toughness")
	PROP_Inventory_Icon ("I_HELT")
END_DEFAULTS

bool AHealthTraining::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (RUNTIME_CLASS(AGunTraining));
		AInventory *coin = Spawn<ACoin> (0,0,0);
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

// Info ---------------------------------------------------------------------

class AInfo : public AInventory
{
	DECLARE_ACTOR (AInfo, AInventory)
public:
	const char *PickupMessage ();
};

FState AInfo::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AInfo, Strife, -1, 0)
	PROP_StrifeType (300)
	PROP_StrifeTeaserType (282)
	PROP_StrifeTeaserType2 (299)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Tag ("info")
	PROP_Inventory_Icon ("I_TOKN")
END_DEFAULTS

const char *AInfo::PickupMessage ()
{
	return "You picked up the info.";
}

// Targeter -----------------------------------------------------------------

class ATargeter : public APowerupGiver
{
	DECLARE_ACTOR (ATargeter, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState ATargeter::States[] =
{
	S_NORMAL (TARG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATargeter, Strife, 207, 0)
	PROP_StrifeType (167)
	PROP_StrifeTeaserType (169)
	PROP_StrifeTeaserType2 (173)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Inventory_MaxAmount (5)
	PROP_Tag ("Targeter")
	PROP_Inventory_Icon ("I_TARG")
	PROP_PowerupGiver_Powerup ("PowerTargeter")
END_DEFAULTS

const char *ATargeter::PickupMessage ()
{
	return "You picked up the Targeter.";
}

// Scanner ------------------------------------------------------------------

class AScanner : public APowerupGiver
{
	DECLARE_ACTOR (AScanner, APowerupGiver)
public:
	const char *PickupMessage ();
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
	PROP_Inventory_MaxAmount (1)
	PROP_Tag ("scanner")
	PROP_Inventory_Icon ("I_PMUP")
	PROP_PowerupGiver_Powerup ("PowerScanner")
END_DEFAULTS

const char *AScanner::PickupMessage ()
{
	return "You picked up the scanner.";
}

bool AScanner::Use (bool pickup)
{
	if (!(level.flags & LEVEL_ALLMAP))
	{
		if (Owner == players[consoleplayer].camera)
		{
			Printf ("The scanner won't work without a map!\n");
		}
		return false;
	}
	return Super::Use (pickup);
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
	if (dropper->target == players[consoleplayer].camera)
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
	if (players[consoleplayer].camera == dropper->target)
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
	AInventory *item = toucher->FindInventory<AClipOfBullets>();
	if (item == NULL)
	{
		item = toucher->GiveInventoryType (RUNTIME_CLASS(AClipOfBullets));
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

	if (!P_GiveBody (toucher->player, skillhealths[gameskill]))
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
END_DEFAULTS

bool AUpgradeStamina::TryPickup (AActor *toucher)
{
	if (toucher->player == NULL || toucher->player->stamina >= 100)
		return false;
	toucher->player->stamina += 10;
	P_GiveBody (toucher->player, 200);
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
