#include "a_keys.h"
#include "a_strifeglobal.h"
#include "p_local.h"

IMPLEMENT_STATELESS_ACTOR (AStrifeKey, Strife, -1, 0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
END_DEFAULTS

// Base Key -----------------------------------------------------------------

class ABaseKey : public AStrifeKey
{
	DECLARE_ACTOR (ABaseKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ABaseKey::States[] =
{
	S_NORMAL (FUSL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABaseKey, Strife, 230, 0)
	PROP_StrifeType (133)
	PROP_StrifeTeaserType (129)
	PROP_StrifeTeaserType2 (132)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (1)
	PROP_Inventory_Icon ("I_FUSL")
	PROP_Tag ("Base_Key")
END_DEFAULTS

const char *ABaseKey::PickupMessage ()
{
	return "You picked up the Base Key.";
}

// Govs Key -----------------------------------------------------------------

class AGovsKey : public AStrifeKey
{
	DECLARE_ACTOR (AGovsKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AGovsKey::States[] =
{
	S_NORMAL (REBL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGovsKey, Strife, -1, 0)
	PROP_StrifeType (134)
	PROP_StrifeTeaserType (130)
	PROP_StrifeTeaserType2 (133)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (2)
	PROP_Inventory_Icon ("I_REBL")
	PROP_Tag ("Govs_Key")	// "Rebel_Key" in the Teaser
END_DEFAULTS

const char *AGovsKey::PickupMessage ()
{
	return "You picked up the Govs Key.";
}

// Passcard -----------------------------------------------------------------

class APasscard : public AStrifeKey
{
	DECLARE_ACTOR (APasscard, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState APasscard::States[] =
{
	S_NORMAL (TPAS, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APasscard, Strife, 185, 0)
	PROP_StrifeType (135)
	PROP_StrifeTeaserType (131)
	PROP_StrifeTeaserType2 (134)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (3)
	PROP_Inventory_Icon ("I_TPAS")
	PROP_Tag ("Passcard")
END_DEFAULTS

const char *APasscard::PickupMessage ()
{
	return "You picked up the Passcard.";
}

const char *APasscard::NeedKeyMessage (bool remote, int keynum)
{
	return remote ? "You need a pass card"
		: "You need a pass card to open this door";
}

// ID Badge -----------------------------------------------------------------

class AIDBadge : public AStrifeKey
{
	DECLARE_ACTOR (AIDBadge, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState AIDBadge::States[] =
{
	S_NORMAL (CRD1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AIDBadge, Strife, 184, 0)
	PROP_StrifeType (136)
	PROP_StrifeTeaserType (132)
	PROP_StrifeTeaserType2 (135)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (4)
	PROP_Inventory_Icon ("I_CRD1")
	PROP_Tag ("ID_Badge")
END_DEFAULTS

const char *AIDBadge::PickupMessage ()
{
	return "You picked up the ID Badge.";
}

const char *AIDBadge::NeedKeyMessage (bool remote, int keynum)
{
	return remote ? "You need an id badge" :
		"You need an id badge to open this door";
}

// Prison Key ---------------------------------------------------------------

class APrisonKey : public AStrifeKey
{
	DECLARE_ACTOR (APrisonKey, AStrifeKey)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState APrisonKey::States[] =
{
	S_NORMAL (PRIS, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APrisonKey, Strife, -1, 0)
	PROP_StrifeType (137)
	PROP_StrifeTeaserType (133)
	PROP_StrifeTeaserType2 (136)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (5)
	PROP_Inventory_Icon ("I_PRIS")
	PROP_Tag ("Prison_Key")
END_DEFAULTS

bool APrisonKey::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[10]);
		return true;
	}
	return false;
}

const char *APrisonKey::PickupMessage ()
{
	return "You picked up the Prsion Key.";
}

const char *APrisonKey::NeedKeyMessage (bool remote, int keynum)
{
	return "You don't have the key to the prison";
}

// Severed Hand -------------------------------------------------------------

class ASeveredHand : public AStrifeKey
{
	DECLARE_ACTOR (ASeveredHand, AStrifeKey)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState ASeveredHand::States[] =
{
	S_NORMAL (HAND, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASeveredHand, Strife, 91, 0)
	PROP_StrifeType (138)
	PROP_StrifeTeaserType (134)
	PROP_StrifeTeaserType2 (137)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (6)
	PROP_Inventory_Icon ("I_HAND")
	PROP_Tag ("Severed_Hand")
END_DEFAULTS

bool ASeveredHand::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[11]);
		return true;
	}
	return false;
}

const char *ASeveredHand::PickupMessage ()
{
	return "You picked up the Severed Hand.";
}

const char *ASeveredHand::NeedKeyMessage (bool remote, int keynum)
{
	return "Hand print not on file";
}

// Power1 Key ---------------------------------------------------------------

class APower1Key : public AStrifeKey
{
	DECLARE_ACTOR (APower1Key, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState APower1Key::States[] =
{
	S_NORMAL (PWR1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APower1Key, Strife, -1, 0)
	PROP_StrifeType (139)
	PROP_StrifeTeaserType (135)
	PROP_StrifeTeaserType2 (138)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (7)
	PROP_Inventory_Icon ("I_PWR1")
	PROP_Tag ("Power1_Key")
END_DEFAULTS

const char *APower1Key::PickupMessage ()
{
	return "You picked up the Power1 Key.";
}

// Power2 Key ---------------------------------------------------------------

class APower2Key : public AStrifeKey
{
	DECLARE_ACTOR (APower2Key, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState APower2Key::States[] =
{
	S_NORMAL (PWR2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APower2Key, Strife, -1, 0)
	PROP_StrifeType (140)
	PROP_StrifeTeaserType (136)
	PROP_StrifeTeaserType2 (139)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (8)
	PROP_Inventory_Icon ("I_PWR2")
	PROP_Tag ("Power2_Key")
END_DEFAULTS

const char *APower2Key::PickupMessage ()
{
	return "You picked up the Power2 Key.";
}

// Power3 Key ---------------------------------------------------------------

class APower3Key : public AStrifeKey
{
	DECLARE_ACTOR (APower3Key, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState APower3Key::States[] =
{
	S_NORMAL (PWR3, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APower3Key, Strife, -1, 0)
	PROP_StrifeType (141)
	PROP_StrifeTeaserType (137)
	PROP_StrifeTeaserType2 (140)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (9)
	PROP_Inventory_Icon ("I_PWR3")
	PROP_Tag ("Power3_Key")
END_DEFAULTS

const char *APower3Key::PickupMessage ()
{
	return "You picked up the Power3 Key.";
}

// Gold Key -----------------------------------------------------------------

class AGoldKey : public AStrifeKey
{
	DECLARE_ACTOR (AGoldKey, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState AGoldKey::States[] =
{
	S_NORMAL (KY1G, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGoldKey, Strife, 40, 0)
	PROP_StrifeType (142)
	PROP_StrifeTeaserType (138)
	PROP_StrifeTeaserType2 (141)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (10)
	PROP_Inventory_Icon ("I_KY1G")
	PROP_Tag ("Gold_Key")
END_DEFAULTS

const char *AGoldKey::PickupMessage ()
{
	return "You picked up the Gold Key.";
}

const char *AGoldKey::NeedKeyMessage (bool remote, int keynum)
{
	return "You need a gold key";
}

// ID Card ------------------------------------------------------------------

class AIDCard : public AStrifeKey
{
	DECLARE_ACTOR (AIDCard, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState AIDCard::States[] =
{
	S_NORMAL (CRD2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AIDCard, Strife, 13, 0)
	PROP_StrifeType (143)
	PROP_StrifeTeaserType (139)
	PROP_StrifeTeaserType2 (142)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (11)
	PROP_Inventory_Icon ("I_CRD2")
	PROP_Tag ("ID_Card")
END_DEFAULTS

const char *AIDCard::PickupMessage ()
{
	return "You picked up the ID Card.";
}

const char *AIDCard::NeedKeyMessage (bool remote, int keynum)
{
	return remote ? "You need an id card"
		: "You need an id card to open this door";
}


// Silver Key ---------------------------------------------------------------

class ASilverKey : public AStrifeKey
{
	DECLARE_ACTOR (ASilverKey, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState ASilverKey::States[] =
{
	S_NORMAL (KY2S, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASilverKey, Strife, 38, 0)
	PROP_StrifeType (144)
	PROP_StrifeTeaserType (140)
	PROP_StrifeTeaserType2 (143)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (12)
	PROP_Inventory_Icon ("I_KY2S")
	PROP_Tag ("Silver_Key")
END_DEFAULTS

const char *ASilverKey::PickupMessage ()
{
	return "You picked up the Silver Key.";
}

const char *ASilverKey::NeedKeyMessage (bool remote, int keynum)
{
	return "You need a silver key";
}

// Oracle Key ---------------------------------------------------------------

class AOracleKey : public AStrifeKey
{
	DECLARE_ACTOR (AOracleKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AOracleKey::States[] =
{
	S_NORMAL (ORAC, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOracleKey, Strife, 61, 0)
	PROP_StrifeType (145)
	PROP_StrifeTeaserType (141)
	PROP_StrifeTeaserType2 (144)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (13)
	PROP_Inventory_Icon ("I_ORAC")
	PROP_Tag ("Oracle_Key")
END_DEFAULTS

const char *AOracleKey::PickupMessage ()
{
	return "You picked up the Oracle Key.";
}

// Military ID --------------------------------------------------------------

class AMilitaryID : public AStrifeKey
{
	DECLARE_ACTOR (AMilitaryID, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AMilitaryID::States[] =
{
	S_NORMAL (GYID, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMilitaryID, Strife, -1, 0)
	PROP_StrifeType (146)
	PROP_StrifeTeaserType (142)
	PROP_StrifeTeaserType2 (145)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (14)
	PROP_Inventory_Icon ("I_GYID")
	PROP_Tag ("Military ID")
END_DEFAULTS

const char *AMilitaryID::PickupMessage ()
{
	return "You picked up the Military ID.";
}

// Order Key ----------------------------------------------------------------

class AOrderKey : public AStrifeKey
{
	DECLARE_ACTOR (AOrderKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AOrderKey::States[] =
{
	S_NORMAL (FUBR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOrderKey, Strife, 86, 0)
	PROP_StrifeType (147)
	PROP_StrifeTeaserType (143)
	PROP_StrifeTeaserType2 (146)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (15)
	PROP_Inventory_Icon ("I_FUBR")
	PROP_Tag ("Order_Key")
END_DEFAULTS

const char *AOrderKey::PickupMessage ()
{
	return "You picked up the Order Key.";
}

// Warehouse Key ------------------------------------------------------------

class AWarehouseKey : public AStrifeKey
{
	DECLARE_ACTOR (AWarehouseKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AWarehouseKey::States[] =
{
	S_NORMAL (WARE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AWarehouseKey, Strife, 166, 0)
	PROP_StrifeType (148)
	PROP_StrifeTeaserType (144)
	PROP_StrifeTeaserType2 (147)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (16)
	PROP_Inventory_Icon ("I_WARE")
	PROP_Tag ("Warehouse_Key")
END_DEFAULTS

const char *AWarehouseKey::PickupMessage ()
{
	return "You picked up the Warehouse Key.";
}

// Brass Key ----------------------------------------------------------------

class ABrassKey : public AStrifeKey
{
	DECLARE_ACTOR (ABrassKey, AStrifeKey)
public:
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState ABrassKey::States[] =
{
	S_NORMAL (KY3B, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABrassKey, Strife, -1, 0)
	PROP_StrifeType (149)
	PROP_StrifeTeaserType (145)
	PROP_StrifeTeaserType2 (148)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (17)
	PROP_Inventory_Icon ("I_KY3B")
	PROP_Tag ("Brass_Key")
END_DEFAULTS

const char *ABrassKey::PickupMessage ()
{
	return "You picked up the Brass Key.";
}

const char *ABrassKey::NeedKeyMessage (bool remote, int keynum)
{
	return "You need a brass key";
}

// Red Crystal Key ----------------------------------------------------------

class ARedCrystalKey : public AStrifeKey
{
	DECLARE_ACTOR (ARedCrystalKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ARedCrystalKey::States[] =
{
	S_BRIGHT (RCRY, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARedCrystalKey, Strife, 192, 0)
	PROP_StrifeType (150)
	PROP_StrifeTeaserType (146)
	PROP_StrifeTeaserType2 (149)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (18)
	PROP_Inventory_Icon ("I_RCRY")
	PROP_Tag ("Red_Crystal_Key")
END_DEFAULTS

const char *ARedCrystalKey::PickupMessage ()
{
	return "You picked up the Red Crystal Key.";
}

// Blue Crystal Key ---------------------------------------------------------

class ABlueCrystalKey : public AStrifeKey
{
	DECLARE_ACTOR (ABlueCrystalKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ABlueCrystalKey::States[] =
{
	S_BRIGHT (BCRY, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABlueCrystalKey, Strife, 193, 0)
	PROP_StrifeType (151)
	PROP_StrifeTeaserType (147)
	PROP_StrifeTeaserType2 (150)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (19)
	PROP_Inventory_Icon ("I_BCRY")
	PROP_Tag ("Blue_Crystal_Key")
END_DEFAULTS

const char *ABlueCrystalKey::PickupMessage ()
{
	return "You picked up the Blue Crystal Key.";
}

// Chapel Key ---------------------------------------------------------------

class AChapelKey : public AStrifeKey
{
	DECLARE_ACTOR (AChapelKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AChapelKey::States[] =
{
	S_NORMAL (CHAP, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AChapelKey, Strife, 195, 0)
	PROP_StrifeType (152)
	PROP_StrifeTeaserType (148)
	PROP_StrifeTeaserType2 (151)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (20)
	PROP_Inventory_Icon ("I_CHAP")
	PROP_Tag ("Chapel_Key")
END_DEFAULTS

const char *AChapelKey::PickupMessage ()
{
	return "You picked up the Chapel Key.";
}

// Catacomb Key -------------------------------------------------------------

class ACatacombKey : public AStrifeKey
{
	DECLARE_ACTOR (ACatacombKey, AStrifeKey)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
};

FState ACatacombKey::States[] =
{
	S_NORMAL (TUNL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACatacombKey, Strife, -1, 0)
	PROP_StrifeType (153)
	PROP_StrifeTeaserType (149)
	PROP_StrifeTeaserType2 (152)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (21)
	PROP_Inventory_Icon ("I_TUNL")
	PROP_Tag ("Catacomb_Key")	// "Tunnel_Key" in the Teaser
END_DEFAULTS

bool ACatacombKey::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[27]);
		return true;
	}
	return false;
}

const char *ACatacombKey::PickupMessage ()
{
	return "You picked up the Catacomb Key.";
}

// Security Key -------------------------------------------------------------

class ASecurityKey : public AStrifeKey
{
	DECLARE_ACTOR (ASecurityKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ASecurityKey::States[] =
{
	S_NORMAL (SECK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASecurityKey, Strife, -1, 0)
	PROP_StrifeType (154)
	PROP_StrifeTeaserType (150)
	PROP_StrifeTeaserType2 (153)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (22)
	PROP_Inventory_Icon ("I_SECK")
	PROP_Tag ("Security_Key")
END_DEFAULTS

const char *ASecurityKey::PickupMessage ()
{
	return "You picked up the Security Key.";
}

// Core Key -----------------------------------------------------------------

class ACoreKey : public AStrifeKey
{
	DECLARE_ACTOR (ACoreKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ACoreKey::States[] =
{
	S_NORMAL (GOID, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACoreKey, Strife, 236, 0)
	PROP_StrifeType (155)
	PROP_StrifeTeaserType (151)
	PROP_StrifeTeaserType2 (154)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (23)
	PROP_Inventory_Icon ("I_GOID")
	PROP_Tag ("Core_Key")	// "New_Key1" in the Teaser
END_DEFAULTS

const char *ACoreKey::PickupMessage ()
{
	return "You picked up the Core Key.";
}

// Mauler Key ---------------------------------------------------------------

class AMaulerKey : public AStrifeKey
{
	DECLARE_ACTOR (AMaulerKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AMaulerKey::States[] =
{
	S_NORMAL (BLTK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMaulerKey, Strife, 233, 0)
	PROP_StrifeType (156)
	PROP_StrifeTeaserType (152)
	PROP_StrifeTeaserType2 (155)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (24)
	PROP_Inventory_Icon ("I_BLTK")
	PROP_Tag ("Mauler_Key")	// "New_Key2" in the Teaser
END_DEFAULTS

const char *AMaulerKey::PickupMessage ()
{
	return "You picked up the Mauler Key.";
}

// Factory Key --------------------------------------------------------------

class AFactoryKey : public AStrifeKey
{
	DECLARE_ACTOR (AFactoryKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AFactoryKey::States[] =
{
	S_NORMAL (PROC, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AFactoryKey, Strife, 234, 0)
	PROP_StrifeType (157)
	PROP_StrifeTeaserType (153)
	PROP_StrifeTeaserType2 (156)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (25)
	PROP_Inventory_Icon ("I_PROC")
	PROP_Tag ("Factory_Key")	// "New_Key3" in the Teaser
END_DEFAULTS

const char *AFactoryKey::PickupMessage ()
{
	return "You picked up the Factory Key.";
}

// Mine Key -----------------------------------------------------------------

class AMineKey : public AStrifeKey
{
	DECLARE_ACTOR (AMineKey, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState AMineKey::States[] =
{
	S_NORMAL (MINE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMineKey, Strife, 235, 0)
	PROP_StrifeType (158)
	PROP_StrifeTeaserType (154)
	PROP_StrifeTeaserType2 (157)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (26)
	PROP_Inventory_Icon ("I_MINE")	// "New_Key4" in the Teaser
	PROP_Tag ("MINE_KEY")
END_DEFAULTS

const char *AMineKey::PickupMessage ()
{
	return "You picked up the Mine Key.";
}

// New Key5 -----------------------------------------------------------------

class ANewKey5 : public AStrifeKey
{
	DECLARE_ACTOR (ANewKey5, AStrifeKey)
public:
	const char *PickupMessage ();
};

FState ANewKey5::States[] =
{
	S_NORMAL (BLTK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ANewKey5, Strife, -1, 0)
	PROP_StrifeType (159)
	PROP_StrifeTeaserType (155)
	PROP_StrifeTeaserType2 (158)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (27)
	PROP_Inventory_Icon ("I_BLTK")
	PROP_Tag ("New_Key5")
END_DEFAULTS

const char *ANewKey5::PickupMessage ()
{
	return "You picked up the New Key5.";
}

// Prison Pass --------------------------------------------------------------

class APrisonPass : public AKey
{
	DECLARE_ACTOR (APrisonPass, AKey)
public:
	bool TryPickup (AActor *toucher);
	bool SpecialDropAction (AActor *dropper);
	const char *PickupMessage ();
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
	PROP_Key_KeyNumber (50)
	PROP_Inventory_Icon ("I_TOKN")
	PROP_Tag ("Prison_pass")
END_DEFAULTS

bool APrisonPass::TryPickup (AActor *toucher)
{
	Super::TryPickup (toucher);
	EV_DoDoor (DDoor::doorOpen, NULL, toucher, 223, 2*FRACUNIT, 0, 0, 0);
	toucher->GiveInventoryType (QuestItemClasses[9]);
	return true;
}

const char *APrisonPass::PickupMessage ()
{
	return "You picked up the Prison pass.";
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

// Oracle Pass --------------------------------------------------------------

class AOraclePass : public AKey
{
	DECLARE_ACTOR (AOraclePass, AKey)
public:
	bool TryPickup (AActor *toucher);
	const char *PickupMessage ();
	const char *NeedKeyMessage (bool remote, int keynum);
};

FState AOraclePass::States[] =
{
	S_NORMAL (OTOK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOraclePass, Strife, -1, 0)
	PROP_StrifeType (311)
	PROP_StrifeTeaserType (292)
	PROP_StrifeTeaserType2 (309)
	PROP_SpawnState (0)
	PROP_Key_KeyNumber (51)
	PROP_Inventory_Icon ("I_OTOK")
	PROP_Tag ("Oracle_Pass")
END_DEFAULTS

bool AOraclePass::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		toucher->GiveInventoryType (QuestItemClasses[17]);
		return true;
	}
	return false;
}

const char *AOraclePass::PickupMessage ()
{
	return "You picked up the Oracle Pass.";
}

const char *AOraclePass::NeedKeyMessage (bool remote, int keynum)
{
	return "You need the Oracle Pass!";
}
