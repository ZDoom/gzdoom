#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "c_console.h"

enum
{
	puzzskull,
	puzzgembig,
	puzzgemred,
	puzzgemgreen1,
	puzzgemgreen2,
	puzzgemblue1,
	puzzgemblue2,
	puzzbook1,
	puzzbook2,
	puzzskull2,
	puzzfweapon,
	puzzcweapon,
	puzzmweapon,
	puzzgear1,
	puzzgear2,
	puzzgear3,
	puzzgear4,
};

IMPLEMENT_STATELESS_ACTOR (APuzzleItem, Any, -1, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_Inventory_FlagsSet (IF_INVBAR)
	PROP_Inventory_DefMaxAmount
	PROP_UseSound ("PuzzleSuccess")
END_DEFAULTS

void APuzzleItem::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PuzzleItemNumber;
}

bool APuzzleItem::HandlePickup (AInventory *item)
{
	// Can't carry more than 1 of each puzzle item in coop netplay
	if (multiplayer && !deathmatch && item->GetClass() == GetClass())
	{
		return true;
	}
	return Super::HandlePickup (item);
}

bool APuzzleItem::Use ()
{
	if (P_UsePuzzleItem (Owner, PuzzleItemNumber))
	{
		if (--Amount == 0)
		{
			Owner->RemoveInventory (this);
			Destroy ();
		}
		return true;
	}
	// [RH] Always play the sound if the use fails.
	S_Sound (Owner, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	C_MidPrintBold (GStrings(TXT_USEPUZZLEFAILED));
	return false;
}

void APuzzleItem::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/i_pkup", 1, ATTN_NORM);
}

bool APuzzleItem::ShouldStay ()
{
	return !!multiplayer;
}

// Yorick's Skull -----------------------------------------------------------

class APuzzSkull : public APuzzleItem
{
	DECLARE_ACTOR (APuzzSkull, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzSkull::States[] =
{
	S_NORMAL (ASKU, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzSkull, Hexen, 9002, 76)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzskull)
	PROP_Inventory_Icon ("ARTISKLL")
END_DEFAULTS

const char *APuzzSkull::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZSKULL);
}

// Heart of D'Sparil --------------------------------------------------------

class APuzzGemBig : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemBig, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemBig::States[] =
{
	S_NORMAL (ABGM, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBig, Hexen, 9003, 77)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgembig)
	PROP_Inventory_Icon ("ARTIBGEM")
END_DEFAULTS

const char *APuzzGemBig::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMBIG);
}

// Red Gem (Ruby Planet) ----------------------------------------------------

class APuzzGemRed : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemRed, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemRed::States[] =
{
	S_NORMAL (AGMR, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemRed, Hexen, 9004, 78)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgemred)
	PROP_Inventory_Icon ("ARTIGEMR")
END_DEFAULTS

const char *APuzzGemRed::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMRED);
}

// Green Gem 1 (Emerald Planet) ---------------------------------------------

class APuzzGemGreen1 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemGreen1, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemGreen1::States[] =
{
	S_NORMAL (AGMG, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemGreen1, Hexen, 9005, 79)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgemgreen1)
	PROP_Inventory_Icon ("ARTIGEMG")
END_DEFAULTS

const char *APuzzGemGreen1::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMGREEN1);
}

// Green Gem 2 (Emerald Planet) ---------------------------------------------

class APuzzGemGreen2 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemGreen2, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemGreen2::States[] =
{
	S_NORMAL (AGG2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemGreen2, Hexen, 9009, 80)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgemgreen2)
	PROP_Inventory_Icon ("ARTIGMG2")
END_DEFAULTS

const char *APuzzGemGreen2::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMGREEN2);
}

// Blue Gem 1 (Sapphire Planet) ---------------------------------------------

class APuzzGemBlue1 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemBlue1, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemBlue1::States[] =
{
	S_NORMAL (AGMB, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBlue1, Hexen, 9006, 81)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgemblue1)
	PROP_Inventory_Icon ("ARTIGEMB")
END_DEFAULTS

const char *APuzzGemBlue1::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMBLUE1);
}

// Blue Gem 2 (Sapphire Planet) ---------------------------------------------

class APuzzGemBlue2 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGemBlue2, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGemBlue2::States[] =
{
	S_NORMAL (AGB2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBlue2, Hexen, 9010, 82)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgemblue2)
	PROP_Inventory_Icon ("ARTIGMB2")
END_DEFAULTS

const char *APuzzGemBlue2::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEMBLUE2);
}

// Book 1 (Daemon Codex) ----------------------------------------------------

class APuzzBook1 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzBook1, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzBook1::States[] =
{
	S_NORMAL (ABK1, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzBook1, Hexen, 9007, 83)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzbook1)
	PROP_Inventory_Icon ("ARTIBOK1")
END_DEFAULTS

const char *APuzzBook1::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZBOOK1);
}

// Book 2 (Liber Oscura) ----------------------------------------------------

class APuzzBook2 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzBook2, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzBook2::States[] =
{
	S_NORMAL (ABK2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzBook2, Hexen, 9008, 84)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzbook2)
	PROP_Inventory_Icon ("ARTIBOK2")
END_DEFAULTS

const char *APuzzBook2::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZBOOK2);
}

// Flame Mask ---------------------------------------------------------------

class APuzzFlameMask : public APuzzleItem
{
	DECLARE_ACTOR (APuzzFlameMask, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzFlameMask::States[] =
{
	S_NORMAL (ASK2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzFlameMask, Hexen, 9014, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzskull2)
	PROP_Inventory_Icon ("ARTISKL2")
END_DEFAULTS

const char *APuzzFlameMask::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZSKULL2);
}

// Fighter Weapon (Glaive Seal) ---------------------------------------------

class APuzzFWeapon : public APuzzleItem
{
	DECLARE_ACTOR (APuzzFWeapon, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzFWeapon::States[] =
{
	S_NORMAL (AFWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzFWeapon, Hexen, 9015, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzfweapon)
	PROP_Inventory_Icon ("ARTIFWEP")
END_DEFAULTS

const char *APuzzFWeapon::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZFWEAPON);
}

// Cleric Weapon (Holy Relic) -----------------------------------------------

class APuzzCWeapon : public APuzzleItem
{
	DECLARE_ACTOR (APuzzCWeapon, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzCWeapon::States[] =
{
	S_NORMAL (ACWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzCWeapon, Hexen, 9016, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzcweapon)
	PROP_Inventory_Icon ("ARTICWEP")
END_DEFAULTS

const char *APuzzCWeapon::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZCWEAPON);
}

// Mage Weapon (Sigil of the Magus) -----------------------------------------

class APuzzMWeapon : public APuzzleItem
{
	DECLARE_ACTOR (APuzzMWeapon, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzMWeapon::States[] =
{
	S_NORMAL (AMWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzMWeapon, Hexen, 9017, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzmweapon)
	PROP_Inventory_Icon ("ARTIMWEP")
END_DEFAULTS

const char *APuzzMWeapon::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZMWEAPON);
}

// Clock Gear 1 -------------------------------------------------------------

class APuzzGear1 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGear1, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGear1::States[] =
{
	S_BRIGHT (AGER, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGER, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGER, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGER, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGER, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGER, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGER, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGER, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear1, Hexen, 9018, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgear1)
	PROP_Inventory_Icon ("ARTIGEAR")
END_DEFAULTS

const char *APuzzGear1::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEAR);
}

// Clock Gear 2 -------------------------------------------------------------

class APuzzGear2 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGear2, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGear2::States[] =
{
	S_BRIGHT (AGR2, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR2, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR2, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR2, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR2, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR2, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR2, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR2, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear2, Hexen, 9019, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgear2)
	PROP_Inventory_Icon ("ARTIGER2")
END_DEFAULTS

const char *APuzzGear2::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEAR);
}

// Clock Gear 3 -------------------------------------------------------------

class APuzzGear3 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGear3, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGear3::States[] =
{
	S_BRIGHT (AGR3, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR3, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR3, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR3, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR3, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR3, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR3, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR3, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear3, Hexen, 9020, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgear3)
	PROP_Inventory_Icon ("ARTIGER3")
END_DEFAULTS

const char *APuzzGear3::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEAR);
}

// Clock Gear 4 -------------------------------------------------------------

class APuzzGear4 : public APuzzleItem
{
	DECLARE_ACTOR (APuzzGear4, APuzzleItem)
public:
	const char *PickupMessage ();
};

FState APuzzGear4::States[] =
{
	S_BRIGHT (AGR4, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR4, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR4, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR4, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR4, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR4, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR4, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR4, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear4, Hexen, 9021, 0)
	PROP_SpawnState (0)
	PROP_PuzzleItem_Number (puzzgear4)
	PROP_Inventory_Icon ("ARTIGER4")
END_DEFAULTS

const char *APuzzGear4::PickupMessage ()
{
	return GStrings(TXT_ARTIPUZZGEAR);
}
